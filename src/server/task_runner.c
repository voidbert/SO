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
 * @file  server/task_runner.c
 * @brief Implementation of methods in server/task_runner.h
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "protocol.h"
#include "server/task_runner.h"
#include "util.h"

/** @brief Waits for all children of the current process. */
void __task_runner_wait_all_children(void) {
    while (1) {
        errno   = 0;
        pid_t p = wait(NULL);
        if (p < 0 && errno == ECHILD) /* No more children */
            break;
    }
}

/**
 * @brief Spawns a program with `stdin`, `stdout` and `stderr` file descriptors.
 *
 * @param program Program to be spawned. Mustn't be `NULL`.
 * @param in      `stdin` file descriptor to be duplicated.
 * @param out     `stderr` file descriptor to be duplicated.
 * @param err     `stderr` file descriptor to be duplicated.
 *
 * @retval 0 Success.
 * @retval 1 Failure (check `errno`).
 *
 * | `errno`  | Cause                 |
 * | -------- | --------------------- |
 * | `EINVAL` | @p program is `NULL`. |
 * | other    | See `man 2 fork`.     |
 */
int __task_runner_spawn(const program_t *program, int in, int out, int err) {
    if (!program) {
        errno = EINVAL;
        return 1;
    }

    const char *const *args = program_get_arguments(program);
    pid_t              p    = fork();
    if (p == 0) {
        close(STDIN_FILENO); /* Don't allow reads from the user's terminal */

        if (in != STDIN_FILENO) {
            dup2(in, STDIN_FILENO);
            close(in);
        }

        if (out != STDOUT_FILENO) {
            dup2(out, STDOUT_FILENO);
            close(out);
        }

        if (err != STDERR_FILENO) {
            dup2(err, STDERR_FILENO);
            close(err);
        }

        execvp(args[0], (char *const *) (uintptr_t) args);

        /* This error message will end up in the stderr file */
        util_error("%s(): exec(\"%s\") failed!\n", __func__, args[0]);
        _exit(1);
    }
    return p < 0;
}

/**
 * @brief   Maximum number of connection openings when the other side of the pipe is closed
 *          prematurely.
 * @details This number must be high, as any communication failure means loss of server scheduling
 *          capacity.
 */
#define TASK_RUNNER_WARN_PARENT_MAX_RETRIES 16

/**
 * @brief   Communicates to the parent server that the task has terminated.
 * @details Errors (even those recovered from) will be outputted to `stderr`.
 *
 * @param slot  Slot in the scheduler where this task was scheduled.
 * @param error Whether an error occurred while running the task.
 *
 * @retval 0 Success.
 * @retval 1 Failure (unspecified `errno`).
 */
int __task_runner_warn_parent(size_t slot, int error) {
    struct timespec time_ended = {0};
    (void) clock_gettime(CLOCK_MONOTONIC, &time_ended);
    protocol_task_done_message_t message = {.type       = PROTOCOL_C2S_TASK_DONE,
                                            .slot       = slot,
                                            .time_ended = time_ended,
                                            .is_status  = 0,
                                            .error      = error};

    ipc_t *ipc = ipc_new(IPC_ENDPOINT_CLIENT);
    if (!ipc) {
        util_perror("__task_runner_warn_parent(): error while opening connection");
        return 1;
    }

    if (ipc_send_retry(ipc,
                       &message,
                       sizeof(protocol_task_done_message_t),
                       TASK_RUNNER_WARN_PARENT_MAX_RETRIES)) {
        util_perror("__task_runner_warn_parent(): error while sending message");
        ipc_free(ipc);
        return 1;
    }

    ipc_free(ipc);
    return 0;
}

int task_runner_main(tagged_task_t *task, size_t slot, const char *directory) {
    uint32_t                task_id = tagged_task_get_id(task);
    size_t                  nprograms;
    const program_t *const *programs = task_get_programs(tagged_task_get_task(task), &nprograms);
    if (!programs) {
        task_procedure_t procedure;
        void            *state;
        task_get_procedure(tagged_task_get_task(task), &procedure, &state);
        return procedure(state, slot);
    }

    if (!nprograms)
        return 1;

    char outpath[PATH_MAX];
    snprintf(outpath, PATH_MAX, "%s/%" PRIu32 ".out", directory, task_id);
    int out = open(outpath, O_CREAT | O_WRONLY | O_TRUNC, 0640);
    if (out < 0) {
        util_perror(
            "task_runner_main(): failed to create output file - redirecting output to stdout");
        out = STDOUT_FILENO;
    }

    char errpath[PATH_MAX];
    snprintf(errpath, PATH_MAX, "%s/%" PRIu32 ".err", directory, task_id);
    int err = open(errpath, O_CREAT | O_WRONLY | O_TRUNC, 0640);
    if (err < 0) {
        util_perror(
            "task_runner_main(): failed to create output file - redirecting output to stdout");
        err = STDERR_FILENO;
    }

    int in = 0;
    for (size_t i = 0; i < nprograms - 1; ++i) {
        int fds[2];
        if (pipe(fds)) {
            char error_msg[LINE_MAX] = {0};
            (void) strerror_r(errno, error_msg, LINE_MAX);
            util_error("%s(): pipe() failed: %s\n", __func__, error_msg);

            __task_runner_wait_all_children(); /* May block forever */
            __task_runner_warn_parent(slot, 1);
            _exit(1);
        }

        if (__task_runner_spawn(programs[i], in, fds[STDOUT_FILENO], err)) {
            char error_msg[LINE_MAX] = {0};
            (void) strerror_r(errno, error_msg, LINE_MAX);
            util_error("%s(): fork() failed: %s\n", __func__, error_msg);

            __task_runner_wait_all_children(); /* May block forever */
            __task_runner_warn_parent(slot, 1);
            _exit(1);
        }

        if (i != 0) /* Don't close stdin */
            (void) close(in);
        (void) close(fds[STDOUT_FILENO]);
        in = fds[STDIN_FILENO];
    }

    __task_runner_spawn(programs[nprograms - 1], in, out, err);

    __task_runner_wait_all_children(); /* May block forever */

    (void) close(err);
    (void) close(out);
    return __task_runner_warn_parent(slot, 0);
}
