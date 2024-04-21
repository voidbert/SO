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
 * @file  server/pipeline.h
 * @brief Handles the execution of pipelines.
 */

#ifndef PIPELINE_H
#define PIPELINE_H

/**
 * @brief Executes a task which is a pipeline.
 * @param taks Task with programs to be executed in a pipeline.
 *
 * @return 1 on failure (check `errno`), or 0 on success.
 *
 * | `errno`  | Cause                                                                              |
 * | -------- | ---------------------------------------------------------------------------------- |                                       |
 * | `ENOMEM` | `fork()` failure from insuficient memory or from terminated "init" process in      |
 * |          |  namespace.                                                                        |
 * | -------- | ---------------------------------------------------------------------------------- |
 * | `EMFILE` | System call `pipe()` failure, due to system resources limits (see `man 2 pipe`).   |
 * | -------- | ---------------------------------------------------------------------------------- |
 * | other    | See `man 2 execve`.                                                                |
 * | -------- | ---------------------------------------------------------------------------------- |
 */
int pipeline_execute(const task_t *task);

#endif
