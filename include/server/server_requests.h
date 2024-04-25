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
 * @file    server/server_requests.h
 * @brief   Emitter and handler of requests from the server to the clients.
 * @details Read the assignment's report for the specification of the protocol used.
 */

#ifndef SERVER_REQUESTS_H
#define SERVER_REQUESTS_H

#include "server/scheduler.h"

/**
 * @brief   Open a listening connection and listens to incoming requests.
 * @details This procedure will output to `stderr` in case of error.
 *
 * @param policy Task scheduling policy.
 * @param ntasks Maximum number of tasks scheduled concurrently. Can't be `0`.
 *
 * @returns This function only exits on failure (`1`, check `errno`). It keeps running otherwise.
 *
 * | `errno`  | Cause                           |
 * | -------- | ------------------------------- |
 * | `EINVAL` | Invalid @p policy or @p ntasks. |
 * | `ENOMEM` | Allocation failure.             |
 */
int server_requests_listen(scheduler_policy_t policy, size_t ntasks, char *outputdir);

#endif
