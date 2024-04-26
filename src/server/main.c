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
 * @file  main.c
 * @brief Contains the entry point to the server program.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "server/server_requests.h"

/**
 * @brief  Prints the usage of the program to `stderr`.
 * @param  program_name `argv[0]`.
 * @return Always `1`.
 */
int __main_help_message(const char *program_name) {
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  See this message: %s help\n", program_name);
    fprintf(stderr,
            "  Run server:       %s (output folder) (number of tasks) (policy)\n",
            program_name);
    fprintf(stderr, "    where policy = fcfs | sjf\n");
    return 1;
}

/**
 * @brief  The entry point to the program.
 * @retval 0 Success
 * @retval 1 Insuccess
 */
int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "help") == 0) {
        (void) __main_help_message(argv[0]);
        return 0;
    } else if (argc == 4) {
        if (mkdir(argv[1], 0700) && errno != EEXIST) {
            perror("Failed to create server's directory");
        }

        char         *integer_end;
        unsigned long ntasks = strtoul(argv[2], &integer_end, 10);
        if (!*(argv[2]) || *integer_end)
            return __main_help_message(argv[0]);

        scheduler_policy_t policy;
        if (strcmp(argv[3], "fcfs") == 0)
            policy = SCHEDULER_POLICY_FCFS;
        else if (strcmp(argv[3], "sjf") == 0)
            policy = SCHEDULER_POLICY_SJF;
        else
            return __main_help_message(argv[0]);

        return server_requests_listen(policy, ntasks, argv[1]);
    } else {
        return __main_help_message(argv[0]);
    }
}
