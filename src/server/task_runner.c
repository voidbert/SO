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

/**
 * @brief Closes file descriptors used for pipe comunication.
 *
 * @param fd_pairs Array with file descriptors to be closed.
 * @param size     Number of pairs of file descriptors in @p fd_pairs.
 */
void __task_runner_close_pipes_descriptors(int (*fd_pairs)[2], size_t size) {
    for (size_t i = 0; i < size; ++i) {
        close(fd_pairs[i][STDOUT_FILENO]);
        close(fd_pairs[i][STDIN_FILENO]);
    }
}

int __task_runner_execute(const char *const program, const char *const *args, uint32_t task_id) {
    execvp(program, (char *const *) (uintptr_t) args);

    fprintf(stderr, "exec() failed running task %" PRIu32 "!\n", task_id);
    __task_runner_wait_all_children();
    return 1;
}

int __task_runner_run_pipeline(const program_t *const *programs,
                               size_t                  nprograms,
                               uint32_t                task_id) {

    int fd_pairs[nprograms - 1][2];

    if (pipe(fd_pairs[0]))
        return 1;

    pid_t p = fork();
    if (!p) {
        dup2(fd_pairs[0][STDOUT_FILENO], STDOUT_FILENO);

        __task_runner_close_pipes_descriptors(fd_pairs, 1);

        if (__task_runner_execute(program_get_arguments(programs[0])[0],
                                  program_get_arguments(programs[0]),
                                  task_id))
            return 1;

    } else if (p == -1) {
        fprintf(stderr, "fork() failed running task %" PRIu32 "!\n", task_id);
        __task_runner_wait_all_children();
        return 1;
    }

    for (size_t i = 1; i < nprograms - 1; ++i) {
        if (pipe(fd_pairs[i]))
            return 1;

        p = fork();
        if (!p) {
            dup2(fd_pairs[i - 1][STDIN_FILENO], STDIN_FILENO);
            dup2(fd_pairs[i][STDOUT_FILENO], STDOUT_FILENO);

            __task_runner_close_pipes_descriptors(fd_pairs, i + 1);

            if (__task_runner_execute(program_get_arguments(programs[i])[0],
                                      program_get_arguments(programs[i]),
                                      task_id))
                return 1;

        } else if (p == -1) {
            fprintf(stderr, "fork() failed running task %" PRIu32 "!\n", task_id);
            __task_runner_wait_all_children();
            return 1;
        }
    }

    p = fork();
    if (!p) {
        dup2(fd_pairs[nprograms - 2][STDIN_FILENO], STDIN_FILENO);

        __task_runner_close_pipes_descriptors(fd_pairs, nprograms - 1);

        if (__task_runner_execute(program_get_arguments(programs[nprograms - 1])[0],
                                  program_get_arguments(programs[nprograms - 1]),
                                  task_id))
            return 1;

    } else if (p == -1) {
        fprintf(stderr, "fork() failed running task %" PRIu32 "!\n", task_id);
        __task_runner_wait_all_children();
        return 1;
    }

    __task_runner_close_pipes_descriptors(fd_pairs, nprograms - 1);
    __task_runner_wait_all_children();

    return 0;
}

int __task_runner_run_single_program(const program_t *const program, uint32_t task_id) {
    const char *const *args = program_get_arguments(program);

    pid_t p = fork();
    if (p == 0) {
        if (__task_runner_execute(args[0], args, task_id))
            return 1;

    } else if (p < 0) {
        fprintf(stderr, "fork() failed running task %" PRIu32 "!\n", task_id);
        __task_runner_wait_all_children();
        return 1;
    }

    __task_runner_wait_all_children();

    return 0;
}

int task_runner_main(tagged_task_t *task, size_t slot, uint64_t secret) {
    size_t                  nprograms;
    const program_t *const *programs = task_get_programs(tagged_task_get_task(task), &nprograms);
    if (!nprograms)
        return 1;

    if (nprograms == 1) {
        if(__task_runner_run_single_program(programs[0], tagged_task_get_id(task)))
            return 1;
    } else {
        if(__task_runner_run_pipeline(programs, nprograms, tagged_task_get_id(task)))
            return 1;
    }

    /* TODO - communicate termination to parent through FIFO */
    (void) slot;
    (void) secret;
    return 0;
}