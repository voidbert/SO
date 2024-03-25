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
 * @file  priority_queue.h
 * @brief A priority queue.
 */

#ifndef PRIORITY_QUEUE
#define PRIORITY_QUEUE

#include <stddef.h>

typedef struct tagged_task {
    int i;
} tagged_task_t; /* TODO - remove */

typedef int (*priority_queue_compare_function_t)(tagged_task_t *a, tagged_task_t *b);

typedef int (*priority_queue_clone_function_t)(tagged_task_t *element);

typedef void (*priority_queue_free_function_t)(tagged_task_t *element);

typedef struct priority_queue priority_queue_t;

priority_queue_t *priority_queue_new(priority_queue_compare_function_t cmp_func,
                                     priority_queue_clone_function_t   clone_func,
                                     priority_queue_free_function_t    free_func);

void priority_queue_free(priority_queue_t *queue);

void priority_queue_insert(priority_queue_t *queue, tagged_task_t *element);

int priority_queue_remove_top(priority_queue_t *queue, tagged_task_t **element);

size_t priority_queue_element_count(priority_queue_t *queue);

#endif
