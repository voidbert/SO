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
#include <sys/types.h>
#include <unistd.h>

#include "server/log_file.h"
#include "server/task_iterator.h"

struct log_file {
    int create_fd;
    int read_fd;
    int write_fd;
};

log_file_t *log_file_new(const char *filename) {
    if (!filename) {
        errno = EINVAL;
        return NULL;
    }

    log_file_t *log_file = malloc(sizeof(log_file_t));
    if (log_file == NULL) {
        errno = ENOMEM;
        return NULL;
    }

    log_file->create_fd = open(filename, O_CREAT | O_TRUNC, 0644);
    if (log_file->create_fd < 0) {
        free(log_file);
        errno = EINVAL;
        return NULL;
    }

    log_file->read_fd = open(filename, O_RDONLY, 0644);
    if (log_file->read_fd < 0) {
        free(log_file);
        errno = EINVAL;
        return NULL;
    }

    log_file->write_fd = open(filename, O_WRONLY | O_APPEND, 0644);
    if (log_file->write_fd < 0) {
        free(log_file);
        errno = EINVAL;
        return NULL;
    }

    return log_file;
}

void log_file_free(log_file_t *log_file) {
    if (!log_file) {
        errno = EINVAL;
        return;
    }

    close(log_file->create_fd);
    close(log_file->read_fd);
    close(log_file->write_fd);
    free(log_file);
}

int log_file_get_create_fd(const log_file_t *log_file) {
    if (!log_file) {
        errno = EINVAL;
        return -1;
    }

    return log_file->create_fd;
}

int log_file_get_read_fd(const log_file_t *log_file) {
    if (!log_file) {
        errno = EINVAL;
        return -1;
    }

    return log_file->read_fd;
}

int log_file_get_write_fd(const log_file_t *log_file) {
    if (!log_file) {
        errno = EINVAL;
        return -1;
    }

    return log_file->write_fd;
}

void log_file_write_task(const log_file_t *log_file, const tagged_task_t *task) {
    if (!task || !log_file) {
        errno = EINVAL;
        return;
    }

    int write_fd = log_file_get_write_fd(log_file);
    write(write_fd, task, tagged_task_sizeof());
}

/**
 * @brief Callback function for every task in the log file.
 *
 * @param task Task to be printed.
 * @param data Unused.
 *
 * @return 0 on success, -1 on error.
 *
 * | `errno`  | Cause                  |
 * | -------- | ---------------------- |
 * | `EINVAL` | @p task is `NULL`. |
*/
int __log_file_read_task_callback(const tagged_task_t *task, void *data) {
    (void) data;
    if (!task) {
        errno = EINVAL;
        return -1;
    }

    uint32_t tag_id = tagged_task_get_id(task);

    const struct timespec *time_arrived = tagged_task_get_time(task, TAGGED_TASK_TIME_ARRIVED);
    const struct timespec *time_ended   = tagged_task_get_time(task, TAGGED_TASK_TIME_ENDED);

    char *task_str = NULL;
    sprintf(task_str,
            "#%u: %ld.%ld - %ld.%ld\n",
            tag_id,
            time_arrived->tv_sec,
            time_arrived->tv_nsec,
            time_ended->tv_sec,
            time_ended->tv_nsec);
    write(STDOUT_FILENO, task_str, strlen(task_str));
    return 0;
}

void log_file_read_task(const log_file_t *log_file) {
    if (!log_file) {
        errno = EINVAL;
        return;
    }

    int read_fd = log_file_get_write_fd(log_file);
    task_iterator_iterate(read_fd, NULL, __log_file_read_task_callback);
}
