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

#include <server/output.h>
#include <server/tagged_task.h>
#include <stdio.h>

/**
 * @brief The entry point to the program.
 * @retval 0 Success
 * @retval 1 Insuccess
 */
int main(void) {
    output_create_task_time_file();

    tagged_task_t *task = tagged_task_new("echo Hello, world!", 1, 1);
    tagged_task_set_time(task,
                         TAGGED_TASK_TIME_ARRIVED,
                         &(struct timespec){.tv_sec = 0, .tv_nsec = 0});
    tagged_task_set_time(task,
                         TAGGED_TASK_TIME_COMPLETED,
                         &(struct timespec){.tv_sec = 1, .tv_nsec = 0});
    output_write_task_time(task);

    output_read_task_times();

    puts("Server says hello!");
    return 0;
}
