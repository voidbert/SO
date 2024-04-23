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
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

#include "protocol.h"
#include "server/task_runner.h"

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
 * @brief Spawns a program with input and output file descriptors.
 *
 * @param program Program to be spawned.
 * @param in      Input file descriptor to be duplicated.
 * @param out     Output file descriptor to be duplicated.
 *
 * @retval 0 Success
 * @retval 1 `fork()` failure.
 */
int __task_runner_spawn(const program_t *program, int in, int out) {
    const char *const *args = program_get_arguments(program);
    pid_t              p    = fork();
    if (p == 0) {
        close(STDIN_FILENO); /* Don't allow reads from the user's terminal */

        if (in != STDIN_FILENO) {
            dup2(in, STDIN_FILENO);
            close(in);
        }

        if (out != STDOUT_FILENO) { /* TODO - remove if-statement when outputting to file */
            dup2(out, STDOUT_FILENO);
            close(out);
        }

        /* TODO - support stderr duplication */
      
        execvp(args[0], (char *const *) (uintptr_t) args);

        /* This error message will end up in the stderr file */
        fprintf(stderr, "exec() failed!\n");
        __task_runner_wait_all_children(); /* May block forever */
        _exit(1);
    }
    return 0;
}

/**
 * @brief   Communicates to the parent server that the task has terminated.
 * @details Errors will be outputted to `stderr`.
 *
 * @param slot   Slot in the scheduler where this task was scheduled.
 * @param secret Secret random number needed to authenticate the termination of the task.
 *
 * @retval 0 Success.
 * @retval 1 Failure (unspecified `errno`).
 */
int __task_runner_warn_parent(size_t slot, uint64_t secret) {
    struct timespec time_ended = {0};
    (void) clock_gettime(CLOCK_MONOTONIC, &time_ended);
    protocol_task_done_message_t message = {.type       = PROTOCOL_C2S_TASK_DONE,
                                            .slot       = slot,
                                            .secret     = secret,
                                            .time_ended = time_ended};

    ipc_t *ipc = ipc_new(IPC_ENDPOINT_CLIENT);
    if (!ipc) {
        if (errno == ENOENT)
            fprintf(stderr, "Task completion communication: server's FIFO not found.\n");
        else
            perror("Task completion communication: Failed to open() server's FIFO");
        return 1;
    }

    if (ipc_send(ipc, &message, sizeof(protocol_task_done_message_t))) {
        perror("Task completion communication: failed to write() message to server");
        ipc_free(ipc);
        return 1;
    }

    ipc_free(ipc);
    return 0;
}

int task_runner_main(tagged_task_t *task, size_t slot, uint64_t secret) {
    uint32_t                task_id = tagged_task_get_id(task);
    size_t                  nprograms;
    const program_t *const *programs = task_get_programs(tagged_task_get_task(task), &nprograms);
    if (!nprograms)
        return 1;

    int in = 0;
    for (size_t i = 0; i < nprograms - 1; ++i) {
        int fds[2];
        if (pipe(fds)) {
            fprintf(stderr, "pipe() failed running task %" PRIu32 "!\n", task_id);
            __task_runner_wait_all_children(); /* May block forever */
            _exit(1);
        }

        if (__task_runner_spawn(programs[i], in, fds[STDOUT_FILENO])) {
            fprintf(stderr, "fork() failed running task %" PRIu32 "!\n", task_id);
            __task_runner_wait_all_children(); /* May block forever */
            _exit(1);
        }
        if (i != 0) /* Don't close stdin */
            close(in);
        close(fds[STDOUT_FILENO]);
        in = fds[STDIN_FILENO];
    }
    __task_runner_spawn(programs[nprograms - 1], in, 1);

    __task_runner_wait_all_children(); /* May block forever */
    return __task_runner_warn_parent(slot, secret);
}
