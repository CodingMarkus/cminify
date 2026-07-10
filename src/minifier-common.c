/*
 * Copyright 2026 CodingMarkus
 * Copyright 2024-2026 Jumping-Beaver
 *
 * SPDX-License-Identifier: ISC
 */

#include <stdio.h>
#include <string.h>

#include "minifier.h"

bool is_whitespace(const char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

bool skip_whitespaces_comments(struct Minification *m, const char *input, size_t *i, char *min,
    size_t *min_length, enum CommentVariant comment_variant)
{
    bool skipped_all_comments = true;
    do {
        while (is_whitespace(input[*i])) {
            *i += 1;
        }
        const char *preserved_comment = NULL;
        if (input[*i] == '\0') {
            break;
        }
        else if (input[*i] == '/' && input[*i + 1] == '*') {
            size_t comment_start = *i;
            if (input[*i + 2] == '!') {
                preserved_comment = &input[*i];
            }
            *i += 2;
            while (input[*i] != '\0' && (input[*i] != '*' || input[*i + 1] != '/')) {
                *i += 1;
            }
            if (input[*i] == '\0') {
                m->result = NULL;
                m->error_position = comment_start;
                snprintf(m->error, sizeof m->error,
                    "Unclosed multi-line comment starting in line %%zu, column %%zu\n");
                return false;
            }
            *i += 2;
        }
        else if (comment_variant == COMMENT_VARIANT_JS && input[*i] == '/' && input[*i + 1] == '/') {
            *i += 2;
            while (input[*i] != '\0' && input[*i] != '\n') {
                *i += 1;
            }
        }
        else {
            break;
        }
        if (preserved_comment != NULL) {
            skipped_all_comments = false;
            if (min != NULL) {
                memcpy(&min[*min_length], preserved_comment, &input[*i] - preserved_comment);
                *min_length += &input[*i] - preserved_comment;
            }
        }
    } while (true);
    return skipped_all_comments;
}

int strnicmp(const char *s1, const char *s2, size_t length)
{
    int diff = 0;
    while (length--) {
        if ((diff = *s1 - *s2)) {
            if ((unsigned char) (*s1 - 'A') <= 'Z' - 'A') {
                diff += 'a' - 'A';
            }
            if ((unsigned char) (*s2 - 'A') <= 'Z' - 'A') {
                diff -= 'a' - 'A';
            }
            if (diff != 0) {
                break;
            }
        }
        if (*s1 == '\0') {
            break;
        }
        s1++;
        s2++;
    }
    return diff;
}
