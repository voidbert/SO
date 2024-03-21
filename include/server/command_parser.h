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
 * @file  server/command_parser.h
 * @brief A parser of command lines, be them single programs or pipelines.
 */

#include <server/task.h>

/**
 * @brief Parses a command line that cannot be a pipeline.
 * @param command_line Command line to be parsed.
 *
 * @return On success, a program. On failure, `NULL` will be returned.
 *
 * | `errno`  | Cause                                         |
 * | -------- | --------------------------------------------- |
 * | `EINVAL` | @p command_line is `NULL` or parsing failure. |
 * | `ENOMEM` | Allocation failure.                           |
 */
program_t *command_parser_parse_command(const char *command_line);

/**
 * @brief Parses a command line.
 * @param command_line Command line to be parsed.
 *
 * @return On success, a task composed of either a single program or a pipeline. On failure, `NULL`
 *         will be returned.
 *
 * | `errno`  | Cause                                         |
 * | -------- | --------------------------------------------- |
 * | `EINVAL` | @p command_line is `NULL` or parsing failure. |
 * | `ENOMEM` | Allocation failure.                           |
 */
task_t *command_parser_parse_task(const char *command_line);
