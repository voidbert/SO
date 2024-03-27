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
 * @brief A priority queue for tasks.
 */

#ifndef PRIORITY_QUEUE
#define PRIORITY_QUEUE

#include <stddef.h>

/* TODO - Remove when its properly implemented */
typedef struct tagged_task tagged_task_t;
#define tagged_task_clone(task) (task)
#define tagged_task_free(task)                                                                     \
    do {                                                                                           \
    } while (0);

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
typedef int (*priority_queue_compare_function_t)(const tagged_task_t *a, const tagged_task_t *b);

/** @brief A priority queue for tasks. */
typedef struct priority_queue priority_queue_t;

/**
 * @brief  Creates an empty priority queue of ::tagged_task_t's.
 * @param  cmp_func Method called two compare two tasks, to order them in the queue.
 * @return A pointer to a new ::priority_queue_t, or `NULL` on failure.
 *
 * | `errno`  | Cause                  |
 * | -------- | ---------------------- |
 * | `EINVAL` | @p cmp_func is `NULL`. |
 * | `ENOMEM` | Allocation failure.    |
 */
priority_queue_t *priority_queue_new(priority_queue_compare_function_t cmp_func);

/**
 * @brief Clones a priority queue.
 * @param queue Priority queue to be cloned.
 * @return A pointer to a new ::priority_queue_t, or `NULL` on failure.
 *
 * | `errno`  | Cause               |
 * | -------- | ------------------- |
 * | `EINVAL` | @p queue is `NULL`. |
 * | `ENOMEM` | Allocation failure. |
 */
priority_queue_t *priority_queue_clone(const priority_queue_t *queue);

/**
 * @brief Inserts a new ::tagged_task_t into a priority queue.
 *
 * @param queue   Priority queue to insert @p element in. Musn't be `NULL`.
 * @param element Element to be inserted in @p queue. Will be cloned before insertion.
 *                Musn't be `NULL`.
 *
 * @retval 0 Success.
 * @retval 1 Failure (see `errno` below).
 *
 * | `errno`  | Cause                             |
 * | -------- | --------------------------------- |
 * | `EINVAL` | @p queue or @p element is `NULL`. |
 * | `ENOMEM` | Allocation failure.               |
 */
int priority_queue_insert(priority_queue_t *queue, tagged_task_t *element);

/**
 * @brief Removes the top element from a priority queue.
 *
 * @param queue   Priority queue to take the top element from. Musn't be `NULL` or empty.
 * @param element Used to store the removed element.
 *
 * @return The topmost element when successful, `NULL` on failure (`errno = EINVAL` due to `NULL` or
 *         empty @p queue). Ownership of the returned task is passed to the caller.
 */
tagged_task_t *priority_queue_remove_top(priority_queue_t *queue);

/**
 * @brief Returns the number of elements in a priority queue.
 * @param queue Priority queue to have its number of elements returned. Musn't be `NULL`.
 * @return The number of elements stored in the @p queue, or `(size_t) -1` on failure
 *         (`errno = EINVAL`).
 */
size_t priority_queue_element_count(const priority_queue_t *queue);

/**
 * @brief Frees the memory used by a priority queue.
 * @param queue Queue to be freed.
 */
void priority_queue_free(priority_queue_t *queue);

#endif
