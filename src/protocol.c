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
 * @file  protocol.c
 * @brief Implementation of methods in client/client_requests.h
 */

#include <errno.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "protocol.h"

int protocol_send_program_task_message_new(protocol_send_program_task_message_t *out,
                                           size_t                               *out_size,
                                           int                                   multiprogram,
                                           const char                           *command_line,
                                           uint32_t                              expected_time) {
    if (!out || !out_size || !command_line) {
        errno = EINVAL;
        return 1;
    }

    size_t len = strlen(command_line);
    if (len == 0 || len > PROTOCOL_MAXIMUM_COMMAND_LENGTH) {
        errno = EMSGSIZE;
        return 1;
    }
    *out_size = sizeof(uint8_t) + sizeof(pid_t) + sizeof(struct timespec) + sizeof(uint32_t) + len;

    out->type          = multiprogram ? PROTOCOL_C2S_SEND_TASK : PROTOCOL_C2S_SEND_PROGRAM;
    out->client_pid    = getpid();
    out->expected_time = expected_time;

    struct timespec t = {0};
    (void) clock_gettime(CLOCK_MONOTONIC, &t);
    out->time_sent = t;

    memcpy(out->command_line, command_line, len); /* Purposely don't copy null terminator */
    return 0;
}

int protocol_send_program_task_message_check_length(size_t length) {
    if (length <= sizeof(uint8_t) + sizeof(pid_t) + sizeof(struct timespec) + sizeof(uint32_t) ||
        length > IPC_MAXIMUM_MESSAGE_LENGTH)
        return 0;
    return 1;
}

size_t protocol_send_program_task_message_get_error_length(size_t message_length) {
    return message_length - sizeof(uint8_t) - sizeof(pid_t) - sizeof(struct timespec) -
           sizeof(uint32_t);
}

int protocol_error_message_new(protocol_error_message_t *out, size_t *out_size, const char *error) {
    if (!out || !out_size || !error) {
        errno = EINVAL;
        return 1;
    }

    size_t len = strlen(error);
    if (len == 0 || len > PROTOCOL_MAXIMUM_ERROR_LENGTH) {
        errno = EMSGSIZE;
        return 1;
    }

    *out_size = sizeof(uint8_t) + len;
    out->type = PROTOCOL_S2C_ERROR;
    memcpy(out->error, error, len);
    return 0;
}

int protocol_error_message_check_length(size_t length) {
    return length > sizeof(uint8_t) && length <= IPC_MAXIMUM_MESSAGE_LENGTH;
}

size_t protocol_error_message_get_error_length(size_t message_length) {
    return message_length - sizeof(uint8_t);
}
