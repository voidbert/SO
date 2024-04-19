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
 * @file  server/server_requests.c
 * @brief Implementation of methods in server/server_requests.h
 */

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "client/client_requests.h"
#include "ipc.h"
#include "protocol.h"

/**
 * @struct server_state_t
 * @brief  The state of the server, made up by everything it needs to operate.
 *
 * @var server_state_t::ipc
 *     @brief IPC connection currently listening for new messages.
 */
typedef struct {
    ipc_t *ipc;
} server_state_t;

/**
 * @brief Listens to new messages coming from the clients.
 *
 * @param message    Bytes of the received message.
 * @param length     Number of bytes in @p message.
 * @param state_data A pointer to a ::server_state_t.
 *
 * @retval 0 Always, even on error, not to drop any message.
 */
int __server_requests_on_message(uint8_t *message, size_t length, void *state_data) {
    server_state_t *state = state_data;

    /* Zero-length messages are disallowed in ipc layer */
    protocol_c2s_msg_type type = (protocol_c2s_msg_type) message[0];
    switch (type) {
        case PROTOCOL_C2S_SEND_PROGRAM:
        case PROTOCOL_C2S_SEND_TASK: {
            if (!protocol_send_program_task_message_check_length(length)) {
                fprintf(stderr, "Invalid C2S_SEND_PROGRAM / C2S_SEND_TASK message received!\n");
                return 0;
            }

            protocol_send_program_task_message_t *fields =
                (protocol_send_program_task_message_t *) message;

            /* TODO - actually schedule a task */

            if (ipc_server_open_sending(state->ipc, fields->client_pid)) {
                perror("Failed to open() communication with the client");
                return 0;
            }

            protocol_task_id_message_t send_message = {.type = PROTOCOL_S2C_TASK_ID, .id = 69420};
            if (ipc_send(state->ipc, &send_message, sizeof(protocol_task_id_message_t)))
                perror("Failure during write() to client");

            (void) ipc_server_close_sending(state->ipc);
        } break;

        default:
            fprintf(stderr, "Message with bad type received!\n");
            break;
    }

    return 0;
}

/**
 * @brief Called before waiting for new connections, which are always accepted.
 * @param state A pointer to a ::server_state_t.
 * @retval -1 Refuse connection.
 */
int __server_requests_before_block(void *state) {
    (void) state;
    return 0; /* Always keep listening for new connections */
}

void server_requests_listen(void) {
    ipc_t *ipc = ipc_new(IPC_ENDPOINT_SERVER);
    if (!ipc) {
        if (errno == EEXIST)
            fprintf(stderr, "Server's FIFO already exists. Is the server running?\n");
        else
            perror("Failed to open() server's FIFO");
        return;
    }

    server_state_t state = {.ipc = ipc};
    (void) ipc_listen(ipc, __server_requests_on_message, __server_requests_before_block, &state);
    ipc_free(ipc);
}
