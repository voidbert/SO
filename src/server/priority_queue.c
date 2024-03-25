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

#include "server/priority_queue.h"
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

#define PRIORITY_QUEUE_VALUES_INITIAL_SIZE 50

typedef struct priority_queue_data {
    tagged_task_t **values;
    size_t          size;
    size_t          capacity;
} priority_queue_data_t;

struct priority_queue {
    priority_queue_compare_function_t cmp_func;
    priority_queue_free_function_t    free_func;

    priority_queue_data_t *data;
};

priority_queue_t *priority_queue_new(priority_queue_compare_function_t cmp_func,
                                     priority_queue_free_function_t    free_func) {

    if (!cmp_func)
        return NULL;

    priority_queue_t *new_queue = malloc(sizeof(priority_queue_t));
    if (!new_queue)
        return NULL;

    new_queue->cmp_func   = cmp_func;
    new_queue->free_func  = free_func;

    priority_queue_data_t *queue_data = malloc(sizeof(priority_queue_data_t));
    if (!new_queue)
        return NULL;

    queue_data->values   = malloc(PRIORITY_QUEUE_VALUES_INITIAL_SIZE * sizeof(tagged_task_t *));
    queue_data->size     = 0;
    queue_data->capacity = PRIORITY_QUEUE_VALUES_INITIAL_SIZE;

    new_queue->data = queue_data;

    return new_queue;
}

void __priority_queue_swap (tagged_task_t *a, tagged_task_t *b) {

    tagged_task_t c = *a;
    *a = *b;
    *b = c;
}

void __priority_queue_insert_bubble_up (priority_queue_t *queue,
                                        size_t placement_index) {

    priority_queue_data_t *data = queue->data;
    size_t parent_index = ((ssize_t) placement_index - 1) / 2;

    while (queue->cmp_func(data->values[placement_index], data->values[parent_index]) < 0) {
        __priority_queue_swap(data->values[placement_index], data->values[parent_index]);

        placement_index = parent_index;
        parent_index    = ((ssize_t) placement_index - 1) / 2;
    }
}

int priority_queue_insert(priority_queue_t *queue, tagged_task_t *element) {

    priority_queue_data_t *data = queue->data;

    if (data->size == data->capacity) {
        data->capacity += PRIORITY_QUEUE_VALUES_INITIAL_SIZE;
        data->values = realloc(data->values, data->capacity * sizeof(tagged_task_t *));
        if (!data->values)
            return 1;
    }

    data->values[data->size] = element;
    __priority_queue_insert_bubble_up(queue, data->size);
    data->size++;

    return 0;
}

void __priority_queue_remove_bubble_down(size_t placement_index, priority_queue_t *queue) {

    priority_queue_data_t *data = queue->data;
    size_t chosen_child, left_child, right_child;

    while (2 * placement_index + 1 < data->size) {
        left_child  = 2 * placement_index + 1;
        right_child = left_child + 1;

        if (right_child < data->size &&
            queue->cmp_func(data->values[right_child], data->values[left_child]) < 0)
            chosen_child = right_child;
        else
            chosen_child = left_child;

        if (queue->cmp_func(data->values[placement_index], data->values[chosen_child]) < 0) break;

        __priority_queue_swap(data->values[placement_index], data->values[chosen_child]);
        placement_index = chosen_child;
    }

}

int priority_queue_remove_top(priority_queue_t *queue, tagged_task_t **element) {

    if (!queue->data->size)
        return 1;


    queue->data->size--;
    if (element)
        *element = queue->data->values[queue->data->size];
    else
        return 1;

    __priority_queue_swap(queue->data->values[queue->data->size], queue->data->values[0]);
    __priority_queue_remove_bubble_down(0, queue);

    return 0;
}

size_t priority_queue_element_count(priority_queue_t *queue) {
    return queue->data->size;
}

void priority_queue_free(priority_queue_t *queue) {

    if (!queue->free_func) {
        free(queue->data->values);
    } else {
        for (size_t i = 0; i < queue->data->size; ++i)
            queue->free_func(queue->data->values[i]);

        free(queue->data->values);
    }

    free(queue->data);
    free(queue);
}

/* TODO - Remove */
void print_minheap(priority_queue_t *queue) {

    priority_queue_data_t *data = queue->data;

    for (size_t i = 0; i < data->size; ++i) {
        printf("[%d]", (data->values[i])->i);
        if (i == 0 || i == 2 || i == 6 || i == 14 || i == 30)
            printf("\n");
    }
    printf("\n");
}