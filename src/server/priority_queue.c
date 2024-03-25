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
 * @file  priority_queue.c
 * @brief A priority queue implementation.
 */

/* TODO - do error handling */

#include "server/priority_queue.h"
#include <stddef.h>
#include <stdlib.h>

#define PRIORITY_QUEUE_ARRAY_INITIAL_SIZE 50

typedef struct priority_queue_data {
    tagged_task_t **array;
    size_t          size;
    size_t          capacity;
} priority_queue_data_t;

struct priority_queue {
    priority_queue_compare_function_t cmp_func;
    priority_queue_clone_function_t   clone_func;
    priority_queue_free_function_t    free_func;

    priority_queue_data_t *data;
};

priority_queue_t *priority_queue_new(priority_queue_compare_function_t cmp_func,
                                     priority_queue_clone_function_t   clone_func,
                                     priority_queue_free_function_t    free_func) {

    priority_queue_t *new_queue = malloc(sizeof(priority_queue_t));

    new_queue->cmp_func   = cmp_func;
    new_queue->clone_func = clone_func;
    new_queue->free_func  = free_func;

    priority_queue_data_t *queue_data = malloc(sizeof(priority_queue_data_t));

    queue_data->array    = malloc(PRIORITY_QUEUE_ARRAY_INITIAL_SIZE * sizeof(tagged_task_t *));
    queue_data->size     = 0;
    queue_data->capacity = PRIORITY_QUEUE_ARRAY_INITIAL_SIZE;

    return new_queue;
}

void priority_queue_free(priority_queue_t *queue) {

    if (!queue->free_func) {
        free(queue->data->array);
    } else {
        for (size_t i = 0; i < queue->data->size; ++i)
            queue->free_func(queue->data->array[i]);

        free(queue->data->array);
    }

    free(queue->data);
    free(queue);
}

/* TODO - Do swap from pointers only */

void __priority_queue_swap(tagged_task_t *array, size_t child, size_t parent) {

    tagged_task_t *temp = array[parent];
    array[parent]       = array[child];
    array[child]        = temp;
}

/* TODO - Refactor to an iteractive method (AlgC files) */

void __priority_queue_insert_bubble_up(priority_queue_data_t *data, size_t index) {

    size_t parent = (index - 1) / 2;

    if (data->array[parent] > data->array[index]) {
        __priority_queue_swap(data->array, index, parent);
        __priority_queue_insert_bubble_up(data, parent);
    }
}

void priority_queue_insert(priority_queue_t *queue, tagged_task_t *element) {

    priority_queue_data_t *data = queue->data;

    if (data->size == data->capacity) {
        data->capacity += PRIORITY_QUEUE_ARRAY_INITIAL_SIZE;
        data->array = realloc(data->array, data->capacity * sizeof(tagged_task_t *));
    }

    data->array[data->size] = element;
    __priority_queue_insert_bubble_up(data, data->size);
    data->size++;
}

/* TODO - Remove top (Algc files) */

size_t priority_queue_element_count(priority_queue_t *queue) {
    return queue->data->size;
}