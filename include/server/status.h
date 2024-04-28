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
 * @file  server/status.h
 * @brief Subprogram that tells the client the server's status.
 */

#ifndef STATUS_H
#define STATUS_H

#include "ipc.h"
#include "server/log_file.h"
#include "server/scheduler.h"

/**
 * @struct status_state_t
 * @brief  Data the status program needs to operate.
 *
 * @var status_state_t::ipc
 *     @brief ::IPC_ENDPOINT_SERVER connection, not yet open to the client.
 * @var status_state_t::client_pid
 *     @brief The PID of the client to send the status data to.
 * @var status_state_t::log
 *     @brief The server's log file, to get completed task information from.
 * @var status_state_t::scheduler
 *     @brief Scheduler information about scheduled and currently running.
 */
typedef struct {
    ipc_t       *ipc;
    pid_t        client_pid;
    log_file_t  *log;
    scheduler_t *scheduler;
} status_state_t;

/**
 * @brief Entry point to the subprogram that provides the status to the client.
 *
 * @param state_data A pointer to aa ::status_state_t. It's only a `void *` to match the signature
 *                   of ::task_prcedure_t. Mustn't be `NULL`.
 * @param slot       Slot where the task was scheduled.
 *
 * @return The exit code of the program. The value of `errno` is unspecified.
 */
int status_main(void *state_data, size_t slot);

#endif
