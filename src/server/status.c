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
#include <stdio.h>

#include "protocol.h"
#include "server/status.h"
#include "server/tagged_task.h"

/**
 * @brief Sends a message to the client with information about a tasks.
 *
 * @param ipc   Connection to the client.
 * @param error Whether an error happenned while running @p task.
 * @param task  Task to send to the client.
 *
 * @retval 0 Success.
 * @retval 1 `write()` failure (see `man 2 write` for the value of `errno`).
 */
int __status_send_message(ipc_t *ipc, int error, const tagged_task_t *task) {
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
        perror("Failure during write() to client");
        return 1;
    }
    return 0;
}

/**
 * @brief Method called for every task in the log file.
 *
 * @param task      Task in log file.
 * @param error     Whether an error happenned while running @p task.
 * @param state_ipc An `ipc_t *` to use for client communication.
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
 * @param task      Task in the scheduler.
 * @param state_ipc An `ipc_t *` to use for client communication.
 *
 * @retval 0 Always successful, ignoring `write()` errors.
 */
int __status_foreach_scheduler_task(const tagged_task_t *task, void *state_ipc) {
    (void) __status_send_message(state_ipc, 0, task); /* Ignore writing failures */
    return 0;
}

/**
 * @brief   Communicates to the parent server that the status task has terminated.
 * @details Errors will be outputted to `stderr`.
 *
 * @param slot   Slot in the scheduler where this task was scheduled.
 * @param secret Secret random number needed to authenticate the termination of the task.
 * @param error  Whether an error occurred while running the task.
 *
 * @retval 0 Success.
 * @retval 1 Failure (unspecified `errno`).
 */
int __status_warn_parent(size_t slot, uint64_t secret) {
    struct timespec time_ended = {0};
    (void) clock_gettime(CLOCK_MONOTONIC, &time_ended);
    protocol_task_done_message_t message = {.type       = PROTOCOL_C2S_TASK_DONE,
                                            .slot       = slot,
                                            .secret     = secret,
                                            .time_ended = time_ended,
                                            .is_status  = 1,
                                            .error      = 0};

    ipc_t *ipc = ipc_new(IPC_ENDPOINT_CLIENT);
    if (!ipc) {
        if (errno == ENOENT)
            fprintf(stderr, "Task completion communication: server's FIFO not found.\n");
        else
            perror("Task completion communication: Failed to open() server's FIFO");
        return 1;
    }

    if (ipc_send(ipc, &message, sizeof(protocol_task_done_message_t))) {
        perror("Task completion communication: failed to write() message to server");
        ipc_free(ipc);
        return 1;
    }

    ipc_free(ipc);
    return 0;
}

int status_main(void *state_data, size_t slot, uint64_t secret) {
    if (!state_data)
        return 1;

    status_state_t *state = (status_state_t *) state_data;

    if (ipc_server_open_sending(state->ipc, state->client_pid)) {
        perror("Failed to open() communication with the client");
        return 1;
    }

    if (log_file_read_tasks(state->log, __status_foreach_log_entry, state->ipc)) {
        perror("Failed to read from log file");
    }

    (void) scheduler_get_running_tasks(state->scheduler,
                                       __status_foreach_scheduler_task,
                                       state->ipc);
    (void) scheduler_get_scheduled_tasks(state->scheduler,
                                         __status_foreach_scheduler_task,
                                         state->ipc);

    (void) ipc_server_close_sending(state->ipc);
    __status_warn_parent(slot, secret);
    return 0;
}
