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
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "ipc.h"
#include "util.h"

/* Ignore __attribute__((packed)) if it's unavailable. */
#ifndef __GNUC__
    /** @cond FALSE */
    #define __attribute__()
    /** @endcond */
#else
    /*
     * The program may break if the compiler doesn't support the removal of struct padding. However,
     * there is no standard way of throwing an error to warn the user.
     */
#endif

/** @brief File path of FIFO that the server listens to. */
#define IPC_SERVER_FIFO_PATH "/tmp/orchestrator.fifo"

/**
 * @brief   File path of FIFO that a client listens to.
 * @details Format string, where `%ld` is replaced by the client's PID (casted to a `long`).
 */
#define IPC_CLIENT_FIFO_PATH "/tmp/client%ld.fifo"

/** @brief Bytes used to identify the beginning of an IPC message. */
#define IPC_MESSAGE_HEADER_SIGNATURE 0xFEEDFEED

/**
 * @struct ipc
 * @brief  An inter-process connection using named pipes.
 *
 * @var ipc::this_endpoint
 *     @brief The type of this program in the communication (client or server).
 * @var ipc::send_fd
 *     @brief   The file descriptor to write to in order to transmit data.
 *     @details Will be `-1` for newly created ::IPC_ENDPOINT_SERVER connections, as the client
 *              isn't known before the first message is received.
 * @var ipc::receive_fd
 *     @brief   The file descriptor to read from in order to receive data.
 *     @details Will be `-1` for any connection not currently being listened on.
 * @var ipc::send_fd_pid
 *     @brief   PID of the process the server is communicating with (only for ::IPC_ENDPOINT_SERVER)
 *     @details Will be `-1` for new connection.
 */
struct ipc {
    ipc_endpoint_t this_endpoint;
    int            send_fd, receive_fd;
    pid_t          send_fd_pid;
};

/**
 * @struct ipc_frame_t
 * @brief  A frame containing a message, sent by an ::ipc_t.
 *
 * @var ipc_frame_t::signature
 *     @brief Must be ::IPC_MESSAGE_HEADER_SIGNATURE.
 * @var ipc_frame_t::payload_length
 *     @brief Number of bytes in ipc_frame_t::message.
 * @var ipc_frame_t::message
 *     @brief Encapsulated message with a length of ipc_frame_t::payload_length bytes.
 */
typedef struct __attribute__((packed)) {
    uint32_t signature, payload_length;
    uint8_t message[IPC_MAXIMUM_MESSAGE_LENGTH];
} ipc_frame_t;

/**
 * @brief Generates the path to the named pipe owned by this endpoint based on its type.
 *
 * @param this_endpoint What type of program is communicating.
 * @param path          Where to output the path to the FIFO. Mustn't be `NULL` (not checked).
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

        /* Client can read and write and everyone else can write (don't know server's group) */
        if (mkfifo(fifo_path, 0622)) {
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
    ret->send_fd_pid = -1;

    return ret;
}

void ipc_free(ipc_t *ipc) {
    if (!ipc)
        return; /* Don't set errno, as that's not typical free behavior. */

    if (ipc->send_fd > 0)
        (void) close(ipc->send_fd);

    char fifo_path[PATH_MAX];
    __ipc_get_owned_fifo_path(ipc->this_endpoint, fifo_path);
    (void) unlink(fifo_path);

    free(ipc);
}

int ipc_send(ipc_t *ipc, const void *message, size_t length) {
    if (!ipc || ipc->send_fd < 0 || !message) {
        errno = EINVAL;
        return 1;
    }

    if (length > IPC_MAXIMUM_MESSAGE_LENGTH || length == 0) {
        errno = EMSGSIZE;
        return 1;
    }

    ipc_frame_t frame = {.signature = IPC_MESSAGE_HEADER_SIGNATURE, .payload_length = length};
    memcpy(frame.message, message, length);
    ssize_t frame_length = length + 2 * sizeof(uint32_t);

    (void) signal(SIGPIPE, SIG_DFL); /* No errors other than EINVAL */
    if (write(ipc->send_fd, &frame, frame_length) != frame_length)
        return 1;
    return 0;
}

int ipc_send_retry(ipc_t *ipc, const void *message, size_t length, unsigned int max_tries) {
    if (!ipc || ipc->send_fd < 0 || !message || !max_tries) {
        errno = EINVAL;
        return 1;
    }

    if (length > IPC_MAXIMUM_MESSAGE_LENGTH || length == 0) {
        errno = EMSGSIZE;
        return 1;
    }

    ipc_frame_t frame = {.signature = IPC_MESSAGE_HEADER_SIGNATURE, .payload_length = length};
    memcpy(frame.message, message, length);
    ssize_t frame_length = length + 2 * sizeof(uint32_t);

    (void) signal(SIGPIPE, SIG_IGN); /* No errors other than EINVAL */
    unsigned int recovered = 0;
    for (unsigned int i = 0; i < max_tries; ++i) {
        if (write(ipc->send_fd, &frame, frame_length) == frame_length) {
            if (recovered)
                util_error("%s(): IPC synchronization error recovered from (%u attempts)\n",
                           __func__,
                           recovered);
            return 0;
        }

        if (errno == EPIPE || errno == EINTR) {
            char fifo_path[PATH_MAX];
            if (ipc->this_endpoint == IPC_ENDPOINT_SERVER)
                snprintf(fifo_path, PATH_MAX, IPC_CLIENT_FIFO_PATH, (long) ipc->send_fd_pid);
            else
                strcpy(fifo_path, IPC_SERVER_FIFO_PATH);

            if (((ipc->send_fd = open(fifo_path, O_WRONLY))) < 0)
                return 1; /* Keep errno. Also this shouldn't fail */

            recovered++;
        } else {
            return 1; /* Keep errno */
        }
    }

    errno = ETIMEDOUT;
    return 1;
}

int ipc_server_open_sending(ipc_t *ipc, pid_t client_pid) {
    if (!ipc || ipc->this_endpoint != IPC_ENDPOINT_SERVER || ipc->send_fd > 0) {
        errno = EINVAL;
        return 1;
    }

    char client_fifo_path[PATH_MAX];
    snprintf(client_fifo_path, PATH_MAX, IPC_CLIENT_FIFO_PATH, (long) client_pid);
    if ((ipc->send_fd = open(client_fifo_path, O_WRONLY)) < 0)
        return 1; /* Keep errno */
    ipc->send_fd_pid = client_pid;
    return 0;
}

int ipc_server_close_sending(ipc_t *ipc) {
    if (!ipc || ipc->this_endpoint != IPC_ENDPOINT_SERVER || ipc->send_fd < 0) {
        errno = EINVAL;
        return 1;
    }

    (void) close(ipc->send_fd);
    ipc->send_fd     = -1;
    ipc->send_fd_pid = -1;
    return 0; /* Don't care about closing success */
}

/**
 * @brief   Size of buffer when reading from an IPC.
 * @details Must be at least as large as `PIPE_BUF`.
 */
#define IPC_SERVER_LISTEN_BUFFER_SIZE (4 * PIPE_BUF)

/**
 * @brief   Reads everything from a connection and closes its receiving pipe.
 * @details Auxiliary function for ::ipc_listen.
 * @param   ipc Connection whose receiving end is to be flushed and closed.
 */
void __ipc_flush_and_close(ipc_t *ipc) {
    uint8_t buf[IPC_SERVER_LISTEN_BUFFER_SIZE];
    while (read(ipc->receive_fd, buf, IPC_SERVER_LISTEN_BUFFER_SIZE) > 0)
        ;

    int errno2 = errno;
    (void) close(ipc->receive_fd);
    errno = errno2;

    ipc->receive_fd = -1;
}

int ipc_listen(ipc_t                         *ipc,
               ipc_on_message_callback_t      message_cb,
               ipc_on_before_block_callback_t block_cb,
               void                          *state) {
    if (!ipc || !message_cb || !block_cb) {
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
                util_perror("ipc_listen(): Recovering from read() error");
                (void) close(ipc->receive_fd);
                ipc->receive_fd = -1;
                break;
            }

            ssize_t  remaining   = residuals + bytes_read;
            uint8_t *buffer_read = buf;
            while (remaining > (ssize_t) (2 * sizeof(uint32_t))) {
                ipc_frame_t *frame = (ipc_frame_t *) buffer_read;

                if (frame->signature != IPC_MESSAGE_HEADER_SIGNATURE ||
                    frame->payload_length == 0 ||
                    frame->payload_length > IPC_MAXIMUM_MESSAGE_LENGTH) {
                    util_error("%s(): dropping input frames! Invalid frame!\n", __func__);
                    __ipc_flush_and_close(ipc);
                    break;
                }

                /* Validate header in context */
                uint32_t frame_length = frame->payload_length + 2 * sizeof(uint32_t);
                if (frame_length > remaining) {
                    if (bytes_read == 0) { /* EOF */
                        util_error("%s(): dropping input frame! Not enough data!\n", __func__);
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
                int mcb_ret = message_cb(frame->message, frame->payload_length, state);
                if (mcb_ret) {
                    __ipc_flush_and_close(ipc);
                    return mcb_ret;
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

                if (remaining > 0 && remaining <= (ssize_t) (2 * sizeof(uint32_t)))
                    util_error("%s(): dropping input frame! Not enough data!\n", __func__);
            } else {
                break;
            }
        }

        int bcb_ret = block_cb(state);
        if (bcb_ret)
            return bcb_ret;
    }

    return 0;
}
