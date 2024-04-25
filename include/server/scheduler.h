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
 * @file  server/scheduler.h
 * @brief The scheduler and dispatcher of tasks (::task_t).
 */

#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <sys/types.h>

#include "server/tagged_task.h"

/** @brief Scheduling policy used in a ::scheduler_t. */
typedef enum {
    SCHEDULER_POLICY_FCFS, /**< @brief <b>F</b>irst <b>C</b>ome <b>F</b>irst <b>S</b>erved */
    SCHEDULER_POLICY_SJF   /**< @brief <b>S</b>hortest <b>J</b>ob <b>F</b>irst */
} scheduler_policy_t;

/** @brief A scheduler and dispatcher of tasks (::task_t). */
typedef struct scheduler scheduler_t;

/**
 * @brief Creates a new scheduler.
 *
 * @param policy    Scheduling policy.
 * @param ntasks    Maximum number of tasks scheduled concurrently. Can't be `0`.
 * @param outputdir Directory path where to write output and error files.
 *
 * @return A new scheduler on success, `NULL` on failure (check `errno`).
 *
 * | `errno`  | Cause                           |
 * | -------- | ------------------------------- |
 * | `EINVAL` | Invalid @p policy or @p ntasks. |
 * | `ENOMEM` | Allocation failure.             |
 */
scheduler_t *scheduler_new(scheduler_policy_t policy, size_t ntasks, char *outputdir);

/**
 * @brief   Frees memory used by a scheduler.
 * @details If tasks are still running, the parent process won't `wait()` for them, and they'll
 *          become zombies when they exit.
 * @param   scheduler Scheduler to be deleted.
 */
void scheduler_free(scheduler_t *scheduler);

/**
 * @brief Adds a new task to be executed by a scheduler.
*
 * @param scheduler Scheduler to add @p task to. Mustn't be `NULL`.
 * @param task      Task to be added to @p scheduler. Will be cloned first. Mustn't be `NULL`.
 *
 * @retval 0 Success.
 * @retval 1 Failure (check `errno`).
 *
 * | `errno`  | Cause                               |
 * | -------- | ----------------------------------- |
 * | `EINVAL` | @p scheduler or @p task are `NULL`. |
 * | `ENOMEM` | Allocation failure.                 |
 */
int scheduler_add_task(scheduler_t *scheduler, const tagged_task_t *task);

/**
 * @brief   Tries to dispatch tasks in the scheduler's queue without going over its concurrency
 *          limit.
 * @details If dispatching a task fails, the scheduler won't try to reschedule that task later.
 *          Also, this procedure will write to `stdout` when these failures happen.
 *
 * @param scheduler Scheduler to get tasks to dispatch from.
 * @return The number of tasks scheduled, `-1` on failure (check `errno`).
 *
 * | `errno`  | Cause                                                         |
 * | -------- | ------------------------------------------------------------- |
 * | `EINVAL` | @p scheduler is `NULL`.                                       |
 * | `ENOMEM` | From `fork()` or allocation failure during task reeinsertion. |
 * | other    | See `man 2 fork`.                                             |
 */
ssize_t scheduler_dispatch_possible(scheduler_t *scheduler);

/**
 * @brief   Marks a task currently running as complete.
 * @details The scheduler will `wait()` for the task, so make sure it has finished already. If this
 *          calling `waitpid()` fails, a message will be printed to `stderr`.
 *
 * @param scheduler  Scheduler that dispatched the task. Can't be `NULL`.
 * @param slot       Slot where the task was scheduled (provided to child).
 * @param secret     Secret number (provided to child).
 * @param time_ended When the child stopped waiting for task processes. Can't be `NULL`.
 *
 * @return The finished task, now owned by the caller. `NULL` is returned on error (check `errno`).
 *
 * | `errno`       | Cause                                                  |
 * | ------------- | -------------------------------------------------------|
 * | `EINVAL`      | @p scheduler or @p time_ended are `NULL`.              |
 * | `ERANGE`      | @p slot too large or still available.                  |
 * | `EWOULDBLOCK` | Secret does not match and `wait()` can't be performed. |
 * | other         | See `man 2 wait`.                                      |
 */
tagged_task_t *scheduler_mark_done(scheduler_t           *scheduler,
                                   size_t                 slot,
                                   uint64_t               secret,
                                   const struct timespec *time_ended);

#endif
