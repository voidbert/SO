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

    /* TODO - communicate termination to parent through FIFO */
    (void) slot;
    (void) secret;
    return 0;
}
