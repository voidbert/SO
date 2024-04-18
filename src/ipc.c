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
 * @file  ipc.c
 * @brief Implementation of methods in ipc.h
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "ipc.h"

/** @brief File path of FIFO that the server listens to. */
#define IPC_SERVER_FIFO_PATH "/tmp/orchestrator.fifo"

/**
 * @brief   File path of FIFO that a client listens to.
 * @details Format string, where `%ld` is replaced by the client's PID.
 */
#define IPC_CLIENT_FIFO_PATH "/tmp/client%ld.fifo"

/** @brief Bytes used to identify the beginning of an IPC message. */
#define IPC_MESSAGE_HEADER_SIGNATURE 0xFEEDFEED

/**
 * @struct ipc
 * @brief  An inter-process connection.
 *
 * @var ipc::this_endpoint
 *     @brief The type of this program in the communication (client or server).
 * @var ipc::send_fd
 *     @brief   The file descriptor to write to in order to transmit data.
 *     @details Will be `-1` for newly created ::IPC_ENDPOINT_SERVER connections.
 * @var ipc::receive_fd
 *     @brief   The file descriptor to read from in order to receive data.
 *     @details Will be `-1` for any connection not actively being listened on.
 */
struct ipc {
    ipc_endpoint_t this_endpoint;
    int            send_fd, receive_fd;
};

/**
 * @brief Generates the path to the named pipe owned by this endpoint based on its type.
 *
 * @param this_endpoint What type of program is communicating.
 * @param path          Where to output the path to the FIFO. Musn't be `NULL` (not checked).
 *
 * @retval 0 Success.
 * @retval 1 Failure (`errno = EINVAL`, @p this_endpoint assumes an invalid value).
 */
int __ipc_get_owned_fifo_path(ipc_endpoint_t this_endpoint, char path[PATH_MAX]) {
    if (this_endpoint == IPC_ENDPOINT_CLIENT) {
        snprintf(path, PATH_MAX, IPC_CLIENT_FIFO_PATH, (long) getpid());
    } else if (this_endpoint == IPC_ENDPOINT_SERVER) {
        strncpy(path, IPC_SERVER_FIFO_PATH, PATH_MAX);
    } else {
        errno = EINVAL;
        return 1;
    }

    return 0;
}

ipc_t *ipc_new(ipc_endpoint_t this_endpoint) {
    char fifo_path[PATH_MAX];
    if (__ipc_get_owned_fifo_path(this_endpoint, fifo_path))
        return NULL; /* errno = EINVAL guaranteed */

    ipc_t *ret = malloc(sizeof(ipc_t));
    if (!ret)
        return NULL; /* errno = ENOMEM guaranteed */
    ret->this_endpoint = this_endpoint;

    if (this_endpoint == IPC_ENDPOINT_CLIENT) {
        /* Try to delete FIFO file in case any previous client didn't terminate correctly. */
        (void) unlink(fifo_path);
        errno = 0;

        /* Client can read and everyone else can read (don't know server's group) */
        if (mkfifo(fifo_path, 0422)) {
            free(ret);
            return NULL;
        }

        /*
         * Don't open the FIFO now, as that would block before anything would be sent to the server.
         */
        ret->receive_fd = -1;

        if ((ret->send_fd = open(IPC_SERVER_FIFO_PATH, O_WRONLY)) < 0) {
            (void) unlink(fifo_path);
            free(ret);
            return NULL;
        }
    } else {
        /* Server can read and write, and clients (group) can write */
        if (mkfifo(fifo_path, 0620)) {
            free(ret);
            return NULL;
        }

        /* Don't open the FIFO now, as that would block before before starting to listen. */
        ret->receive_fd = ret->send_fd = -1;
    }

    return ret;
}

int ipc_send(ipc_t *ipc, const uint8_t *message, size_t length) {
    if (!ipc || ipc->send_fd < 0 || !message) {
        errno = EINVAL;
        return 1;
    }

    if (length > IPC_MAXIMUM_MESSAGE_LENGTH) {
        errno = EMSGSIZE;
        return 1;
    }

    uint8_t wrapped_message[PIPE_BUF];
    ssize_t wrapped_message_length      = length + 2 * sizeof(uint32_t);
    *((uint32_t *) wrapped_message)     = IPC_MESSAGE_HEADER_SIGNATURE;
    *((uint32_t *) wrapped_message + 1) = (uint32_t) length;
    memcpy(wrapped_message + 2 * sizeof(uint32_t), message, length);

    if (write(ipc->send_fd, wrapped_message, wrapped_message_length) != wrapped_message_length)
        return 1;
    return 0;
}

/**
 * @brief   Size of buffer when reading from an IPC.
 * @details Must be at least as large as `PIPE_BUF`.
 */
#define IPC_SERVER_LISTEN_BUFFER_SIZE (4 * PIPE_BUF)

/**
 * @brief   Reads everything from a connection and closes its receiving pipe.
 * @details Auxiliar function for ::ipc_listen.
 * @param   ipc Connection whose receiving end is to be flushed and closed.
 */
void __ipc_flush_and_close(ipc_t *ipc) {
    uint8_t buf[IPC_SERVER_LISTEN_BUFFER_SIZE];
    while (read(ipc->receive_fd, buf, IPC_SERVER_LISTEN_BUFFER_SIZE) > 0)
        ;
    (void) close(ipc->receive_fd);
    ipc->receive_fd = -1;
}

/**
 * @brief   Validates if the header of a frame transmitted in a pipe is valid.
 * @details Header validity is checked for itself, not in the context the header is in.
 *          Auxiliar function for ::ipc_listen.
 *
 * @param frame          First eight bytes of the frame. Musn't be `NULL` (not checked).
 * @param message_length Where to output the message length to. Musn't be `NULL` (not checked).
 *
 * @retval 0 Valid header.
 * @retval 1 Invalid header.
 */
int __ipc_validate_frame_header(uint8_t frame[8], uint32_t *message_length) {
    if (*((uint32_t *) frame) != IPC_MESSAGE_HEADER_SIGNATURE) {
        fprintf(stderr,
                "Protocol error: dropping input frames! Lost track of header signatures!\n");
        return 1;
    }

    *message_length = *((uint32_t *) frame + 1);
    if (*message_length > IPC_MAXIMUM_MESSAGE_LENGTH) {
        fprintf(stderr, "Protocol error: dropping input frames! Very long frame found!\n");
        return 1;
    }

    return 0;
}

int ipc_listen(ipc_t *ipc, ipc_server_on_message_callback_t cb, void *state) {
    /* TODO - input fuzzing tests to try and break this */

    if (!ipc || !cb) {
        errno = EINVAL;
        return 1;
    }

    char fifo_path[PATH_MAX];
    __ipc_get_owned_fifo_path(ipc->this_endpoint, fifo_path);

    while (1) {
        if ((ipc->receive_fd = open(fifo_path, O_RDONLY)) < 0)
            return 1;

        uint8_t buf[IPC_SERVER_LISTEN_BUFFER_SIZE];
        ssize_t bytes_read, residuals = 0;
        while (1) {
            bytes_read =
                read(ipc->receive_fd, buf + residuals, IPC_SERVER_LISTEN_BUFFER_SIZE - residuals);
            if (bytes_read < 0) { /* Read error */
                (void) close(ipc->receive_fd);
                ipc->receive_fd = -1;
                return 1;
            }

            ssize_t  remaining   = residuals + bytes_read;
            uint8_t *buffer_read = buf;
            while (remaining > 8) {
                /* Validate header by itself */
                uint32_t message_length;
                if (__ipc_validate_frame_header(buffer_read, &message_length)) {
                    __ipc_flush_and_close(ipc);
                    break;
                }

                /* Validate header in context */
                uint32_t frame_length = message_length + 2 * sizeof(uint32_t);
                if (frame_length > remaining) {
                    if (bytes_read == 0) { /* EOF */
                        fprintf(stderr,
                                "Protocol error: dropping input frame! Not enough data!\n");
                        (void) close(ipc->receive_fd);
                        ipc->receive_fd = -1;
                        break;
                    } else {
                        memmove(buf, buffer_read, remaining); /* Avoid aliasing */
                        residuals = remaining;
                        break;
                    }
                }

                /* Dispatch message */
                int cb_ret = cb(buffer_read + 2 * sizeof(uint32_t), message_length, state);
                if (cb_ret) {
                    fprintf(stderr,
                            "Protocol error: dropping input frames! Upper layer read failure!\n");
                    __ipc_flush_and_close(ipc);
                    return cb_ret;
                }

                remaining -= frame_length;
                buffer_read += frame_length;
            }

            if (ipc->receive_fd > 0) { /* No errors */
                if (bytes_read == 0) {
                    (void) close(ipc->receive_fd);
                    ipc->receive_fd = -1;
                    break;
                }

                if (remaining > 0 && remaining <= 8)
                    fprintf(stderr, "Protocol error: dropping input frame! Not enough data!\n");
            } else {
                break;
            }
        }
    }

    return 0;
}

void ipc_free(ipc_t *ipc) {
    if (!ipc)
        return; /* Don't set errno, as that's not typical free behavior. */

    if (ipc->send_fd > 0)
        (void) close(ipc->send_fd);
    if (ipc->receive_fd > 0)
        (void) close(ipc->receive_fd);

    if (ipc->this_endpoint == IPC_ENDPOINT_CLIENT) {
        char fifo_path[PATH_MAX];
        snprintf(fifo_path, PATH_MAX, IPC_CLIENT_FIFO_PATH, (long) getpid());
        (void) unlink(fifo_path);
    } else {
        (void) unlink(IPC_SERVER_FIFO_PATH);
    }

    free(ipc);
}
