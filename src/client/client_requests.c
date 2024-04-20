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
#include <stdio.h>
#include <unistd.h>

#include "client/client_requests.h"
#include "protocol.h"

/**
 * @brief Listens to new messages coming from the server.
 *
 * @param message Bytes of the received message. Mustn't be `NULL` (unchecked).
 * @param length  Number of bytes in @p message. Must be greater than `0` (unchecked).
 * @param state   Always `NULL`.
 *
 * @retval 0 Always, even on error, not to drop any message.
 */
int __client_requests_on_message(uint8_t *message, size_t length, void *state) {
    (void) state;

    /* Zero-length messages are disallowed in ipc layer */
    protocol_s2c_msg_type type = message[0];
    switch (type) {
        case PROTOCOL_S2C_ERROR: {
            if (!protocol_error_message_check_length(length)) {
                fprintf(stderr, "Invalid S2C_ERROR message received!\n");
                return 0;
            }

            protocol_error_message_t *fields = (protocol_error_message_t *) message;
            (void) !write(STDERR_FILENO,
                          fields->error,
                          protocol_error_message_get_error_length(length));
        } break;

        case PROTOCOL_S2C_TASK_ID: {
            if (length != sizeof(protocol_task_id_message_t)) {
                fprintf(stderr, "Invalid S2C_TASK_ID message received!\n");
                return 0;
            }

            protocol_task_id_message_t *fields = (protocol_task_id_message_t *) message;
            printf("Task %" PRIu32 " scheduled\n", fields->id);
        } break;

        default:
            fprintf(stderr, "Message with bad type received!\n");
            break;
    }

    return 0;
}

/**
 * @brief Called before waiting for new connections, which are always refused.
 * @param state Always `NULL`.
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
        fprintf(stderr, "Command empty or too long (max: %ld)!\n", PROTOCOL_MAXIMUM_COMMAND_LENGTH);
        return 1;
    }

    ipc_t *ipc = ipc_new(IPC_ENDPOINT_CLIENT);
    if (!ipc) {
        if (errno == ENOENT)
            fprintf(stderr, "Server's FIFO not found. Is the server running?\n");
        else
            perror("Failed to open() server's FIFO");
        return 1;
    }

    if (ipc_send(ipc, &message, message_size)) {
        perror("Failed to write() message to server");
        ipc_free(ipc);
        return 1;
    }

    if (ipc_listen(ipc, __client_requests_on_message, __client_requests_before_block, NULL) == 1)
        perror("open() error");

    ipc_free(ipc);
    return 0;
}

int client_requests_send_program(const char *command_line, uint32_t expected_time) {
    return __client_requests_send_program_task(command_line, expected_time, 0);
}

int client_requests_send_task(const char *command_line, uint32_t expected_time) {
    return __client_requests_send_program_task(command_line, expected_time, 1);
}
