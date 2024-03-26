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

typedef struct tagged_task tagged_task_t; /* TODO - Remove when its properly implemented */

/**
 * @brief   Type of the function called for comparing two ::tagged_task_t's.
 * @details Used for reorganizing the heap after an insertion or deletion.
 *
 * @param a Value to be compared with @p b.
 * @param b Value to be compared with @p a.
 *
 * @return A negative value if @p a is inferior to @p b, 0 if they are equal, and a positive
 *         value if @p a is superior to @p b.
 */
typedef int (*priority_queue_compare_function_t)(tagged_task_t *a, tagged_task_t *b);

/**
 * @brief   Type of the function called to clone a ::tagged_task_t.
 * @details Used for cloning the elements inside a priority queue when ::priority_queue_clone is
 *          called.
 *
 * @param element Value to be cloned.
 *
 * @return A ::tagged_task_t on success or `NULL` on allocation failure.
 */
typedef tagged_task_t *(*priority_queue_clone_function_t)(tagged_task_t *element);

/**
 * @brief   Type of the function called to free a ::tagged_task_t value.
 * @details Used when freeing a ::priority_queue_t queue.
 *
 * @param element Element to be freed.
 */
typedef void (*priority_queue_free_function_t)(tagged_task_t *element);

/** @brief A definition of a queue. */
typedef struct priority_queue priority_queue_t;

/**
 * @brief   Creates a new priority queue of unique pointers to ::tagged_task_t's.
 * @details For parameter description check the description for each of the parameters types.
 *          A priority queue can be created without a @p free_func if `NULL` is passed in its place,
 *          the same applies to the parameter @p clone_func. If a `NULL` is passed as the
 *          @p free_func, when ::priority_queue_free is called the ::tagged_task_t's stored inside
 *          the queue will not be freed. If `NULL` is passed as the @p clone_function, when
 *          ::priority_queue_clone is called the resulting clone will be shallow.
 *
 * @return A pointer to a new ::priority_queue_t, or `NULL` on failure.
 */
priority_queue_t *priority_queue_new(priority_queue_compare_function_t cmp_func,
                                     priority_queue_clone_function_t   clone_func,
                                     priority_queue_free_function_t    free_func);

/**
 * @brief   Clones a priority queue.
 * @details If the @p queue was not created with a ::priority_queue_clone_fucntion_t set, this
 *          method will only produce a shallow clone. This method will not work if the original
 *          @p queue was created without a ::priority_queue_free_function_t set, as it owns the
 *          individual elements clones, and needs to free them later when ::priority_queue_free is
 *          called.
 *
 * @param queue Priority queue to be cloned.
 *
 * @return A pointer to a new ::priority_queue_t, or `NULL` on failure.
 */
priority_queue_t *priority_queue_clone(priority_queue_t *queue);

/**
 * @brief   Inserts a new, and unique, ::tagged_task_t into a priority queue.
 * @details This method doesn't check if the @p element already exists in the queue. If two elements
 *          have the same adress and ::priority_queue_free is called, it may lead to an invalid
 *          free.
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

/**
 * @brief   Frees the memory used by a priority queue.
 * @details If the @p queue was not created with a ::priority_queue_free_function_t set, this method
 *          will not free the elements inside the priority queue.
 *
 * @param queue Queue to be freed.
 */
void priority_queue_free(priority_queue_t *queue);

#endif
