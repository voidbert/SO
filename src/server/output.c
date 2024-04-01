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
 * @file  server/output.c
 * @brief Implementation of methods in server/output.h
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "server/output.h"

void output_create_task_time_file(void) {
    int fd = open(OUTPUT_TASK_TIME_FILE, O_CREAT | O_WRONLY, 0644);
    if (fd < 0) {
        return;
    }

    ftruncate(fd, 0);
    close(fd);
}

void output_create_stdout_file(void) {
    int fd = open(OUTPUT_STDOUT_FILE, O_CREAT, 0644);
    if (fd < 0) {
        return;
    }

    close(fd);
}

/**
 * @brief Counts the number of lines in the task time file.
 * @return The number of lines in the file, or -1 if the file could not be opened.
 */
int __output_task_time_count_lines(void) {
    int fd = open(OUTPUT_TASK_TIME_FILE, O_RDONLY);
    if (fd < 0) {
        return -1;
    }

    int  lines = 0;
    char c;
    while (read(fd, &c, 1) > 0) {
        if (c == '\n') {
            lines++;
        }
    }

    close(fd);
    return lines;
}

/**
 * @brief Gets the time a task took to complete as a string.
 * @param task  The task to get the time from.
 * @param index The index of the task in the file.
 * @return A string with the time the task took to complete, or `NULL` on failure.
 */
char *__output_get_task_time_to_complete(const tagged_task_t *task, int index) {
    char *time_str = malloc(1024);
    if (time_str == NULL) {
        return NULL;
    }

    const struct timespec *time_arrived   = tagged_task_get_time(task, TAGGED_TASK_TIME_ARRIVED);
    const struct timespec *time_completed = tagged_task_get_time(task, TAGGED_TASK_TIME_COMPLETED);

    // ta a cair aqui não sei porque??
    if (time_arrived == NULL || time_completed == NULL) {
        return NULL;
    }

    double time = (time_completed->tv_sec - time_arrived->tv_sec) +
                  (time_completed->tv_nsec - time_arrived->tv_nsec) / 1e9;

    sprintf(time_str, "#%d: %f\n", index, time);
    return time_str;
}

void output_write_task_time(const tagged_task_t *task) {
    int fd = open(OUTPUT_TASK_TIME_FILE, O_WRONLY | O_APPEND, 0644);
    if (fd < 0) {
        return;
    }

    char *time_str = __output_get_task_time_to_complete(task, __output_task_time_count_lines());
    if (time_str == NULL) {
        close(fd);
        return;
    }

    write(fd, time_str, strlen(time_str));
    close(fd);
}
