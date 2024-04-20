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
 * @file    client/client_requests.h
 * @brief   Emitter of requests from the client to the server and handler of requests in the opposite direction.
 * @details See protocol.h and read the assignment's report for the specification of the protocol
 *          used.
 */

#ifndef CLIENT_REQUESTS_H
#define CLIENT_REQUESTS_H

#include <inttypes.h>

/**
 * @brief   Submits a command (task that cannot contain pipelines) to the server.
 * @details This procedure will output to `stderr` in case of error.
 *
 * @param command_line  Command line of the command to be sent. Mustn't be `NULL`.
 * @param expected_time Expected execution time in milliseconds.
 *
 * @return The value to be returned by `main()`. Final `errno` is unspecified, as all errors are
 *         printed to `stderr`.
 */
int client_requests_send_program(const char *command_line, uint32_t expected_time);

/**
 * @brief   Submits a task (that can contain pipelines) to the server.
 * @details This procedure will output to `stderr` in case of error.
 *
 * @param command_line  Command line of the task to be sent. Mustn't be `NULL`.
 * @param expected_time Expected execution time in milliseconds.
 *
 * @return The value to be returned by `main()`. No `errno` is unspecified, as all errors are
 *         printed to `stderr`.
 */
int client_requests_send_task(const char *command_line, uint32_t expected_time);

#endif
