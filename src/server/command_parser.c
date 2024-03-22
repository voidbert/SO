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
 * @file  server/command_parser.c
 * @brief Implementation of methods in server/command_parser.h
 */

#include <errno.h>
#include <stdlib.h>

#include "server/command_parser.h"

/**
 * @struct command_parser_token_t
 * @brief  Token in a command line.
 *
 * @var command_parser_token_t::string
 *     @brief Contents of the token.
 * @var command_parser_token_t::length
 *     @brief Number of characters in command_parser_token_t::string.
 * @var command_parser_token_t::capacity
 *     @brief Maximum number of characters (including terminator) that
 *            command_parser_token_t::string supports.
 * @var command_parser_token_t::is_pipe
 *     @brief Whether the token is a pipe, as a string comparison to `"|"` has no information about
 *            whether a token is a pipe or just part of a program's arguments.
 */
typedef struct {
    char  *string;
    size_t length, capacity;
    int    is_pipe;
} command_parser_token_t;

/** @brief Initial value of command_parser_token_t::capacity.  */
#define COMMAND_PARSER_INITIAL_TOKEN_CAPACITY 32

/**
 * @brief   Creates a new empty token.
 * @returns A new token on success, `NULL` on allocation error (`errno = ENOMEM`).
 */
command_parser_token_t *__command_parser_token_new_empty(void) {
    command_parser_token_t *ret = malloc(sizeof(command_parser_token_t));
    if (!ret)
        return NULL; /* errno = ENOMEM guaranteed */

    ret->is_pipe  = 0;
    ret->length   = 0;
    ret->capacity = COMMAND_PARSER_INITIAL_TOKEN_CAPACITY;
    ret->string   = malloc(sizeof(char) * ret->capacity);
    if (!ret->string) {
        free(ret);
        return NULL; /* errno = ENOMEM guaranteed */
    }
    ret->string[0] = '\0';

    return ret;
}

/**
 * @brief Frees memory used by a token.
 * @param token Token to have its memory released. Musn't be `NULL`.
 */
void __command_parser_token_free(command_parser_token_t *token) {
    if (!token)
        return; /* Don't set EINVAL, as that's normal free behavior */
    free(token->string);
    free(token);
}

/**
 * @brief Appends a character to a token.
 *
 * @param token Token to have a character appended to it. Musn't be `NULL`.
 * @param c     Character to be added to @p token.
 *
 * @retval 0 Success.
 * @retval 1 Invalid arguments (@p token is `NULL`) or allocation failure.
 *
 * | `errno`  | Cause               |
 * | -------- | ------------------- |
 * | `EINVAL` | @p token is `NULL`. |
 * | `ENOMEM` | Allocation failure. |
 */
int __command_parser_token_append(command_parser_token_t *token, char c) {
    if (!token) {
        errno = EINVAL;
        return 1;
    }

    if (token->length >= token->capacity - 2) {
        size_t new_capacity = token->capacity * 2;
        char  *new_string   = realloc(token->string, sizeof(char) * new_capacity);
        if (!new_string)
            return 1; /* errno = ENOMEM guaranteed */

        token->capacity = new_capacity;
        token->string   = new_string;
    }

    token->string[token->length]     = c;
    token->string[token->length + 1] = '\0';
    token->length++;
    return 0;
}

/**
 * @brief  Wrapper for ::__command_parser_token_append for use in ::__command_parser_token_next,
 *         that automatically frees @p token and returns `NULL` on failure.
 * @param  token See ::__command_parser_token_append.
 * @param  c     See ::__command_parser_token_append.
 */
#define APPEND_ERROR_CHECK(token, c)                                                               \
    do {                                                                                           \
        if (__command_parser_token_append(token, c)) {                                             \
            __command_parser_token_free(token);                                                    \
            return NULL; /* errno = ENOMEM guaranteed */                                           \
        }                                                                                          \
    } while (0)

/**
 * @brief  Gets the next token from a command line.
 * @param  remaining Pointer to part of command line still left to tokenize.  Will be updated to the
 *                   position after the token read.
 * @return A new token, or `NULL` on failure.
 *
 * | `errno`  | Cause                                                                  |
 * | -------- | ---------------------------------------------------------------------- |
 * | `EINVAL` | @p remaining is `NULL`, points to a `NULL` string, or parsing failure. |
 * | `ENOMEM` | Allocation failure.                                                    |
 */
command_parser_token_t *__command_parser_token_next(const char **remaining) {
    if (!remaining || !*remaining) {
        errno = EINVAL;
        return NULL;
    }

    int                     in_double_quotes = 0, in_single_quotes = 0, have_been_quotes = 0;
    command_parser_token_t *token = __command_parser_token_new_empty();
    if (!token)
        return NULL; /* errno = ENOMEM guaranteed */

    const char *cursor;
    for (cursor = *remaining; *cursor; cursor++) {
        switch (*cursor) {
            case '"':
                have_been_quotes = 1;
                if (in_single_quotes)
                    APPEND_ERROR_CHECK(token, '\"');
                else
                    in_double_quotes = !in_double_quotes;
                break;

            case '\'':
                have_been_quotes = 1;
                if (in_double_quotes)
                    APPEND_ERROR_CHECK(token, '\'');
                else
                    in_single_quotes = !in_single_quotes;
                break;

            case '\\':
                if (!in_single_quotes) {
                    cursor++;
                    if (*cursor == '\\' || *cursor == '\"' ||
                        (!in_double_quotes && *cursor == ' ')) {
                        APPEND_ERROR_CHECK(token, *cursor);
                    } else if (*cursor) {
                        APPEND_ERROR_CHECK(token, '\\');
                        APPEND_ERROR_CHECK(token, *cursor);
                    } else {
                        errno = EINVAL; /* Unterminated escape sequence */
                        __command_parser_token_free(token);
                        return NULL;
                    }
                } else {
                    APPEND_ERROR_CHECK(token, '\\');
                }
                break;

            case '\t':
            case ' ':
                if (in_double_quotes || in_single_quotes) {
                    APPEND_ERROR_CHECK(token, *cursor);
                } else if (*token->string || have_been_quotes) {
                    *remaining = cursor + 1;
                    return token;
                }
                break;

            case '|':
                if (in_double_quotes || in_single_quotes) {
                    APPEND_ERROR_CHECK(token, '|');
                } else if (*token->string || have_been_quotes) {
                    *remaining = cursor;
                    return token;
                } else {
                    APPEND_ERROR_CHECK(token, '|');
                    *remaining     = cursor + 1;
                    token->is_pipe = 1;
                    return token;
                }
                break;

            default:
                APPEND_ERROR_CHECK(token, *cursor);
                break;
        }
    }

    if (in_double_quotes || in_single_quotes) {
        /* Parsing error: unclosed quotation marks */
        errno = EINVAL;
        __command_parser_token_free(token);
        return NULL;
    }

    if (*token->string || have_been_quotes) {
        *remaining = cursor;
        return token;
    } else {
        __command_parser_token_free(token);
        return NULL; /* End of string */
    }
}

program_t *command_parser_parse_command(const char *command_line) {
    task_t *full_task = command_parser_parse_task(command_line);
    if (!full_task)
        return NULL; /* Keep errno */

    size_t                  nprograms;
    const program_t *const *programs = task_get_programs(full_task, &nprograms);

    if (nprograms != 1) {
        errno = EINVAL; /* Parsing error: contains pipes */
        task_free(full_task);
        return NULL;
    }

    program_t *ret = program_clone(programs[0]);
    task_free(full_task);
    return ret;
}

task_t *command_parser_parse_task(const char *command_line) {
    if (!command_line) {
        errno = EINVAL;
        return NULL;
    }

    task_t                 *ret             = NULL;
    program_t              *current_program = NULL;
    command_parser_token_t *token           = NULL;

    ret = task_new_empty();
    if (!ret)
        goto FAILURE; /* ENOMEM */

    current_program = program_new_empty();
    if (!current_program)
        goto FAILURE; /* ENOMEM */

    errno                 = 0;
    const char *remaining = command_line;
    while ((token = __command_parser_token_next(&remaining))) {
        if (token->is_pipe) {
            if (program_get_argument_count(current_program) == 0) {
                errno = EINVAL; /* Parsing error (empty command) */
                goto FAILURE;
            }

            if (task_add_program(ret, current_program))
                goto FAILURE; /* ENOMEM */

            program_free(current_program);
            current_program = program_new_empty();
            if (!current_program)
                goto FAILURE; /* ENOMEM */

        } else if (program_add_argument(current_program, token->string)) {
            goto FAILURE;
        }

        __command_parser_token_free(token);
    }

    if (errno != 0)
        goto FAILURE; /* Keep errno from tokenizer */

    if (program_get_argument_count(current_program) == 0) {
        errno = EINVAL; /* Parsing error (empty command) */
        goto FAILURE;
    }

    if (task_add_program(ret, current_program))
        goto FAILURE;

    program_free(current_program);
    return ret;

FAILURE:
    if (token)
        __command_parser_token_free(token);
    if (current_program)
        program_free(current_program);
    if (ret)
        task_free(ret);
    return NULL;
}
