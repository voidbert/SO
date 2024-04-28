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
 * @file  server/scheduler.c
 * @brief Implementation of methods in server/scheduler.h
 */

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "server/priority_queue.h"
#include "server/scheduler.h"
#include "server/task_runner.h"
#include "util.h"

/**
 * @struct scheduler_slot_t
 * @brief  A slot where a task can be scheduled.
 *
 * @var scheduler_slot_t::available
 *     @brief Boolean telling whether a slot is available for a new task to be scheduled.
 * @var scheduler_slot_t::pid
 *     @brief PID of task running in this slot (only if this slot is unavailable).
 * @var scheduler_slot_t::task
 *     @brief Task running in the slot (only if this slot is unavailable).
 */
typedef struct {
    int            available;
    pid_t          pid;
    tagged_task_t *task;
} scheduler_slot_t;

/**
 * @struct scheduler
 * @brief  A scheduler and dispatcher of tasks (::tagged_task_t).
 *
 * @var scheduler::queue
 *     @brief Queue of tasks awaiting CPU time.
 * @var scheduler::ntasks
 *     @brief Maximum number of tasks scheduled concurrently.
 * @var scheduler::slots
 *     @brief Slots where to dispatch tasks (as many as ::scheduler::ntasks).
 * @var scheduler::directory
 *     @brief Path to output directory.
 */
struct scheduler {
    priority_queue_t *queue;
    size_t            ntasks;
    scheduler_slot_t *slots;
    char             *directory;
};

/** @brief ::priority_queue_compare_function_t for ::SCHEDULER_POLICY_FCFS. */
int __scheduler_compare_fcfs(const tagged_task_t *a, const tagged_task_t *b) {
    const struct timespec *a_time = tagged_task_get_time(a, TAGGED_TASK_TIME_ARRIVED);
    const struct timespec *b_time = tagged_task_get_time(b, TAGGED_TASK_TIME_ARRIVED);
    if (!a_time || !b_time) /* At least one invalid task */
        return 0;

    if (a_time->tv_sec > b_time->tv_sec)
        return 1;
    else if (a_time->tv_sec == b_time->tv_sec)
        return a_time->tv_nsec - b_time->tv_nsec;
    else
        return -1;
}

/** @brief ::priority_queue_compare_function_t for ::SCHEDULER_POLICY_SJF. */
int __scheduler_compare_sjf(const tagged_task_t *a, const tagged_task_t *b) {
    return (int64_t) tagged_task_get_expected_time(a) - (int64_t) tagged_task_get_expected_time(b);
}

scheduler_t *scheduler_new(scheduler_policy_t policy, size_t ntasks, const char *directory) {
    if (!ntasks || !directory) {
        errno = EINVAL;
        return NULL;
    }

    scheduler_t *ret = malloc(sizeof(scheduler_t));
    if (!ret)
        return NULL; /* errno = ENOMEM guaranteed */

    switch (policy) {
        case SCHEDULER_POLICY_FCFS:
            ret->queue = priority_queue_new(__scheduler_compare_fcfs);
            break;
        case SCHEDULER_POLICY_SJF:
            ret->queue = priority_queue_new(__scheduler_compare_sjf);
            break;
        default:
            errno = EINVAL;
            free(ret);
            return NULL;
    }
    if (!ret->queue) {
        free(ret);
        return NULL; /* errno = ENOMEM guaranteed */
    }

    ret->slots = malloc(sizeof(scheduler_slot_t) * ntasks);
    if (!ret->slots) {
        priority_queue_free(ret->queue);
        free(ret);
        return NULL; /* errno = ENOMEM guaranteed */
    }
    for (size_t i = 0; i < ntasks; ++i)
        ret->slots[i].available = 1;

    ret->ntasks = ntasks;

    ret->directory = strdup(directory);
    if (!ret->directory) {
        priority_queue_free(ret->queue);
        free(ret->slots);
        free(ret);
        return NULL; /* errno = ENOMEM guaranteed */
    }

    return ret;
}

void scheduler_free(scheduler_t *scheduler) {
    if (!scheduler)
        return; /* Don't set errno, as that's not typical free behavior */

    for (size_t i = 0; i < scheduler->ntasks; ++i)
        if (!scheduler->slots[i].available)
            tagged_task_free(scheduler->slots[i].task);
    free(scheduler->slots);

    priority_queue_free(scheduler->queue);
    free(scheduler->directory);
    free(scheduler);
}

int scheduler_add_task(scheduler_t *scheduler, const tagged_task_t *task) {
    if (!scheduler || !task) {
        errno = EINVAL;
        return 1;
    }
    return priority_queue_insert(scheduler->queue, task);
}

int scheduler_can_schedule_now(scheduler_t *scheduler) {
    for (size_t i = 0; i < scheduler->ntasks; ++i)
        if (scheduler->slots[i].available)
            return 1;
    return 0;
}

ssize_t scheduler_dispatch_possible(scheduler_t *scheduler) {
    if (!scheduler) {
        errno = EINVAL;
        return 1;
    }

    tagged_task_t *task;
    size_t         slot_search = 0, dispatched = 0;
    while ((task = priority_queue_remove_top(scheduler->queue))) {
        /* Find slot where to schedule the task */
        for (; slot_search < scheduler->ntasks; ++slot_search)
            if (scheduler->slots[slot_search].available)
                break;

        if (slot_search == scheduler->ntasks) { /* Can't schedule: reeinsert the task */
            if (priority_queue_insert(scheduler->queue, task)) {
                util_error("%s(): Task %" PRIu32 " was dropped: out of memory\n",
                           __func__,
                           tagged_task_get_id(task));
                tagged_task_free(task);
                return 1;
            }

            tagged_task_free(task);
            return (ssize_t) dispatched;
        }

        tagged_task_set_time(task, TAGGED_TASK_TIME_DISPATCHED, NULL);

        scheduler->slots[slot_search].available = 0;
        scheduler->slots[slot_search].task      = task;

        pid_t p = fork();
        if (p == 0) {
            _exit(task_runner_main(task, slot_search, scheduler->directory));
        } else if (p < 0) {
            char error_msg[LINE_MAX] = {0};
            (void) strerror_r(errno, error_msg, LINE_MAX);
            util_error("%s(): Task %" PRIu32 " was dropped: fork() failed: %s\n",
                       __func__,
                       tagged_task_get_id(task),
                       error_msg);
            tagged_task_free(task);
            return 1;
        } else {
            scheduler->slots[slot_search].pid = p;
        }

        dispatched++;
        slot_search++;
    }

    return (ssize_t) dispatched;
}

tagged_task_t *
    scheduler_mark_done(scheduler_t *scheduler, size_t slot, const struct timespec *time_ended) {
    if (!scheduler || !time_ended) {
        errno = EINVAL;
        return NULL;
    }

    if (slot >= scheduler->ntasks || scheduler->slots[slot].available) {
        errno = ERANGE;
        return NULL;
    }

    if (waitpid(scheduler->slots[slot].pid, NULL, 0) < 0) {
        util_error("waitpid(%ld) failed for (task %" PRIu32 ")!\n",
                   (long) scheduler->slots[slot].pid,
                   tagged_task_get_id(scheduler->slots[slot].task));
        tagged_task_free(scheduler->slots[slot].task);
        scheduler->slots[slot].available = 1;
        return NULL; /* Keep errno */
    }

    tagged_task_t *ret = scheduler->slots[slot].task;
    tagged_task_set_time(ret, TAGGED_TASK_TIME_ENDED, time_ended);
    tagged_task_set_time(ret, TAGGED_TASK_TIME_COMPLETED, NULL);
    scheduler->slots[slot].available = 1;

    return ret;
}

int scheduler_get_running_tasks(scheduler_t              *scheduler,
                                scheduler_task_iterator_t callback,
                                void                     *state) {
    if (!scheduler || !callback) {
        errno = EINVAL;
        return 1;
    }

    for (size_t i = 0; i < scheduler->ntasks; ++i) {
        if (!scheduler->slots[i].available) {
            int cb_ret = callback(scheduler->slots[i].task, state);
            if (cb_ret)
                return cb_ret;
        }
    }
    return 0;
}

int scheduler_get_scheduled_tasks(scheduler_t              *scheduler,
                                  scheduler_task_iterator_t callback,
                                  void                     *state) {
    if (!scheduler || !callback) {
        errno = EINVAL;
        return 1;
    }

    size_t                      ntasks;
    const tagged_task_t *const *tasks = priority_queue_get_tasks(scheduler->queue, &ntasks);

    for (size_t i = 0; i < ntasks; ++i) {
        int cb_ret = callback(tasks[i], state);
        if (cb_ret)
            return cb_ret;
    }
    return 0;
}
