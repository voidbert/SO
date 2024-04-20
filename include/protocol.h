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

#include <inttypes.h>
#include <sys/types.h>

#include "ipc.h"

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
} protocol_c2s_msg_type;

/** @brief Types of the messages sent from the server to the client. */
typedef enum {
    PROTOCOL_S2C_ERROR,   /**< @brief The server reports an error (a string) to the client. */
    PROTOCOL_S2C_TASK_ID, /**< @brief Server received a task and returned its ID. */
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
 *     @brief   Command line containing the task to be parsed.
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
 * @param expected_time Expected execution time in milliseconds.
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
 * @param length Length of the received message.
 *
 * @retval 0 Too long or too short.
 * @retval 1 Valid length.
 */
int protocol_send_program_task_message_check_length(size_t length);

/**
 * @brief  Gets the length of an ::protocol_send_program_task_message_t from the total message
 *         length.
 * @param  message_length Total length of the message.
 * @return The length of protocol_send_program_task_message_t::command_line.
 */
size_t protocol_send_program_task_message_get_error_length(size_t message_length);

/** @brief The maximum length of protocol_error_message_t::error. */
#define PROTOCOL_MAXIMUM_ERROR_LENGTH (IPC_MAXIMUM_MESSAGE_LENGTH - sizeof(uint8_t))

/**
 * @struct protocol_error_message_t
 * @brief  Structure of a message that tells the client an error occurred processing their request.
 *
 * @var protocol_error_message_t::type
 *     @brief Must be ::PROTOCOL_S2C_ERROR.
 * @var protocol_error_message_t::error
 *     @brief Error message.
 */
typedef struct __attribute__((packed)) {
    protocol_s2c_msg_type type : 8;
    char                  error[PROTOCOL_MAXIMUM_ERROR_LENGTH];
} protocol_error_message_t;

/**
 * @brief Creates a new message to report an error to the client.
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
 * @param length Length of the received message.
 *
 * @retval 0 Too long or too short.
 * @retval 1 Valid length.
 */
int protocol_error_message_check_length(size_t length);

/**
 * @brief  Gets the length of an ::protocol_error_message_t from the total message length.
 * @param  message_length Total length of the message.
 * @return The length of protocol_error_message_t::error.
 */
size_t protocol_error_message_get_error_length(size_t message_length);

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

#endif
