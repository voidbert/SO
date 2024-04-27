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
 * @file    protocol.h
 * @brief   Definition of messages sent between the clients and the server (and vice-versa).
 * @details Read the assignment's report for the specification of the protocol used.
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <sys/types.h>

#include "ipc.h"
#include "server/tagged_task.h"

/* Ignore __attribute__((packed)) if it's unavailable. */
#ifndef __GNUC__
    /** @cond FALSE */
    #define __attribute__()
    /** @endcond */
#else
    /*
     * Many methods don't work if the compiler doesn't support the removal of struct padding.
     * However, there is no standard way of throwing an error to warn the user.
     */
#endif

/** @brief Types of the messages sent from the client to the server. */
typedef enum {
    PROTOCOL_C2S_SEND_PROGRAM, /**< @brief Send a command with no pipelines to be executed. */
    PROTOCOL_C2S_SEND_TASK,    /**< @brief Send a task that may contain pipelines to be executed. */
    PROTOCOL_C2S_TASK_DONE,    /**< @brief Server's child completed the execution of a task. */
    PROTOCOL_C2S_STATUS,       /**< @brief Client asks for the server's status. */
} protocol_c2s_msg_type;

/** @brief Types of the messages sent from the server to the client. */
typedef enum {
    PROTOCOL_S2C_ERROR,   /**< @brief The server reports an error (a string) to the client. */
    PROTOCOL_S2C_TASK_ID, /**< @brief Server received a task and returned its ID. */
    PROTOCOL_S2C_STATUS,  /**< @brief Status response with a task. */
} protocol_s2c_msg_type;

/** @brief The maximum length of protocol_send_program_task_message_t::command_line */
#define PROTOCOL_MAXIMUM_COMMAND_LENGTH                                                            \
    (IPC_MAXIMUM_MESSAGE_LENGTH - sizeof(uint8_t) - sizeof(pid_t) - sizeof(struct timespec) -      \
     sizeof(uint32_t))

/**
 * @struct protocol_send_program_task_message_t
 * @brief  Structure of a message for submitting a program or a task to the server.
 *
 * @var protocol_send_program_task_message_t::type
 *     @brief Either ::PROTOCOL_C2S_SEND_PROGRAM or ::PROTOCOL_C2S_SEND_TASK.
 * @var protocol_send_program_task_message_t::client_pid
 *     @brief PID of the client that sent this message.
 * @var protocol_send_program_task_message_t::time_sent
 *     @brief Timestamp when the client sent the task.
 * @var protocol_send_program_task_message_t::expected_time
 *     @brief Expected execution time in milliseconds.
 * @var protocol_send_program_task_message_t::command_line
 *     @brief   Command line to be parsed forming a task.
 *     @details Note that, on reception of a message, not all bytes of this array may be valid, nor
 *              is this string null-terminated, as the real string length is determined by the
 *              message's total length.
 */
typedef struct __attribute__((packed)) {
    protocol_c2s_msg_type type : 8;
    pid_t                 client_pid;
    struct timespec       time_sent;
    uint32_t              expected_time;
    char                  command_line[PROTOCOL_MAXIMUM_COMMAND_LENGTH];
} protocol_send_program_task_message_t;

/**
 * @brief Creates a new message to send a program / pipeline to the server.
 *
 * @param out           Where to output the message to. Mustn't be `NULL`. Will only be modified
 *                      when this function succeeds.
 * @param out_size      Where to output the number of bytes in the final message to. Mustn't be
 *                      `NULL`. Will only be set when this function succeeds.
 * @param multiprogram  Whether @p command_line refers to a pipeline or just a single program.
 * @param command_line  Command line to be sent to the server for parsing and execution. Mustn't be
 *                      `NULL`.
 * @param expected_time Expected execution time in milliseconds reported by the client.
 *
 * @retval 0 Success.
 * @retval 1 Failure (check `errno`).
 *
 * | `errno`    | Cause                                                             |
 * | ---------- |  ---------------------------------------------------------------- |
 * | `EINVAL`   | @p out, @p out_size or @p command_line are `NULL`.                |
 * | `EMSGSIZE` | @p command_line is longer than ::PROTOCOL_MAXIMUM_COMMAND_LENGTH. |
 */
int protocol_send_program_task_message_new(protocol_send_program_task_message_t *out,
                                           size_t                               *out_size,
                                           int                                   multiprogram,
                                           const char                           *command_line,
                                           uint32_t                              expected_time);

/**
 * @brief Checks if a received ::protocol_send_program_task_message_t can have a given length.
 *
 * @param message_length Length of the received message.
 * @param command_length Where to write (on success) the length of
 *                       protocol_send_program_task_message_t::command_line to. Mustn't be `NULL`.
 *
 * @retval 0 Too long, too short or `NULL` @p command_length (in this last case, `errno = EINVAL`).
 * @retval 1 Valid length.
 */
int protocol_send_program_task_message_check_length(size_t message_length, size_t *command_length);

/**
 * @struct  protocol_status_request_message_t
 * @brief   Structure of a message asking a server for its status.
 * @details A constructor and a message length checker isn't available for such a trivial message
 *          type.
 *
 * @var protocol_status_request_message_t::type
 *     @brief Must be ::PROTOCOL_C2S_STATUS.
 * @var protocol_status_request_message_t::client_pid
 *     @brief PID of the client that sent this message.
 */
typedef struct __attribute__((packed)) {
    protocol_c2s_msg_type type : 8;
    pid_t                 client_pid;
} protocol_status_request_message_t;

/**
 * @struct  protocol_task_done_message_t
 * @brief   Structure of a message that tells the server one of its children terminated.
 * @details A constructor and a message length checker isn't available for such a trivial message
 *          type.
 *
 * @var protocol_task_done_message_t::type
 *     @brief Must be ::PROTOCOL_C2S_TASK_DONE.
 * @var protocol_task_done_message_t::slot
 *     @brief Slot where the message was scheduled. See ::scheduler_mark_done.
 * @var protocol_task_done_message_t::time_ended
 *     @brief When task execution ended. See ::scheduler_mark_done.
 * @var protocol_task_done_message_t::is_status
 *     @brief If the scheduled task is a status task.
 * @var protocol_task_done_message_t::error
 *     @brief Whether running the task resulted in an error.
 */
typedef struct __attribute__((packed)) {
    protocol_c2s_msg_type type : 8;
    size_t                slot;
    struct timespec       time_ended;
    uint8_t               is_status;
    uint8_t               error;
} protocol_task_done_message_t;

/** @brief The maximum length of protocol_error_message_t::error. */
#define PROTOCOL_MAXIMUM_ERROR_LENGTH (IPC_MAXIMUM_MESSAGE_LENGTH - sizeof(uint8_t))

/**
 * @struct protocol_error_message_t
 * @brief  Structure of a message that tells the client an error occurred processing their request.
 *
 * @var protocol_error_message_t::type
 *     @brief Must be ::PROTOCOL_S2C_ERROR.
 * @var protocol_error_message_t::error
 *     @brief Error message string.
 */
typedef struct __attribute__((packed)) {
    protocol_s2c_msg_type type : 8;
    char                  error[PROTOCOL_MAXIMUM_ERROR_LENGTH];
} protocol_error_message_t;

/**
 * @brief Creates a new message to report an error to the client.
 *
 * @param out      Where to output the message to. Mustn't be `NULL`. Will only be modified when
 *                 this function succeeds.
 * @param out_size Where to output the number of bytes in the final message to. Mustn't be `NULL`.
 *                 Will only be set when this function succeeds.
 * @param error    Error message to be sent. Mustn't be `NULL`.
 *
 * @retval 0 Success.
 * @retval 1 Failure (check `errno`).
 *
 * | `errno`    | Cause                                                    |
 * | ---------- |  ------------------------------------------------------- |
 * | `EINVAL`   | @p out, @p out_size or @p error are `NULL`.              |
 * | `EMSGSIZE` | @p error is longer than ::PROTOCOL_MAXIMUM_ERROR_LENGTH. |
 */
int protocol_error_message_new(protocol_error_message_t *out, size_t *out_size, const char *error);

/**
 * @brief Checks if a received ::protocol_error_message_t can have a given length.
 *
 * @param message_length Length of the received message.
 * @param error_length   Where to write (on success) the length of protocol_error_message_t::error
 *                       to. Mustn't be `NULL`.
 *
 * @retval 0 Too long, too short or `NULL` @p error_length (in this last case, `errno = EINVAL`).
 * @retval 1 Valid length.
 */
int protocol_error_message_check_length(size_t message_length, size_t *error_length);

/**
 * @struct  protocol_task_id_message_t
 * @brief   Structure of a message that tells the client the identifier of a scheduled task.
 * @details A constructor and a message length checker isn't available for such a trivial message
 *          type.
 *
 * @var protocol_task_id_message_t::type
 *     @brief Must be ::PROTOCOL_S2C_TASK_ID.
 * @var protocol_task_id_message_t::id
 *     @brief Identifier of the task.
 */
typedef struct __attribute__((packed)) {
    protocol_s2c_msg_type type : 8;
    uint32_t              id;
} protocol_task_id_message_t;

/** @brief The maximum length of protocol_status_response_message_t::command_line. */
#define PROTOCOL_STATUS_MAXIMUM_LENGTH                                                             \
    (PIPE_BUF - sizeof(uint8_t) * 3 - sizeof(uint32_t) - 4 * sizeof(double))

/** @brief The status of a task in a ::protocol_status_response_message_t. */
typedef enum {
    PROTOCOL_TASK_STATUS_DONE,      /**< @brief Task done executing. */
    PROTOCOL_TASK_STATUS_EXECUTING, /**< @brief Task currently executing. */
    PROTOCOL_TASK_STATUS_QUEUED,    /**< @brief Task queued for execution. */
} protocol_task_status_t;

/**
 * @struct protocol_status_response_message_t
 * @brief  Structure of a message that tells the client the status of a single task.
 *
 * @var protocol_status_response_message_t::type
 *     @brief Must be ::PROTOCOL_S2C_STATUS.
 * @var protocol_status_response_message_t::status
 *     @brief Status of the task this message refers to.
 * @var protocol_status_response_message_t::id
 *     @brief Identifier of the task this message refers to.
 * @var protocol_status_response_message_t::error
 *     @brief Whether an error occurred while running the task this message refers to.
 * @var protocol_status_response_message_t::time_c2s_fifo
 *     @brief Time in microseconds that it took for the task to get from the client to the server.
 * @var protocol_status_response_message_t::time_waiting
 *     @brief Time in microseconds that the task spent queued. Only applies to
 *            ::PROTOCOL_TASK_STATUS_DONE and ::PROTOCOL_TASK_STATUS_EXECUTING.
 * @var protocol_status_response_message_t::time_executing
 *     @brief Time in microseconds that the task spent executing. Only applies to
 *            ::PROTOCOL_TASK_STATUS_DONE.
 * @var protocol_status_response_message_t::time_s2s_fifo
 *     @brief Time in microseconds that it took the server's fork to warn its parent it had
 *            completed.
 * @var protocol_status_response_message_t::command_line
 *     @brief Command line of the task submitted.
 */
typedef struct __attribute__((packed)) {
    protocol_s2c_msg_type  type   : 8;
    protocol_task_status_t status : 8;
    uint32_t               id;
    uint8_t                error;
    double                 time_c2s_fifo, time_waiting, time_executing, time_s2s_fifo;
    char                   command_line[PROTOCOL_STATUS_MAXIMUM_LENGTH];
} protocol_status_response_message_t;

/**
 * @brief Creates a new message to report the status of a task to the client.
 *
 * @param out          Where to output the message to. Mustn't be `NULL`. Will only be modified when
 *                     this function succeeds.
 * @param out_size     Where to output the number of bytes in the final message to. Mustn't be
 *                     `NULL`. Will only be set when this function succeeds.
 * @param command_line Command line of the task. Mustn't be `NULL`.
 * @param id           Identifier of the task this message refers to.
 * @param error        Whether an error occurred while running the task this message refers to.
 * @param times        Result of calling ::tagged_task_get_time for every ::tagged_task_time_t.
 *
 * @retval 0 Success.
 * @retval 1 Failure (check `errno`).
 *
 * | `errno`    | Cause                                                    |
 * | ---------- |  ------------------------------------------------------- |
 * | `EINVAL`   | @p out, @p out_size or @p error are `NULL`.              |
 * | `EMSGSIZE` | @p error is longer than ::PROTOCOL_MAXIMUM_ERROR_LENGTH. |
 */
int protocol_status_response_message_new(
    protocol_status_response_message_t *out,
    size_t                             *out_size,
    const char                         *command_line,
    uint32_t                            id,
    uint8_t                             error,
    const struct timespec              *times[TAGGED_TASK_TIME_COMPLETED + 1]);

/**
 * @brief Checks if a received ::protocol_status_response_message_t can have a given length.
 *
 * @param message_length Length of the received message.
 * @param command_length Where to write (on success) the length of
 *                       protocol_status_response_message_t::command_line to. Mustn't be `NULL`.
 *
 * @retval 0 Too long, too short or `NULL` @p command_length (in this last case, `errno = EINVAL`).
 * @retval 1 Valid length.
 */
int protocol_status_response_message_check_length(size_t message_length, size_t *command_length);

#endif
