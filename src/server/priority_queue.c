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
 * @brief Implementation of methods in priority_queue.h
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "server/priority_queue.h"

/**
 * @struct priority_queue_data
 * @brief  Stores all the data inside a priority queue.
 *
 * @var priority_queue::values
 *     @brief Array of pointers to ::tagged_task_t values.
 * @var priority_queue::size
 *     @brief Number of elements in ::priority_queue_data::values.
 * @var priority_queue::capcity
 *     @brief Maximum number of elements in ::priority_queue_data::values before reallocation.
 * @var priority_queue::cmp_func
 *     @brief Method used to compare two tasks, and order them in the heap.
 */
typedef struct priority_queue {
    tagged_task_t **values;
    size_t          size;
    size_t          capacity;

    priority_queue_compare_function_t cmp_func;
} priority_queue_data_t;

/** @brief Initial priority_queue::capacity for the array priority_queue::values. */
#define PRIORITY_QUEUE_VALUES_INITIAL_SIZE 32

priority_queue_t *priority_queue_new(priority_queue_compare_function_t cmp_func) {
    if (!cmp_func) {
        errno = EINVAL;
        return NULL;
    }

    priority_queue_t *new_queue = malloc(sizeof(priority_queue_t));
    if (!new_queue)
        return NULL; /* errno = ENOMEM guaranteed */

    new_queue->cmp_func = cmp_func;
    new_queue->size     = 0;
    new_queue->capacity = PRIORITY_QUEUE_VALUES_INITIAL_SIZE;
    new_queue->values   = malloc(new_queue->capacity * sizeof(tagged_task_t *));
    if (!new_queue->values) {
        free(new_queue);
        return NULL; /* errno = ENOMEM guaranteed */
    }

    return new_queue;
}

priority_queue_t *priority_queue_clone(const priority_queue_t *queue) {
    if (!queue) {
        errno = EINVAL;
        return NULL;
    }

    priority_queue_t *queue_clone = malloc(sizeof(priority_queue_t));
    if (!queue_clone)
        return NULL; /* errno = ENOMEM guaranteed */

    queue_clone->cmp_func = queue->cmp_func;
    queue_clone->size     = queue->size;
    queue_clone->capacity = queue->capacity;
    queue_clone->values   = malloc(queue->capacity * sizeof(tagged_task_t *));
    if (!queue_clone->values) {
        free(queue_clone);
        return NULL; /* errno = ENOMEM guaranteed */
    }

    for (size_t i = 0; i < queue->size; ++i) {
        queue_clone->values[i] = tagged_task_clone(queue->values[i]);
        if (!queue_clone->values[i]) {
            for (size_t j = 0; j < i; ++j)
                tagged_task_free(queue_clone->values[j]);

            free(queue_clone->values);
            free(queue_clone);
            return NULL;
        }
    }

    return queue_clone;
}

/**
 * @brief Swaps the values from two memory locations.
 * @param a Memory location to store the value pointed to by @p b.
 * @param b Memory location to store the value pointed to by @p a.
 */
void __priority_queue_swap(tagged_task_t **a, tagged_task_t **b) {
    tagged_task_t *temp = *a;
    *a                  = *b;
    *b                  = temp;
}

/**
 * @brief   Reorganizes a min-heap to keep its properties after an insertion.
 * @details Auxiliary method to ::priority_queue_insert.
 *
 * @param queue     Queue to be reorganized.
 * @param placement Index of the previously inserted element.
 *
 * @retval 0 Success.
 * @retval 1 Failure because @p queue is `NULL`.
 */
int __priority_queue_insert_bubble_up(priority_queue_t *queue, size_t placement) {
    if (!queue) {
        errno = EINVAL;
        return 1;
    }

    size_t parent = ((ssize_t) placement - 1) / 2;
    while (placement > 0 && queue->cmp_func(queue->values[placement], queue->values[parent]) < 0) {
        __priority_queue_swap(queue->values + placement, queue->values + parent);

        placement = parent;
        parent    = ((ssize_t) placement - 1) / 2;
    }

    return 0;
}

int priority_queue_insert(priority_queue_t *queue, tagged_task_t *element) {
    if (queue->size >= queue->capacity) {
        queue->capacity *= 2;

        tagged_task_t **new_values =
            realloc(queue->values, queue->capacity * sizeof(tagged_task_t *));
        if (!new_values)
            return 1; /* errno = ENOMEM guaranteed */
        queue->values = new_values;
    }

    queue->values[queue->size] = tagged_task_clone(element);
    if (!queue->values[queue->size])
        return 1; /* errno = ENOMEM guaranteed */

    __priority_queue_insert_bubble_up(queue, queue->size);
    queue->size++;
    return 0;
}

/**
 * @brief   Reorganizes a min-heap to keep its properties after the removal of the top element.
 * @details Auxiliary method to ::priority_queue_remove_top.
 *
 * @param queue   Queue to be reorganized.
 * @param removal Index of the removed element.
 *
 * @retval 0 Success.
 * @retval 1 Failure because @p queue is `NULL` or @p removal is out-of-bounds.
 */
int __priority_queue_remove_bubble_down(priority_queue_t *queue, size_t removal) {
    if (!queue || removal >= queue->size) {
        errno = EINVAL;
        return 1;
    }

    while (2 * removal + 1 < queue->size) {
        size_t chosen_child, left_child, right_child;
        left_child  = 2 * removal + 1;
        right_child = left_child + 1;

        if (right_child < queue->size &&
            queue->cmp_func(queue->values[right_child], queue->values[left_child]) < 0)
            chosen_child = right_child;
        else
            chosen_child = left_child;

        if (queue->cmp_func(queue->values[removal], queue->values[chosen_child]) < 0)
            break;

        __priority_queue_swap(queue->values + removal, queue->values + chosen_child);
        removal = chosen_child;
    }
    return 0;
}

tagged_task_t *priority_queue_remove_top(priority_queue_t *queue) {
    if (!queue || queue->size == 0) {
        errno = EINVAL;
        return NULL;
    }

    tagged_task_t *ret = queue->values[0];
    queue->size--;
    __priority_queue_swap(queue->values + queue->size, queue->values);
    __priority_queue_remove_bubble_down(queue, 0);

    return ret;
}

size_t priority_queue_element_count(const priority_queue_t *queue) {
    if (!queue)
        return (size_t) -1;
    return queue->size;
}

void priority_queue_free(priority_queue_t *queue) {
    if (!queue)
        return; /* Don't set errno, as frees typically don't do that */

    for (size_t i = 0; i < queue->size; ++i)
        tagged_task_free(queue->values[i]);

    free(queue->values);
    free(queue);
}
