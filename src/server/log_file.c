/*
 * Copyright 2024 Humberto Gomes, José Lopes, José Matos
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @file  server/log_file.c
 * @brief Implementation of methods in server/log_file.h
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "protocol.h"
#include "server/log_file.h"

/**
 * @struct log_file
 * @brief  A handle for a log file.
 *
 * @var log_file::fd
 *     @brief   File descriptor of the open log file.
 *     @details Must be kept at the end of the file.
 * @var log_file::writable
 *     @brief Whether it's possible to `write()` to log_file::fd.
 */
struct log_file {
    int fd, writable;
};

/**
 * @struct log_file_serialized_task_t
 * @brief  Information about a ::tagged_task_t in a serializable form.
 *
 * @var log_file_serialized_task_t::id
 *     @brief See tagged_task::id.
 * @var log_file_serialized_task_t::command_length
 *     @brief Length of string in log_file_serialized_task_t::command_line.
 * @var log_file_serialized_task_t::expected_time
 *     @brief See tagged_task::expected_time.
 * @var log_file_serialized_task_t::times
 *     @brief See tagged_task::times.
 * @var log_file_serialized_task_t::command_line
 *     @brief See tagged_task::command_line. **Not null-terminated.**
 */
typedef struct __attribute__((packed)) {
    uint32_t        id, command_length, expected_time;
    struct timespec times[TAGGED_TASK_TIME_COMPLETED + 1];
    char            command_line[PROTOCOL_MAXIMUM_COMMAND_LENGTH];
} log_file_serialized_task_t;

/**
 * @brief Serializes a ::tagged_task_t.
 *
 * @param in  Task to serialize.
 * @param out Where to write the serialized task to.
 *
 * @retval 0 Success.
 * @retval 1 Failure (check `errno`).
 *
 * | `errno`    | Cause                                    |
 * | ---------- | ---------------------------------------- |
 * | `EINVAL`   | @p in or @p out are `NULL`.              |
 * | `EMSGSIZE` | tagged_task_t::command_line is too long. |
 */
int __log_file_serialize_task(const tagged_task_t *in, log_file_serialized_task_t *out) {
    if (!in || !out) {
        errno = EINVAL;
        return 1;
    }

    out->id            = tagged_task_get_id(in);
    out->expected_time = tagged_task_get_expected_time(in);

    memset(out->command_line, 0, PROTOCOL_MAXIMUM_COMMAND_LENGTH);
    const char *command_line = tagged_task_get_command_line(in);
    out->command_length      = strlen(command_line);
    if (out->command_length > PROTOCOL_MAXIMUM_COMMAND_LENGTH) {
        errno = EMSGSIZE;
        return 1;
    }
    memcpy(out->command_line, command_line, out->command_length);

    for (tagged_task_time_t i = TAGGED_TASK_TIME_SENT; i <= TAGGED_TASK_TIME_COMPLETED; ++i) {
        const struct timespec *time = tagged_task_get_time(in, i);
        if (time)
            out->times[i] = *time;
        else
            out->times[i].tv_sec = out->times[i].tv_nsec = 0;
    }

    return 0;
}

/**
 * @brief Deserializes a task in a log file.
 * @param in Serialized task to deserialize.
 *
 * @return A task owned by the caller on success, `NULL` on failure (check `errno`).
 *
 * | `errno`    | Cause                                                |
 * | ---------- | ---------------------------------------------------- |
 * | `EINVAL`   | @p in is `NULL`.                                     |
 * | `EMSGSIZE` | Invalid serialized task with very long command line. |
 * | `ENOMEM`   | Allocation failure.                                  |
 */
tagged_task_t *__log_file_deserialize_task(const log_file_serialized_task_t *in) {
    if (!in) {
        errno = EINVAL;
        return NULL;
    }

    char command_line[PROTOCOL_MAXIMUM_COMMAND_LENGTH + 1];
    if (in->command_length <= PROTOCOL_MAXIMUM_COMMAND_LENGTH) {
        memcpy(command_line, in->command_line, in->command_length);
        command_line[in->command_length] = '\0';
    } else {
        errno = EMSGSIZE;
        return NULL;
    }

    tagged_task_t *task =
        tagged_task_new_from_command_line(command_line, in->id, in->expected_time);
    if (!task)
        return NULL; /* Keep ENOMEM */

    for (tagged_task_time_t i = TAGGED_TASK_TIME_SENT; i <= TAGGED_TASK_TIME_COMPLETED; ++i) {
        struct timespec aligned_copy = in->times[i];
        (void) tagged_task_set_time(task, i, &aligned_copy);
    }

    return task;
}

log_file_t *log_file_new(const char *path, int writable) {
    if (!path) {
        errno = EINVAL;
        return NULL;
    }

    log_file_t *log_file = malloc(sizeof(log_file_t));
    if (!log_file) {
        errno = ENOMEM;
        return NULL;
    }

    log_file->writable = writable;
    if (writable)
        log_file->fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0600);
    else
        log_file->fd = open(path, O_RDONLY);

    if (log_file->fd < 0) {
        free(log_file);
        return NULL; /* Keep errno */
    }
    return log_file;
}

void log_file_free(log_file_t *log_file) {
    if (!log_file)
        return; /* Don't set errno, as that's not typical free() behavior */

    (void) close(log_file->fd);
    free(log_file);
}

int log_file_write_task(log_file_t *log_file, const tagged_task_t *task) {
    if (!task || !log_file || !log_file->writable) {
        errno = EINVAL;
        return 1;
    }

    log_file_serialized_task_t serialized;
    if (__log_file_serialize_task(task, &serialized))
        return 1; /* Keep errno */

    /* TODO - variable size message */
    if (write(log_file->fd, &serialized, sizeof(log_file_serialized_task_t)) !=
        sizeof(log_file_serialized_task_t)) /* No partial write risk */
        return 1;                           /* Keep errno */
    return 0;
}

/** @brief Number of bytes read in ::log_file_read_tasks */
#define LOG_FILE_READ_BUFFER_SIZE (4 * sizeof(log_file_serialized_task_t))

int log_file_read_tasks(log_file_t *log_file, log_file_task_callback_t task_cb, void *state) {
    if (!log_file || !task_cb) {
        errno = EINVAL;
        return 1;
    }

    if (lseek(log_file->fd, 0, SEEK_SET) == (off_t) -1)
        return 1;

    uint8_t buf[LOG_FILE_READ_BUFFER_SIZE];
    ssize_t bytes_read = 0;
    while ((bytes_read = read(log_file->fd, buf, LOG_FILE_READ_BUFFER_SIZE))) {
        size_t tasks_read = bytes_read / sizeof(log_file_serialized_task_t);
        if (bytes_read % sizeof(log_file_serialized_task_t) != 0) {
            fprintf(stderr, "Read too many / few bytes for task in log file\n");
            (void) lseek(log_file->fd, 0, SEEK_END);
            errno = EILSEQ;
            return 1;
        }

        for (size_t i = 0; i < tasks_read; ++i) {
            log_file_serialized_task_t *serialized = (log_file_serialized_task_t *) buf + i;
            tagged_task_t              *task       = __log_file_deserialize_task(serialized);
            if (!task) {
                int errno2 = errno;
                fprintf(stderr, "Log file: task deserialization failure!\n");
                (void) lseek(log_file->fd, 0, SEEK_END);

                if (errno2 == ENOMEM)
                    errno = ENOMEM;
                else
                    errno = EILSEQ;
                return 1;
            }

            int cb_ret = task_cb(task, state);
            if (cb_ret) {
                tagged_task_free(task);
                (void) lseek(log_file->fd, 0, SEEK_END);
                return cb_ret;
            }

            tagged_task_free(task);
        }
    }

    (void) lseek(log_file->fd, 0, SEEK_END);
    return bytes_read != 0;
}
