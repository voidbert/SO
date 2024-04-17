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
 * @file  server/task_iterator.h
 * @brief File responsible for iterating through tasks from a file.
 */

#ifndef TASK_ITERATOR_H
#define TASK_ITERATOR_H

#include "server/tagged_task.h"

/** @brief Callback function for iterating through tasks. */
typedef int(task_iterator_callback_t)(const tagged_task_t *task, void *data);

/**
 * @brief Iterates through tasks in a file descriptor.
 *
 * @param fd       File descriptor to read tasks from.
 * @param data     Data to be passed to the callback.
 * @param callback Callback function to be called for each task.
 *
 * @return 0 on success, -1 on failure.
 *
 * | `errno`  | Cause                  |
 * | -------- | ---------------------- |
 * | `EINVAL` | @p fd is `NULL`.       |
 * | `EINVAL` | @p callback is `NULL`. |
 */
int task_iterator_iterate(int fd, void *data, task_iterator_callback_t callback);

#endif
