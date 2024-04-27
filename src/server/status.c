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
 * @file  server/status.c
 * @brief Implementation of methods in server/status.h
 */

#include <errno.h>

#include "protocol.h"
#include "server/status.h"
#include "server/tagged_task.h"
#include "util.h"

/**
 * @brief Sends a message to the client with information about a single task.
 *
 * @param ipc   Connection to the client. Mustn't be `NULL`.
 * @param error Whether an error happenned while running @p task.
 * @param task  Task to send to the client. Mustn't be `NULL`.
 *
 * @retval 0 Success.
 * @retval 1 `write()` failure (check `errno`).
 *
 * | `errno`    | Cause                        |
 * | ---------- |  --------------------------- |
 * | `EINVAL`   | @p ipc or @p task is `NULL`. |
 * | other      | See `man 2 write`.           |
 */
int __status_send_message(ipc_t *ipc, int error, const tagged_task_t *task) {
    if (!ipc || !task) {
        errno = EINVAL;
        return 1;
    }

    const struct timespec *times[TAGGED_TASK_TIME_COMPLETED + 1];
    for (tagged_task_time_t i = 0; i <= TAGGED_TASK_TIME_COMPLETED; ++i)
        times[i] = tagged_task_get_time(task, i);

    protocol_status_response_message_t message;
    size_t                             message_length;
    (void) protocol_status_response_message_new(&message,
                                                &message_length,
                                                tagged_task_get_command_line(task),
                                                times);

    (void) error; /* TODO - Inform client of errors */

    if (ipc_send(ipc, &message, message_length)) {
        util_perror("__status_send_message(): error while sending message to client");
        return 1;
    }
    return 0;
}

/**
 * @brief Method called for every task in the log file.
 *
 * @param task      Task in log file. Mustn't be `NULL` (unchecked).
 * @param error     Whether an error happenned while running @p task.
 * @param state_ipc An `ipc_t *` to use for client communication. Mustn't be `NULL` (unchecked).
 *
 * @retval 0 Always successful, ignoring `write()` errors.
 */
int __status_foreach_log_entry(const tagged_task_t *task, int error, void *state_ipc) {
    (void) __status_send_message(state_ipc, error, task); /* Ignore writing failures */
    return 0;
}

/**
 * @brief Method called for every task currently in the scheduler.
 *
 * @param task      Task in the scheduler (running or scheduled). Mustn't be `NULL` (unchecked).
 * @param state_ipc An `ipc_t *` to use for client communication. Mustn't be `NULL` (unchecked).
 *
 * @retval 0 Always successful, ignoring `write()` errors.
 */
int __status_foreach_scheduler_task(const tagged_task_t *task, void *state_ipc) {
    (void) __status_send_message(state_ipc, 0, task); /* Ignore writing failures */
    return 0;
}

/**
 * @brief   Maximum number of connection openings when the other side of the pipe is closed
 *          prematurely.
 * @details This number must be high, as any communication failure means loss of server scheduling
 *          capacity.
 */
#define STATUS_WARN_PARENT_MAX_RETRIES 16

/**
 * @brief   Communicates to the parent server that the status task has terminated.
 * @details Errors will be outputted to `stderr`.
 *
 * @param slot Slot in the scheduler where this task was scheduled.
 *
 * @retval 0 Success.
 * @retval 1 Failure (unspecified `errno`).
 */
int __status_warn_parent(size_t slot) {
    struct timespec time_ended = {0};
    (void) clock_gettime(CLOCK_MONOTONIC, &time_ended);
    protocol_task_done_message_t message = {.type       = PROTOCOL_C2S_TASK_DONE,
                                            .slot       = slot,
                                            .time_ended = time_ended,
                                            .is_status  = 1,
                                            .error      = 0};

    ipc_t *ipc = ipc_new(IPC_ENDPOINT_CLIENT);
    if (!ipc) {
        util_perror("__status_warn_parent(): error while opening connection");
        return 1;
    }

    if (ipc_send_retry(ipc,
                       &message,
                       sizeof(protocol_task_done_message_t),
                       STATUS_WARN_PARENT_MAX_RETRIES)) {
        util_perror("__staus_warn_parent(): error while sending message to parent");
        ipc_free(ipc);
        return 1;
    }

    ipc_free(ipc);
    return 0;
}

int status_main(void *state_data, size_t slot) {
    if (!state_data)
        return 1;

    status_state_t *state = (status_state_t *) state_data;

    if (ipc_server_open_sending(state->ipc, state->client_pid)) {
        util_perror("status_main(): failed to open() connection with the client");
        return 1;
    }

    if (log_file_read_tasks(state->log, __status_foreach_log_entry, state->ipc))
        util_perror("status_main(): failed to read from log file. continuing");

    (void) scheduler_get_running_tasks(state->scheduler,
                                       __status_foreach_scheduler_task,
                                       state->ipc);
    (void) scheduler_get_scheduled_tasks(state->scheduler,
                                         __status_foreach_scheduler_task,
                                         state->ipc);

    ipc_server_close_sending(state->ipc);
    return __status_warn_parent(slot);
}
