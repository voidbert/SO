#include "server/task.h"

#include <unistd.h>
#include <sys/wait.h>
#include <stdint.h>

void __close_pipes_descriptors(int (*fd_pairs)[2], size_t size) {
    for (size_t i = 0; i < size; ++i) {
        close(fd_pairs[i][STDOUT_FILENO]);
        close(fd_pairs[i][STDIN_FILENO]);
    }
}

int pipeline_execute(const task_t *task) {
    size_t nprograms;
    const program_t *const *programs = task_get_programs(task, &nprograms);


    if (nprograms < 2)
        return 1; /* Not a pipeline */

    int fd_pairs[nprograms - 1][2];

    if (pipe(fd_pairs[0]))
        return 1;

    if (!fork()) {
        dup2(fd_pairs[0][STDOUT_FILENO], STDOUT_FILENO);

        __close_pipes_descriptors(fd_pairs, 1);

        execvp(program_get_arguments(programs[0])[0],
               (char *const *) (uintptr_t) program_get_arguments(programs[0]));

        /* In case of exec failure */
        _exit(127);
    }

    for (size_t i = 1; i < nprograms - 1; ++i) {
        if (pipe(fd_pairs[i]))
            return 1;

        if (!fork()) {
            dup2(fd_pairs[i - 1][STDIN_FILENO], STDIN_FILENO);
            dup2(fd_pairs[i][STDOUT_FILENO], STDOUT_FILENO);

            __close_pipes_descriptors(fd_pairs, i + 1);

            execvp(program_get_arguments(programs[i])[0],
                   (char *const *) (uintptr_t) program_get_arguments(programs[i]));

            /* In case of exec failure */
            _exit(127);
        }
    }

    if (!fork()) {
        dup2(fd_pairs[nprograms - 2][STDIN_FILENO], STDIN_FILENO);

        __close_pipes_descriptors(fd_pairs, nprograms - 1);

        execvp(program_get_arguments(programs[nprograms - 1])[0],
               (char *const *) (uintptr_t) program_get_arguments(programs[nprograms - 1]));

        /* In case of exec failure */
        _exit(127);
    }

    __close_pipes_descriptors(fd_pairs, nprograms - 1);

    for (size_t i = 0; i < nprograms; ++i)
        wait(NULL);

    return 0;
}