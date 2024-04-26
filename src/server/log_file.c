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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "protocol.h"
#include "server/log_file.h"
#include "util.h"

/**
 * @struct log_file
 * @brief  A handle for an open log file.
 *
 * @var log_file::fd
 *     @brief   File descriptor of the open log file.
 *     @details Its offset must always be kept at the end of the file.
 * @var log_file::writable
 *     @brief Whether it's possible to `write()` to log_file::fd.
 * @var log_file::task_count
 *     @brief   The number of tasks written to a file.
 *     @details For synchronization purposes, as children that read the server's status mustn't read
 *              more from the file that what was there when `fork()` was called.
 */
struct log_file {
    int    fd, writable;
    size_t task_count;
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
 * @var log_file_serialized_task_t::error
 *     @brief Whether an error occurred while running this task.
 * @var log_file_serialized_task_t::times
 *     @brief See tagged_task::times.
 * @var log_file_serialized_task_t::command_line
 *     @brief See tagged_task::command_line. **Not null-terminated.**
 */
typedef struct __attribute__((packed)) {
    uint32_t        id, command_length, expected_time;
    uint8_t         error;
    struct timespec times[TAGGED_TASK_TIME_COMPLETED + 1];
    char            command_line[PROTOCOL_MAXIMUM_COMMAND_LENGTH];
} log_file_serialized_task_t;

/**
 * @brief Serializes a ::tagged_task_t.
 *
 * @param task  Task to serialize.
 * @param out   Where to write the serialized task to.
 * @param error Whether an error occurred while running the task.
 *
 * @retval 0 Success.
 * @retval 1 Failure (check `errno`).
 *
 * | `errno`    | Cause                                    |
 * | ---------- | ---------------------------------------- |
 * | `EINVAL`   | @p task or @p out are `NULL`.            |
 * | `EMSGSIZE` | tagged_task_t::command_line is too long. |
 */
int __log_file_serialize_task(const tagged_task_t        *task,
                              log_file_serialized_task_t *out,
                              int                         error) {
    if (!task || !out) {
        errno = EINVAL;
        return 1;
    }

    const char *command_line = tagged_task_get_command_line(task);
    out->command_length      = strlen(command_line);
    if (out->command_length > PROTOCOL_MAXIMUM_COMMAND_LENGTH) {
        errno = EMSGSIZE;
        return 1;
    }
    memset(out->command_line, 0, PROTOCOL_MAXIMUM_COMMAND_LENGTH);
    memcpy(out->command_line, command_line, out->command_length);

    out->id            = tagged_task_get_id(task);
    out->expected_time = tagged_task_get_expected_time(task);
    out->error         = error;

    for (tagged_task_time_t i = TAGGED_TASK_TIME_SENT; i <= TAGGED_TASK_TIME_COMPLETED; ++i) {
        const struct timespec *time = tagged_task_get_time(task, i);
        if (time)
            out->times[i] = *time;
        else
            out->times[i].tv_sec = out->times[i].tv_nsec = 0;
    }
    return 0;
}

/**
 * @brief Deserializes a task in a log file.
 * @param task Serialized task to deserialize.
 *
 * @return A task owned by the caller on success, `NULL` on failure (check `errno`).
 *
 * | `errno`    | Cause                                                |
 * | ---------- | ---------------------------------------------------- |
 * | `EINVAL`   | @p in is `NULL`.                                     |
 * | `EMSGSIZE` | Invalid serialized task with very long command line. |
 * | `ENOMEM`   | Allocation failure.                                  |
 */
tagged_task_t *__log_file_deserialize_task(const log_file_serialized_task_t *task) {
    if (!task) {
        errno = EINVAL;
        return NULL;
    }

    char command_line[PROTOCOL_MAXIMUM_COMMAND_LENGTH + 1];
    if (task->command_length <= PROTOCOL_MAXIMUM_COMMAND_LENGTH) {
        memcpy(command_line, task->command_line, task->command_length);
        command_line[task->command_length] = '\0';
    } else {
        errno = EMSGSIZE;
        return NULL;
    }

    tagged_task_t *ret =
        tagged_task_new_from_command_line(command_line, task->id, task->expected_time);
    if (!task)
        return NULL; /* Keep ENOMEM */

    for (tagged_task_time_t i = TAGGED_TASK_TIME_SENT; i <= TAGGED_TASK_TIME_COMPLETED; ++i) {
        struct timespec aligned_copy = task->times[i];
        tagged_task_set_time(ret, i, &aligned_copy);
    }

    return ret;
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

    log_file->writable   = writable;
    log_file->task_count = 0;
    if (writable)
        log_file->fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0640);
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

int log_file_write_task(log_file_t *log_file, const tagged_task_t *task, int error) {
    if (!task || !log_file || !log_file->writable) {
        errno = EINVAL;
        return 1;
    }

    log_file_serialized_task_t serialized;
    if (__log_file_serialize_task(task, &serialized, error))
        return 1; /* Keep errno */

    if (write(log_file->fd, &serialized, sizeof(log_file_serialized_task_t)) !=
        sizeof(log_file_serialized_task_t)) /* No partial write risk */
        return 1;                           /* Keep errno */
    log_file->task_count++;
    return 0;
}

/** @brief Number of bytes read in ::log_file_read_tasks. */
#define LOG_FILE_READ_BUFFER_SIZE (4 * sizeof(log_file_serialized_task_t))

int log_file_read_tasks(log_file_t *log_file, log_file_task_callback_t task_cb, void *state) {
    if (!log_file || !task_cb) {
        errno = EINVAL;
        return 1;
    }

    if (lseek(log_file->fd, 0, SEEK_SET) == (off_t) -1)
        return 1;

    uint8_t buf[LOG_FILE_READ_BUFFER_SIZE];
    ssize_t bytes_read      = 0;
    size_t  outputted_tasks = 0;
    while ((bytes_read = read(log_file->fd, buf, LOG_FILE_READ_BUFFER_SIZE))) {
        size_t tasks_read = bytes_read / sizeof(log_file_serialized_task_t);
        if (bytes_read % sizeof(log_file_serialized_task_t) != 0) {
            util_error("%s(): read too many / few bytes for task in log file\n", __func__);
            (void) lseek(log_file->fd, 0, SEEK_END);
            errno = EILSEQ;
            return 1;
        }

        for (size_t i = 0; i < tasks_read; ++i) {
            log_file_serialized_task_t *serialized = (log_file_serialized_task_t *) buf + i;
            tagged_task_t              *task       = __log_file_deserialize_task(serialized);
            if (!task) {
                int errno2 = errno;
                util_error("%s(): task deserialization failure!\n", __func__);
                (void) lseek(log_file->fd, 0, SEEK_END);

                if (errno2 == ENOMEM)
                    errno = ENOMEM;
                else
                    errno = EILSEQ;
                return 1;
            }

            int cb_ret = task_cb(task, serialized->error, state);
            if (cb_ret) {
                tagged_task_free(task);
                (void) lseek(log_file->fd, 0, SEEK_END);
                return cb_ret;
            }

            tagged_task_free(task);

            outputted_tasks++;
            if (outputted_tasks == log_file->task_count)
                break;
        }
    }

    (void) lseek(log_file->fd, 0, SEEK_END);
    return bytes_read != 0;
}
