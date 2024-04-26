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
 * @brief Contains the entry point to the client program.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "client/client_requests.h"

/**
 * @brief  Prints the usage of the program to `stderr`.
 * @param  program_name `argv[0]`.
 * @return Always `1`.
 */
int __main_help_message(const char *program_name) {
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  See this message:    %s help\n", program_name);
    fprintf(stderr, "  Query server status: %s status\n", program_name);
    fprintf(stderr, "  Run single program:  %s execute (time) -u (command line)\n", program_name);
    fprintf(stderr, "  Run pipeline:        %s execute (time) -p (command line)\n", program_name);
    return 1;
}

/**
 * @brief  The entry point to the program.
 * @retval 0 Success
 * @retval 1 Insuccess
 */
int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "status") == 0) {
        return client_request_ask_status();
    } else if (argc == 2 && strcmp(argv[1], "help") == 0) {
        (void) __main_help_message(argv[0]);
        return 0;
    } else if (argc == 5 && strcmp(argv[1], "execute") == 0) {
        char    *integer_end;
        uint32_t expected_time = strtoul(argv[2], &integer_end, 10);
        if (!*(argv[2]) || *integer_end)
            return __main_help_message(argv[0]);

        if (strcmp(argv[3], "-u") == 0)
            return client_requests_send_program(argv[4], expected_time);
        else if (strcmp(argv[3], "-p") == 0)
            return client_requests_send_task(argv[4], expected_time);
        else
            return __main_help_message(argv[0]);
    } else {
        return __main_help_message(argv[0]);
    }
}
