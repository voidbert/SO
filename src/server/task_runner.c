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
 * @brief   Maximum number of connection reopenings when the other side of the pipe is closed
 *          prematurely.
 * @details This number must be high, as any communication failure means loss of server scheduling
 *          capacity.
 */
#define TASK_RUNNER_WARN_PARENT_MAX_RETRIES 16

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

    if (ipc_send_retry(ipc,
                       &message,
                       sizeof(protocol_task_done_message_t),
                       TASK_RUNNER_WARN_PARENT_MAX_RETRIES)) {
        perror("Task completion communication: failed to write() message to server");
        ipc_free(ipc);
        return 1;
    }

    ipc_free(ipc);
    return 0;
}

int task_runner_main(tagged_task_t *task, size_t slot, uint64_t secret) {
    /* TODO - correctly run pipelines */

    size_t                  nprograms;
    const program_t *const *programs = task_get_programs(tagged_task_get_task(task), &nprograms);
    if (!nprograms)
        return 1;

    for (size_t i = 0; i < nprograms; ++i) {
        const char *const *args = program_get_arguments(programs[i]);

        pid_t p = fork();
        if (p == 0) {
            execvp(args[0], (char *const *) (uintptr_t) args);

            fprintf(stderr, "exec() failed running task %" PRIu32 "!\n", tagged_task_get_id(task));
            __task_runner_wait_all_children();
            return 1;
        } else if (p < 0) {
            fprintf(stderr, "fork() failed running task %" PRIu32 "!\n", tagged_task_get_id(task));
            __task_runner_wait_all_children();
            return 1;
        }
    }

    __task_runner_wait_all_children();
    return __task_runner_warn_parent(slot, secret);
}
