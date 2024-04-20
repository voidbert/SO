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
#include <stdio.h>
#include <string.h>

#include "protocol.h"
#include "server/server_requests.h"

/**
 * @struct server_state_t
 * @brief  The state of the server, made up by everything it needs to operate.
 *
 * @var server_state_t::ipc
 *     @brief IPC connection currently listening for new messages.
 * @var server_state_t::scheduler
 *     @brief Task scheduler.
 * @var server_state_t::next_task_id
 *     @brief The identifier that will be attributed to the next scheduled task.
 */
typedef struct {
    ipc_t       *ipc;
    scheduler_t *scheduler;
    uint32_t     next_task_id;
} server_state_t;

/**
 * @brief   Handles an incoming ::protocol_send_program_task_message_t.
 * @details Returns nothing, as all errors are printed to `stderr`.
 *
 * @param state   State of the server. Mustn't be `NULL` (unchecked).
 * @param message Bytes of the received message. Mustn't be `NULL` (unchecked).
 * @param length  Number of bytes in @p message.
 */
void __server_requests_on_schedule_message(server_state_t *state, uint8_t *message, size_t length) {
    if (!protocol_send_program_task_message_check_length(length)) {
        fprintf(stderr, "Invalid C2S_SEND_PROGRAM / C2S_SEND_TASK message received!\n");
        return;
    }
    protocol_send_program_task_message_t *fields = (protocol_send_program_task_message_t *) message;

    /* Fix non-terminated string */
    char   command_line[PROTOCOL_MAXIMUM_COMMAND_LENGTH + 1];
    size_t command_length = protocol_send_program_task_message_get_error_length(length);
    memcpy(command_line, fields->command_line, command_length);
    command_line[command_length] = '\0';

    /* Try to create and schedule task */
    int            parsing_failure = 0;
    tagged_task_t *task = tagged_task_new(command_line, state->next_task_id, fields->expected_time);
    if (!task && errno == EINVAL)
        parsing_failure = 1;

    if (task && fields->type == PROTOCOL_C2S_SEND_PROGRAM) {
        size_t program_count;
        (void) task_get_programs(tagged_task_get_task(task), &program_count);
        if (program_count != 1)
            parsing_failure = 1;
    }

    if (!parsing_failure) {
        if (scheduler_add_task(state->scheduler, task)) {
            /*
            * Informing the client of this failure may require allocations. Just log it and let the
            * client block.
            */
            fprintf(stderr, "Allocation failure! Go buy more RAM!\n");
            tagged_task_free(task); /* Handles task = NULL in case of allocation failure */
            return;
        } else {
            state->next_task_id++;
        }
    }
    tagged_task_free(task); /* Handles task = NULL in case of allocation failure */

    /* Reply to client */
    if (ipc_server_open_sending(state->ipc, fields->client_pid)) {
        perror("Failed to open() communication with the client");
        return;
    }

    if (parsing_failure) {
        size_t                   error_message_size;
        protocol_error_message_t error_message;
        protocol_error_message_new(&error_message, &error_message_size, "Parsing failure!\n");

        if (ipc_send(state->ipc, &error_message, error_message_size))
            perror("Failure during write() to client");
    } else {
        protocol_task_id_message_t success_message = {.type = PROTOCOL_S2C_TASK_ID,
                                                      .id   = state->next_task_id - 1};

        if (ipc_send(state->ipc, &success_message, sizeof(protocol_task_id_message_t)))
            perror("Failure during write() to client");
    }

    (void) ipc_server_close_sending(state->ipc);
}

/**
 * @brief Listens to new messages coming from the clients.
 *
 * @param message    Bytes of the received message. Mustn't be `NULL` (unchecked).
 * @param length     Number of bytes in @p message. Must be greater than `0` (unchecked).
 * @param state_data A pointer to a ::server_state_t. Mustn't be `NULL` (unchecked).
 *
 * @retval 0 Always, even on error, not to drop any message.
 */
int __server_requests_on_message(uint8_t *message, size_t length, void *state_data) {
    server_state_t *state = state_data;

    /* Zero-length messages are disallowed in ipc layer */
    protocol_c2s_msg_type type = (protocol_c2s_msg_type) message[0];
    switch (type) {
        case PROTOCOL_C2S_SEND_PROGRAM:
        case PROTOCOL_C2S_SEND_TASK:
            __server_requests_on_schedule_message(state, message, length);
            break;
        default:
            fprintf(stderr, "Message with bad type received!\n");
            break;
    }

    return 0;
}

/**
 * @brief   Called before waiting for new connections, which are always accepted.
 * @details This method also starts running scheduled tasks if there's any availability.
 *
 * @param state_data A pointer to a ::server_state_t. Mustn't be `NULL` (unchecked).
 *
 * @retval -1 Refuse connection.
 */
int __server_requests_before_block(void *state_data) {
    server_state_t *state = state_data;
    if (scheduler_dispatch_possible(state->scheduler) < 0)
        perror("Scheduler failure");
    return 0; /* Always keep listening for new connections */
}

int server_requests_listen(scheduler_policy_t policy, size_t ntasks) {
    scheduler_t *scheduler = scheduler_new(policy, ntasks);
    if (!scheduler)
        return 1;

    ipc_t *ipc = ipc_new(IPC_ENDPOINT_SERVER);
    if (!ipc) {
        if (errno == EEXIST)
            fprintf(stderr, "Server's FIFO already exists. Is the server running?\n");
        else
            perror("Failed to open() server's FIFO");

        scheduler_free(scheduler);
        return 1;
    }

    server_state_t state = {.ipc = ipc, .scheduler = scheduler, .next_task_id = 1};
    if (ipc_listen(ipc, __server_requests_on_message, __server_requests_before_block, &state) == 1)
        perror("open() error");

    scheduler_free(scheduler);
    ipc_free(ipc);
    return 0;
}
