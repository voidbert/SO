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
 * @file  server/task_iterator.c
 * @brief Implementation of methods in server/task_iterator.h
 */

#include <errno.h>
#include <unistd.h>

#include "server/task_iterator.h"

int task_iterator_iterate(int fd, void *data, task_iterator_callback_t callback) {
    if (!fd || !callback) {
        errno = EINVAL;
        return -1;
    }

    tagged_task_t *task = NULL;
    while (read(fd, task, tagged_task_sizeof()) > 0) { // TODO: error here
        if (callback(task, data) < 0) {
            return -1;
        }
    }

    return 0;
}
