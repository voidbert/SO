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
 * @file    util.h
 * @brief   Utility macros for printing to `stdout` and `stderr`.
 * @details The professors don't let us use `printf` and `fprintf`.
 */

#ifndef UTIL_H
#define UTIL_H

/* Ignore __attribute__((format)) if it's unavailable. */
#ifndef __GNUC__
    /** @cond FALSE */
    #define __attribute__()
    /** @endcond */
#endif

/**
 * @brief Writes a message to `stdout`.
 * @param format Format string (like `printf`).
 * @param ...    Values to be replaced in @p format.
 */
void util_log(const char *format, ...) __attribute__((format(printf, 1, 2)));

/**
 * @brief Writes a message to `stderr`.
 * @param format Format string (like `printf`).
 * @param ...    Values to be replaced in @p format.
 */
void util_error(const char *format, ...) __attribute__((format(printf, 1, 2)));

/**
 * @brief `perror` replacement.
 * @param user_msg `perror`'s message.
 */
void util_perror(const char *user_msg);

#endif
