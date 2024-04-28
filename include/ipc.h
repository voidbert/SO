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
 * @file  ipc.h
 * @brief Inter-process communication between the client and the server using named pipes.
 */

#ifndef IPC_H
#define IPC_H

#include <inttypes.h>
#include <limits.h>

/** @brief The type of endpoint (this program is) in an IPC. */
typedef enum {
    IPC_ENDPOINT_CLIENT, /**< Client side (this includes orchestrator's children). */
    IPC_ENDPOINT_SERVER, /**< Server side. */
} ipc_endpoint_t;

/**
 * @brief   Type of procedure called for every message received in an IPC.
 * @details See ::ipc_listen.
 *
 * @param message Bytes of the received message.
 * @param length  Number of bytes in @p message.
 * @param state   A pointer passed to ::ipc_listen so that this callback can modify the program's
 *                state.
 *
 * @retval 0 Success. Continue listening for messages.
 * @retval 1 Failure. Stop listening for messages. All messages already in the FIFO will be
 *           discarded.
 */
typedef int (*ipc_on_message_callback_t)(uint8_t *message, size_t length, void *state);

/**
 * @brief   Type of procedure called when there's no more data to read from an IPC.
 * @details This is called before an `open()` call that would block the program, except before
 *          first of these `open()` calls. The programmer can choose to keep listening for new
 *          connections or to stop.
 *
 * @param state A pointer passed to ::ipc_listen so that this callback can modify the program's
 *              state.
 *
 * @retval 0 Success. Continue listening and proceed to blocking opening.
 * @retval 1 Failure. Stop listening for messages.
 */
typedef int (*ipc_on_before_block_callback_t)(void *state);

/** @brief The maximum length of a message that can be sent by ::ipc_send. */
#define IPC_MAXIMUM_MESSAGE_LENGTH (PIPE_BUF - 2 * sizeof(uint32_t))

/** @brief A bidirectional inter-process connection using named pipes. */
typedef struct ipc ipc_t;

/**
 * @brief   Creates a new IPC connection using named pipes.
 * @details This procedure will block if @p this_endpoint is ::IPC_ENDPOINT_CLIENT and the server
 *          has created its FIFO but is not listening for messages.
 *
 *          Newly created server connections (::IPC_ENDPOINT_SERVER) are unidirectional, as extra
 *          information is needed to connect with particular clients (see ::ipc_server_open_sending
 *          amd ::ipc_server_close_sending).
 *
 * @param   this_endpoint The type of program initiating this connection (server or client).
 * @return  A new connection on success, `NULL` on failure (check `errno`).
 *
 * | `errno`  | Cause                                                                |
 * | -------- | -------------------------------------------------------------------- |
 * | `EINVAL` | @p this_endpoint is invalid.                                         |
 * | `ENOMEM` | Allocation failure (or see `man 2 open`).                            |
 * | `EEXIST` | File already exists. Another server running? (::IPC_ENDPOINT_SERVER) |
 * | `ENOENT` | Server not running (::IPC_ENDPOINT_CLIENT).                          |
 * | other    | See `man 3 mkfifo` and `man 2 open`.                                 |
 */
ipc_t *ipc_new(ipc_endpoint_t this_endpoint);

/**
 * @brief Closes an IPC connection and frees memory associated to it.
 * @param ipc IPC connection to be freed.
 */
void ipc_free(ipc_t *ipc);

/**
 * @brief   Sends a message through IPC.
 * @details This may fail due to a `SIGPIPE`, whose signal handler is replaced by the default action
 *          (terminate).
 *
 * @param ipc     Connection to send traffic through. Mustn't be `NULL`. If this is a
 *                ::IPC_ENDPOINT_SERVER connection, it must have been prepared for send data
 *                (::ipc_server_open_sending).
 * @param message Buffer containing data to send. Mustn't be `NULL`.
 * @param length  Number of bytes in @p buf. Must be greater than zero and can't exceed
 *                ::IPC_MAXIMUM_MESSAGE_LENGTH.
 *
 * @retval 0 Success.
 * @retval 1 Failure (check `errno`).
 *
 * | `errno`    | Cause                                                               |
 * | ---------- |  ------------------------------------------------------------------ |
 * | `EINVAL`   | @p ipc is `NULL` or not ready for writing, or @p message is `NULL`. |
 * | `EMSGSIZE` | Message either empty or too long.                                   |
 * | other      | See `man 2 write`.                                                  |
 */
int ipc_send(ipc_t *ipc, const void *message, size_t length);

/**
 * @brief   Sends a message through IPC, retrying if pipe errors occur.
 * @details Pipe error (synchronization error) recovery is important for the server child processes
 *          that need to warn their parent they're about to terminate. If that message doesn't get
 *          to the server, it doesn't know that it can schedule more tasks.
 *
 *          This will write to `stderr` when errors are recovered from.
 *
 *          The signal handler for `SIGPIPE` will be replaced.
 *
 * @param ipc       Connection to send traffic through. Mustn't be `NULL`. If this is a
 *                  ::IPC_ENDPOINT_SERVER connection, it must have been prepared for send data
 *                  (::ipc_server_open_sending).
 * @param message   Buffer containing data to send. Mustn't be `NULL`.
 * @param length    Number of bytes in @p buf. Must be greater than zero and can't exceed
 *                  ::IPC_MAXIMUM_MESSAGE_LENGTH.
 * @param max_tries Maximum number of `write()` attempts (reconnection attempts plus one).
 *
 * @retval 0 Success (errors recovered from may have happened).
 * @retval 1 Failure (check `errno`).
 *
 * | `errno`     | Cause                                                                                       |
 * | ----------- |  ------------------------------------------------------------------------------------------ |
 * | `EINVAL`    | @p ipc is `NULL` or not ready for writing, or @p message is `NULL`, or @p max_tries is `0`. |
 * | `EMSGSIZE`  | Message either empty or too long.                                                           |
 * | `ETIMEDOUT` | Maximum number of attempts reached.                                                         |
 * | other       | See `man 2 write` or `man 2 open`.                                                          |
 */
int ipc_send_retry(ipc_t *ipc, const void *message, size_t length, unsigned int max_tries);

/**
 * @brief   Prepares a connection open on the server to send data to a client.
 * @details This will block the server if the client dies and stops listening to the pipe. We tried
 *          to prevent it by allowing the server to drop messages, but the professors would rather
 *          have the server block.
 *
 * @param ipc        Connection to be prepared for sending data. Mustn't be `NULL` and must be a
 *                   ::IPC_ENDPOINT_SERVER connection. This connection should be newly created or,
 *                   if ::ipc_server_open_sending has been called before, ::ipc_server_close_sending
 *                   must have been called after it.
 * @param client_pid The PID of the client.
 *
 * @retval 0 Success.
 * @retval 1 Failure (check `errno`).
 *
 * | `errno`  | Cause                                                                         |
 * | -------- |  ---------------------------------------------------------------------------- |
 * | `EINVAL` | @p ipc is `NULL`, not ::IPC_ENDPOINT_SERVER, or already prepared for sending. |
 * | `ENOENT` | Named pipe doesn't exist (likely the wrong PID was given).                    |
 * | other    | See `man 2 open`.                                                             |
 */
int ipc_server_open_sending(ipc_t *ipc, pid_t client_pid);

/**
 * @brief Closes the side of a connection from the server to the client.
 *
 * @param ipc Connection to have one of its sides closed. Mustn't be `NULL`, must be a
 *            ::IPC_ENDPOINT_SERVER connection and ::ipc_server_open_sending must have been called
 *            before.
 *
 * @retval 0 Success (`close()` failures will be ignored).
 * @retval 1 Failure (`errno = EINVAL` because @p is `NULL`, not a ::IPC_ENDPOINT_SERVER connection
 *           or closed for sending).
 */
int ipc_server_close_sending(ipc_t *ipc);

/**
 * @brief   Listens for messages received in a connection.
 * @details Protocol / `read()` errors that are recovered from will be printed to `stderr`.
 *
 * @param ipc        Connection to receive traffic from. Mustn't be `NULL`.
 * @param message_cb Callback called for every message. Mustn't be `NULL`.
 * @param block_cb   Callback called before possible blocking. Mustn't be `NULL`.
 * @param state      Pointer passed to @p message_cb and @p block_cb so that they can modify the
 *                   program's state.
 *
 * @retval 0     Success, but protocol / `read()` errors that are recovered from can still occur.
 * @retval 1     `NULL` arguments (`errno = EINVAL`) or `open()` errors (other values of
 *               `errno`).
 * @retval other Value returned by @p message_cb or @p block_cb on error.
 */
int ipc_listen(ipc_t                         *ipc,
               ipc_on_message_callback_t      message_cb,
               ipc_on_before_block_callback_t block_cb,
               void                          *state);

#endif
