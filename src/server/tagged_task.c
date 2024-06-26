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
 * @file  server/tagged_task.c
 * @brief Implementation of methods in server/tagged_task.h
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "server/command_parser.h"
#include "server/tagged_task.h"

/**
 * @struct tagged_task
 * @brief  A task (see ::task_t) with extra information needed for task management.
 *
 * @var tagged_task::task
 *     @brief Task (single program / pipeline / procedure) that needs to be run.
 * @var tagged_task::command_line
 *     @brief  Command line that was parsed to originate a task.
 *     @details This will have another value for procedure tasks.
 * @var tagged_task::id
 *     @brief Identifier of the task.
 * @var tagged_task::expected_time
 *     @brief Time that the execution of the task is supposed to take (reported by the client).
 * @var tagged_task::times
 *     @brief Timestamps associated with events regarding the processes of task execution.
 */
struct tagged_task {
    task_t  *task;
    char    *command_line;
    uint32_t id, expected_time;

    struct timespec times[TAGGED_TASK_TIME_COMPLETED + 1];
};

tagged_task_t *tagged_task_new_from_command_line(const char *command_line,
                                                 uint32_t    id,
                                                 uint32_t    expected_time) {
    if (!command_line) {
        errno = EINVAL;
        return NULL;
    }

    tagged_task_t *ret = malloc(sizeof(tagged_task_t));
    if (!ret)
        return NULL; /* errno = ENOMEM guaranteed */

    ret->id            = id;
    ret->expected_time = expected_time;
    memset(ret->times, 0, sizeof(ret->times));

    ret->command_line = strdup(command_line);
    if (!ret->command_line) {
        free(ret);
        return NULL; /* errno = ENOMEM guaranteed */
    }

    ret->task = command_parser_parse_task(command_line);
    if (!ret->task) {
        free(ret->command_line);
        free(ret);
        return NULL; /* EILSEQ or ENOMEM */
    }

    return ret;
}

tagged_task_t *tagged_task_new_from_procedure(task_procedure_t procedure,
                                              void            *state,
                                              uint32_t         id,
                                              uint32_t         expected_time) {
    if (!procedure) {
        errno = EINVAL;
        return NULL;
    }

    tagged_task_t *ret = malloc(sizeof(tagged_task_t));
    if (!ret)
        return NULL; /* errno = ENOMEM guaranteed */

    ret->id            = id;
    ret->expected_time = expected_time;
    memset(ret->times, 0, sizeof(ret->times));

    ret->command_line = strdup("PROCEDURE TASK");
    if (!ret->command_line) {
        free(ret);
        return NULL; /* errno = ENOMEM guaranteed */
    }

    ret->task = task_new_from_procedure(procedure, state);
    if (!ret->task) {
        free(ret->command_line);
        free(ret);
        return NULL; /* errno = ENOMEM guaranteed */
    }

    return ret;
}

void tagged_task_free(tagged_task_t *task) {
    if (!task)
        return; /* Don't set errno, as that's not typical free behavior */

    task_free(task->task);
    free(task->command_line);
    free(task);
}

tagged_task_t *tagged_task_clone(const tagged_task_t *task) {
    if (!task) {
        errno = EINVAL;
        return NULL;
    }

    tagged_task_t *ret = malloc(sizeof(tagged_task_t));
    if (!ret)
        return NULL; /* errno = ENOMEM guaranteed */

    ret->id            = task->id;
    ret->expected_time = task->expected_time;
    memcpy(ret->times, task->times, sizeof(task->times));

    ret->command_line = strdup(task->command_line);
    if (!ret->command_line) {
        free(ret); /* errno = ENOMEM guaranteed */
        return NULL;
    }

    ret->task = task_clone(task->task);
    if (!ret->task) {
        free(ret->command_line);
        free(ret);
        return NULL; /* errno = ENOMEM guaranteed */
    }

    return ret;
}

const task_t *tagged_task_get_task(const tagged_task_t *task) {
    if (!task) {
        errno = EINVAL;
        return NULL;
    }
    return task->task;
}

const char *tagged_task_get_command_line(const tagged_task_t *task) {
    if (!task) {
        errno = EINVAL;
        return NULL;
    }
    return task->command_line;
}

uint32_t tagged_task_get_id(const tagged_task_t *task) {
    if (!task) {
        errno = EINVAL;
        return (uint32_t) -1;
    }
    return task->id;
}

uint32_t tagged_task_get_expected_time(const tagged_task_t *task) {
    if (!task) {
        errno = EINVAL;
        return (uint32_t) -1;
    }
    return task->expected_time;
}

const struct timespec *tagged_task_get_time(const tagged_task_t *task, tagged_task_time_t id) {
    if (!task || id < 0 || id > TAGGED_TASK_TIME_COMPLETED) {
        errno = EINVAL;
        return NULL;
    }

    if (task->times[id].tv_sec == 0 && task->times[id].tv_nsec == 0) {
        errno = EDOM;
        return NULL;
    }

    return task->times + id;
}

int tagged_task_set_time(tagged_task_t *task, tagged_task_time_t id, const struct timespec *time) {
    if (!task || id < 0 || id > TAGGED_TASK_TIME_COMPLETED) {
        errno = EINVAL;
        return 1;
    }

    if (time)
        task->times[id] = *time;
    else if (clock_gettime(CLOCK_MONOTONIC, task->times + id) < 0)
        task->times[id].tv_sec = task->times[id].tv_nsec = 0;
    return 0;
}
