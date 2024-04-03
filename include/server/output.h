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
 * @file  server/output.h
 * @brief File responsible for the output of tasks as they are completed.
 */

#ifndef OUTPUT_H
#define OUTPUT_H

#define OUTPUT_TASK_TIME_FILE "task_times.txt"

#include "server/tagged_task.h"

/**
 * @brief   Creates a file to store the time each task was completed.
 * @details If the file already exists, it will be reset.
 * @note    This function should be called before any write operation.
 */
void output_create_task_time_file(void);

/**
 * @brief   Writes the time a task was completed to a file.
 * @param   task The task to write the time of.
 */
void output_write_task_time(const tagged_task_t *task);

/**
 * @brief Reads all the file with the task times.
 */
void output_read_task_times(void);

#endif
