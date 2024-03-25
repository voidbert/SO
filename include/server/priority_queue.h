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

/**
 * @brief   Type of the method called for comparing two ::tagged_task_t values.
 * @details Used for reorganizing the heap after an insertion or deletion.
 *
 * @param a Value to be compared with @p b.
 * @param b Value to be compared with @p a.
 *
 * @return Return a negative value if @p a is inferior to @p b, 0 if they are equal, and a positive
 *         value if @p a is superior to @p b.
 */
typedef int (*priority_queue_compare_function_t)(tagged_task_t *a, tagged_task_t *b);

/**
 * @brief   Type of the method called to free a ::tagged_task_t value.
 * @details Used when freeing a ::priority_queue_t queue.
 *
 * @param element Element to be freed.
 */
typedef void (*priority_queue_free_function_t)(tagged_task_t *element);

/** @brief A definition of a queue. */
typedef struct priority_queue priority_queue_t;

/**
 * @brief   Creates a new priority queue.
 * @details For parameter description check the description for each of the parameters types.
 *          A priority queue can be created without a @p free_func if `NULL` is passed in its place.
 *          If so is the case, when ::priority_queue_free is called the ::tagged_task_t's stored
 *          inside the queue will not be freed.
 *
 * @return A pointer to a new ::priority_queue_t, or `NULL` on failure.
 */
priority_queue_t *priority_queue_new(priority_queue_compare_function_t cmp_func,
                                     priority_queue_free_function_t    free_func);

/**
 * @brief Frees the memory used by a priority queue.
 *
 * @param queue Queue to be freed.
 */
void priority_queue_free(priority_queue_t *queue);

/**
 * @brief Inserts a new ::tagged_task_t into a priority queue.
 *
 * @param queue   Priority queue to insert the @p element on.
 * @param element Element to be inserted in the @p queue.
 *
 * @return 0 on success or 1 on failure.
 */
int priority_queue_insert(priority_queue_t *queue, tagged_task_t *element);

/**
 * @brief Removes the top element from a priority queue.
 *
 * @param queue   Priority queue to take the top element from.
 * @param element Used to store the removed element.
 *
 * @return 0 on success or 1 on failure. This method fails when the queue has no elements.
 */
int priority_queue_remove_top(priority_queue_t *queue, tagged_task_t **element);

/**
 * @brief Counts the number of elements in a priority queue.
 *
 * @param queue Priority queue with elements to be counted.
 *
 * @return The number of elements stored in the @p queue.
 */
size_t priority_queue_element_count(priority_queue_t *queue);

/* TODO - Remove */

void print_minheap(priority_queue_t *data);

#endif
