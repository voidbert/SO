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
#include "server/log_file.h"
#include "server/server_requests.h"
#include "server/status.h"
#include "util.h"

/**
 * @brief Maximum number of connection openings when the other side of the pipe is closed
 *        prematurely.
 */
#define SERVER_REQUESTS_MAX_RETRIES 16

/**
 * @struct server_state_t
 * @brief  The state of the server, made up by everything it needs to operate.
 *
 * @var server_state_t::ipc
 *     @brief IPC connection currently listening for new messages.
 * @var server_state_t::scheduler
 *     @brief Scheduler for client-submitted tasks.
 * @var server_state_t::status_scheduler
 *     @brief Scheduler for status tasks.
 * @var server_state_t::next_task_id
 *     @brief The identifier that will be attributed to the next scheduled task.
 * @var server_state_t::log
 *     @brief Where to log completed tasks to.
 */
typedef struct {
    ipc_t       *ipc;
    scheduler_t *scheduler, *status_scheduler;
    uint32_t     next_task_id;
    log_file_t  *log;
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
    size_t command_length;
    if (!protocol_send_program_task_message_check_length(length, &command_length)) {
        util_error("%s(): invalid message received!\n", __func__);
        return;
    }
    protocol_send_program_task_message_t *fields = (protocol_send_program_task_message_t *) message;

    /* Fix non-terminated string */
    char command_line[PROTOCOL_MAXIMUM_COMMAND_LENGTH + 1];
    memcpy(command_line, fields->command_line, command_length);
    command_line[command_length] = '\0';

    /* Try to create and schedule task */
    int            parsing_failure = 0;
    tagged_task_t *task =
        tagged_task_new_from_command_line(command_line, state->next_task_id, fields->expected_time);
    if (!task) {
        if (errno == EILSEQ) {
            parsing_failure = 1;
        } else {
            util_perror("__server_requests_on_schedule_message(): failed to create task");
            return;
        }
    } else {
        struct timespec time_sent = fields->time_sent, time_arrived = {0};
        (void) clock_gettime(CLOCK_MONOTONIC, &time_arrived);
        tagged_task_set_time(task, TAGGED_TASK_TIME_SENT, &time_sent);
        tagged_task_set_time(task, TAGGED_TASK_TIME_ARRIVED, &time_arrived);

        if (fields->type == PROTOCOL_C2S_SEND_PROGRAM) {
            size_t program_count;
            task_get_programs(tagged_task_get_task(task), &program_count);
            if (program_count != 1)
                parsing_failure = 1;
        }
    }

    if (!parsing_failure) {
        if (scheduler_add_task(state->scheduler, task)) {
            /* Out of memory: don't try to inform the client. */
            util_perror("__server_requests_on_schedule_message(): scheduler failure");
            tagged_task_free(task); /* Handles task = NULL in case of allocation failure */
            return;
        } else {
            state->next_task_id++;
        }
    }
    tagged_task_free(task);

    /* Reply to client */
    if (ipc_server_open_sending(state->ipc, fields->client_pid)) {
        util_perror("__server_requests_on_schedule_message(): failed to open connection");
        return;
    }

    if (parsing_failure) {
        size_t                   error_message_size;
        protocol_error_message_t error_message;
        protocol_error_message_new(&error_message, &error_message_size, "Parsing failure!\n");

        if (ipc_send_retry(state->ipc,
                           &error_message,
                           error_message_size,
                           SERVER_REQUESTS_MAX_RETRIES))
            util_perror("__server_requests_on_schedule_message(): failure sending message");
    } else {
        protocol_task_id_message_t success_message = {.type = PROTOCOL_S2C_TASK_ID,
                                                      .id   = state->next_task_id - 1};

        if (ipc_send_retry(state->ipc,
                           &success_message,
                           sizeof(protocol_task_id_message_t),
                           SERVER_REQUESTS_MAX_RETRIES))
            util_perror("__server_requests_on_schedule_message(): failure sending message");
    }

    ipc_server_close_sending(state->ipc);
}

/**
 * @brief   Handles an incoming ::protocol_task_done_message_t.
 * @details Returns nothing, as all errors are printed to `stderr`.
 *
 * @param state   State of the server. Mustn't be `NULL` (unchecked).
 * @param message Bytes of the received message. Mustn't be `NULL` (unchecked).
 * @param length  Number of bytes in @p message.
 */
void __server_requests_on_done_message(server_state_t *state, uint8_t *message, size_t length) {
    if (length != sizeof(protocol_task_done_message_t)) {
        util_error("%s(): invalid message received!\n", __func__);
        return;
    }
    protocol_task_done_message_t *fields = (protocol_task_done_message_t *) message;

    scheduler_t *target_scheduler = fields->is_status ? state->status_scheduler : state->scheduler;
    struct timespec time_ended    = fields->time_ended;
    tagged_task_t  *task = scheduler_mark_done(target_scheduler, fields->slot, &time_ended);
    if (!task) {
        util_error("%s(): message with invalid slot!\n", __func__);
        return;
    }

    if (!fields->is_status)
        if (log_file_write_task(state->log, task, fields->error))
            util_perror(
                "__server_requests_on_done_message(): failed to log completed task to file");
    tagged_task_free(task);
}

/**
 * @brief   Handles an incoming ::PROTOCOL_C2S_STATUS message.
 * @details Returns nothing, as all errors are printed to `stderr`.
 *
 * @param state   State of the server. Mustn't be `NULL` (unchecked).
 * @param message Bytes of the received message. Mustn't be `NULL` (unchecked).
 * @param length  Number of bytes in @p message.
 */
void __server_requests_on_status_message(server_state_t *state, uint8_t *message, size_t length) {
    if (length != sizeof(protocol_status_request_message_t)) {
        util_error("%s(): invalid message received!\n", __func__);
        return;
    }

    protocol_status_request_message_t *fields       = (protocol_status_request_message_t *) message;
    status_state_t                     status_state = {.ipc        = state->ipc,
                                                       .client_pid = fields->client_pid,
                                                       .log        = state->log,
                                                       .scheduler  = state->scheduler};
    tagged_task_t *task = tagged_task_new_from_procedure(status_main, &status_state, 0, 0);
    if (!task) {
        util_perror("__server_requests_on_status_message(): failed to create task");
        return;
    }

    if (!scheduler_can_schedule_now(state->status_scheduler)) {
        if (ipc_server_open_sending(state->ipc, fields->client_pid)) {
            util_perror("__server_requests_on_status_message(): failed to open connection");
            tagged_task_free(task);
            return;
        }

        size_t                   error_message_size;
        protocol_error_message_t error_message;
        protocol_error_message_new(&error_message, &error_message_size, "No capacity available!\n");

        if (ipc_send_retry(state->ipc,
                           &error_message,
                           error_message_size,
                           SERVER_REQUESTS_MAX_RETRIES))
            util_perror("__server_requests_on_status_message(): failure sending message");

        ipc_server_close_sending(state->ipc);
    } else if (scheduler_add_task(state->status_scheduler, task)) {
        util_perror("__server_requests_on_status_message(): scheduler failure");
        tagged_task_free(task);
        return;
    } else {
        if (scheduler_dispatch_possible(state->status_scheduler) < 0)
            util_perror("__server_requests_on_status_message(): scheduler failure");
        tagged_task_free(task);
    }
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
        case PROTOCOL_C2S_TASK_DONE:
            __server_requests_on_done_message(state, message, length);
            break;
        case PROTOCOL_C2S_STATUS:
            __server_requests_on_status_message(state, message, length);
            break;
        default:
            util_error("%s(): message with bad type received!\n", __func__);
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
 * @retval -1 Always. Refuse connection.
 */
int __server_requests_before_block(void *state_data) {
    server_state_t *state = state_data;
    if (scheduler_dispatch_possible(state->scheduler) < 0) /* New task or old task terminated */
        util_perror("__server_requests_before_block(): scheduler failure");
    return 0; /* Always keep listening for new connections */
}

/** @brief Maximum number of concurrent status tasks. */
#define SERVER_REQUESTS_MAXIMUM_STATUS_TASKS 32

int server_requests_listen(scheduler_policy_t policy, size_t ntasks, const char *directory) {
    if (!directory) {
        errno = EINVAL;
        return 1;
    }

    scheduler_t *scheduler = scheduler_new(policy, ntasks, directory);
    if (!scheduler) {
        util_perror("server_requests_listen(): failed to create main scheduler");
        return 1; /* errno = EINVAL guaranteed */
    }

    scheduler_t *status_scheduler =
        scheduler_new(SCHEDULER_POLICY_FCFS, SERVER_REQUESTS_MAXIMUM_STATUS_TASKS, "");
    if (!status_scheduler) {
        util_perror("server_requests_listen(): failed to create status scheduler");
        return 1; /* errno = EINVAL guaranteed */
    }

    ipc_t *ipc = ipc_new(IPC_ENDPOINT_SERVER);
    if (!ipc) {
        if (errno == EEXIST)
            util_error("Server's FIFO already exists. Is the server running?\n");
        else
            util_perror("server_requests_listen(): failed to open() server's FIFO");

        scheduler_free(scheduler);
        return 1;
    }

    char log_path[PATH_MAX];
    snprintf(log_path, PATH_MAX, "%s/log.bin", directory);
    log_file_t *log = log_file_new(log_path, 1);
    if (!log) {
        util_perror("server_requests_listen(): failed to open log file");
        scheduler_free(scheduler);
        ipc_free(ipc);
        return 1;
    }

    server_state_t state = {.ipc              = ipc,
                            .scheduler        = scheduler,
                            .status_scheduler = status_scheduler,
                            .next_task_id     = 1,
                            .log              = log};
    if (ipc_listen(ipc, __server_requests_on_message, __server_requests_before_block, &state) == 1)
        util_perror("server_requests_listen(): error opening connection");

    log_file_free(log);
    scheduler_free(status_scheduler);
    scheduler_free(scheduler);
    ipc_free(ipc);
    return 0;
}
