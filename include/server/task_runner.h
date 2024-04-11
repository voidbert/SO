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
 * @file  server/task_runner.h
 * @brief Procedures used in the child program that runs all processes in a task.
 */

#ifndef TASK_RUNNER_H
#define TASK_RUNNER_H

#include "server/tagged_task.h"

/**
 * @brief Entry point to the child that runs processes in tasks.
 *
 * @param task   Task to be run.
 * @param slot   Slot in the scheduler where this task was scheduled, used to identify this task
 *               when compeleted.
 * @param secret Secret random number needed to autheticize the termination of the task.
 *
 * @return 0 Success.
 * @return 1 Failure. `errno` is not set as the process `_exit()`s after calling this.
 */
int task_runner_main(tagged_task_t *task, size_t slot, uint64_t secret);

#endif
