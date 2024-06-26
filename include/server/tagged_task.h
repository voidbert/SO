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
 * @file  server/tagged_task.h
 * @brief A task (see ::task_t) with extra information needed for task management.
 */

#ifndef TAGGED_TASK_H
#define TAGGED_TASK_H

#include <inttypes.h>
#include <time.h>

#include "server/task.h"

/**
 * @enum  tagged_task_time_t
 * @brief Meaning of every timestamp stored in a ::tagged_task_t.
 */
typedef enum {
    /** @brief When the task was sent by the client to be executed (self-reported). */
    TAGGED_TASK_TIME_SENT,
    /** @brief When the task was received by the server. */
    TAGGED_TASK_TIME_ARRIVED,
    /** @brief When the task started being executed by the server. */
    TAGGED_TASK_TIME_DISPATCHED,
    /** @brief When the server's child realized the task finished executing. */
    TAGGED_TASK_TIME_ENDED,
    /** @brief When the server's main process realized the task finished executing. */
    TAGGED_TASK_TIME_COMPLETED
} tagged_task_time_t;

/** @brief A task (see ::task_t) with extra information needed for task management. */
typedef struct tagged_task tagged_task_t;

/**
 * @brief Creates a new task from a command line to be parsed.
 *
 * @param command_line  Command line to parse. Mustn't be `NULL`.
 * @param id            Identifier of the task.
 * @param expected_time Time the client expects this task to consume in execution.
 *
 * @return A new task on success, `NULL` on failure (check `errno`).
 *
 * | `errno`  | Cause                      |
 * | -------- | -------------------------- |
 * | `EINVAL` | @p command_line is `NULL`. |
 * | `EILSEQ` | Parsing failure.           |
 * | `ENOMEM` | Allocation failure.        |
 */
tagged_task_t *tagged_task_new_from_command_line(const char *command_line,
                                                 uint32_t    id,
                                                 uint32_t    expected_time);

/**
 * @brief Creates a new task from a procedure that is executed in a child process.
 *
 * @param procedure     Procedure to run. Mustn't be `NULL`.
 * @param state         Argument passed to @p procedure when its executed. No ownership of this data
 *                      is taken.
 * @param id            Identifier of the task.
 * @param expected_time Time the client expects this task to consume in execution.
 *
 * @return A new task on success, `NULL` on failure.
 *
 * | `errno`  | Cause                   |
 * | -------- | ----------------------- |
 * | `EINVAL` | @p procedure is `NULL`. |
 * | `ENOMEM` | Allocation failure.     |
 */
tagged_task_t *tagged_task_new_from_procedure(task_procedure_t procedure,
                                              void            *state,
                                              uint32_t         id,
                                              uint32_t         expected_time);

/**
 * @brief Frees memory used by a tagged task.
 * @param task Tagged task to be deleted.
 */
void tagged_task_free(tagged_task_t *task);

/**
 * @brief  Creates a deep copy of a task.
 * @param  task Task to be cloned.
 * @return A copy of @p task on success, `NULL` on failure (check `errno`).
 *
 * | `errno`  | Cause               |
 * | -------- | ------------------- |
 * | `EINVAL` | @p task is `NULL`.  |
 * | `ENOMEM` | Allocation failure. |
 */
tagged_task_t *tagged_task_clone(const tagged_task_t *task);

/**
 * @brief  Gets the program / pipeline / procedure contained inside a tagged task.
 * @param  task Tagged task to get task payload from. Mustn't be `NULL`.
 * @return A ::task_t on success, or `NULL` on failure (`errno = EINVAL`).
 */
const task_t *tagged_task_get_task(const tagged_task_t *task);

/**
 * @brief   Gets the command line that generated a tagged task.
 * @details This value is won't be a runnable program for procedure tasks.
 * @param   task Tagged task to get command line from. Mustn't be `NULL`.
 * @return  A string on success, or `NULL` on failure (`errno = EINVAL`).
 */
const char *tagged_task_get_command_line(const tagged_task_t *task);

/**
 * @brief  Gets the identifier of a tagged task.
 * @param  task Tagged task to get identifier from. Mustn't be `NULL`.
 * @return The identifier on success, or `(uint32_t) -1` on failure (`errno = EINVAL`).
 */
uint32_t tagged_task_get_id(const tagged_task_t *task);

/**
 * @brief  Gets the time the user expects this task to take in execution.
 * @param  task Tagged task to get expected time from. Mustn't be `NULL`.
 * @return The time in milliseconds, or `(uint32_t) -1` on failure (`errno = EINVAL`).
 */
uint32_t tagged_task_get_expected_time(const tagged_task_t *task);

/**
 * @brief Gets one of the many timestamps stored in tagged task.
 *
 * @param task Task to get a timestamp from. Mustn't be NULL.
 * @param id   Identifier of the timestamp to get.
 *
 * @return A pointer to a timestamp in @p task on success, `NULL` on failure (check `errno`).
 *
 * | `errno`  | Cause                                   |
 * | -------- | --------------------------------------- |
 * | `EINVAL` | @p task is `NULL` or @p id isn't valid. |
 * | `EDOM`   | Specified time not yet set.             |
 */
const struct timespec *tagged_task_get_time(const tagged_task_t *task, tagged_task_time_t id);

/**
 * @brief Sets one of the many timestamps in a tagged task.
 *
 * @param task Task to have one of its timestamps set. Mustn't be `NULL`.
 * @param id   Which of timestamps should be set.
 * @param time Timestamp to be copied to the task. If `NULL`, the current time will be used.
 *
 * @retval 0 Success.
 * @retval 1 Failure, because @p task is `NULL`, or @p id is invalid (`errno = EINVAL`).
 */
int tagged_task_set_time(tagged_task_t *task, tagged_task_time_t id, const struct timespec *time);

#endif
