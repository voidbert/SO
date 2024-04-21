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
 * @file  pipeline.c
 * @brief Implementation of methods in server/pipeline.h.
 */

#include <errno.h>
#include <stdint.h>
#include <sys/wait.h>
#include <unistd.h>

#include "server/pipeline.h"

/**
 * @brief Closes file descriptors used for pipe comunication.
 *
 * @param fd_pairs Array with file descriptors to be closed.
 * @param size     Number of pairs of file descriptors in @p fd_pairs.
 */
void __close_pipes_descriptors(int (*fd_pairs)[2], size_t size) {
    for (size_t i = 0; i < size; ++i) {
        close(fd_pairs[i][STDOUT_FILENO]);
        close(fd_pairs[i][STDIN_FILENO]);
    }
}

int pipeline_execute(const task_t *task) {
    size_t                  nprograms;
    const program_t *const *programs = task_get_programs(task, &nprograms);

    if (!programs || nprograms < 2) {
        errno = EINVAL; /* Parsing error, programs is `NULL` or `task` isn't a pipeline */
        return 1;
    }

    int fd_pairs[nprograms - 1][2];

    if (pipe(fd_pairs[0]))
        return 1;

    pid_t p = fork();
    if (!p) {
        dup2(fd_pairs[0][STDOUT_FILENO], STDOUT_FILENO);

        __close_pipes_descriptors(fd_pairs, 1);

        execvp(program_get_arguments(programs[0])[0],
               (char *const *) (uintptr_t) program_get_arguments(programs[0]));

        /* In case of exec failure */
        _exit(127);
    } else if (p == -1) {
        return 1;
    }

    for (size_t i = 1; i < nprograms - 1; ++i) {
        if (pipe(fd_pairs[i]))
            return 1;

        p = fork();
        if (!p) {
            dup2(fd_pairs[i - 1][STDIN_FILENO], STDIN_FILENO);
            dup2(fd_pairs[i][STDOUT_FILENO], STDOUT_FILENO);

            __close_pipes_descriptors(fd_pairs, i + 1);

            execvp(program_get_arguments(programs[i])[0],
                   (char *const *) (uintptr_t) program_get_arguments(programs[i]));

            /* In case of exec failure */
            _exit(127);
        } else if (p == -1) {
            return 1;
        }
    }

    p = fork();
    if (!p) {
        dup2(fd_pairs[nprograms - 2][STDIN_FILENO], STDIN_FILENO);

        __close_pipes_descriptors(fd_pairs, nprograms - 1);

        execvp(program_get_arguments(programs[nprograms - 1])[0],
               (char *const *) (uintptr_t) program_get_arguments(programs[nprograms - 1]));

        /* In case of exec failure */
        _exit(127);
    } else if (p == -1) {
        return 1;
    }

    __close_pipes_descriptors(fd_pairs, nprograms - 1);

    for (size_t i = 0; i < nprograms; ++i)
        wait(NULL);

    return 0;
}
