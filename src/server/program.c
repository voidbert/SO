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
 * @file  server/program.c
 * @brief Implementation of methods in server/program.h
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "server/program.h"

/**
 * @struct program
 * @brief  A single program that must be executed (may be part of a pipeline).
 *
 * @var program::argv
 *     @brief `NULL`-terminated array of program arguments (`argv[0]` is the program's name).
 * @var program::length
 *     @brief Number of arguments in program::argv.
 * @var program::capacity
 *     @brief Maximum number of arguments (plus `NULL` terminator) in program::argv before
 *            reallocation.
 */
struct program {
    char **argv;
    size_t length;
    size_t capacity;
};

/** @brief Value of program::capacity for newly created empty programs. */
#define PROGRAM_INITIAL_CAPACITY 8

program_t *program_new_empty(void) {
    return program_new_from_arguments(NULL, 0);
}

program_t *program_new_from_arguments(const char *const *arguments, ssize_t length) {
    if (!arguments && length != 0) {
        errno = EINVAL;
        return NULL;
    }

    if (length == -1) {
        length = 0;
        while (arguments[length])
            length++;
    }

    program_t *ret = malloc(sizeof(program_t));
    if (!ret)
        return NULL; /* errno = ENOMEM guaranteed */

    ssize_t first_power = 1;
    while (first_power < length)
        first_power *= 2;
    if (first_power < PROGRAM_INITIAL_CAPACITY)
        first_power = PROGRAM_INITIAL_CAPACITY;

    ret->length   = length;
    ret->capacity = first_power;
    ret->argv     = malloc(sizeof(char *) * ret->capacity);
    if (!ret->argv) {
        free(ret); /* Won't modify errno = ENOMEM */
        return NULL;
    }

    for (ssize_t i = 0; i < length; ++i) {
        ret->argv[i] = strdup(arguments[i]);
        if (!ret->argv[i]) {
            ret->length = i;
            program_free(ret);
            return NULL; /* errno = ENOMEM guaranteed */
        }
    }
    ret->argv[length] = NULL;

    return ret;
}

program_t *program_clone(const program_t *program) {
    if (!program) {
        errno = EINVAL;
        return NULL;
    }

    return program_new_from_arguments((const char *const *) program->argv, program->length);
}

void program_free(program_t *program) {
    if (!program)
        return; /* Don't set EINVAL, as that's normal free behavior. */

    for (size_t i = 0; i < program->length; ++i)
        free(program->argv[i]);
    free(program->argv);
    free(program);
}

int program_equals(const program_t *a, const program_t *b) {
    if (a == NULL || b == NULL)
        return a == b;

    if (a->length != b->length)
        return 0;

    for (size_t i = 0; i < a->length; ++i)
        if (strcmp(a->argv[i], b->argv[i]) != 0)
            return 0;
    return 1;
}

int program_add_argument(program_t *program, const char *argument) {
    if (!program || !argument) {
        errno = EINVAL;
        return 1;
    }

    char *new_argument = strdup(argument);
    if (!new_argument)
        return 1; /* errno = ENOMEM guaranteed */

    if (program->length >= program->capacity - 2) {
        size_t new_capacity = program->capacity * 2;
        char **new_argv     = realloc(program->argv, sizeof(char *) * new_capacity);
        if (!new_argv)
            return 1; /* errno = ENOMEM guaranteed */

        program->capacity = new_capacity;
        program->argv     = new_argv;
    }

    program->argv[program->length]     = new_argument;
    program->argv[program->length + 1] = NULL;
    program->length++;
    return 0;
}

const char *const *program_get_arguments(const program_t *program) {
    if (!program) {
        errno = EINVAL;
        return NULL;
    }
    return (const char *const *) program->argv;
}

ssize_t program_get_argument_count(const program_t *program) {
    if (!program) {
        errno = EINVAL;
        return -1;
    }
    return (ssize_t) program->length;
}
