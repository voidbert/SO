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
 * @file  util.c
 * @brief Implementation of methods in util.h
 */

#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "util.h"

/** @brief Buffer size used for printing messages and errors. */
#define UTIL_PRINT_BUFFER_SIZE (4 * LINE_MAX)

void util_log(const char *format, ...) {
    char buf[UTIL_PRINT_BUFFER_SIZE];

    va_list list;
    va_start(list, format);

    (void) !write(STDOUT_FILENO, buf, vsnprintf(buf, UTIL_PRINT_BUFFER_SIZE, format, list));

    va_end(list);
}

void util_error(const char *format, ...) {
    char buf[UTIL_PRINT_BUFFER_SIZE];

    va_list list;
    va_start(list, format);

    (void) !write(STDERR_FILENO, buf, vsnprintf(buf, UTIL_PRINT_BUFFER_SIZE, format, list));

    va_end(list);
}

void util_perror(const char *user_msg) {
    if (errno) {
        char error_msg[LINE_MAX];
        (void) strerror_r(errno, error_msg, LINE_MAX);

        if (user_msg && *user_msg)
            util_error("%s: %s\n", user_msg, error_msg);
        else
            util_error("%s: %s\n", error_msg);
    }
}
