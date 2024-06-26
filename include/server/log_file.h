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
 * @brief Methods for logging completed tasks.
 */

#ifndef LOG_FILE_H
#define LOG_FILE_H

#include "server/tagged_task.h"

/** @brief A handle for an open log file. */
typedef struct log_file log_file_t;

/**
 * @brief   Callback called for every task in a log file.
 * @details See ::log_file_read_tasks.
 *
 * @param task  Task read from file.
 * @param error Whether an error occurred while running @p task.
 * @param state Pointer passed to ::log_file_read_tasks so that this procedure can modify the
 *              program's state.
 *
 * @retval 0 Success.
 * @retval 1 Failure. Stop log file iteration.
 */
typedef int (*log_file_task_callback_t)(const tagged_task_t *task, int error, void *state);

/**
 * @brief   Opens a new log file for reading or for writing.
 * @details If the specified file already exists and @p writable is true, the file's contents will
 *          be deleted.
 *
 * @param path     Path to the file to be opened.
 * @param writable Whether the file can be written to. Reading will still be possible if this is
 *                 `1`.
 *
 * @return A new handle for a log file on success, `NULL` on failure (check `errno`).
 *
 * | `errno`  | Cause                                     |
 * | -------- | ----------------------------------------- |
 * | `EINVAL` | @p path is `NULL`.                        |
 * | `ENOMEM` | Allocation failure (or see `man 2 open`). |
 * | other    | See `man 2 open`.                         |
 */
log_file_t *log_file_new(const char *path, int writable);

/**
 * @brief Frees the memory of a log file, closing its file descriptor.
 * @param log_file Log file to be freed.
 */
void log_file_free(log_file_t *log_file);

/**
 * @brief Writes a task to a log file.
 *
 * @param log_file Log file to write @p task to. Mustn't be `NULL` and must be writable.
 * @param task     Task to be written to @p log_file. Mustn't be `NULL`.
 * @param error    Whether an error occurred while running the task.
 *
 * @retval 0 Success.
 * @retval 1 Failure (check `errno`).
 *
 * | `errno`    | Cause                                                     |
 * | ---------- | --------------------------------------------------------- |
 * | `EINVAL`   | @p log_file `NULL` or not writable, or @p task is `NULL`. |
 * | `EMSGSIZE` | tagged_task_t::command_line is too long.                  |
 * | other      | See `man 2 write`.                                        |
 */
int log_file_write_task(log_file_t *log_file, const tagged_task_t *task, int error);

/**
 * @brief   Reads all tasks from a log file.
 * @details Deserialization errors will be written to `stderr`.
 *
 * @param log_file Log file to read from. Mustn't be `NULL`.
 * @param task_cb  Callback to be called for every task. Mustn't be `NULL`.
 * @param state    Pointer passed to @p task_cb so that it can modify the program's state.
 *
 * @retval 0     Success (recoverable errors may have occurred).
 * @retval 1     Failure.
 * @retval other Value returned by @p task_cb on failure.
 *
 * | `errno`  | Cause                                           |
 * | -------- | ----------------------------------------------- |
 * | `EINVAL` | @p log_file or @p task_cb are `NULL`.           |
 * | `ENOMEM` | Allocation failure (or see man 2 open``).       |
 * | `EILSEQ` | Invalid file contents.                          |
 * | other    | See `man 2 lseek`, `man 2 open`, or @p task_cb. |
 */
int log_file_read_tasks(log_file_t *log_file, log_file_task_callback_t task_cb, void *state);

#endif
