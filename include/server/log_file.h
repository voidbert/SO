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
 * @file  server/log_file.h
 * @brief File responsible for the logging of tasks as they are completed.
 */

#ifndef LOG_FILE_H
#define LOG_FILE_H

#include "server/tagged_task.h"

/** @brief A log_file type containing all file descriptors. */
typedef struct log_file log_file_t;

/**
 * @brief   Creates a new log_file_t, containing all file descriptors.
 * @details If a file already exists, it's content will be reset.
 * @note    This function should be called before any other operation.
 *
 * @param   filename Name of the file to be created.
 *
 * @return  A new log_file on success, `NULL` on failure.
 *
 * | `errno`  | Cause                  |
 * | -------- | ---------------------- |
 * | `EINVAL` | @p filename is `NULL`. |
 * | `ENOMEM` | Allocation failure.    |
 */
log_file_t *log_file_new(const char *filename);

/**
 * @brief Close all file descriptors and frees the log_file_t.
 * @param log_file Log file to be freed.
 */
void log_file_free(log_file_t *log_file);

/**
 * @brief Get the `create` file descriptor for the log file.
 *
 * @param log_file Log file to be freed.
 *
 * @return The `create` file descriptor, -1 on failure.
 *
 * | `errno`  | Cause                  |
 * | -------- | ---------------------- |
 * | `EINVAL` | @p log_file is `NULL`. |
 */
int log_file_get_create_fd(const log_file_t *log_file);

/**
 * @brief Get the `read` file descriptor for the log file.
 *
 * @param log_file Log file to be freed.
 *
 * @return The `read` file descriptor, -1 on failure.
 *
 * | `errno`  | Cause                  |
 * | -------- | ---------------------- |
 * | `EINVAL` | @p log_file is `NULL`. |
 */
int log_file_get_read_fd(const log_file_t *log_file);

/**
 * @brief Get the `write` file descriptor for the log file.
 *
 * @param log_file Log file to be freed.
 *
 * @return The `write` file descriptor, -1 on failure.
 *
 * | `errno`  | Cause                  |
 * | -------- | ---------------------- |
 * | `EINVAL` | @p log_file is `NULL`. |
 */
int log_file_get_write_fd(const log_file_t *log_file);

/**
 * @brief Write a task to the log file.
 *
 * @param log_file Log file to write to.
 * @param task     Task to be written.
 *
 * @return 0 on success, -1 on error.
 *
 * | `errno`  | Cause                  |
 * | -------- | ---------------------- |
 * | `EINVAL` | @p log_file is `NULL`. |
 * | `EINVAL` | @p task is `NULL`.     |
 */
int log_file_write_task(const log_file_t *log_file, const tagged_task_t *task);

/**
 * @brief   Read a task from the log file.
 * @details This function calls a task_iterator to iterate through the log file.
 *
 * @param   log_file Log file to read from.
 *
 * @return 0 on success, -1 on error.
 *
 * | `errno`  | Cause                  |
 * | -------- | ---------------------- |
 * | `EINVAL` | @p log_file is `NULL`. |
 */
int log_file_read_task(const log_file_t *log_file);

#endif
