/*
 * Copyright 2026 CodingMarkus
 * Copyright 2024-2026 Jumping-Beaver
 *
 * SPDX-License-Identifier: ISC
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "minifier.h"

struct Minification minify_json(const char *json)
{
    struct Minification m = {.result = malloc(strlen(json) + 1)};

    size_t bracket_types_capacity = 512;
    char *bracket_types = malloc(bracket_types_capacity * sizeof *bracket_types);

    if (m.result == NULL || bracket_types == NULL) {
        snprintf(m.error, sizeof m.error, "Cannot allocate memory\n");
        goto error;
    }

    size_t nesting_level = 0;
    size_t result_length = 0;
    size_t i = 0;

    while (true) {
        while (is_whitespace(json[i])) {
            i += 1;
        }
        if (json[i] == '\0') {
            m.result[result_length] = '\0';
            break;
        }
        if ((json[i] == ',' || json[i] == '}') && m.result[result_length - 1] == ':') {
            m.error_position = i;
            snprintf(m.error, sizeof m.error, "No value after `:` in line %%zu, column %%zu\n");
            goto error;
        }
        if (json[i] == '[' || json[i] == '{') {
            if (++nesting_level > bracket_types_capacity) {
                bracket_types_capacity += 512;
                char *bracket_types_realloc = realloc(
                    bracket_types, bracket_types_capacity * sizeof *bracket_types);
                if (bracket_types_realloc == NULL) {
                    goto error;
                }
                bracket_types = bracket_types_realloc;
            }
            bracket_types[nesting_level - 1] = json[i];
            m.result[result_length++] = json[i];
            i += 1;
            continue;
        }
        if (json[i] == ']' || json[i] == '}') {
            if (m.result[result_length - 1] == ',') {
                m.error_position = i;
                snprintf(m.error, sizeof m.error, "Illegal `,` before bracket in line %%zu, column %%zu\n");
                goto error;
            }
            if (nesting_level == 0 || bracket_types[nesting_level - 1] != json[i] - 2) {
                m.error_position = i;
                snprintf(m.error, sizeof m.error, "Unexpected `%c` in line %%zu, column %%zu\n", json[i]);
                goto error;
            }
            nesting_level -= 1;
            m.result[result_length++] = json[i];
            i += 1;
            continue;
        }

        bool is_key = nesting_level > 0 && (bracket_types[nesting_level - 1] == '{' &&
            (m.result[result_length - 1] == ',' || m.result[result_length - 1] == '{'));

        if (json[i] == '"') {
            i += 1;
            m.result[result_length++] = '"';
            bool active_backslash = false;
            while (json[i] != '\0' && (json[i] != '"' || active_backslash)) {
                if (json[i] == '\n') {
                    m.error_position = i - 1;
                    snprintf(m.error, sizeof m.error, "Illegal line break in JSON string after line %%zu\n");
                    goto error;
                }
                active_backslash = (json[i] == '\\') * !active_backslash;
                if (active_backslash && strchr("\"\\/bfnrtu", json[i + 1]) == NULL) {
                    m.error_position = i;
                    snprintf(m.error, sizeof m.error,
                        "Invalid JSON escape sequence `\\%c` in line %%zu, column %%zu\n", json[i + 1]);
                    goto error;
                }
                if (active_backslash && json[i + 1] == 'u') {
                    bool invalid_unicode = false;
                    size_t k;
                    for (k = i + 2; k <= i + 5; ++k) {
                        if (!(
                            (json[k] >= '0' && json[k] <= '9') ||
                            (json[k] >= 'a' && json[k] <= 'f') ||
                            (json[k] >= 'A' && json[k] <= 'F')
                        )) {
                            invalid_unicode = true;
                        }
                        if (json[k] == '"' || json[k] == '\n' || json[k] == '\0') {
                            break;
                        }
                    }
                    if (invalid_unicode) {
                        m.error_position = i - 1;
                        snprintf(m.error, sizeof m.error,
                            "Invalid JSON escape sequence `%.*s` in line %%zu, column %%zu\n",
                            (int) (k - i), &json[i]);
                        goto error;
                    }
                }
                m.result[result_length++] = json[i];
                i += 1;
            }
            if (json[i] == '\0') {
                m.error_position = i - 1;
                snprintf(m.error, sizeof m.error,
                    "Unexpected end of JSON document, expected `\"` after line %%zu, column %%zu\n");
                goto error;
            }
            m.result[result_length++] = '"';
            i += 1;
            if (!is_key) {
                continue;
            }
            while (is_whitespace(json[i])) {
                i += 1;
            }
            if (json[i] != ':') {
                m.error_position = i;
                snprintf(m.error, sizeof m.error,
                    "Expected `:` instead of `%c` in line %%zu, column %%zu\n", json[i]);
                goto error;
            }
            m.result[result_length++] = ':';
            i += 1;
            continue;
        }

        if (is_key) {
            m.error_position = i;
            snprintf(m.error, sizeof m.error,
                "Expected `\"`, `[` or `{` instead of `%c` in line %%zu, column %%zu\n", json[i]);
            goto error;
        }

        if (!strncmp(&json[i], "true", sizeof "true" - 1) &&
            (json[i + sizeof "true" - 1] == '\0' || strchr(" \r\t\n],}", json[i + sizeof "true" - 1])))
        {
            strcpy(&m.result[result_length], "true");
            result_length += sizeof "true" - 1;
            i += sizeof "true" - 1;
            continue;
        }
        if (!strncmp(&json[i], "false", sizeof "false" - 1) &&
            (json[i + sizeof "false" - 1] == '\0' || strchr("\r\t\n],}", json[i + sizeof "false" - 1])))
        {
            strcpy(&m.result[result_length], "false");
            result_length += sizeof "false" - 1;
            i += sizeof "false" - 1;
            continue;
        }
        if (!strncmp(&json[i], "null", sizeof "null" - 1) &&
            (json[i + sizeof "null" - 1] == '\0' || strchr("\r\t\n],}", json[i + sizeof "null" - 1])))
        {
            strcpy(&m.result[result_length], "null");
            result_length += sizeof "null" - 1;
            i += sizeof "null" - 1;
            continue;
        }
        if (json[i] >= '0' && json[i] <= '9') {
            size_t k = i;
            while (json[k] >= '0' && json[k] <= '9') {
                k += 1;
            }
            if (json[k] == '.') {
                k += 1;
            }
            while (json[k] >= '0' && json[k] <= '9') {
                k += 1;
            }
            if ((json[k] == 'e' || json[k] == 'E') && (json[k + 1] == '+' || json[k + 1] == '-') &&
                json[k + 2] >= '0' && json[k + 2] <= '9')
            {
                k += 2;
            }
            while (json[k] >= '0' && json[k] <= '9') {
                k += 1;
            }
            memcpy(&m.result[result_length], &json[i], k - i);
            result_length += k - i;
            i = k;
            continue;
        }
        if (nesting_level > 0 && json[i] == ',' && m.result[result_length - 1] != ',') {
            m.result[result_length++] = ',';
            i += 1;
            continue;
        }
        if (json[i] != '\0' && !is_whitespace(json[i])) {
            m.error_position = i;
            snprintf(m.error, sizeof m.error,
                "Unexpected data starting with `%c` in line %%zu, column %%zu\n", json[i]);
            goto error;
        }
    }
    if (nesting_level != 0) {
        do {
            i -= 1;
        } while (is_whitespace(json[i]));
        m.error_position = i;
        snprintf(m.error, sizeof m.error,
            "Missing `%c` after line %%zu, column %%zu\n", bracket_types[nesting_level - 1]);
        goto error;
    }
    free(bracket_types);
    return m;

error:
    free(bracket_types);
    free(m.result);
    m.result = NULL;
    return m;
}
