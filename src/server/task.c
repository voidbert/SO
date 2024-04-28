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
 * @file  server/task.c
 * @brief Implementation of methods in server/task.h
 */

#include <errno.h>
#include <stdlib.h>

#include "server/task.h"

/**
 * @struct task
 * @brief  A single program or pipeline that must be executed, or a procedure executable in a child
 *        processs.
 *
 * @var task::programs
 *     @brief   Array of programs in the pipeline.
 *     @details Will be `NULL` if task::programs has a value.
 * @var task::length
 *     @brief Number of programs in task::programs.
 * @var task::capacity
 *     @brief Maximum number of programs in task::argv before reallocation.
 * @var task::procedure
 *     @brief   Procedure that constitutes the task.
 *     @details Will be `NULL` if task::programs has a value.
 * @var task::procedure_state
 *     @brief Argument that will passed to task::procedure.
 */
struct task {
    program_t **programs;
    size_t      length;
    size_t      capacity;

    task_procedure_t procedure;
    void            *procedure_state;
};

/** @brief Value of task::capacity for newly created empty tasks. */
#define TASK_INITIAL_CAPACITY 8

task_t *task_new_empty(void) {
    return task_new_from_programs(NULL, 0);
}

task_t *task_new_from_programs(const program_t *const *programs, size_t length) {
    if (!programs && length != 0) {
        errno = EINVAL;
        return NULL;
    }

    task_t *ret = malloc(sizeof(task_t));
    if (!ret)
        return NULL; /* errno = ENOMEM guaranteed */

    size_t first_power = 1;
    while (first_power < length)
        first_power *= 2;
    if (first_power < TASK_INITIAL_CAPACITY)
        first_power = TASK_INITIAL_CAPACITY;

    ret->length   = length;
    ret->capacity = first_power;
    ret->programs = malloc(sizeof(program_t *) * ret->capacity);
    if (!ret->programs) {
        free(ret); /* Won't modify errno = ENOMEM */
        return NULL;
    }

    for (size_t i = 0; i < length; ++i) {
        ret->programs[i] = program_clone(programs[i]);
        if (!ret->programs[i]) {
            ret->length = i;
            task_free(ret);
            return NULL; /* errno = ENOMEM guaranteed */
        }
    }

    ret->procedure       = NULL;
    ret->procedure_state = NULL;
    return ret;
}

task_t *task_new_from_procedure(task_procedure_t procedure, void *state) {
    if (!procedure) {
        errno = EINVAL;
        return NULL;
    }

    task_t *ret = malloc(sizeof(task_t));
    if (!ret)
        return NULL; /* errno = ENOMEM guaranteed */

    ret->programs = NULL;
    ret->length = ret->capacity = 0;
    ret->procedure              = procedure;
    ret->procedure_state        = state;
    return ret;
}

void task_free(task_t *task) {
    if (!task)
        return; /* Don't set EINVAL, as that's normal free behavior. */

    if (task->programs) {
        for (size_t i = 0; i < task->length; ++i)
            program_free(task->programs[i]);
        free(task->programs);
    }
    free(task);
}

task_t *task_clone(const task_t *task) {
    if (!task) {
        errno = EINVAL;
        return NULL;
    }

    if (task->programs)
        return task_new_from_programs((const program_t *const *) task->programs, task->length);
    else
        return task_new_from_procedure(task->procedure, task->procedure_state);
}

int task_add_program(task_t *task, const program_t *program) {
    if (!task || !program || !task->programs) {
        errno = EINVAL;
        return 1;
    }

    program_t *new_program = program_clone(program);
    if (!new_program)
        return 1; /* errno = ENOMEM guaranteed */

    if (task->length >= task->capacity) {
        size_t      new_capacity = task->capacity * 2;
        program_t **new_programs = realloc(task->programs, sizeof(program_t *) * new_capacity);
        if (!new_programs)
            return 1; /* errno = ENOMEM guaranteed */

        task->capacity = new_capacity;
        task->programs = new_programs;
    }

    task->programs[task->length] = new_program;
    task->length++;
    return 0;
}

const program_t *const *task_get_programs(const task_t *task, size_t *count) {
    if (!task || !count || !task->programs) {
        errno = EINVAL;
        return NULL;
    }

    *count = task->length;
    return (const program_t *const *) task->programs;
}

int task_get_procedure(const task_t *task, task_procedure_t *procedure, void **state) {
    if (!task || !task->procedure) {
        errno = EINVAL;
        return 1;
    }

    *procedure = task->procedure;
    *state     = task->procedure_state;
    return 0;
}
