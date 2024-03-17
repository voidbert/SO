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
 * @file  server/program.h
 * @brief A single program that must be executed (may be part of a pipeline).
 */

#include <sys/types.h>

/** @brief A single program that must be executed (may be part of a pipeline). */
typedef struct program program_t;

/**
 * @brief   Creates a empty program.
 * @details This program isn't valid, and needs to be populated with arguments.
 * @return  A new program on success, or `NULL` on allocation failure (`errno = ENOMEM`).
 */
program_t *program_new_empty(void);

/**
 * @brief Creates a new program from its arguments.
 *
 * @param arguments Array of program arguments (including program name). Can be `NULL` for this
 *                  method to have the same behavior as ::program_new_empty.
 * @param length    Number of arguments in @p arguments. Can be `-1` if @p arguments is
 *                  `NULL`-terminated. Must be `0` if @p arguments is NULL.
 *
 * @return A new program on success, or `NULL` on failure (check `errno`).
 *
 * | `errno`  | Cause                                            |
 * | -------- | ------------------------------------------------ |
 * | `EINVAL` | @p arguments is `NULL` and @p length is not `0`. |
 * | `ENOMEM` | Allocation failure.                              |
 */
program_t *program_new_from_arguments(const char *const *arguments, ssize_t length);

/**
 * @brief  Creates a deep copy of a program.
 * @param  program Program to be cloned.
 * @return A copy of @p program on success, NULL on failure (check `errno`).
 *
 * | `errno`  | Cause                |
 * | -------- | -------------------- |
 * | `EINVAL` | @p program is `NULL` |
 * | `ENOMEM` | Allocation failure.  |
 */
program_t *program_clone(const program_t *program);

/**
 * @brief Frees memory used by a program.
 * @param program Program to be deleted.
 */
void program_free(program_t *program);

/**
 * @brief Checks if two programs are equal.
 *
 * @param a First program to compare. Can be `NULL`.
 * @param b Second program to compare. Can be `NULL`.
 *
 * @return Whether @p a and @p b are the same command line.
 */
int program_equals(const program_t *a, const program_t *b);

/**
 * @brief Appends an argument to a program's argument list.
 *
 * @param program  Program to be modified.
 * @param argument Argument to be added to @p program.
 *
 * @retval 0 Success.
 * @retval 1 Invalid arguments or allocation failure (check `errno`).
 *
 * | `errno`  | Cause                                         |
 * | -------- | --------------------------------------------- |
 * | `EINVAL` | @p program or @p argument is `NULL` or empty. |
 * | `ENOMEM` | Allocation failure.                           |
 */
int program_add_argument(program_t *program, const char *argument);

/**
 * @brief  Gets the list of arguments in a program.
 * @param  program Program to get arguments from. Musn't be `NULL`.
 * @return A `NULL`-terminated list of program arguments, including the program's name, or `NULL` on
 *         failure (`errno = EINVAL`).
 */
const char *const *program_get_arguments(const program_t *program);

/**
 * @brief  Gets the number of arguments in a program (including the program's name).
 * @param  program Program to get number of arguments from. Musn't be `NULL`.
 * @return The number of arguments in a program (including the program's name), or `NULL` on
 *         failure (`errno = EINVAL`).
 */
ssize_t program_get_argument_count(const program_t *program);
