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
#include <math.h>
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

int protocol_send_program_task_message_check_length(size_t message_length, size_t *command_length) {
    if (message_length <=
            sizeof(uint8_t) + sizeof(pid_t) + sizeof(struct timespec) + sizeof(uint32_t) ||
        message_length > IPC_MAXIMUM_MESSAGE_LENGTH)
        return 0;

    if (!command_length) {
        errno = EINVAL;
        return 0;
    }

    *command_length = message_length - sizeof(uint8_t) - sizeof(pid_t) - sizeof(struct timespec) -
                      sizeof(uint32_t);
    return 1;
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

int protocol_error_message_check_length(size_t message_length, size_t *error_length) {
    if (message_length <= sizeof(uint8_t) || message_length > IPC_MAXIMUM_MESSAGE_LENGTH)
        return 0;

    if (!error_length) {
        errno = EINVAL;
        return 0;
    }

    *error_length = message_length - sizeof(uint8_t);
    return 1;
}

/**
 * @brief Calculates the difference in microseconds between two `struct timespec`s.
 *
 * @param a Larger timestamp. Can be `NULL`.
 * @param b Smaller timestamp. Can be `NULL`.
 *
 * @return `a - b` in microseconds, `NaN` if one of them is `NULL`.
 */
double __protocol_status_time_diff(const struct timespec *a, const struct timespec *b) {
    if (!a || !b)
        return NAN;
    return (a->tv_sec - b->tv_sec) * 1000000.0 + (a->tv_nsec - b->tv_nsec) / 1000.0;
}

int protocol_status_response_message_new(
    protocol_status_response_message_t *out,
    size_t                             *out_size,
    const char                         *command_line,
    uint32_t                            id,
    uint8_t                             error,
    const struct timespec              *times[TAGGED_TASK_TIME_COMPLETED + 1]) {

    if (!out || !out_size || !command_line || !times) {
        errno = EINVAL;
        return 1;
    }

    out->type  = PROTOCOL_S2C_STATUS;
    out->id    = id;
    out->error = error;

    if (times[TAGGED_TASK_TIME_COMPLETED])
        out->status = PROTOCOL_TASK_STATUS_DONE;
    else if (times[TAGGED_TASK_TIME_DISPATCHED])
        out->status = PROTOCOL_TASK_STATUS_EXECUTING;
    else
        out->status = PROTOCOL_TASK_STATUS_QUEUED;

    out->time_c2s_fifo =
        __protocol_status_time_diff(times[TAGGED_TASK_TIME_ARRIVED], times[TAGGED_TASK_TIME_SENT]);
    out->time_waiting   = __protocol_status_time_diff(times[TAGGED_TASK_TIME_DISPATCHED],
                                                    times[TAGGED_TASK_TIME_ARRIVED]);
    out->time_executing = __protocol_status_time_diff(times[TAGGED_TASK_TIME_ENDED],
                                                      times[TAGGED_TASK_TIME_DISPATCHED]);
    out->time_s2s_fifo  = __protocol_status_time_diff(times[TAGGED_TASK_TIME_COMPLETED],
                                                     times[TAGGED_TASK_TIME_ENDED]);

    size_t len = strlen(command_line);
    if (len > PROTOCOL_STATUS_MAXIMUM_LENGTH) {
        errno = EMSGSIZE;
        return 1;
    }
    memcpy(out->command_line, command_line, len);

    *out_size = 3 * sizeof(uint8_t) + sizeof(uint32_t) + 4 * sizeof(double) + len;
    return 0;
}

int protocol_status_response_message_check_length(size_t message_length, size_t *command_length) {
    if (message_length <= 3 * sizeof(uint8_t) + sizeof(uint32_t) + 4 * sizeof(double) ||
        message_length > IPC_MAXIMUM_MESSAGE_LENGTH)
        return 0;

    if (!command_length) {
        errno = EINVAL;
        return 0;
    }

    *command_length = message_length - 3 * sizeof(uint8_t) - sizeof(uint32_t) - 4 * sizeof(double);
    return 1;
}
