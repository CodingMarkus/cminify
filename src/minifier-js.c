/*
 * Copyright 2026 CodingMarkus
 * Copyright 2024-2026 Jumping-Beaver
 *
 * SPDX-License-Identifier: ISC
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "js-mangler.h"
#include "minifier.h"

struct Minification minify_js(const char *js)
{
    struct Minification m = {.result = malloc(strlen(js) + 1)};

    size_t curly_blocks_capacity = 64;
    struct CurlyBlock {
        enum {
            CURLY_BLOCK_GLOBAL, // Needed to track do_nesting_level
            CURLY_BLOCK_UNKNOWN,
            CURLY_BLOCK_DO,
            CURLY_BLOCK_TRY_FINALLY,
            CURLY_BLOCK_STANDALONE,
            CURLY_BLOCK_FUNC_BODY,
            CURLY_BLOCK_FUNC_BODY_STANDALONE,
            CURLY_BLOCK_CONDITION_BODY,
            CURLY_BLOCK_STRING_INTERPOLATION,
            CURLY_BLOCK_ARROWFUNC_BODY,
        } type;
        size_t do_nesting_level;
    } *curly_blocks = malloc(curly_blocks_capacity * sizeof *curly_blocks);

    size_t round_blocks_capacity = 64;
    enum RoundBlockType {
        ROUND_BLOCK_DO_WHILE,
        ROUND_BLOCK_PREFIXED_CONDITION,
        ROUND_BLOCK_UNKNOWN,
        ROUND_BLOCK_CONDITION,
        ROUND_BLOCK_CATCH_SWITCH,
        ROUND_BLOCK_PARAM,
        ROUND_BLOCK_PARAM_STANDALONE,
        ROUND_BLOCK_PARAM_ARROWFUNC_SINGLE,
    } *round_blocks = malloc(round_blocks_capacity * sizeof *round_blocks);

    if (m.result == NULL || curly_blocks == NULL || round_blocks == NULL) {
        snprintf(m.error, sizeof m.error, "Cannot allocate memory\n");
        goto error;
    }

    curly_blocks[0] = (struct CurlyBlock) {CURLY_BLOCK_GLOBAL, 0};
    size_t curly_nesting_level = 1;
    size_t round_nesting_level = 0;

    size_t result_length = 0;
    size_t i = 0;
    size_t last_open_curly_bracket_i = 0, last_open_round_bracket_i = 0;
    const char *identifier_delimiters = "'\"`%<>+*/-=,(){}[]!~;|&^:? \t\r\n";

	#define JS_SKIP_WHITESPACES_COMMENTS(js, ptr_i, out, ptr_out_length) \
		skip_whitespaces_comments(&m, js, ptr_i, out, ptr_out_length, COMMENT_VARIANT_JS); \
		if (m.result == NULL) { \
			goto error; \
		}

    #define INCR_CURLY_NESTING_LEVEL \
        if (++curly_nesting_level > curly_blocks_capacity) { \
            curly_blocks_capacity += 512; \
            struct CurlyBlock *curly_blocks_realloc = realloc( \
                curly_blocks, curly_blocks_capacity * sizeof *curly_blocks \
            ); \
            if (curly_blocks_realloc == NULL) { \
                snprintf(m.error, sizeof m.error, "Cannot allocate memory\n"); \
                goto error; \
            } \
            curly_blocks = curly_blocks_realloc; \
        } \
        curly_blocks[curly_nesting_level - 1].do_nesting_level = 0; \
        last_open_curly_bracket_i = i;

    #define INCR_ROUND_NESTING_LEVEL \
        if (++round_nesting_level > round_blocks_capacity) { \
            round_blocks_capacity += 512; \
            enum RoundBlockType *round_blocks_realloc = realloc( \
                round_blocks, round_blocks_capacity * sizeof *round_blocks \
            ); \
            if (round_blocks_realloc == NULL) { \
                snprintf(m.error, sizeof m.error, "Cannot allocate memory\n"); \
                goto error; \
            } \
            round_blocks = round_blocks_realloc; \
        } \
        last_open_round_bracket_i = i;

    while (true) {
        if (js[i] == '\0') {
            m.result[result_length] = '\0';
            break;
        }

        size_t next_word_length = strcspn(&js[i], identifier_delimiters);
        if (next_word_length == 0) {
            goto after_keywords;
        }

        // Keywords lose their meaning when used as object keys

        {
            size_t k = i + next_word_length;
            JS_SKIP_WHITESPACES_COMMENTS(js, &k, NULL, NULL);
            if (js[k] == ':') {
                memcpy(&m.result[result_length], &js[i], next_word_length);
                result_length += next_word_length;
                i += next_word_length;
                continue;
            }
        }

        // Next we handle keywords

        if ((next_word_length == sizeof "switch" - 1 &&
             !strncmp(&js[i], "switch", next_word_length)) ||
            (next_word_length == sizeof "catch" - 1 &&
             !strncmp(&js[i], "catch", next_word_length)))
        {
            memcpy(&m.result[result_length], &js[i], next_word_length);
            result_length += next_word_length;
            i += next_word_length;

            JS_SKIP_WHITESPACES_COMMENTS(js, &i, m.result, &result_length);
            if (js[i] == '(') {
                INCR_ROUND_NESTING_LEVEL;
                round_blocks[round_nesting_level - 1] = ROUND_BLOCK_CATCH_SWITCH;
                m.result[result_length++] = '(';
                i += 1;
            }
            else if (js[i] == '{') {
                INCR_CURLY_NESTING_LEVEL;
                curly_blocks[curly_nesting_level - 1].type = CURLY_BLOCK_CONDITION_BODY;
                m.result[result_length++] = '{';
                i += 1;
            }
            else {
                m.error_position = i;
                snprintf(m.error, sizeof m.error, "Expected `(` or `{` in line %%zu, column %%zu\n");
                goto error;
            }
            continue;
        }
        if (next_word_length == sizeof "do" - 1 && !strncmp(&js[i], "do", next_word_length)) {
            memcpy(&m.result[result_length], &js[i], next_word_length);
            result_length += next_word_length;
            i += next_word_length;
            JS_SKIP_WHITESPACES_COMMENTS(js, &i, m.result, &result_length);
            if (js[i] == '{') {
                size_t k = i + 1;
                bool skipped_all_comments = JS_SKIP_WHITESPACES_COMMENTS(js, &k, NULL, NULL);
                if (skipped_all_comments && js[k] == '}') {
                    curly_blocks[curly_nesting_level - 1].do_nesting_level += 1;
                    m.result[result_length++] = ';';
                    i = k + 1;
                    continue;
                }
                INCR_CURLY_NESTING_LEVEL;
                curly_blocks[curly_nesting_level - 1].type = CURLY_BLOCK_DO;
                m.result[result_length++] = '{';
                i += 1;
                continue;
            }
            if (strchr(identifier_delimiters, js[i]) == NULL) {
                m.result[result_length++] = ' ';
            }
            curly_blocks[curly_nesting_level - 1].do_nesting_level += 1;
            continue;
        }
        if (
            (next_word_length == sizeof "try" - 1 &&
             !strncmp(&js[i], "try", next_word_length)) ||
            (next_word_length == sizeof "finally" - 1 &&
             !strncmp(&js[i], "finally", next_word_length))
        ) {
            memcpy(&m.result[result_length], &js[i], next_word_length);
            result_length += next_word_length;
            i += next_word_length;

            JS_SKIP_WHITESPACES_COMMENTS(js, &i, m.result, &result_length);
            if (js[i] != '{') {
                m.error_position = i;
                snprintf(m.error, sizeof m.error, "Expected `{` in line %%zu, column %%zu\n");
                goto error;
            }
            INCR_CURLY_NESTING_LEVEL;
            curly_blocks[curly_nesting_level - 1].type = CURLY_BLOCK_TRY_FINALLY;
            m.result[result_length++] = '{';
            i += 1;
            continue;
        }
        if (next_word_length == sizeof "function" - 1 && !strncmp(&js[i], "function", next_word_length)) {
            // We consume the input until `(` of the parameter list.
            //
            // Regular functions cannot be safely replaced by arrow functions.  Arrow functions
            // cannot be used as constructors: `new arrow_function()` where `arrow_function` is an
            // arrow function is invalid.
            //
            // `standalone` means that the function object is not assigned to a variable. In this case it
            // is possible to omit newlines and semicolons after the function body.

            bool standalone =
                result_length == 0 ||
                m.result[result_length - 1] == ';' ||
                m.result[result_length - 1] == '}' ||
                m.result[result_length - 1] == '{';

            memcpy(&m.result[result_length], &js[i], next_word_length);
            result_length += next_word_length;
            i += next_word_length;

            JS_SKIP_WHITESPACES_COMMENTS(js, &i, m.result, &result_length);

            if (js[i] == '*') {
                m.result[result_length++] = '*';
                i += 1;
                JS_SKIP_WHITESPACES_COMMENTS(js, &i, m.result, &result_length);
            }
            if (js[i] != '(') {
                m.result[result_length++] = ' ';
                while (strchr(identifier_delimiters, js[i]) == NULL) {
                    m.result[result_length++] = js[i++];
                }
            }
            JS_SKIP_WHITESPACES_COMMENTS(js, &i, m.result, &result_length);
            if (js[i] != '(') {
                m.error_position = i;
                snprintf(m.error, sizeof m.error, "Expected `(` in line %%zu, column %%zu\n");
                goto error;
            }
            INCR_ROUND_NESTING_LEVEL;
            round_blocks[round_nesting_level - 1] =
                standalone ? ROUND_BLOCK_PARAM_STANDALONE : ROUND_BLOCK_PARAM;
            m.result[result_length++] = '(';
            i += 1;
            continue;
        }
        if (next_word_length == sizeof "while" - 1 && !strncmp(&js[i], "while", next_word_length)) {
            char curly_bracket_before_while = result_length > 0 && m.result[result_length - 1] == '}';
            memcpy(&m.result[result_length], &js[i], next_word_length);
            result_length += next_word_length;
            i += next_word_length;
            JS_SKIP_WHITESPACES_COMMENTS(js, &i, m.result, &result_length);
            if (js[i] != '(') {
                m.error_position = i;
                snprintf(m.error, sizeof m.error, "Expected `(` in line %%zu, column %%zu\n");
                goto error;
            }
            INCR_ROUND_NESTING_LEVEL;
            if (curly_bracket_before_while && curly_blocks[curly_nesting_level].type == CURLY_BLOCK_DO) {
                round_blocks[round_nesting_level - 1] = ROUND_BLOCK_DO_WHILE;
            }
            else if (curly_blocks[curly_nesting_level - 1].do_nesting_level > 0) {
                curly_blocks[curly_nesting_level - 1].do_nesting_level -= 1;
                round_blocks[round_nesting_level - 1] = ROUND_BLOCK_DO_WHILE;
            }
            else {
                round_blocks[round_nesting_level - 1] = ROUND_BLOCK_PREFIXED_CONDITION;
            }
            m.result[result_length++] = '(';
            i += 1;
            continue;
        }
        if (
            (next_word_length == sizeof "if" - 1 &&
             !strncmp(&js[i], "if", next_word_length)) ||
            (next_word_length == sizeof "for" - 1 &&
             !strncmp(&js[i], "for", next_word_length))
        ) {
            memcpy(&m.result[result_length], &js[i], next_word_length);
            result_length += next_word_length;
            i += next_word_length;
            JS_SKIP_WHITESPACES_COMMENTS(js, &i, m.result, &result_length);
            if (js[i] != '(') {
                m.error_position = i;
                snprintf(m.error, sizeof m.error, "Expected `(` in line %%zu, column %%zu\n");
                goto error;
            }
            INCR_ROUND_NESTING_LEVEL;
            round_blocks[round_nesting_level - 1] = ROUND_BLOCK_PREFIXED_CONDITION;
            m.result[result_length++] = '(';
            i += 1;
            continue;
        }
        if (next_word_length == sizeof "else" - 1 && !strncmp(&js[i], "else", next_word_length)) {
            memcpy(&m.result[result_length], &js[i], next_word_length);
            result_length += next_word_length;
            i += next_word_length;
            size_t k = i;
            JS_SKIP_WHITESPACES_COMMENTS(js, &k, NULL, NULL);
            if (js[k] != '{') {
                continue;
            }
            JS_SKIP_WHITESPACES_COMMENTS(js, &i, m.result, &result_length);
            i += 1;
            k = i;
            bool skipped_all_comments = JS_SKIP_WHITESPACES_COMMENTS(js, &k, NULL, NULL);
            if (skipped_all_comments && js[k] == '}') {
                JS_SKIP_WHITESPACES_COMMENTS(js, &i, m.result, &result_length);
                m.result[result_length++] = ';';
                do {
                    i += 1;
                    JS_SKIP_WHITESPACES_COMMENTS(js, &i, m.result, &result_length);
                } while (js[i] == ';');
            }
            else {
                INCR_CURLY_NESTING_LEVEL;
                m.result[result_length++] = '{';
                curly_blocks[curly_nesting_level - 1].type = CURLY_BLOCK_CONDITION_BODY;
            }
            continue;
        }
        // Cannot make this replacement because it is possible to redefine `undefined` in the local scope.
        // if (next_word_length == sizeof "undefined" - 1 && !strncmp(&js[i], "undefined", next_word_length)) {
        //    strcpy(&m.result[result_length], "void 0");
        //    i += next_word_length;
        //    result_length += sizeof "void 0" - 1;
        //    continue;
        //}
        if ((next_word_length == sizeof "true" - 1 &&
             !strncmp(&js[i], "true", next_word_length)) ||
            (next_word_length == sizeof "false" - 1 &&
             !strncmp(&js[i], "false", next_word_length)))
        {
            if (result_length > 0 && m.result[result_length - 1] == ' ') {
                result_length -= 1;
            }
            m.result[result_length++] = '!';
            m.result[result_length++] = js[i] == 't' ? '0' : '1';
            i += next_word_length;
            continue;
        }

        memcpy(&m.result[result_length], &js[i], next_word_length);
        result_length += next_word_length;
        i += next_word_length;

    after_keywords:

        if (js[i] == '{') {
            INCR_CURLY_NESTING_LEVEL;
            i += 1;
            if (result_length > 0 && m.result[result_length - 1] == ')' &&
                round_blocks[round_nesting_level] == ROUND_BLOCK_PREFIXED_CONDITION)
            {
                // Replacing `if(1){}` by `if(1);`
                bool skipped_all_comments = JS_SKIP_WHITESPACES_COMMENTS(js, &i, m.result, &result_length);
                if (skipped_all_comments && js[i] == '}') {
                    m.result[result_length++] = ';';
                    do {
                        i += 1;
                        JS_SKIP_WHITESPACES_COMMENTS(js, &i, m.result, &result_length);
                    } while (js[i] == ';');
                    curly_nesting_level -= 1;
                    continue;
                }
                else {
                    curly_blocks[curly_nesting_level - 1].type = CURLY_BLOCK_CONDITION_BODY;
                }
            }
            else if (result_length >= 1 && m.result[result_length - 1] == ')' &&
                     round_blocks[round_nesting_level] == ROUND_BLOCK_CATCH_SWITCH)
            {
                curly_blocks[curly_nesting_level - 1].type = CURLY_BLOCK_CONDITION_BODY;
            }
            else if (result_length >= 2 &&
                     m.result[result_length - 2] == '=' && m.result[result_length - 1] == '>')
            {
                curly_blocks[curly_nesting_level - 1].type = CURLY_BLOCK_ARROWFUNC_BODY;
            }
            else if (result_length >= 1 && m.result[result_length - 1] == ')' &&
                     round_blocks[round_nesting_level] == ROUND_BLOCK_PARAM)
            {
                curly_blocks[curly_nesting_level - 1].type = CURLY_BLOCK_FUNC_BODY;
            }
            else if (result_length >= 1 && m.result[result_length - 1] == ')' &&
                     round_blocks[round_nesting_level] == ROUND_BLOCK_PARAM_STANDALONE)
            {
                curly_blocks[curly_nesting_level - 1].type = CURLY_BLOCK_FUNC_BODY_STANDALONE;
            }
            else if (
                result_length >= 1 &&
                (
                    m.result[result_length - 1] == '}' ||
                    m.result[result_length - 1] == ';' ||
                    m.result[result_length - 1] == '{' ||
                    m.result[result_length - 1] == '\n'
                )
            ) {
                curly_blocks[curly_nesting_level - 1].type = CURLY_BLOCK_STANDALONE;
            }
            else {
                curly_blocks[curly_nesting_level - 1].type = CURLY_BLOCK_UNKNOWN;
            }
            m.result[result_length++] = '{';
            continue;
        }
        if (js[i] == '(') {
            INCR_ROUND_NESTING_LEVEL;
            i += 1;

            // Removing round brackets around single parameters with no default
            // value of arrow functions.

            bool remove_round_brackets_around_param = false;

            size_t k = i;
            JS_SKIP_WHITESPACES_COMMENTS(js, &k, NULL, NULL);
            if (js[k] != '.') { // Can't remove round brackets in `(...arg)=>{}`
                size_t arg_start = k;
                while (strchr(identifier_delimiters, js[k]) == NULL) {
                    k += 1;
                }
                JS_SKIP_WHITESPACES_COMMENTS(js, &k, NULL, NULL);
                if (k > arg_start && js[k] == ')') {
                    k += 1;
                    JS_SKIP_WHITESPACES_COMMENTS(js, &k, NULL, NULL);
                    if (js[k] == '=' && js[k + 1] == '>') {
                        remove_round_brackets_around_param = true;
                    }
                }
            }

            if (remove_round_brackets_around_param) {
                round_blocks[round_nesting_level - 1] = ROUND_BLOCK_PARAM_ARROWFUNC_SINGLE;
            }
            else {
                round_blocks[round_nesting_level - 1] = ROUND_BLOCK_UNKNOWN;
                m.result[result_length++] = '(';
            }
            continue;
        }
        if (js[i] == ')') {
            if (round_blocks[round_nesting_level - 1] != ROUND_BLOCK_PARAM_ARROWFUNC_SINGLE) {
                m.result[result_length++] = ')';
            }
            if (round_nesting_level-- == 0) {
                m.error_position = i;
                snprintf(m.error, sizeof m.error, "Unexpected `)` in line %%zu, column %%zu\n");
                goto error;
            }
            i += 1;
            continue;
        }
        if (js[i] == ';') {
            if (result_length == 0) {
                i += 1;
                continue;
            }
            if (round_nesting_level > 0 &&
                round_blocks[round_nesting_level - 1] == ROUND_BLOCK_PREFIXED_CONDITION)
            {
                // Do not remove `;` in `for(;;i++){…}`
                m.result[result_length++] = ';';
                i += 1;
                continue;
            }
            char before_semicolon = m.result[result_length - 1];
            do {
                i += 1;
                JS_SKIP_WHITESPACES_COMMENTS(js, &i, m.result, &result_length);
            } while (js[i] == ';');

            // `;` can be removed before `}` and at the end of the document except
            // when it follows the `)` of a condition.

            if (
                (js[i] == '\0' || js[i] == '}') &&
                !(
                    before_semicolon == ')' &&
                    round_blocks[round_nesting_level] == ROUND_BLOCK_PREFIXED_CONDITION
                )
            ) {
                continue;
            }

            if (
                before_semicolon == '}' &&
                (
                    curly_blocks[curly_nesting_level].type == CURLY_BLOCK_FUNC_BODY_STANDALONE ||
                    curly_blocks[curly_nesting_level].type == CURLY_BLOCK_STANDALONE
                )
            ) {
                continue;
            }

            if (before_semicolon == ')' && round_blocks[round_nesting_level] == ROUND_BLOCK_DO_WHILE) {
                continue;
            }

            m.result[result_length++] = ';';
            continue;
        }
        if (js[i] == '/' && js[i + 1] != '/' && js[i + 1] != '*' &&
            (result_length == 0 ||
             strchr("^!&|([{><+-*%:?~,;=", m.result[result_length - 1]) != NULL ||
             (m.result[result_length - 1] == ' ' &&
              m.result[result_length - 2] == '<')))
        {
            // This is a regex object.

            size_t regex_start_i = i;
            m.result[result_length++] = '/';
            i += 1;
            bool active_backslash = false;
            bool in_angular_brackets = false;
            while (js[i] != '\0' && (js[i] != '/' || active_backslash || in_angular_brackets)) {
                if (js[i] == '\n') {
                    m.error_position = i - 1;
                    snprintf(m.error, sizeof m.error,
                        "Illegal line break in regex at the end of line %%zu\n");
                    goto error;
                }
                m.result[result_length++] = js[i];
                if (js[i] == '[' && !active_backslash) {
                    in_angular_brackets = true;
                }
                else if (js[i] == ']' && !active_backslash) {
                    in_angular_brackets = false;
                }
                active_backslash = js[i++] == '\\' && !active_backslash;
            }
            if (js[i] != '/') {
                m.error_position = regex_start_i;
                snprintf(m.error, sizeof m.error, "Unclosed regex starting in line %%zu, column %%zu\n");
                goto error;
            }
            m.result[result_length++] = '/';
            i += 1;
            continue;
        }
        if (js[i] == '`' || js[i] == '"' || js[i] == '\'' ||
            (js[i] == '}' &&
             curly_blocks[curly_nesting_level - 1].type ==
                 CURLY_BLOCK_STRING_INTERPOLATION))
        {
            if (js[i] == '}') {
                curly_nesting_level -= 1;
            }
            m.result[result_length++] = js[i];
            size_t quote_i;
        merge_strings:
            quote_i = i;
            i += 1;
            bool active_backslash = false;
            while (js[i] != '\0') {
                if (!active_backslash && js[i] == js[quote_i] && js[quote_i] != '}') {
                    break;
                }
                if (!active_backslash && js[quote_i] == '}' && js[i] == '`') {
                    break;
                }
                if (js[i] == '\n') {
                    if (active_backslash) {
                        i += 1;
                        result_length -= 1;
                        continue;
                    }
                    else if (js[quote_i] != '`' && js[quote_i] != '}') {
                        m.error_position = i;
                        snprintf(m.error, sizeof m.error,
                            "String contains unescaped line break in line %%zu, column %%zu\n");
                        goto error;
                    }
                }
                m.result[result_length++] = js[i];
                if (!active_backslash && (js[quote_i] == '}' || js[quote_i] == '`') &&
                    js[i - 1] == '$' && js[i] == '{')
                {
                    INCR_CURLY_NESTING_LEVEL;
                    curly_blocks[curly_nesting_level - 1].type = CURLY_BLOCK_STRING_INTERPOLATION;
                    break;
                }
				if (result_length >= sizeof "</script" - 1 &&
					i < quote_i + sizeof "</script" - 1 &&
					!strnicmp(&m.result[result_length - sizeof "</script" + 1], "</script",
						sizeof "</script" - 1))
				{
                    strcpy(&m.result[result_length - sizeof "</script" + 1], "<\\/script");
                    result_length += 1;
                }
                active_backslash = js[i++] == '\\' && !active_backslash;
            }
            if (js[i] == '{') {
                i += 1;
                continue;
            }
            if (js[i] != js[quote_i] && !(js[quote_i] == '}' && js[i] == '`')) {
                while (is_whitespace(js[i - 1])) {
                    i -= 1;
                }
                m.error_position = i - 1;
                snprintf(m.error, sizeof m.error,
                    "Unexpected end of script, expected `%c` after line %%zu, column %%zu\n", js[quote_i]);
                goto error;
            }
            i += 1;
            size_t k = i;
            bool skipped_all_comments = JS_SKIP_WHITESPACES_COMMENTS(js, &k, NULL, NULL);
            if (!skipped_all_comments || js[k] != '+') {
                m.result[result_length++] = js[quote_i] == '}' ? '`' : js[quote_i];
                continue;
            }
            k += 1;
            skipped_all_comments = JS_SKIP_WHITESPACES_COMMENTS(js, &k, NULL, NULL);
            if (!skipped_all_comments ||
                (js[k] != js[quote_i] &&
                 (js[quote_i] != '}' || js[k] != '`')))
            {
                m.result[result_length++] = js[quote_i] == '}' ? '`' : js[quote_i];
                continue;
            }

            JS_SKIP_WHITESPACES_COMMENTS(js, &i, m.result, &result_length);
            i += 1; // Skipping the plus character
            JS_SKIP_WHITESPACES_COMMENTS(js, &i, m.result, &result_length);

            goto merge_strings;
        }
        else if (js[i] == '}') {
            if (curly_blocks[curly_nesting_level - 1].do_nesting_level != 0) {
                m.error_position = i;
                snprintf(m.error, sizeof m.error,
                    "Unclosed `do` block before `}` in line %%zu, column %%zu\n");
                goto error;
            }
            if (curly_nesting_level-- == 1) {
                m.error_position = i;
                snprintf(m.error, sizeof m.error, "Unexpected `}` in line %%zu, column %%zu\n");
                goto error;
            }
            m.result[result_length++] = '}';
            i += 1;
            continue;
        }
        if (is_whitespace(js[i]) ||
            (js[i] == '/' && js[i + 1] == '*') ||
            (js[i] == '/' && js[i + 1] == '/'))
        {
            size_t whitespace_comment_i = i;
            JS_SKIP_WHITESPACES_COMMENTS(js, &i, m.result, &result_length);
            if (result_length == 0) {
                continue;
            }
            if (js[i] == '}' || js[i] == '\0') {
                continue;
            }

            if ((js[i] == '+' && m.result[result_length - 1] == '+') ||
                (js[i] == '-' && m.result[result_length - 1] == '-'))
            {
                m.result[result_length++] = ' ';
                continue;
            }
            if (
                m.result[result_length - 1] == ')' &&
                (
                    round_blocks[round_nesting_level] == ROUND_BLOCK_PREFIXED_CONDITION ||
                    round_blocks[round_nesting_level] == ROUND_BLOCK_PARAM ||
                    round_blocks[round_nesting_level] == ROUND_BLOCK_PARAM_STANDALONE ||
                    js[i] == '='
                )
            ) {
                continue;
            }
            if (
                m.result[result_length - 1] == '}' &&
                (
                    curly_blocks[curly_nesting_level].type == CURLY_BLOCK_TRY_FINALLY ||
                    curly_blocks[curly_nesting_level].type == CURLY_BLOCK_CONDITION_BODY ||
                    curly_blocks[curly_nesting_level].type == CURLY_BLOCK_FUNC_BODY_STANDALONE ||
                    curly_blocks[curly_nesting_level].type == CURLY_BLOCK_STANDALONE
                )
            ) {
                continue;
            }

            // Newlines terminate a preceding statement even when they are in a comment.
            // Try it out: `Math.sin(1)/*\n*/Math.sin(1)` is valid; without `\n` it is invalid.

            bool has_line_break = false;
            do {
                if (js[whitespace_comment_i++] == '\n') {
                    has_line_break = true;
                    break;
                }
            } while (whitespace_comment_i < i);

            if (has_line_break) {
                // In JavaScript, `\n` can end a statement similar to `;`. We only remove `\n` when we are
                // sure that it neither ends a statement nor is required as a whitespace between keywords or
                // identifiers. To keep this minifier simple, we accept to miss some occasions were `\n` can
                // be removed.
                //
                // Replacing `\n` between keywords and identifiers by `;` or ` ` would be quite difficult and
                // just cosmetic, therefore we preserve these newlines. How to replace depends not only on
                // the keyword but also on its context. For example, `await` is only a keyword in async
                // functions, `get` and `set` are keywords only in object definition blocks, and `async` and
                // access modifiers can be used as variables inside function blocks. Additionally we would
                // need to consider backward and forward compatibility with different JavaScript versions and
                // with TypeScript.

                const char trim_newline_after[] = ".([{;=*-+^!~?:,><-+/|&";
                if (strchr(trim_newline_after, m.result[result_length - 1]) != NULL) {
                    continue;
                }

                // Standalone lines may start with: +-~!"'`/ and more

                const char trim_newline_before[] = ")]}.;=*^?:,><|&";
                if (strchr(trim_newline_before, js[i]) == NULL) {
                    m.result[result_length++] = '\n';
                }
            }
            else {
                // Minifying a smaller-than comparison with a specific regex may produce a `</script` tag that
                // breaks inline JavaScript in HTML. The HTML minifier could recognize such cases but does
                // not know how to escape it correctly. It is nontrivial to distinguish such a smaller-than
                // regex from a `</script` tag created by merging strings. Therefore this is the right place
                // to handle the issue.

                const char trim_space_around[] = ".()[]{},=*;?!:><-+'\"/|&`";
                if ((strchr(trim_space_around, js[i]) == NULL &&
                     strchr(trim_space_around,
                         m.result[result_length - 1]) == NULL) ||
                    (m.result[result_length - 1] == '<' &&
                     !strnicmp(&js[i], "/script",
                         sizeof "/script" - 1)))
                {
                    m.result[result_length++] = ' ';
                }
            }
            continue;
        }
        m.result[result_length++] = js[i];
        i += 1;
    }
    if (round_nesting_level != 0) {
        m.error_position = last_open_round_bracket_i;
        snprintf(m.error, sizeof m.error, "Unclosed round bracket in line %%zu, column %%zu\n");
        goto error;
    }
    if (curly_nesting_level != 1) {
        m.error_position = last_open_curly_bracket_i;
        snprintf(m.error, sizeof m.error, "Unclosed curly bracket in line %%zu, column %%zu\n");
        goto error;
    }
    free(round_blocks);
    free(curly_blocks);
    return m;

error:
    free(round_blocks);
    free(curly_blocks);
    free(m.result);
    m.result = NULL;
    return m;
}

struct Minification minify_js_with_options(const char *js)
{
    struct Minification m = minify_js(js);

    if (m.result != NULL && option_mangle_js_identifiers) {
        struct Minification mangled = mangle_js_identifiers(m.result, false);
        free(m.result);
        m = mangled;
    }

    return m;
}

struct Minification minify_js_module_with_options(const char *js)
{
    struct Minification m = minify_js(js);

    if (m.result != NULL && option_mangle_js_identifiers) {
        const char *module_marker = "import\"\";";
        size_t module_marker_length = strlen(module_marker);
        size_t result_length = strlen(m.result);
        char *module_js = malloc(module_marker_length + result_length + 1);
        if (module_js == NULL) {
            free(m.result);
            m.result = NULL;
            snprintf(m.error, sizeof m.error, "Cannot allocate memory\n");
            return m;
        }
        memcpy(module_js, module_marker, module_marker_length);
        memcpy(&module_js[module_marker_length], m.result, result_length + 1);

        struct Minification mangled = mangle_js_identifiers(module_js, false);
        free(module_js);
        free(m.result);
        m = mangled;
        if (m.result != NULL) {
            memmove(m.result, &m.result[module_marker_length],
                strlen(&m.result[module_marker_length]) + 1);
        }
    }

    return m;
}
