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
 * @brief Inter-process communication between the client and the server using named pipos.
 */

#ifndef IPC_H
#define IPC_H

#include <inttypes.h>
#include <limits.h>

/** @brief The type of endpoint (this program) in an IPC. */
typedef enum {
    IPC_ENDPOINT_CLIENT, /**< Client side. */
    IPC_ENDPOINT_SERVER, /**< Server side. */
} ipc_endpoint_t;

/**
 * @brief Type of procedure called for every message received in an IPC.
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
typedef int (*ipc_server_on_message_callback_t)(uint8_t *message, size_t length, void *state);

/** @brief The maximum length of a message that can be sent by ::ipc_send. */
#define IPC_MAXIMUM_MESSAGE_LENGTH (PIPE_BUF - 2 * sizeof(uint32_t))

/** @brief A bidirectional inter-process connection using named pipes. */
typedef struct ipc ipc_t;

/**
 * @brief   Creates a new IPC connection using named pipes.
 * @details This procedure will block if @p this_endpoint is ::IPC_ENDPOINT_CLIENT and the server
 *          has created its FIFO but is not listening for messages.
 *
 *          Newly created server connections are unidirectional, as extra information is needed to
 *          connect with particular clients.
 *
 * @param   this_endpoint The type of program initiating this connection (server or client).
 * @return  A new connection on success, `NULL` on failure (check `errno`).
 *
 * | `errno`  | Cause                                                                |
 * | -------- | -------------------------------------------------------------------- |
 * | `EINVAL` | @p this_endpoint is invalid.                                         |
 * | `ENOMEM` | Allocation failure.                                                  |
 * | `EEXIST` | File already exists. Another server running? (::IPC_ENDPOINT_SERVER) |
 * | `ENOENT` | Server not running (::IPC_ENDPOINT_CLIENT).                          |
 * | other    | See `man 3 mkfifo` and `man 2 open`.                                 |
 */
ipc_t *ipc_new(ipc_endpoint_t this_endpoint);

/**
 * @brief   Sends a message through IPC.
 * @details This will block if the output FIFO is full.
 *
 * @param ipc     Connection to send traffic through. Musn't be `NULL`. If this is a
 *                ::IPC_ENDPOINT_CLIENT connection, it must have been prepared for send data
 *                (::ipc_server_open_sending).
 * @param message Buffer containing data to send. Musn't be `NULL`.
 * @param length  Number of bytes in @p buf. Mustn't exceed ::IPC_MAXIMUM_MESSAGE_LENGTH.
 *
 * @retval 0 Success.
 * @retval 1 Failure (check `errno`).
 *
 * | `errno`    | Cause                                                               |
 * | ---------- |  ------------------------------------------------------------------ |
 * | `EINVAL`   | @p ipc is `NULL` or not ready for writing, or @p message is `NULL`. |
 * | `EMSGSIZE` | Message too long.                                                   |
 * | other      | See `man 2 write`.                                                  |
 */
int ipc_send(ipc_t *ipc, const uint8_t *message, size_t length);

/* TODO - implement bidirectional connection */
int ipc_server_open_sending(void);
int ipc_server_close_sending(void);

/**
 * @brief   Listens for messages received in a connection.
 * @details Protocol errors that are recovered from will be printed to `stderr`.
 *
 * @param ipc   Connection to receive traffic from. Musn't be `NULL`.
 * @param cb    Callback called for every message. Musn't be `NULL`.
 * @param state Pointer passed to @p cb so that it can modify the program's state.
 *
 * @retval 0     Success, but protocol errors that are recovered from can still occurr.
 * @retval 1     `NULL` arguments (`errno = EINVAL`) or `open()` / `read()` errors (other values of
 *               `errno`).
 * @retval other Value returned by @p cb on error.
 */
int ipc_listen(ipc_t *ipc, ipc_server_on_message_callback_t cb, void *state);

/**
 * @brief Closes an IPC connection and frees memory associated to it.
 * @param ipc IPC connection to be freed.
 */
void ipc_free(ipc_t *ipc);

#endif
