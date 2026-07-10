/*
 * Copyright 2026 CodingMarkus
 * Copyright 2024-2026 Jumping-Beaver
 *
 * SPDX-License-Identifier: ISC
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "minifier.h"

struct Minification minify_css(const char *css)
{
    struct Minification m = {.result = malloc(strlen(css) + 1)};
    if (m.result == NULL) {
        snprintf(m.error, sizeof m.error, "Cannot allocate memory\n");
        return m;
    }

    enum {
        SYNTAX_BLOCK_STYLE,
        SYNTAX_BLOCK_RULE_START,
        SYNTAX_BLOCK_QRULE,
        SYNTAX_BLOCK_QRULE_ROUND_BRACKETS,
        SYNTAX_BLOCK_QRULE_SQUARE_BRACKETS,
        SYNTAX_BLOCK_ATRULE,
        SYNTAX_BLOCK_ATRULE_ROUND_BRACKETS,
        SYNTAX_BLOCK_ATRULE_SQUARE_BRACKETS,
    } syntax_block = SYNTAX_BLOCK_RULE_START;
    size_t result_length = 0;
    const char *atrule = NULL;
    size_t atrule_length;
    size_t i = 0;
    size_t nesting_level = 0;

	#define CSS_SKIP_WHITESPACES_COMMENTS(css, ptr_i, out, ptr_out_length) \
		skip_whitespaces_comments(&m, css, ptr_i, out, ptr_out_length, COMMENT_VARIANT_CSS); \
		if (m.result == NULL) { \
			goto error; \
		}

    CSS_SKIP_WHITESPACES_COMMENTS(css, &i, m.result, &result_length);
    while (true) {
        if (css[i] == '\0') {
            if (syntax_block != SYNTAX_BLOCK_RULE_START) {
                while (i > 0 && is_whitespace(css[i - 1])) {
                    i -= 1;
                }
                if (syntax_block == SYNTAX_BLOCK_STYLE) {
                    snprintf(m.error, sizeof m.error,
                        "Unexpected end of stylesheet, expected `}` after line %%zu, column %%zu\n");
                }
                else if (syntax_block == SYNTAX_BLOCK_QRULE) {
                    snprintf(m.error, sizeof m.error,
                        "Unexpected end of stylesheet, expected `{…}` after line %%zu, column %%zu\n");
                }
                else if (syntax_block == SYNTAX_BLOCK_ATRULE) {
                    snprintf(m.error, sizeof m.error,
                        "Unexpected end of stylesheet, expected `;` or `{…}` after line %%zu, column %%zu\n");
                }
                else {
                    snprintf(m.error, sizeof m.error,
                        "Unexpected end of stylesheet after line %%zu, column %%zu\n");
                }
                m.error_position = i - 1;
                goto error;
            }
            m.result[result_length] = '\0';
            break;
        }
        if (css[i] == '}') {
            do {
                if (nesting_level == 0) {
                    m.error_position = i;
                    snprintf(m.error, sizeof m.error, "Unexpected `}` in line %%zu, column %%zu\n");
                    goto error;
                }
                m.result[result_length++] = '}';
                nesting_level -= 1;
                i += 1;
                CSS_SKIP_WHITESPACES_COMMENTS(css, &i, m.result, &result_length);
            } while (css[i] == '}');
            syntax_block = SYNTAX_BLOCK_RULE_START;
            continue;
        }
        if (syntax_block == SYNTAX_BLOCK_RULE_START) {
            if (css[i] == '{' || css[i] == '}' || css[i] == '"' || css[i] == '\'') {
                m.error_position = i;
                snprintf(m.error, sizeof m.error, "Unexpected `%c` in line %%zu, column %%zu\n", css[i]);
                goto error;
            }
            m.result[result_length++] = css[i];
            if (css[i] == '@') {
                syntax_block = SYNTAX_BLOCK_ATRULE;
                atrule = &css[i];
                i += 1;
                atrule_length = 1;
                while (isalnum(css[i])) {
                    m.result[result_length++] = css[i];
                    atrule_length += 1;
                    i += 1;
                }
            }
            else {
                syntax_block = SYNTAX_BLOCK_QRULE;
                i += 1;
            }
            continue;
        }
        if (i >= 3 && !strncmp(&css[i - 3], "url(", 4)) {
            m.result[result_length++] = '(';
            i += 1;
            while (is_whitespace(css[i])) {
                i += 1;
            }
            if (css[i] == '"' || css[i] == '\'') {
                size_t quote_start_i = i;
                char quot = css[i];
                bool active_backslash = false;
                do {
                    active_backslash = (css[i] == '\\') * !active_backslash;
                    m.result[result_length++] = css[i];
                    i += 1;
                } while ((css[i] != quot || active_backslash) && css[i] != '\0');
                if (css[i] == '\0') {
                    m.error_position = quote_start_i;
                    snprintf(m.error, sizeof m.error,
                        "Unclosed string starting in line %%zu, column %%zu\n");
                    goto error;
                }
                m.result[result_length++] = quot;
                i += 1;
                while (is_whitespace(css[i])) {
                    i += 1;
                }
                if (css[i] != ')') {
                    m.error_position = i;
                    snprintf(m.error, sizeof m.error, "Expected `)` in line %%zu, column %%zu\n");
                    goto error;
                }
            }
            else {
                while ((css[i] != ')' || css[i - 1] == '\\') && css[i] != '\0' && !is_whitespace(css[i])) {
                    m.result[result_length++] = css[i];
                    i += 1;
                }
                while (is_whitespace(css[i])) {
                    i += 1;
                }
                if (css[i] != ')') {
                    if (css[i] == '\0') {
                        m.error_position = i;
                        snprintf(m.error, sizeof m.error,
                            "Unexpected end of stylesheet, expected `)` in line %%zu, column %%zu\n");
                        goto error;
                    }
                    else if (is_whitespace(css[i - 1])) {
                        m.error_position = i;
                        snprintf(m.error, sizeof m.error,
                            "Illegal whitespace in URL in line %%zu, column %%zu\n");
                        goto error;
                    }
                }
            }
            m.result[result_length++] = ')';
            i += 1;
            continue;
        }
        if (css[i] == '\\') {
            m.result[result_length++] = css[i++];
            bool active_backslash = true;
            while (css[i] == '\\') {
                active_backslash = !active_backslash;
                m.result[result_length++] = css[i++];
            }
            if (active_backslash) {
                m.result[result_length++] = css[i++];
            }
            continue;
        }
        if (css[i] == '"' || css[i] == '\'') {
            size_t quote_start_i = i;
            m.result[result_length++] = css[i++];
            bool active_backslash = false;
            while (css[i] != '\0' && (css[i] != css[quote_start_i] || active_backslash)) {
                active_backslash = (css[i] == '\\') * !active_backslash;
                m.result[result_length++] = css[i];
                i += 1;
            }
            if (css[i] == '\0') {
                m.error_position = quote_start_i;
                snprintf(m.error, sizeof m.error, "Unclosed string starting in line %%zu, column %%zu\n");
                goto error;
            }
            m.result[result_length++] = css[quote_start_i];
            i += 1;
            continue;
        }
        if (css[i] == ';' && syntax_block != SYNTAX_BLOCK_QRULE) {
            do {
                i += 1;
                CSS_SKIP_WHITESPACES_COMMENTS(css, &i, m.result, &result_length);
            } while (css[i] == ';');
            if (css[i] != '}') {
                m.result[result_length++] = ';';
            }
            if (syntax_block == SYNTAX_BLOCK_ATRULE) {
                syntax_block = SYNTAX_BLOCK_RULE_START;
            }
            continue;
        }
        if (css[i] == '{') {
            nesting_level += 1;
            if (syntax_block == SYNTAX_BLOCK_STYLE)  {
                m.error_position = i;
                snprintf(m.error, sizeof m.error, "Unexpected `{` in line %%zu, column %%zu\n");
                goto error;
            }
            m.result[result_length++] = '{';
            i += 1;
            CSS_SKIP_WHITESPACES_COMMENTS(css, &i, m.result, &result_length);
            if (syntax_block == SYNTAX_BLOCK_QRULE) {
                syntax_block = SYNTAX_BLOCK_STYLE;
            }
            else if (syntax_block == SYNTAX_BLOCK_ATRULE) {
                bool is_nestable_atrule =
                    (sizeof "@media" - 1 == atrule_length &&
                     !strnicmp(atrule, "@media", atrule_length)) ||

                    (sizeof "@layer " - 1 == atrule_length &&
                     !strnicmp(atrule, "@layer", atrule_length)) ||

                    (sizeof "@container" - 1 == atrule_length &&
                     !strnicmp(atrule, "@container", atrule_length)) ||

                    (sizeof "@keyframes" - 1 == atrule_length &&
                     !strnicmp(atrule, "@keyframes", atrule_length));

                syntax_block = is_nestable_atrule ? SYNTAX_BLOCK_RULE_START : SYNTAX_BLOCK_STYLE;
            }
            continue;
        }
        if (css[i] == '0' && css[i + 1] == '.' && (i == 0 || css[i - 1] < '0' || css[i - 1] > '9')) {
            // Converting for example `0.1` to `.1`
            i += 1;
            continue;
        }
        if (css[i] == '(' && syntax_block == SYNTAX_BLOCK_ATRULE) {
            syntax_block = SYNTAX_BLOCK_ATRULE_ROUND_BRACKETS;
            m.result[result_length++] = '(';
            i += 1;
            continue;
        }
        if (css[i] == '[' && syntax_block == SYNTAX_BLOCK_ATRULE) {
            syntax_block = SYNTAX_BLOCK_ATRULE_SQUARE_BRACKETS;
            m.result[result_length++] = '[';
            i += 1;
            continue;
        }
        if (css[i] == ')' && syntax_block == SYNTAX_BLOCK_ATRULE_ROUND_BRACKETS) {
            syntax_block = SYNTAX_BLOCK_ATRULE;
            m.result[result_length++] = ')';
            i += 1;
            continue;
        }
        if (css[i] == ']' && syntax_block == SYNTAX_BLOCK_ATRULE_SQUARE_BRACKETS) {
            syntax_block = SYNTAX_BLOCK_ATRULE;
            m.result[result_length++] = ']';
            i += 1;
            continue;
        }
        if (css[i] == '(' && syntax_block == SYNTAX_BLOCK_QRULE) {
            syntax_block = SYNTAX_BLOCK_QRULE_ROUND_BRACKETS;
            m.result[result_length++] = '(';
            i += 1;
            continue;
        }
        if (css[i] == '[' && syntax_block == SYNTAX_BLOCK_QRULE) {
            syntax_block = SYNTAX_BLOCK_QRULE_SQUARE_BRACKETS;
            m.result[result_length++] = '[';
            i += 1;
            continue;
        }
        if (css[i] == ')' && syntax_block == SYNTAX_BLOCK_QRULE_ROUND_BRACKETS) {
            syntax_block = SYNTAX_BLOCK_QRULE;
            m.result[result_length++] = ')';
            i += 1;
            continue;
        }
        if (css[i] == ']' && syntax_block == SYNTAX_BLOCK_QRULE_SQUARE_BRACKETS) {
            syntax_block = SYNTAX_BLOCK_QRULE;
            m.result[result_length++] = ']';
            i += 1;
            continue;
        }
        if (is_whitespace(css[i]) ||
            (css[i] == '/' && css[i + 1] == '*'))
        {
            if (syntax_block == SYNTAX_BLOCK_ATRULE_ROUND_BRACKETS ||
                syntax_block == SYNTAX_BLOCK_QRULE_ROUND_BRACKETS)
            {
                // Removing whitespace around `:` in `@media (with : 3 px){}` but not in `@page :left{}`

                CSS_SKIP_WHITESPACES_COMMENTS(css, &i, m.result, &result_length);
                if (strchr("(,<>:", m.result[result_length - 1]) == NULL &&
                    strchr("),<>:", css[i]) == NULL)
                {
                    m.result[result_length++] = ' ';
                }
            }
            else if (syntax_block == SYNTAX_BLOCK_ATRULE_SQUARE_BRACKETS ||
                     syntax_block == SYNTAX_BLOCK_QRULE_SQUARE_BRACKETS)
            {
                CSS_SKIP_WHITESPACES_COMMENTS(css, &i, m.result, &result_length);
                if (strchr("[=,", m.result[result_length - 1]) == NULL &&
                    strchr("]=,*$^-|", css[i]) == NULL)
                {
                    m.result[result_length++] = ' ';
                }
            }
            else if (syntax_block == SYNTAX_BLOCK_ATRULE) {
                size_t before_whitespace = i;
                CSS_SKIP_WHITESPACES_COMMENTS(css, &i, m.result, &result_length);

                // Removing whitespace before `(` in `@media (...){}` but not in `@media all and (...){}`

                if ((css[i] != '(' || &atrule[atrule_length - 1] != &css[before_whitespace] - 1) &&
                    strchr(",)(", m.result[result_length - 1]) == NULL &&
                    strchr(",);{", css[i]) == NULL)
                {
                    m.result[result_length++] = ' ';
                }
            }
            else if (syntax_block == SYNTAX_BLOCK_QRULE) {
                CSS_SKIP_WHITESPACES_COMMENTS(css, &i, m.result, &result_length);
                if (strchr("~>+,]", m.result[result_length - 1]) == NULL &&
                    strchr("~>+,[{", css[i]) == NULL)
                {
                    m.result[result_length++] = ' ';
                }
            }
            else if (syntax_block == SYNTAX_BLOCK_STYLE) {
                CSS_SKIP_WHITESPACES_COMMENTS(css, &i, m.result, &result_length);
                if (strchr("{:,", m.result[result_length - 1]) == NULL &&
                    strchr("}:,;!", css[i]) == NULL)
                {
                    m.result[result_length++] = ' ';
                }
            }
            continue;
        }
        m.result[result_length++] = css[i];
        i += 1;
    }
    return m;

error:
    free(m.result);
    m.result = NULL;
    return m;
}
