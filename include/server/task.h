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
 * @file  server/task.h
 * @brief A single program or pipeline that must be executed, or a procedure executable in a child
 *        processs.
 */

#ifndef TASK_H
#define TASK_H

#include <inttypes.h>
#include <sys/types.h>

#include "server/program.h"

/** @brief A single program or pipeline that must be executed. */
typedef struct task task_t;

/**
 * @brief Type of procedure that can be a task run in a child process.
 *
 * @param state  Pointer to the program's state, so that the process can read it.
 * @param slot   Slot where the task was scheduled.
 * @param secret Secret number to authenticate the termination of the task.
 *
 * @return The exit code of the child process.
 */
typedef int (*task_procedure_t)(void *state, size_t slot, uint64_t secret);

/**
 * @brief   Creates a empty task composed of programs.
 * @details This task isn't valid, and needs to be populated with programs.
 * @return  A new task on success, or `NULL` on allocation failure (`errno = ENOMEM`).
 */
task_t *task_new_empty(void);

/**
 * @brief Creates a new task from the programs that constitute it.
 *
 * @param programs Array of programs. Can be `NULL` for this method to have the same behavior as
 *                 ::task_new_empty.
 * @param length   Number of programs in @p programs. Must be `0` if @p programs is NULL.
 *
 * @return A new task on success, or `NULL` on failure (check `errno`).
 *
 * | `errno`  | Cause                                           |
 * | -------- | ----------------------------------------------- |
 * | `EINVAL` | @p programs is `NULL` and @p length is not `0`. |
 * | `ENOMEM` | Allocation failure.                             |
 */
task_t *task_new_from_programs(const program_t *const *programs, size_t length);

/**
 * @brief Creates a new task that will run a procedure.
 *
 * @param procedure Procedure to run.
 * @param state     Argument passed to @p procedure when its executed. No ownership of this data is
 *                  taken.
 *
 * @return A new task on success, or `NULL` on failure (check `errno`).
 *
 * | `errno`  | Cause                  |
 * | -------- | ---------------------- |
 * | `EINVAL` | @p procedure in `NULL` |
 * | `ENOMEM` | Allocation failure.    |
 */
task_t *task_new_from_procedure(task_procedure_t procedure, void *state);

/**
 * @brief  Creates a deep copy of a task.
 * @param  task Task to be cloned.
 * @return A copy of @p task on success, NULL on failure (check `errno`).
 *
 * | `errno`  | Cause               |
 * | -------- | ------------------- |
 * | `EINVAL` | @p task is `NULL`   |
 * | `ENOMEM` | Allocation failure. |
 */
task_t *task_clone(const task_t *task);

/**
 * @brief Frees memory used by a task.
 * @param task task to be deleted.
 */
void task_free(task_t *task);

/**
 * @brief Appends a program to a task's program list.
 *
 * @param task    Task to be modified.
 * @param program Program to be added to @p task.
 *
 * @retval 0 Success.
 * @retval 1 Invalid arguments or allocation failure (check `errno`).
 *
 * | `errno`  | Cause                                                          |
 * | -------- | -------------------------------------------------------------- |
 * | `EINVAL` | @p program is `NULL` or @p task is `NULL` or a procedure task. |
 * | `ENOMEM` | Allocation failure.                                            |
 */
int task_add_program(task_t *task, const program_t *program);

/**
 * @brief Gets the list of programs in a task.
 *
 * @param task  Task to get programs from. Mustn't be `NULL`.
 * @param count Where to place number of programs in list. Will only be updated on success. Mustn't
 *              be `NULL` or a task composed of a procedure.
 *
 * @return An array of programs, or `NULL` on failure (`errno = EINVAL`).
 */
const program_t *const *task_get_programs(const task_t *task, size_t *count);

/**
 * @brief Gets the procedure of a task composed by one.
 *
 * @param task      Task to get procedure from. Mustn't be `NULL` or a task made of programs.
 * @param procedure Where to write the procedure to. Won't be modified on error.
 * @param state     Where to write the procedure's argument to. Won't be modified on error.
 *
 * @retval 0 Success.
 * @retval 1 Failure (`errno = EINVAL`).
 */
int task_get_procedure(const task_t *task, task_procedure_t *procedure, void **state);

#endif
