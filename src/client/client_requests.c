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
 * @file  client/client_requests.c
 * @brief Implementation of methods in client/client_requests.h
 */

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "client/client_requests.h"
#include "protocol.h"
#include "util.h"

/**
 * @brief Maximum number of connection openings when the other side of the pipe is closed
 *        prematurely.
 */
#define CLIENT_REQUESTS_MAX_RETRIES 16

/**
 * @brief Chooses the adequate unit to represent time.
 * @param time Time to be represented, in microseconds.
 * @param out  Where to output formatted time to.
 */
void __client_request_print_time_unit(double time, char *out) {
    if (time >= 1000000.0)
        sprintf(out, "%10.3lfs", time / 1000000.0);
    else if (time >= 1000.0)
        sprintf(out, "%.3lfms", time / 1000.0);
    else if (isnan(time))
        sprintf(out, "|-?-|");
    else
        sprintf(out, "%.3lfus", time);
}

/**
 * @brief   Handles an incoming ::PROTOCOL_C2S_STATUS message.
 * @details Returns nothing, as all errors are printed to `stderr`.
 *
 * @param message Bytes of the received message. Mustn't be `NULL` (unchecked).
 * @param length  Number of bytes in @p message.
 */
void __client_request_on_status_message(uint8_t *message, size_t length) {
    size_t command_length;
    if (!protocol_status_response_message_check_length(length, &command_length)) {
        util_error("%s(): invalid message received!\n", __func__);
        return;
    }
    protocol_status_response_message_t *fields = (protocol_status_response_message_t *) message;

    /* Fix non-terminated string */
    char command_line[PROTOCOL_STATUS_MAXIMUM_LENGTH + 1];
    memcpy(command_line, fields->command_line, command_length);
    command_line[command_length] = '\0';

    const char *status_str;
    switch (fields->status) {
        case PROTOCOL_TASK_STATUS_DONE:
            status_str = "DONE";
            break;
        case PROTOCOL_TASK_STATUS_EXECUTING:
            status_str = "EXECUTING";
            break;
        case PROTOCOL_TASK_STATUS_QUEUED:
            status_str = "QUEUED";
            break;
        default:
            status_str = "?";
            break;
    }

    char time_c2s_fifo_str[32], time_waiting_str[32], time_executing_str[32], time_s2s_fifo_str[32];
    __client_request_print_time_unit(fields->time_c2s_fifo, time_c2s_fifo_str);
    __client_request_print_time_unit(fields->time_waiting, time_waiting_str);
    __client_request_print_time_unit(fields->time_executing, time_executing_str);
    __client_request_print_time_unit(fields->time_s2s_fifo, time_s2s_fifo_str);

    util_log("(%s) %" PRIu32 ": \"%s\" %s %s %s %s%s\n",
             status_str,
             fields->id,
             command_line,
             time_c2s_fifo_str,
             time_waiting_str,
             time_executing_str,
             time_s2s_fifo_str,
             fields->error ? " (FAILED)" : "");
}

/**
 * @brief Listens to new messages coming from the server.
 *
 * @param message Bytes of the received message. Mustn't be `NULL` (unchecked).
 * @param length  Number of bytes in @p message. Must be greater than `0` (unchecked).
 * @param state   Always `NULL`.
 *
 * @retval 0 Success.
 * @retval 1 Failure (error message from client).
 */
int __client_requests_on_message(uint8_t *message, size_t length, void *state) {
    (void) state;

    /* Zero-length messages are disallowed in ipc layer */
    protocol_s2c_msg_type type = message[0];
    switch (type) {
        case PROTOCOL_S2C_ERROR: {
            size_t error_length;
            if (!protocol_error_message_check_length(length, &error_length)) {
                util_error("%s(): invalid S2C_ERROR message received!\n", __func__);
                return 0;
            }

            protocol_error_message_t *fields = (protocol_error_message_t *) message;
            (void) !write(STDERR_FILENO, fields->error, error_length);
            return 2;
        } break;

        case PROTOCOL_S2C_TASK_ID: {
            if (length != sizeof(protocol_task_id_message_t)) {
                util_error("%s(): invalid S2C_TASK_ID message received!\n", __func__);
                return 0;
            }

            protocol_task_id_message_t *fields = (protocol_task_id_message_t *) message;
            util_log("Task %" PRIu32 " scheduled\n", fields->id);
        } break;

        case PROTOCOL_S2C_STATUS:
            __client_request_on_status_message(message, length);
            break;

        default:
            util_error("%s(): message with bad type received!\n", __func__);
            break;
    }

    return 0;
}

/**
 * @brief  Called before waiting for new connections, which are always refused.
 * @param  state Always `NULL`.
 * @retval -1 Refuse connection.
 */
int __client_requests_before_block(void *state) {
    (void) state;
    return -1; /* Only allow one connection to be opened */
}

/**
 * @brief Submits a task or a program to be executed by the server.
 *
 * @param command_line  Command line containing the single command / pipeline. Mustn't be `NULL`.
 * @param expected_time Expected execution time in milliseconds.
 * @param multiprogram  Whether @p command_line can contain pipelines.
 *
 * @retval 0 Success.
 * @retval 1 Failure (unspecified `errno`).
 */
int __client_requests_send_program_task(const char *command_line,
                                        uint32_t    expected_time,
                                        int         multiprogram) {
    size_t                               message_size;
    protocol_send_program_task_message_t message;
    if (protocol_send_program_task_message_new(&message,
                                               &message_size,
                                               multiprogram,
                                               command_line,
                                               expected_time)) {
        /* Assume command_line isn't NULL for error message */
        util_error("Command empty or too long (max: %ld)!\n", PROTOCOL_MAXIMUM_COMMAND_LENGTH);
        return 1;
    }

    ipc_t *ipc = ipc_new(IPC_ENDPOINT_CLIENT);
    if (!ipc) {
        if (errno == ENOENT)
            util_error("Server's FIFO not found. Is the server running?\n");
        else
            util_perror("client_request_ask_status(): failed to open() server's FIFO");
        return 1;
    }

    if (ipc_send(ipc, &message, message_size)) {
        util_perror("client_request_ask_status(): failed to send message to server");
        ipc_free(ipc);
        return 1;
    }

    int listen_res =
        ipc_listen(ipc, __client_requests_on_message, __client_requests_before_block, NULL);
    if (listen_res == 1)
        util_perror("client_requests_ask_status(): error opening connection");

    ipc_free(ipc);
    return listen_res == 2; /* 2 -> server failure */
}

int client_requests_send_program(const char *command_line, uint32_t expected_time) {
    return __client_requests_send_program_task(command_line, expected_time, 0);
}

int client_requests_send_task(const char *command_line, uint32_t expected_time) {
    return __client_requests_send_program_task(command_line, expected_time, 1);
}

int client_request_ask_status(void) {
    ipc_t *ipc = ipc_new(IPC_ENDPOINT_CLIENT);
    if (!ipc) {
        if (errno == ENOENT)
            util_error("Server's FIFO not found. Is the server running?\n");
        else
            util_perror("client_request_ask_status(): failed to open() server's FIFO");
        return 1;
    }

    protocol_status_request_message_t message = {.type       = PROTOCOL_C2S_STATUS,
                                                 .client_pid = getpid()};
    if (ipc_send_retry(ipc,
                       &message,
                       sizeof(protocol_status_request_message_t),
                       CLIENT_REQUESTS_MAX_RETRIES)) {
        util_perror("client_request_ask_status(): failed to send message to server");
        ipc_free(ipc);
        return 1;
    }

    if (ipc_listen(ipc, __client_requests_on_message, __client_requests_before_block, NULL) == 1)
        util_perror("client_requests_ask_status(): error opening connection");
    ipc_free(ipc);
    return 0;
}
