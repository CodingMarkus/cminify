#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

static char *file_get_content(const char *filename)
{
    FILE *fp;
    if (filename[0] == '-' && filename[1] == '\0') {
        fp = stdin;
    }
    else {
        fp = fopen(filename, "r");
        if (fp == NULL) {
            return NULL;
        }
    }
    size_t buffer_size = BUFSIZ + 1;
    char *buffer = malloc(buffer_size);
    if (buffer == NULL) {
        fclose(fp);
        return NULL;
    }
    size_t read = 0;
    char *larger_buffer;
    do {
        read += fread(&buffer[read], 1, buffer_size - read - 1, fp);
        if (ferror(fp) != 0) {
            free(buffer);
            fclose(fp);
            return NULL;
        }
        if (feof(fp) != 0) {
            break;
        }
        buffer_size += BUFSIZ;
        larger_buffer = realloc(buffer, buffer_size);
        if (larger_buffer == NULL) {
            free(buffer);
            fclose(fp);
            return NULL;
        }
        buffer = larger_buffer;
    } while (true);
    buffer[read] = '\0';
    return buffer;
}

static bool is_whitespace(const char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

struct Minification
{
    char *result;
    char error[256];
    size_t error_position;
};

enum CommentVariant {COMMENT_VARIANT_CSS, COMMENT_VARIANT_JS};

static bool option_mangle_js_identifiers = false;

static bool skip_whitespaces_comments(struct Minification *m, const char *input, size_t *i, char *min,
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

struct JsMangleEdit
{
    size_t start;
    size_t length;
    const char *replacement;
    char replacement_storage[32];
    size_t replacement_length;
    bool replacement_dynamic;
};

struct JsMangleBinding
{
    const char *name;
    size_t length;
    struct JsMangleNameIndex *name_index;
    char replacement[16];
    size_t replacement_length;
    bool import_bare;
};

struct JsMangleMap
{
    const char *name;
    size_t length;
    struct JsMangleNameIndex *name_index;
    char replacement[16];
    size_t replacement_length;
    size_t scope_start;
    size_t scope_end;
    size_t visible_generation;
    struct JsMangleMap *next_for_name;
};

struct JsMangleRange
{
    size_t start;
    size_t end;
};

enum JsMangleTokenKind
{
    JS_MANGLE_TOKEN_IDENTIFIER,
    JS_MANGLE_TOKEN_PUNCTUATION,
    JS_MANGLE_TOKEN_STRING,
    JS_MANGLE_TOKEN_REGEX,
};

struct JsMangleNameIndex;

struct JsMangleToken
{
    enum JsMangleTokenKind kind;
    size_t start;
    size_t length;
    char punctuation;
    struct JsMangleNameIndex *name_index;
    size_t enclosing_open;
    size_t matching_token;
};

struct JsMangleScope
{
    size_t parent;
    size_t token_start;
    size_t token_end;
};

struct JsMangleDeclaration
{
    size_t token;
    size_t scope;
};

struct JsMangleReference
{
    size_t token;
    size_t scope;
};

struct JsMangleNameIndex
{
    const char *name;
    size_t length;
    size_t occurrences_start;
    size_t occurrences_length;
    size_t visible_generation;
    struct JsMangleBinding *binding;
    struct JsMangleMap *first_map;
};

struct JsMangleProgram
{
    struct JsMangleToken *tokens;
    size_t tokens_length;
    size_t tokens_capacity;

    struct JsMangleScope *scopes;
    size_t scopes_length;
    size_t scopes_capacity;

    struct JsMangleDeclaration *declarations;
    size_t declarations_length;
    size_t declarations_capacity;

    struct JsMangleReference *references;
    size_t references_length;
    size_t references_capacity;

    struct JsMangleNameIndex *name_indices;
    size_t name_indices_capacity;
    size_t *name_occurrences;
    size_t name_occurrences_capacity;
    size_t visible_generation;
};

struct JsMangleCounts
{
    size_t tokens;
    size_t identifiers;
    size_t scopes;
};

struct JsMangleState
{
    struct JsMangleEdit *edits;
    size_t edits_length;
    size_t edits_capacity;

    struct JsMangleMap *maps;
    size_t maps_length;
    size_t maps_capacity;

    struct JsMangleRange *unsafe_ranges;
    size_t unsafe_ranges_length;
    size_t unsafe_ranges_capacity;
};

static const char *js_mangle_failure_reason = NULL;

static void js_mangle_previous_next(const char *js, size_t start, size_t end,
    size_t word_start, size_t word_end, char *previous, char *next);

static bool js_identifier_start(char c)
{
    return c == '_' || c == '$' ||
        c >= 'a' && c <= 'z' ||
        c >= 'A' && c <= 'Z';
}

static bool js_identifier_part(char c)
{
    return js_identifier_start(c) || c >= '0' && c <= '9';
}

static bool js_word_equals(const char *js, size_t i, const char *word)
{
    size_t length = strlen(word);
    return !strncmp(&js[i], word, length) &&
        !js_identifier_part(js[i + length]);
}

static bool js_keyword(const char *name, size_t length)
{
    static const char *keywords[] = {
        "break", "case", "catch", "class", "const", "continue", "debugger",
        "default", "delete", "do", "else", "export", "extends", "false",
        "finally", "for", "function", "if", "import", "in", "instanceof",
        "let", "new", "null", "return", "super", "switch", "this", "throw",
        "true", "try", "typeof", "var", "void", "while", "with", "yield"
    };
    for (size_t i = 0; i < sizeof keywords / sizeof keywords[0]; ++i) {
        if (strlen(keywords[i]) == length &&
            !strncmp(name, keywords[i], length))
        {
            return true;
        }
    }
    return false;
}

static size_t js_declaration_keyword_length(const char *js, size_t i)
{
    if (js_word_equals(js, i, "var") || js_word_equals(js, i, "let")) {
        return 3;
    }
    if (js_word_equals(js, i, "const")) {
        return 5;
    }
    return 0;
}

static size_t js_skip_quoted(const char *js, size_t i, size_t end)
{
    char quote = js[i++];
    bool active_backslash = false;
    while (i < end) {
        if (!active_backslash && js[i] == quote) {
            return i + 1;
        }
        active_backslash = js[i++] == '\\' && !active_backslash;
    }
    return end;
}

static size_t js_skip_regex(const char *js, size_t i, size_t end)
{
    bool active_backslash = false;
    bool in_brackets = false;
    i += 1;
    while (i < end) {
        if (!active_backslash && !in_brackets && js[i] == '/') {
            return i + 1;
        }
        if (!active_backslash && js[i] == '[') {
            in_brackets = true;
        }
        else if (!active_backslash && js[i] == ']') {
            in_brackets = false;
        }
        active_backslash = js[i++] == '\\' && !active_backslash;
    }
    return end;
}

static size_t js_skip_comment(const char *js, size_t i, size_t end)
{
    if (js[i + 1] == '*') {
        i += 2;
        while (i < end && (js[i] != '*' || js[i + 1] != '/')) {
            i += 1;
        }
        return i < end ? i + 2 : end;
    }
    i += 2;
    while (i < end && js[i] != '\n') {
        i += 1;
    }
    return i;
}

static bool js_regex_start(const char *js, size_t start, size_t i)
{
    size_t k = i;
    while (k > start && is_whitespace(js[k - 1])) {
        k -= 1;
    }
    if (k == start || strchr("^!&|([{><+-*%:?~,;=", js[k - 1]) != NULL) {
        return true;
    }
    if (!js_identifier_part(js[k - 1])) {
        return false;
    }
    size_t word_start = k - 1;
    while (word_start > start && js_identifier_part(js[word_start - 1])) {
        word_start -= 1;
    }
    return js_word_equals(js, word_start, "return") ||
        js_word_equals(js, word_start, "throw") ||
        js_word_equals(js, word_start, "case") ||
        js_word_equals(js, word_start, "delete") ||
        js_word_equals(js, word_start, "typeof") ||
        js_word_equals(js, word_start, "void");
}

static size_t js_find_matching(const char *js, size_t i, size_t end,
    char open, char close)
{
    size_t nesting = 1;
    for (i += 1; i < end; ++i) {
        if (js[i] == '"' || js[i] == '\'' || js[i] == '`') {
            i = js_skip_quoted(js, i, end) - 1;
        }
        else if (js[i] == '/' && (js[i + 1] == '*' || js[i + 1] == '/')) {
            i = js_skip_comment(js, i, end) - 1;
        }
        else if (js[i] == '/' && js[i + 1] != '/' && js[i + 1] != '*' &&
            js_regex_start(js, 0, i))
        {
            i = js_skip_regex(js, i, end) - 1;
        }
        else if (js[i] == open) {
            nesting += 1;
        }
        else if (js[i] == close && --nesting == 0) {
            return i;
        }
    }
    return end;
}

static bool js_mangle_add_token(struct JsMangleProgram *program,
    enum JsMangleTokenKind kind, size_t start, size_t length,
    char punctuation)
{
    if (program->tokens_length >= program->tokens_capacity) {
        return false;
    }
    program->tokens[program->tokens_length++] =
        (struct JsMangleToken) {
            kind, start, length, punctuation, NULL, SIZE_MAX, SIZE_MAX
        };
    return true;
}

static bool js_mangle_tokenize(const char *js, size_t end,
    struct JsMangleProgram *program)
{
    for (size_t i = 0; i < end; ++i) {
        if (is_whitespace(js[i])) {
            continue;
        }
        if (js[i] == '"' || js[i] == '\'' || js[i] == '`') {
            size_t token_start = i;
            i = js_skip_quoted(js, i, end) - 1;
            if (!js_mangle_add_token(program, JS_MANGLE_TOKEN_STRING,
                token_start, i - token_start + 1, '\0'))
            {
                return false;
            }
        }
        else if (js[i] == '/' && (js[i + 1] == '*' || js[i + 1] == '/')) {
            i = js_skip_comment(js, i, end) - 1;
        }
        else if (js[i] == '/' && js_regex_start(js, 0, i)) {
            size_t token_start = i;
            i = js_skip_regex(js, i, end) - 1;
            if (!js_mangle_add_token(program, JS_MANGLE_TOKEN_REGEX,
                token_start, i - token_start + 1, '\0'))
            {
                return false;
            }
        }
        else if (js_identifier_start(js[i])) {
            size_t token_start = i;
            while (js_identifier_part(js[i])) {
                i += 1;
            }
            if (!js_mangle_add_token(program, JS_MANGLE_TOKEN_IDENTIFIER,
                token_start, i - token_start, '\0'))
            {
                return false;
            }
            i -= 1;
        }
        else {
            if (!js_mangle_add_token(program, JS_MANGLE_TOKEN_PUNCTUATION,
                i, 1, js[i]))
            {
                return false;
            }
        }
    }
    return true;
}

static void js_mangle_count_program(const char *js, size_t end,
    struct JsMangleCounts *counts)
{
    counts->scopes = 1;
    for (size_t i = 0; i < end; ++i) {
        if (is_whitespace(js[i])) {
            continue;
        }
        counts->tokens += 1;
        if (js[i] == '"' || js[i] == '\'' || js[i] == '`') {
            i = js_skip_quoted(js, i, end) - 1;
        }
        else if (js[i] == '/' && (js[i + 1] == '*' || js[i + 1] == '/')) {
            counts->tokens -= 1;
            i = js_skip_comment(js, i, end) - 1;
        }
        else if (js[i] == '/' && js_regex_start(js, 0, i)) {
            i = js_skip_regex(js, i, end) - 1;
        }
        else if (js_identifier_start(js[i])) {
            counts->identifiers += 1;
            while (js_identifier_part(js[i])) {
                i += 1;
            }
            i -= 1;
        }
        else if (js[i] == '{') {
            counts->scopes += 1;
        }
    }
}

static bool js_mangle_alloc_program(struct JsMangleProgram *program,
    struct JsMangleCounts counts)
{
    program->tokens_capacity = counts.tokens;
    program->scopes_capacity = counts.scopes;
    program->declarations_capacity = counts.identifiers;
    program->references_capacity = counts.identifiers;
    program->name_indices_capacity = counts.identifiers * 2 + 1;
    program->name_occurrences_capacity = counts.identifiers;

    if (program->tokens_capacity != 0) {
        program->tokens = malloc(
            program->tokens_capacity * sizeof *program->tokens);
    }
    if (program->scopes_capacity != 0) {
        program->scopes = malloc(
            program->scopes_capacity * sizeof *program->scopes);
    }
    if (program->declarations_capacity != 0) {
        program->declarations = malloc(
            program->declarations_capacity * sizeof *program->declarations);
    }
    if (program->references_capacity != 0) {
        program->references = malloc(
            program->references_capacity * sizeof *program->references);
    }
    if (program->name_indices_capacity != 0) {
        program->name_indices = calloc(program->name_indices_capacity,
            sizeof *program->name_indices);
    }
    if (program->name_occurrences_capacity != 0) {
        program->name_occurrences = malloc(
            program->name_occurrences_capacity *
            sizeof *program->name_occurrences);
    }
    return (program->tokens_capacity == 0 || program->tokens != NULL) &&
        (program->scopes_capacity == 0 || program->scopes != NULL) &&
        (
            program->declarations_capacity == 0 ||
            program->declarations != NULL
        ) &&
        (
            program->references_capacity == 0 ||
            program->references != NULL
        ) &&
        (
            program->name_indices_capacity == 0 ||
            program->name_indices != NULL
        ) &&
        (
            program->name_occurrences_capacity == 0 ||
            program->name_occurrences != NULL
        );
}

static bool js_mangle_alloc_state(struct JsMangleState *state,
    struct JsMangleCounts counts)
{
    state->edits_capacity = counts.identifiers * 2 + 1;
    state->maps_capacity = counts.identifiers + 1;
    state->unsafe_ranges_capacity = counts.identifiers + 1;

    state->edits = malloc(state->edits_capacity * sizeof *state->edits);
    state->maps = malloc(state->maps_capacity * sizeof *state->maps);
    state->unsafe_ranges = malloc(
        state->unsafe_ranges_capacity * sizeof *state->unsafe_ranges);
    return state->edits != NULL && state->maps != NULL &&
        state->unsafe_ranges != NULL;
}

static size_t js_mangle_count_identifiers(const char *js, size_t start,
    size_t end)
{
    size_t count = 0;
    for (size_t i = start; i < end; ++i) {
        if (js[i] == '"' || js[i] == '\'' || js[i] == '`') {
            i = js_skip_quoted(js, i, end) - 1;
        }
        else if (js[i] == '/' && (js[i + 1] == '*' || js[i + 1] == '/')) {
            i = js_skip_comment(js, i, end) - 1;
        }
        else if (js[i] == '/' && js_regex_start(js, start, i))
        {
            i = js_skip_regex(js, i, end) - 1;
        }
        else if (js_identifier_start(js[i])) {
            count += 1;
            while (js_identifier_part(js[i])) {
                i += 1;
            }
            i -= 1;
        }
    }
    return count;
}

static bool js_mangle_alloc_bindings(struct JsMangleBinding **bindings,
    size_t *capacity, size_t count)
{
    *capacity = count;
    if (*capacity == 0) {
        *bindings = NULL;
        return true;
    }
    *bindings = malloc(*capacity * sizeof **bindings);
    return *bindings != NULL;
}

static bool js_mangle_add_scope(struct JsMangleProgram *program,
    size_t parent, size_t token_start)
{
    if (program->scopes_length >= program->scopes_capacity) {
        return false;
    }
    program->scopes[program->scopes_length++] =
        (struct JsMangleScope) {parent, token_start, token_start};
    return true;
}

static bool js_mangle_add_declaration(struct JsMangleProgram *program,
    size_t token, size_t scope)
{
    if (program->declarations_length >= program->declarations_capacity) {
        return false;
    }
    program->declarations[program->declarations_length++] =
        (struct JsMangleDeclaration) {token, scope};
    return true;
}

static bool js_mangle_add_reference(struct JsMangleProgram *program,
    size_t token, size_t scope)
{
    if (program->references_length >= program->references_capacity) {
        return false;
    }
    program->references[program->references_length++] =
        (struct JsMangleReference) {token, scope};
    return true;
}

static bool js_mangle_token_equals(const char *js,
    const struct JsMangleToken *token, const char *word)
{
    size_t length = strlen(word);
    return token->kind == JS_MANGLE_TOKEN_IDENTIFIER &&
        token->length == length && !strncmp(&js[token->start], word, length);
}

static size_t js_mangle_name_hash(const char *name, size_t length)
{
    size_t hash = 1469598103934665603ull;
    for (size_t i = 0; i < length; ++i) {
        hash ^= (unsigned char) name[i];
        hash *= 1099511628211ull;
    }
    return hash;
}

static struct JsMangleNameIndex *js_mangle_find_name_index(
    struct JsMangleProgram *program, const char *name, size_t length)
{
    if (program->name_indices_capacity == 0) {
        return NULL;
    }
    size_t i = js_mangle_name_hash(name, length) %
        program->name_indices_capacity;
    for (size_t probes = 0; probes < program->name_indices_capacity;
        ++probes)
    {
        struct JsMangleNameIndex *index = &program->name_indices[i];
        if (index->name == NULL) {
            return index;
        }
        if (index->length == length &&
            !strncmp(index->name, name, length))
        {
            return index;
        }
        i += 1;
        if (i == program->name_indices_capacity) {
            i = 0;
        }
    }
    return NULL;
}

static bool js_mangle_build_name_index(struct JsMangleProgram *program,
    const char *js)
{
    for (size_t i = 0; i < program->tokens_length; ++i) {
        struct JsMangleToken *token = &program->tokens[i];
        if (token->kind != JS_MANGLE_TOKEN_IDENTIFIER) {
            continue;
        }
        struct JsMangleNameIndex *index = js_mangle_find_name_index(program,
            &js[token->start], token->length);
        if (index == NULL) {
            return false;
        }
        if (index->name == NULL) {
            index->name = &js[token->start];
            index->length = token->length;
        }
        index->occurrences_length += 1;
    }

    size_t occurrences_start = 0;
    for (size_t i = 0; i < program->name_indices_capacity; ++i) {
        struct JsMangleNameIndex *index = &program->name_indices[i];
        if (index->name == NULL) {
            continue;
        }
        index->occurrences_start = occurrences_start;
        occurrences_start += index->occurrences_length;
        index->occurrences_length = 0;
    }
    if (occurrences_start > program->name_occurrences_capacity) {
        return false;
    }

    for (size_t i = 0; i < program->tokens_length; ++i) {
        struct JsMangleToken *token = &program->tokens[i];
        if (token->kind != JS_MANGLE_TOKEN_IDENTIFIER) {
            continue;
        }
        struct JsMangleNameIndex *index = js_mangle_find_name_index(program,
            &js[token->start], token->length);
        if (index == NULL || index->name == NULL) {
            return false;
        }
        token->name_index = index;
        program->name_occurrences[
            index->occurrences_start + index->occurrences_length++] = i;
    }
    return true;
}

static bool js_mangle_matching_punctuation(char open, char close)
{
    return (open == '{' && close == '}') ||
        (open == '(' && close == ')') ||
        (open == '[' && close == ']');
}

static bool js_mangle_build_punctuation_index(
    struct JsMangleProgram *program)
{
    if (program->tokens_length == 0) {
        return true;
    }
    size_t *stack = malloc(program->tokens_length * sizeof *stack);
    if (stack == NULL) {
        return false;
    }
    size_t stack_length = 0;
    for (size_t i = 0; i < program->tokens_length; ++i) {
        struct JsMangleToken *token = &program->tokens[i];
        token->enclosing_open = stack_length == 0 ?
            SIZE_MAX : stack[stack_length - 1];
        if (token->kind != JS_MANGLE_TOKEN_PUNCTUATION) {
            continue;
        }
        if (token->punctuation == '{' || token->punctuation == '(' ||
            token->punctuation == '[')
        {
            stack[stack_length++] = i;
        }
        else if (token->punctuation == '}' || token->punctuation == ')' ||
            token->punctuation == ']')
        {
            while (stack_length > 0) {
                size_t open_i = stack[--stack_length];
                struct JsMangleToken *open = &program->tokens[open_i];
                if (js_mangle_matching_punctuation(open->punctuation,
                    token->punctuation))
                {
                    open->matching_token = i;
                    token->matching_token = open_i;
                    token->enclosing_open = stack_length == 0 ?
                        SIZE_MAX : stack[stack_length - 1];
                    break;
                }
            }
        }
    }
    free(stack);
    return true;
}

static size_t js_mangle_find_token_start(struct JsMangleProgram *program,
    size_t start)
{
    size_t left = 0;
    size_t right = program->tokens_length;
    while (left < right) {
        size_t middle = left + (right - left) / 2;
        if (program->tokens[middle].start < start) {
            left = middle + 1;
        }
        else {
            right = middle;
        }
    }
    return left;
}

static size_t js_mangle_find_occurrence_start(
    struct JsMangleProgram *program, struct JsMangleNameIndex *index,
    size_t start)
{
    size_t left = 0;
    size_t right = index->occurrences_length;
    while (left < right) {
        size_t middle = left + (right - left) / 2;
        size_t token_i = program->name_occurrences[
            index->occurrences_start + middle];
        if (program->tokens[token_i].start < start) {
            left = middle + 1;
        }
        else {
            right = middle;
        }
    }
    return left;
}

static bool js_mangle_build_program(const char *js, size_t end,
    struct JsMangleProgram *program)
{
    if (!js_mangle_tokenize(js, end, program)) {
        return false;
    }
    if (!js_mangle_build_punctuation_index(program)) {
        return false;
    }
    if (!js_mangle_build_name_index(program, js)) {
        return false;
    }
    if (!js_mangle_add_scope(program, 0, 0)) {
        return false;
    }

    size_t scope = 0;
    for (size_t i = 0; i < program->tokens_length; ++i) {
        struct JsMangleToken *token = &program->tokens[i];
        if (token->kind == JS_MANGLE_TOKEN_PUNCTUATION &&
            token->punctuation == '{')
        {
            if (!js_mangle_add_scope(program, scope, i)) {
                return false;
            }
            scope = program->scopes_length - 1;
        }
        else if (token->kind == JS_MANGLE_TOKEN_PUNCTUATION &&
            token->punctuation == '}' && scope != 0)
        {
            program->scopes[scope].token_end = i;
            scope = program->scopes[scope].parent;
        }
        else if (js_mangle_token_equals(js, token, "var") ||
            js_mangle_token_equals(js, token, "let") ||
            js_mangle_token_equals(js, token, "const"))
        {
            if (i + 1 < program->tokens_length &&
                program->tokens[i + 1].kind == JS_MANGLE_TOKEN_IDENTIFIER)
            {
                if (!js_mangle_add_declaration(program, i + 1, scope)) {
                    return false;
                }
            }
        }
        else if (token->kind == JS_MANGLE_TOKEN_IDENTIFIER &&
            !js_keyword(&js[token->start], token->length))
        {
            if (!js_mangle_add_reference(program, i, scope)) {
                return false;
            }
        }
    }
    program->scopes[0].token_end = program->tokens_length;
    return true;
}

static void js_mangle_free_program(struct JsMangleProgram *program)
{
    free(program->tokens);
    free(program->scopes);
    free(program->declarations);
    free(program->references);
    free(program->name_indices);
    free(program->name_occurrences);
}

static void js_mangle_rebase_edits(struct JsMangleState *state)
{
    for (size_t i = 0; i < state->edits_length; ++i) {
        if (!state->edits[i].replacement_dynamic) {
            state->edits[i].replacement =
                state->edits[i].replacement_storage;
        }
    }
}

static bool js_mangle_ensure_edit_capacity(struct JsMangleState *state)
{
    if (state->edits_length < state->edits_capacity) {
        return true;
    }
    size_t capacity = state->edits_capacity > 0 ?
        state->edits_capacity * 2 : 1;
    struct JsMangleEdit *edits = realloc(state->edits,
        capacity * sizeof *state->edits);
    if (edits == NULL) {
        return false;
    }
    state->edits = edits;
    state->edits_capacity = capacity;
    js_mangle_rebase_edits(state);
    return true;
}

static void js_mangle_free_edit(struct JsMangleEdit *edit)
{
    if (edit->replacement_dynamic) {
        free((void *) edit->replacement);
        edit->replacement = NULL;
        edit->replacement_dynamic = false;
    }
}

static void js_mangle_clear_edit(struct JsMangleEdit *edit)
{
    edit->replacement = NULL;
    edit->replacement_length = 0;
    edit->replacement_dynamic = false;
}

static bool js_mangle_set_edit_replacement(struct JsMangleEdit *edit,
    const char *replacement, size_t replacement_length)
{
    if (replacement_length < sizeof edit->replacement_storage) {
        edit->replacement = edit->replacement_storage;
        memcpy(edit->replacement_storage, replacement, replacement_length);
        edit->replacement_dynamic = false;
        return true;
    }
    char *dynamic_replacement = malloc(replacement_length);
    if (dynamic_replacement == NULL) {
        return false;
    }
    memcpy(dynamic_replacement, replacement, replacement_length);
    edit->replacement = dynamic_replacement;
    edit->replacement_dynamic = true;
    return true;
}

static void js_mangle_free_edits(struct JsMangleState *state)
{
    for (size_t i = 0; i < state->edits_length; ++i) {
        js_mangle_free_edit(&state->edits[i]);
    }
}

static bool js_mangle_add_edit(struct JsMangleState *state, size_t start,
    size_t length, const char *replacement, size_t replacement_length)
{
    if (!js_mangle_ensure_edit_capacity(state)) {
        return false;
    }
    struct JsMangleEdit *edit = &state->edits[state->edits_length++];
    edit->start = start;
    edit->length = length;
    edit->replacement_length = replacement_length;
    if (!js_mangle_set_edit_replacement(edit, replacement,
        replacement_length))
    {
        state->edits_length -= 1;
        return false;
    }
    return true;
}

static bool js_mangle_add_shorthand_edit(struct JsMangleState *state,
    size_t start, size_t length, const char *replacement,
    size_t replacement_length)
{
    if (!js_mangle_ensure_edit_capacity(state)) {
        return false;
    }
    struct JsMangleEdit *edit = &state->edits[state->edits_length++];
    size_t edit_replacement_length = replacement_length * 2 + 1;
    char *replacement_buffer = malloc(edit_replacement_length);
    if (replacement_buffer == NULL) {
        state->edits_length -= 1;
        return false;
    }
    edit->start = start;
    edit->length = length;
    memcpy(replacement_buffer, replacement, replacement_length);
    replacement_buffer[replacement_length] = ':';
    memcpy(&replacement_buffer[replacement_length + 1], replacement,
        replacement_length);
    edit->replacement_length = edit_replacement_length;
    if (!js_mangle_set_edit_replacement(edit, replacement_buffer,
        edit_replacement_length))
    {
        free(replacement_buffer);
        state->edits_length -= 1;
        return false;
    }
    free(replacement_buffer);
    return true;
}

static bool js_mangle_add_import_alias_edit(struct JsMangleState *state,
    size_t start, const char *original, size_t length,
    const char *replacement, size_t replacement_length)
{
    const char as[] = " as ";
    if (!js_mangle_ensure_edit_capacity(state)) {
        return false;
    }
    struct JsMangleEdit *edit = &state->edits[state->edits_length++];
    size_t edit_replacement_length = length + sizeof as - 1 +
        replacement_length;
    char *replacement_buffer = malloc(edit_replacement_length);
    if (replacement_buffer == NULL) {
        state->edits_length -= 1;
        return false;
    }
    edit->start = start;
    edit->length = length;
    memcpy(replacement_buffer, original, length);
    memcpy(&replacement_buffer[length], as, sizeof as - 1);
    memcpy(&replacement_buffer[length + sizeof as - 1], replacement,
        replacement_length);
    edit->replacement = replacement_buffer;
    edit->replacement_length = edit_replacement_length;
    edit->replacement_dynamic = true;
    return true;
}

static bool js_mangle_add_pattern_alias_edit(struct JsMangleState *state,
    size_t start, const char *original, size_t length,
    const char *replacement, size_t replacement_length)
{
    if (!js_mangle_ensure_edit_capacity(state)) {
        return false;
    }
    struct JsMangleEdit *edit = &state->edits[state->edits_length++];
    size_t edit_replacement_length = length + 1 + replacement_length;
    char *replacement_buffer = malloc(edit_replacement_length);
    if (replacement_buffer == NULL) {
        state->edits_length -= 1;
        return false;
    }
    edit->start = start;
    edit->length = length;
    memcpy(replacement_buffer, original, length);
    replacement_buffer[length] = ':';
    memcpy(&replacement_buffer[length + 1], replacement,
        replacement_length);
    edit->replacement = replacement_buffer;
    edit->replacement_length = edit_replacement_length;
    edit->replacement_dynamic = true;
    return true;
}

static int js_mangle_compare_edits(const void *a, const void *b)
{
    const struct JsMangleEdit *edit_a = a;
    const struct JsMangleEdit *edit_b = b;
    return (edit_a->start > edit_b->start) - (edit_a->start < edit_b->start);
}

static bool js_mangle_add_map(struct JsMangleProgram *program,
    struct JsMangleState *state, struct JsMangleBinding binding,
    size_t scope_start, size_t scope_end)
{
    if (state->maps_length >= state->maps_capacity) {
        return false;
    }
    struct JsMangleNameIndex *index = js_mangle_find_name_index(program,
        binding.name, binding.length);
    struct JsMangleMap *map = &state->maps[state->maps_length++];
    *map = (struct JsMangleMap) {
        binding.name, binding.length, index, "", binding.replacement_length,
        scope_start, scope_end, 0, NULL
    };
    memcpy(map->replacement, binding.replacement,
        binding.replacement_length + 1);
    if (index != NULL) {
        map->next_for_name = index->first_map;
        index->first_map = map;
    }
    return true;
}

static bool js_mangle_add_unsafe_range(struct JsMangleState *state,
    size_t start, size_t end)
{
    if (state->unsafe_ranges_length >= state->unsafe_ranges_capacity) {
        return false;
    }
    state->unsafe_ranges[state->unsafe_ranges_length++] =
        (struct JsMangleRange) {start, end};
    return true;
}

static bool js_mangle_in_unsafe_range(struct JsMangleState *state, size_t i)
{
    for (size_t r = 0; r < state->unsafe_ranges_length; ++r) {
        if (i > state->unsafe_ranges[r].start &&
            i < state->unsafe_ranges[r].end)
        {
            return true;
        }
    }
    return false;
}

static bool js_mangle_same_name(const char *a, size_t a_length,
    const char *b, size_t b_length)
{
    return a_length == b_length && !strncmp(a, b, a_length);
}

static bool js_mangle_add_binding(struct JsMangleBinding **bindings,
    size_t *bindings_length, size_t *bindings_capacity, const char *name,
    size_t length)
{
    if (length <= 1 || js_keyword(name, length)) {
        return true;
    }
    for (size_t i = 0; i < *bindings_length; ++i) {
        if (js_mangle_same_name((*bindings)[i].name, (*bindings)[i].length,
            name, length))
        {
            return true;
        }
    }
    if (*bindings_length >= *bindings_capacity) {
        return false;
    }
    (*bindings)[*bindings_length] =
        (struct JsMangleBinding) {name, length, NULL, "", 0, false};
    *bindings_length += 1;
    return true;
}

static bool js_mangle_add_import_binding(struct JsMangleBinding **bindings,
    size_t *bindings_length, size_t *bindings_capacity, const char *name,
    size_t length, bool import_bare)
{
    if (!js_mangle_add_binding(bindings, bindings_length, bindings_capacity,
        name, length))
    {
        return false;
    }
    for (size_t i = 0; i < *bindings_length; ++i) {
        if (js_mangle_same_name((*bindings)[i].name, (*bindings)[i].length,
            name, length))
        {
            (*bindings)[i].import_bare = import_bare;
            break;
        }
    }
    return true;
}

static bool js_mangle_binding_list_contains(struct JsMangleBinding *bindings,
    size_t bindings_length, const char *name, size_t length)
{
    for (size_t i = 0; i < bindings_length; ++i) {
        if (js_mangle_same_name(bindings[i].name, bindings[i].length, name,
            length))
        {
            return true;
        }
    }
    return false;
}

static bool js_mangle_add_global_binding(struct JsMangleBinding **bindings,
    size_t *bindings_length, size_t *bindings_capacity,
    struct JsMangleBinding *excluded, size_t excluded_length,
    const char *name, size_t length)
{
    if (js_mangle_binding_list_contains(excluded, excluded_length, name,
        length))
    {
        return true;
    }
    return js_mangle_add_binding(bindings, bindings_length,
        bindings_capacity, name, length);
}

static bool js_mangle_add_global_import_binding(
    struct JsMangleBinding **bindings, size_t *bindings_length,
    size_t *bindings_capacity, struct JsMangleBinding *excluded,
    size_t excluded_length, const char *name, size_t length, bool import_bare)
{
    if (js_mangle_binding_list_contains(excluded, excluded_length, name,
        length))
    {
        return true;
    }
    return js_mangle_add_import_binding(bindings, bindings_length,
        bindings_capacity, name, length, import_bare);
}

static bool js_mangle_collect_pattern_bindings(const char *js, size_t start,
    size_t end, struct JsMangleBinding **bindings, size_t *bindings_length,
    size_t *bindings_capacity)
{
    char containers[64];
    size_t containers_length = 0;

    for (size_t i = start; i < end; ++i) {
        if (js[i] == '"' || js[i] == '\'' || js[i] == '`') {
            i = js_skip_quoted(js, i, end) - 1;
        }
        else if ((js[i] == '[' || js[i] == '{') &&
            containers_length < sizeof containers)
        {
            containers[containers_length++] = js[i];
        }
        else if ((js[i] == ']' || js[i] == '}') && containers_length > 0) {
            containers_length -= 1;
        }
        else if (containers_length > 0 && js_identifier_start(js[i])) {
            size_t word_start = i;
            while (js_identifier_part(js[i])) {
                i += 1;
            }
            char previous, next;
            js_mangle_previous_next(js, start, end, word_start, i, &previous,
                &next);
            bool rest_element = word_start >= start + 3 &&
                js[word_start - 1] == '.' && js[word_start - 2] == '.' &&
                js[word_start - 3] == '.';
            bool binding =
                containers[containers_length - 1] == '[' &&
                (previous == '[' || previous == ',' || rest_element);
            binding = binding ||
                containers[containers_length - 1] == '{' &&
                (previous == ':' || previous == '{' || previous == ',' ||
                rest_element);
            if (binding && next != ':') {
                if (!js_mangle_add_binding(bindings, bindings_length,
                    bindings_capacity, &js[word_start], i - word_start))
                {
                    return false;
                }
            }
            i -= 1;
        }
    }
    return true;
}

static bool js_mangle_collect_global_pattern_bindings(const char *js,
    size_t start, size_t end, struct JsMangleBinding **bindings,
    size_t *bindings_length, size_t *bindings_capacity,
    struct JsMangleBinding *excluded, size_t excluded_length)
{
    struct JsMangleBinding *pattern_bindings = NULL;
    size_t pattern_bindings_length = 0;
    size_t pattern_bindings_capacity = 0;
    bool success = false;

    if (!js_mangle_alloc_bindings(&pattern_bindings,
        &pattern_bindings_capacity, js_mangle_count_identifiers(js, start,
        end)))
    {
        goto done;
    }
    if (!js_mangle_collect_pattern_bindings(js, start, end,
        &pattern_bindings, &pattern_bindings_length,
        &pattern_bindings_capacity))
    {
        goto done;
    }
    for (size_t i = 0; i < pattern_bindings_length; ++i) {
        if (!js_mangle_add_global_binding(bindings, bindings_length,
            bindings_capacity, excluded, excluded_length,
            pattern_bindings[i].name, pattern_bindings[i].length))
        {
            goto done;
        }
    }
    success = true;

done:
    free(pattern_bindings);
    return success;
}

static size_t js_mangle_name_count(struct JsMangleProgram *program,
    const char *js, size_t start, size_t end, const char *name,
    size_t length)
{
    struct JsMangleNameIndex *index = js_mangle_find_name_index(program,
        name, length);
    if (index == NULL || index->name == NULL) {
        return 0;
    }
    size_t count = 0;
    for (size_t i = js_mangle_find_occurrence_start(program, index, start);
        i < index->occurrences_length; ++i)
    {
        struct JsMangleToken *token = &program->tokens[
            program->name_occurrences[index->occurrences_start + i]];
        if (token->start >= end) {
            break;
        }
        if (token->start + token->length > end) {
            continue;
        }
        count += 1;
    }
    return count;
}

static bool js_mangle_name_used(struct JsMangleProgram *program,
    const char *js, size_t start, size_t end, const char *name,
    size_t length)
{
    return js_mangle_name_count(program, js, start, end, name, length) != 0;
}

static size_t js_mangle_collect_visible_maps(struct JsMangleProgram *program,
    const char *js, size_t start, size_t end, struct JsMangleState *state,
    struct JsMangleMap *visible_maps)
{
    size_t visible_maps_length = 0;
    size_t generation = ++program->visible_generation;
    for (size_t t = js_mangle_find_token_start(program, start);
        t < program->tokens_length; ++t)
    {
        struct JsMangleToken *token = &program->tokens[t];
        if (token->start >= end) {
            break;
        }
        if (token->kind != JS_MANGLE_TOKEN_IDENTIFIER ||
            token->start + token->length > end ||
            token->name_index == NULL)
        {
            continue;
        }
        if (token->name_index->visible_generation == generation) {
            continue;
        }
        token->name_index->visible_generation = generation;
        for (struct JsMangleMap *map = token->name_index->first_map;
            map != NULL; map = map->next_for_name)
        {
            if (map->visible_generation == generation ||
                map->scope_start > start || map->scope_end < end)
            {
                continue;
            }
            map->visible_generation = generation;
            visible_maps[visible_maps_length++] = *map;
        }
    }
    return visible_maps_length;
}

static bool js_mangle_replacement_visible(struct JsMangleMap *visible_maps,
    size_t visible_maps_length, const char *name, size_t length)
{
    for (size_t i = 0; i < visible_maps_length; ++i) {
        if (js_mangle_same_name(visible_maps[i].replacement,
            visible_maps[i].replacement_length, name, length))
        {
            return true;
        }
    }
    return false;
}

static void js_mangle_write_name_suffix(size_t index, size_t width,
    const char *alphabet, size_t alphabet_length, char *name, size_t *length)
{
    size_t start = *length;
    for (size_t i = 0; i < width; ++i) {
        name[start + width - i - 1] = alphabet[index % alphabet_length];
        index /= alphabet_length;
    }
    *length += width;
    name[*length] = '\0';
}

static void js_mangle_make_name(size_t index, char *name, size_t *length)
{
    const char alphabet[] =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    size_t alphabet_length = sizeof alphabet - 1;
    if (index < alphabet_length) {
        name[0] = alphabet[index];
        name[1] = '\0';
        *length = 1;
        return;
    }
    index -= alphabet_length;
    name[0] = '_';
    *length = 1;
    size_t width = 1;
    size_t group_size = alphabet_length;
    while (index >= group_size && *length + width < 15) {
        index -= group_size;
        width += 1;
        group_size *= alphabet_length;
    }
    js_mangle_write_name_suffix(index, width, alphabet, alphabet_length,
        name, length);
}

static void js_mangle_make_global_name(size_t index, char *name,
    size_t *length)
{
    const char alphabet[] =
        "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    size_t alphabet_length = sizeof alphabet - 1;
    name[0] = 'G';
    *length = 1;
    size_t width = 1;
    size_t group_size = alphabet_length;
    while (index >= group_size && *length + width < 15) {
        index -= group_size;
        width += 1;
        group_size *= alphabet_length;
    }
    js_mangle_write_name_suffix(index, width, alphabet, alphabet_length,
        name, length);
}

static void js_mangle_previous_next(const char *js, size_t start, size_t end,
    size_t word_start, size_t word_end, char *previous, char *next)
{
    size_t i = word_start;
    *previous = '\0';
    *next = '\0';
    while (i > start && is_whitespace(js[i - 1])) {
        i -= 1;
    }
    if (i > start) {
        *previous = js[i - 1];
    }
    i = word_end;
    while (i < end && is_whitespace(js[i])) {
        i += 1;
    }
    if (i < end) {
        *next = js[i];
    }
}

static bool js_mangle_previous_word_equals(const char *js, size_t start,
    size_t word_start, const char *word)
{
    size_t i = word_start;
    while (i > start && is_whitespace(js[i - 1])) {
        i -= 1;
    }
    if (i == start || !js_identifier_part(js[i - 1])) {
        return false;
    }
    size_t previous_end = i;
    while (i > start && js_identifier_part(js[i - 1])) {
        i -= 1;
    }
    return strlen(word) == previous_end - i &&
        !strncmp(&js[i], word, previous_end - i);
}

static bool js_mangle_function_unsafe(const char *js, size_t start, size_t end)
{
    for (size_t i = start; i < end; ++i) {
        if (js[i] == '"' || js[i] == '\'') {
            i = js_skip_quoted(js, i, end) - 1;
        }
        else if (js[i] == '/' && (js[i + 1] == '*' || js[i + 1] == '/')) {
            i = js_skip_comment(js, i, end) - 1;
        }
        else if (js[i] == '/' && js_regex_start(js, start, i))
        {
            i = js_skip_regex(js, i, end) - 1;
        }
        else if (js[i] == '`') {
            return true;
        }
        else if (js[i] == '=' && js[i + 1] == '>') {
            return true;
        }
        else if (js_identifier_start(js[i])) {
            size_t word_start = i;
            while (js_identifier_part(js[i])) {
                i += 1;
            }
            char previous, next;
            js_mangle_previous_next(js, start, end, word_start, i, &previous,
                &next);
            if (js_word_equals(js, word_start, "function")) {
                while (i < end && is_whitespace(js[i])) {
                    i += 1;
                }
                if (js[i] == '*') {
                    i += 1;
                    while (i < end && is_whitespace(js[i])) {
                        i += 1;
                    }
                }
                while (js_identifier_part(js[i])) {
                    i += 1;
                }
                while (i < end && is_whitespace(js[i])) {
                    i += 1;
                }
                if (js[i] == '(') {
                    size_t params_end = js_find_matching(js, i, end, '(', ')');
                    i = params_end + 1;
                    while (i < end && is_whitespace(js[i])) {
                        i += 1;
                    }
                    if (params_end < end && js[i] == '{') {
                        i = js_find_matching(js, i, end, '{', '}');
                    }
                }
                continue;
            }
            if (previous != '.' && js_word_equals(js, word_start, "with"))
            {
                return true;
            }
            if (previous != '.' && js_word_equals(js, word_start, "eval")) {
                size_t k = i;
                while (k < end && is_whitespace(js[k])) {
                    k += 1;
                }
                if (js[k] == '(') {
                    return true;
                }
            }
            i -= 1;
        }
    }
    return false;
}

static bool js_mangle_is_module(const char *js,
    struct JsMangleProgram *program)
{
    for (size_t i = 0; i < program->tokens_length; ++i) {
        if (js_mangle_token_equals(js, &program->tokens[i], "import") ||
            js_mangle_token_equals(js, &program->tokens[i], "export"))
        {
            return true;
        }
    }
    return false;
}

static char js_mangle_previous_punctuation(struct JsMangleProgram *program,
    size_t token_i, size_t min_start);

static bool js_mangle_top_level_unsafe(const char *js,
    struct JsMangleProgram *program)
{
    size_t curly_nesting = 0;
    size_t round_nesting = 0;
    size_t square_nesting = 0;

    for (size_t i = 0; i < program->tokens_length; ++i) {
        struct JsMangleToken *token = &program->tokens[i];
        if (token->kind == JS_MANGLE_TOKEN_PUNCTUATION &&
            token->punctuation == '{')
        {
            curly_nesting += 1;
        }
        else if (token->kind == JS_MANGLE_TOKEN_PUNCTUATION &&
            token->punctuation == '}')
        {
            curly_nesting -= curly_nesting > 0;
        }
        else if (token->kind == JS_MANGLE_TOKEN_PUNCTUATION &&
            token->punctuation == '(')
        {
            round_nesting += 1;
        }
        else if (token->kind == JS_MANGLE_TOKEN_PUNCTUATION &&
            token->punctuation == ')')
        {
            round_nesting -= round_nesting > 0;
        }
        else if (token->kind == JS_MANGLE_TOKEN_PUNCTUATION &&
            token->punctuation == '[')
        {
            square_nesting += 1;
        }
        else if (token->kind == JS_MANGLE_TOKEN_PUNCTUATION &&
            token->punctuation == ']')
        {
            square_nesting -= square_nesting > 0;
        }
        else if (curly_nesting == 0 && round_nesting == 0 &&
            square_nesting == 0)
        {
            char previous = js_mangle_previous_punctuation(program, i, 0);
            if (previous != '.' && js_mangle_token_equals(js, token, "with"))
            {
                return true;
            }
            if (previous != '.' && js_mangle_token_equals(js, token, "eval") &&
                i + 1 < program->tokens_length &&
                program->tokens[i + 1].kind == JS_MANGLE_TOKEN_PUNCTUATION &&
                program->tokens[i + 1].punctuation == '(')
            {
                return true;
            }
        }
    }
    return false;
}

static bool js_mangle_collect_params(const char *js, size_t start,
    size_t end, struct JsMangleBinding **bindings, size_t *bindings_length,
    size_t *bindings_capacity)
{
    bool expect_name = true;
    size_t nesting = 0;

    for (size_t i = start; i < end; ++i) {
        if (js[i] == '"' || js[i] == '\'' || js[i] == '`') {
            i = js_skip_quoted(js, i, end) - 1;
        }
        else if (nesting == 0 && expect_name &&
            (js[i] == '[' || js[i] == '{'))
        {
            size_t pattern_end = js_find_matching(js, i, end, js[i],
                js[i] == '[' ? ']' : '}');
            if (pattern_end >= end) {
                return true;
            }
            if (!js_mangle_collect_pattern_bindings(js, i, pattern_end + 1,
                bindings, bindings_length, bindings_capacity))
            {
                return false;
            }
            i = pattern_end;
            expect_name = false;
        }
        else if (js[i] == '(' || js[i] == '[' || js[i] == '{') {
            nesting += 1;
        }
        else if (js[i] == ')' || js[i] == ']' || js[i] == '}') {
            nesting -= nesting > 0;
        }
        else if (nesting == 0 && js[i] == ',') {
            expect_name = true;
        }
        else if (nesting == 0 && expect_name && js[i] == '.' &&
            js[i + 1] == '.' && js[i + 2] == '.')
        {
            i += 2;
        }
        else if (nesting == 0 && expect_name && js_identifier_start(js[i])) {
            size_t word_start = i;
            while (js_identifier_part(js[i])) {
                i += 1;
            }
            if (!js_mangle_add_binding(bindings, bindings_length,
                bindings_capacity, &js[word_start], i - word_start))
            {
                return false;
            }
            expect_name = false;
            i -= 1;
        }
    }
    return true;
}

static bool js_mangle_collect_export_clause(const char *js, size_t start,
    size_t end, struct JsMangleBinding **bindings, size_t *bindings_length,
    size_t *bindings_capacity)
{
    size_t close = js_find_matching(js, start, end, '{', '}');
    if (close >= end) {
        return true;
    }

    size_t i = close + 1;
    while (i < end && is_whitespace(js[i])) {
        i += 1;
    }
    if (js_word_equals(js, i, "from")) {
        return true;
    }

    for (i = start + 1; i < close; ++i) {
        while (i < close && !js_identifier_start(js[i])) {
            if (js[i] == '"' || js[i] == '\'') {
                i = js_skip_quoted(js, i, close) - 1;
            }
            i += 1;
        }
        if (i >= close) {
            break;
        }
        size_t word_start = i;
        while (js_identifier_part(js[i])) {
            i += 1;
        }
        if (js_word_equals(js, word_start, "as")) {
            continue;
        }
        if (!js_mangle_add_binding(bindings, bindings_length,
            bindings_capacity, &js[word_start], i - word_start))
        {
            return false;
        }
        while (i < close && js[i] != ',') {
            i += 1;
        }
    }
    return true;
}

static bool js_mangle_collect_exported_declarations(const char *js,
    size_t end, struct JsMangleBinding **bindings, size_t *bindings_length,
    size_t *bindings_capacity)
{
    size_t curly_nesting = 0;
    size_t round_nesting = 0;
    size_t square_nesting = 0;

    for (size_t i = 0; i < end; ++i) {
        if (js[i] == '"' || js[i] == '\'' || js[i] == '`') {
            i = js_skip_quoted(js, i, end) - 1;
        }
        else if (js[i] == '/' && (js[i + 1] == '*' || js[i + 1] == '/')) {
            i = js_skip_comment(js, i, end) - 1;
        }
        else if (js[i] == '/' && js_regex_start(js, 0, i)) {
            i = js_skip_regex(js, i, end) - 1;
        }
        else if (js[i] == '{') {
            curly_nesting += 1;
        }
        else if (js[i] == '}') {
            curly_nesting -= curly_nesting > 0;
        }
        else if (js[i] == '(') {
            round_nesting += 1;
        }
        else if (js[i] == ')') {
            round_nesting -= round_nesting > 0;
        }
        else if (js[i] == '[') {
            square_nesting += 1;
        }
        else if (js[i] == ']') {
            square_nesting -= square_nesting > 0;
        }
        else if (curly_nesting == 0 && round_nesting == 0 &&
            square_nesting == 0 && js_word_equals(js, i, "export"))
        {
            i += sizeof "export" - 1;
            while (i < end && is_whitespace(js[i])) {
                i += 1;
            }
            if (js_word_equals(js, i, "default")) {
                i += sizeof "default" - 1;
                while (i < end && is_whitespace(js[i])) {
                    i += 1;
                }
                if (js_identifier_start(js[i]) &&
                    !js_word_equals(js, i, "function") &&
                    !js_word_equals(js, i, "class"))
                {
                    size_t word_start = i;
                    while (js_identifier_part(js[i])) {
                        i += 1;
                    }
                    if (!js_mangle_add_binding(bindings, bindings_length,
                        bindings_capacity, &js[word_start], i - word_start))
                    {
                        return false;
                    }
                }
            }
            else if (js[i] == '{') {
                if (!js_mangle_collect_export_clause(js, i, end, bindings,
                    bindings_length, bindings_capacity))
                {
                    return false;
                }
                i = js_find_matching(js, i, end, '{', '}');
            }
            else if (js_word_equals(js, i, "function")) {
                i += sizeof "function" - 1;
                while (i < end && is_whitespace(js[i])) {
                    i += 1;
                }
                if (js[i] == '*') {
                    i += 1;
                    while (i < end && is_whitespace(js[i])) {
                        i += 1;
                    }
                }
                if (js_identifier_start(js[i])) {
                    size_t word_start = i;
                    while (js_identifier_part(js[i])) {
                        i += 1;
                    }
                    if (!js_mangle_add_binding(bindings, bindings_length,
                        bindings_capacity, &js[word_start], i - word_start))
                    {
                        return false;
                    }
                }
            }
            else if (js_word_equals(js, i, "var") ||
                js_word_equals(js, i, "let") ||
                js_word_equals(js, i, "const"))
            {
                i += js_declaration_keyword_length(js, i);
                while (i < end && is_whitespace(js[i])) {
                    i += 1;
                }
                int nesting = 0;
                bool expect_name = true;
                while (i < end && js[i] != ';') {
                    if (js[i] == '(' || js[i] == '[' || js[i] == '{') {
                        if (nesting == 0 && expect_name &&
                            (js[i] == '[' || js[i] == '{'))
                        {
                            size_t pattern_end = js_find_matching(js, i,
                                end, js[i], js[i] == '[' ? ']' : '}');
                            if (pattern_end >= end) {
                                break;
                            }
                            if (!js_mangle_collect_pattern_bindings(js, i,
                                pattern_end + 1, bindings, bindings_length,
                                bindings_capacity))
                            {
                                return false;
                            }
                            i = pattern_end;
                            expect_name = false;
                            continue;
                        }
                        nesting += 1;
                    }
                    else if (js[i] == ')' || js[i] == ']' || js[i] == '}') {
                        nesting -= 1;
                    }
                    else if (nesting == 0 && js[i] == ',') {
                        expect_name = true;
                    }
                    else if (expect_name && js_identifier_start(js[i])) {
                        size_t word_start = i;
                        while (js_identifier_part(js[i])) {
                            i += 1;
                        }
                        if (!js_mangle_add_binding(bindings, bindings_length,
                            bindings_capacity, &js[word_start],
                            i - word_start))
                        {
                            return false;
                        }
                        expect_name = false;
                        i -= 1;
                    }
                    i += 1;
                }
            }
        }
    }
    return true;
}

static bool js_mangle_collect_global_declarations(const char *js, size_t end,
    struct JsMangleBinding **bindings, size_t *bindings_length,
    size_t *bindings_capacity, struct JsMangleBinding *excluded,
    size_t excluded_length)
{
    size_t curly_nesting = 0;
    size_t round_nesting = 0;
    size_t square_nesting = 0;

    for (size_t i = 0; i < end; ++i) {
        if (js[i] == '"' || js[i] == '\'' || js[i] == '`') {
            i = js_skip_quoted(js, i, end) - 1;
        }
        else if (js[i] == '/' && (js[i + 1] == '*' || js[i + 1] == '/')) {
            i = js_skip_comment(js, i, end) - 1;
        }
        else if (js[i] == '/' && js_regex_start(js, 0, i)) {
            i = js_skip_regex(js, i, end) - 1;
        }
        else if (js[i] == '{') {
            curly_nesting += 1;
        }
        else if (js[i] == '}') {
            curly_nesting -= curly_nesting > 0;
        }
        else if (js[i] == '(') {
            round_nesting += 1;
        }
        else if (js[i] == ')') {
            round_nesting -= round_nesting > 0;
        }
        else if (js[i] == '[') {
            square_nesting += 1;
        }
        else if (js[i] == ']') {
            square_nesting -= square_nesting > 0;
        }
        else if (curly_nesting == 0 && round_nesting == 0 &&
            square_nesting == 0 && js_identifier_start(js[i]))
        {
            if (js_word_equals(js, i, "import")) {
                i += sizeof "import" - 1;
                while (i < end && is_whitespace(js[i])) {
                    i += 1;
                }
                if (js[i] == '"' || js[i] == '\'') {
                    i = js_skip_quoted(js, i, end) - 1;
                    continue;
                }
                if (js_identifier_start(js[i])) {
                    size_t word_start = i;
                    while (js_identifier_part(js[i])) {
                        i += 1;
                    }
                    if (!js_mangle_add_global_import_binding(bindings,
                        bindings_length, bindings_capacity, excluded,
                        excluded_length, &js[word_start], i - word_start,
                        false))
                    {
                        return false;
                    }
                    while (i < end && js[i] != ';' && js[i] != ',') {
                        i += 1;
                    }
                    if (js[i] != ',') {
                        continue;
                    }
                    i += 1;
                }
                while (i < end && is_whitespace(js[i])) {
                    i += 1;
                }
                if (js[i] == '*') {
                    i += 1;
                    while (i < end && is_whitespace(js[i])) {
                        i += 1;
                    }
                    if (js_word_equals(js, i, "as")) {
                        i += sizeof "as" - 1;
                    }
                    while (i < end && is_whitespace(js[i])) {
                        i += 1;
                    }
                    if (js_identifier_start(js[i])) {
                        size_t word_start = i;
                        while (js_identifier_part(js[i])) {
                            i += 1;
                        }
                        if (!js_mangle_add_global_import_binding(bindings,
                            bindings_length, bindings_capacity, excluded,
                            excluded_length, &js[word_start],
                            i - word_start, false))
                        {
                            return false;
                        }
                    }
                    continue;
                }
                if (js[i] == '{') {
                    i += 1;
                    while (i < end && js[i] != '}') {
                        while (i < end && !js_identifier_start(js[i]) &&
                            js[i] != '}')
                        {
                            i += 1;
                        }
                        if (js[i] == '}') {
                            break;
                        }
                        size_t source_start = i;
                        while (js_identifier_part(js[i])) {
                            i += 1;
                        }
                        size_t source_length = i - source_start;
                        while (i < end && is_whitespace(js[i])) {
                            i += 1;
                        }
                        bool import_bare = true;
                        if (js_word_equals(js, i, "as")) {
                            i += sizeof "as" - 1;
                            while (i < end && is_whitespace(js[i])) {
                                i += 1;
                            }
                            import_bare = false;
                            source_start = i;
                            while (js_identifier_part(js[i])) {
                                i += 1;
                            }
                            source_length = i - source_start;
                        }
                        if (!js_mangle_add_global_import_binding(bindings,
                            bindings_length, bindings_capacity, excluded,
                            excluded_length, &js[source_start],
                            source_length, import_bare))
                        {
                            return false;
                        }
                    }
                    continue;
                }
            }
            if (js_word_equals(js, i, "export")) {
                i += sizeof "export" - 2;
                continue;
            }
            if (js_word_equals(js, i, "function")) {
                char previous, next;
                while (js_identifier_part(js[i])) {
                    i += 1;
                }
                js_mangle_previous_next(js, 0, end, i - sizeof "function" + 1,
                    i, &previous, &next);
                if (previous != '\0' && previous != ';' && previous != '}') {
                    i -= 1;
                    continue;
                }
                while (i < end && is_whitespace(js[i])) {
                    i += 1;
                }
                if (js[i] == '*') {
                    i += 1;
                    while (i < end && is_whitespace(js[i])) {
                        i += 1;
                    }
                }
                if (js_identifier_start(js[i])) {
                    size_t word_start = i;
                    while (js_identifier_part(js[i])) {
                        i += 1;
                    }
                    if (!js_mangle_add_global_binding(bindings,
                        bindings_length, bindings_capacity, excluded,
                        excluded_length, &js[word_start], i - word_start))
                    {
                        return false;
                    }
                    i -= 1;
                }
                continue;
            }
            if (!(js_word_equals(js, i, "var") ||
                js_word_equals(js, i, "let") ||
                js_word_equals(js, i, "const")))
            {
                while (js_identifier_part(js[i])) {
                    i += 1;
                }
                i -= 1;
                continue;
            }
            i += js_declaration_keyword_length(js, i);
            while (i < end && is_whitespace(js[i])) {
                i += 1;
            }
            int nesting = 0;
            bool expect_name = true;
            while (i < end && js[i] != ';') {
                if (js[i] == '(' || js[i] == '[' || js[i] == '{') {
                    if (nesting == 0 && expect_name &&
                        (js[i] == '[' || js[i] == '{'))
                    {
                        size_t pattern_end = js_find_matching(js, i, end,
                            js[i], js[i] == '[' ? ']' : '}');
                        if (pattern_end >= end) {
                            break;
                        }
                        if (!js_mangle_collect_global_pattern_bindings(js, i,
                            pattern_end + 1, bindings, bindings_length,
                            bindings_capacity, excluded, excluded_length))
                        {
                            return false;
                        }
                        i = pattern_end;
                        expect_name = false;
                        continue;
                    }
                    nesting += 1;
                    if (expect_name && (js[i] == '[' || js[i] == '{')) {
                        break;
                    }
                }
                else if (js[i] == ')' || js[i] == ']' || js[i] == '}') {
                    nesting -= 1;
                }
                else if (nesting == 0 && js[i] == ',') {
                    expect_name = true;
                }
                else if (expect_name && js_identifier_start(js[i])) {
                    size_t word_start = i;
                    while (js_identifier_part(js[i])) {
                        i += 1;
                    }
                    if (!js_mangle_add_global_binding(bindings,
                        bindings_length, bindings_capacity, excluded,
                        excluded_length, &js[word_start], i - word_start))
                    {
                        return false;
                    }
                    expect_name = false;
                    i -= 1;
                }
                i += 1;
            }
        }
    }
    return true;
}

static bool js_mangle_collect_declarations(const char *js, size_t start,
    size_t end, struct JsMangleBinding **bindings, size_t *bindings_length,
    size_t *bindings_capacity)
{
    for (size_t i = start; i < end; ++i) {
        if (js[i] == '"' || js[i] == '\'' || js[i] == '`') {
            i = js_skip_quoted(js, i, end) - 1;
        }
        else if (js[i] == '/' && (js[i + 1] == '*' || js[i + 1] == '/')) {
            i = js_skip_comment(js, i, end) - 1;
        }
        else if (js[i] == '/' && js_regex_start(js, start, i))
        {
            i = js_skip_regex(js, i, end) - 1;
        }
        else if (js_identifier_start(js[i]) &&
            js_word_equals(js, i, "function"))
        {
            i += sizeof "function" - 1;
            while (i < end && is_whitespace(js[i])) {
                i += 1;
            }
            if (js[i] == '*') {
                i += 1;
                while (i < end && is_whitespace(js[i])) {
                    i += 1;
                }
            }
            if (js_identifier_start(js[i])) {
                size_t word_start = i;
                while (js_identifier_part(js[i])) {
                    i += 1;
                }
                size_t name_end = i;
                while (i < end && is_whitespace(js[i])) {
                    i += 1;
                }
                if (js[i] == '(') {
                    size_t params_end = js_find_matching(js, i, end, '(', ')');
                    i = params_end + 1;
                    while (i < end && is_whitespace(js[i])) {
                        i += 1;
                    }
                    if (params_end < end && js[i] == '{') {
                        size_t body_start = i + 1;
                        size_t body_end = js_find_matching(js, i, end,
                            '{', '}');
                        if (body_end < end &&
                            js_mangle_function_unsafe(js, body_start,
                            body_end))
                        {
                            i = name_end - 1;
                            continue;
                        }
                    }
                }
                if (!js_mangle_add_binding(bindings, bindings_length,
                    bindings_capacity, &js[word_start], name_end - word_start))
                {
                    return false;
                }
                i = name_end - 1;
            }
        }
        else if (js_identifier_start(js[i]) &&
            js_word_equals(js, i, "catch"))
        {
            i += sizeof "catch" - 1;
            while (i < end && is_whitespace(js[i])) {
                i += 1;
            }
            if (js[i] == '(') {
                i += 1;
                while (i < end && is_whitespace(js[i])) {
                    i += 1;
                }
                if (js_identifier_start(js[i])) {
                    size_t word_start = i;
                    while (js_identifier_part(js[i])) {
                        i += 1;
                    }
                    if (!js_mangle_add_binding(bindings, bindings_length,
                        bindings_capacity, &js[word_start], i - word_start))
                    {
                        return false;
                    }
                }
            }
        }
        else if (js_identifier_start(js[i]) &&
            (js_word_equals(js, i, "var") || js_word_equals(js, i, "let") ||
            js_word_equals(js, i, "const")))
        {
            i += js_declaration_keyword_length(js, i);
            while (i < end && is_whitespace(js[i])) {
                i += 1;
            }
            int nesting = 0;
            bool expect_name = true;
            while (i < end && js[i] != ';') {
                if (js[i] == '(' || js[i] == '[' || js[i] == '{') {
                    if (nesting == 0 && expect_name &&
                        (js[i] == '[' || js[i] == '{'))
                    {
                        size_t pattern_end = js_find_matching(js, i, end,
                            js[i], js[i] == '[' ? ']' : '}');
                        if (pattern_end >= end) {
                            break;
                        }
                        if (!js_mangle_collect_pattern_bindings(js, i,
                            pattern_end + 1, bindings, bindings_length,
                            bindings_capacity))
                        {
                            return false;
                        }
                        i = pattern_end;
                        expect_name = false;
                        continue;
                    }
                    nesting += 1;
                    if (expect_name && (js[i] == '[' || js[i] == '{')) {
                        break;
                    }
                }
                else if (js[i] == ')' || js[i] == ']' || js[i] == '}') {
                    nesting -= 1;
                }
                else if (nesting == 0 && js[i] == ',') {
                    expect_name = true;
                }
                else if (expect_name && js_identifier_start(js[i])) {
                    size_t word_start = i;
                    while (js_identifier_part(js[i])) {
                        i += 1;
                    }
                    if (!js_mangle_add_binding(bindings, bindings_length,
                        bindings_capacity, &js[word_start], i - word_start))
                    {
                        return false;
                    }
                    expect_name = false;
                    i -= 1;
                }
                i += 1;
            }
        }
    }
    return true;
}

static bool js_mangle_assign_global_names(struct JsMangleProgram *program,
    const char *js, size_t end, struct JsMangleState *state,
    struct JsMangleBinding *bindings, size_t bindings_length)
{
    size_t name_index = 0;
    for (size_t i = 0; i < bindings_length; ++i) {
        if (bindings[i].length <= sizeof "g0" - 1 ||
            js_mangle_name_count(program, js, 0, end, bindings[i].name,
            bindings[i].length) <= 1)
        {
            bindings[i].replacement_length = 0;
            continue;
        }
        do {
            js_mangle_make_global_name(name_index++, bindings[i].replacement,
                &bindings[i].replacement_length);
        } while (js_mangle_name_used(program, js, 0, end,
            bindings[i].replacement, bindings[i].replacement_length));

        if (bindings[i].replacement_length >= bindings[i].length) {
            bindings[i].replacement_length = 0;
            continue;
        }
        if (!js_mangle_add_map(program, state, bindings[i], 0, end)) {
            return false;
        }
    }
    return true;
}

static struct JsMangleBinding *js_mangle_find_binding(
    struct JsMangleBinding *bindings, size_t bindings_length,
    const char *name, size_t length)
{
    for (size_t i = 0; i < bindings_length; ++i) {
        if (js_mangle_same_name(bindings[i].name, bindings[i].length, name,
            length))
        {
            return &bindings[i];
        }
    }
    return NULL;
}

static bool js_mangle_index_bindings(struct JsMangleProgram *program,
    struct JsMangleBinding *bindings, size_t bindings_length)
{
    for (size_t i = 0; i < bindings_length; ++i) {
        if (bindings[i].name_index == NULL) {
            bindings[i].name_index = js_mangle_find_name_index(program,
                bindings[i].name, bindings[i].length);
        }
        if (bindings[i].name_index == NULL) {
            return false;
        }
        bindings[i].name_index->binding = &bindings[i];
    }
    return true;
}

static void js_mangle_clear_binding_index(struct JsMangleBinding *bindings,
    size_t bindings_length)
{
    for (size_t i = 0; i < bindings_length; ++i) {
        if (bindings[i].name_index != NULL &&
            bindings[i].name_index->binding == &bindings[i])
        {
            bindings[i].name_index->binding = NULL;
        }
    }
}

static bool js_mangle_name_used_raw(const char *js, size_t start, size_t end,
    const char *name, size_t length)
{
    for (size_t i = start; i < end; ++i) {
        if (js[i] == '"' || js[i] == '\'' || js[i] == '`') {
            i = js_skip_quoted(js, i, end) - 1;
        }
        else if (js[i] == '/' && (js[i + 1] == '*' || js[i + 1] == '/')) {
            i = js_skip_comment(js, i, end) - 1;
        }
        else if (js[i] == '/' && js_regex_start(js, start, i))
        {
            i = js_skip_regex(js, i, end) - 1;
        }
        else if (js_identifier_start(js[i])) {
            size_t word_start = i;
            while (js_identifier_part(js[i])) {
                i += 1;
            }
            if (js_mangle_same_name(&js[word_start], i - word_start, name,
                length))
            {
                return true;
            }
            i -= 1;
        }
    }
    return false;
}

static bool js_mangle_replacement_used_in_scope(
    struct JsMangleBinding *bindings, size_t bindings_length,
    const char *name, size_t length)
{
    for (size_t i = 0; i < bindings_length; ++i) {
        if (bindings[i].replacement_length != 0 &&
            js_mangle_same_name(bindings[i].replacement,
            bindings[i].replacement_length, name, length))
        {
            return true;
        }
    }
    return false;
}

static bool js_mangle_assign_names(struct JsMangleProgram *program,
    const char *js, size_t start, size_t end, size_t signature_end,
    struct JsMangleState *state, struct JsMangleBinding *bindings,
    size_t bindings_length)
{
    struct JsMangleMap *visible_maps = NULL;
    size_t visible_maps_length = 0;
    size_t name_index = 0;

    if (state->maps_length != 0) {
        visible_maps = malloc(state->maps_length * sizeof *visible_maps);
        if (visible_maps == NULL) {
            return false;
        }
        visible_maps_length = js_mangle_collect_visible_maps(program, js,
            start, end, state, visible_maps);
    }
    for (size_t i = 0; i < bindings_length; ++i) {
        do {
            js_mangle_make_name(name_index++, bindings[i].replacement,
                &bindings[i].replacement_length);
        } while (
            js_mangle_replacement_used_in_scope(bindings, i,
                bindings[i].replacement,
                bindings[i].replacement_length) ||
            js_mangle_name_used_raw(js, start, signature_end,
                bindings[i].replacement,
                bindings[i].replacement_length) ||
            js_mangle_name_used(program, js, start, end,
                bindings[i].replacement,
                bindings[i].replacement_length) ||
            js_mangle_replacement_visible(visible_maps, visible_maps_length,
                bindings[i].replacement,
                bindings[i].replacement_length)
        );

        if (bindings[i].replacement_length >= bindings[i].length) {
            bindings[i].replacement_length = 0;
            continue;
        }
        if (!js_mangle_add_map(program, state, bindings[i], start, end)) {
            free(visible_maps);
            return false;
        }
    }
    free(visible_maps);
    return true;
}

static bool js_mangle_in_import_specifier(const char *js, size_t word_start)
{
    size_t statement_start = word_start;
    while (statement_start > 0 && js[statement_start - 1] != ';') {
        statement_start -= 1;
    }
    size_t i = statement_start;
    while (is_whitespace(js[i])) {
        i += 1;
    }
    if (!js_word_equals(js, i, "import")) {
        return false;
    }
    bool in_braces = false;
    for (; i < word_start; ++i) {
        if (js[i] == '"' || js[i] == '\'') {
            i = js_skip_quoted(js, i, word_start) - 1;
        }
        else if (js[i] == '{') {
            in_braces = true;
        }
        else if (js[i] == '}') {
            in_braces = false;
        }
    }
    return in_braces;
}

static bool js_mangle_in_declaration_pattern(const char *js, size_t start,
    size_t end, size_t word_start)
{
    size_t search = word_start;
    while (search > start) {
        size_t open = search;
        while (open > start && js[open] != '{' && js[open] != ';') {
            open -= 1;
        }
        if (js[open] != '{') {
            return false;
        }

        size_t close = js_find_matching(js, open, end, '{', '}');
        if (close >= end || word_start > close) {
            return false;
        }
        size_t i = close + 1;
        while (i < end && is_whitespace(js[i])) {
            i += 1;
        }
        if (js[i] == '=') {
            i = open;
            while (i > start && is_whitespace(js[i - 1])) {
                i -= 1;
            }
            while (i > start && js[i - 1] != ';' && js[i - 1] != '{' &&
                js[i - 1] != '}')
            {
                i -= 1;
            }
            while (i < open && is_whitespace(js[i])) {
                i += 1;
            }
            return js_word_equals(js, i, "let") ||
                js_word_equals(js, i, "const") ||
                js_word_equals(js, i, "var");
        }
        if (open == start) {
            break;
        }
        search = open - 1;
    }
    return false;
}

static bool js_mangle_in_parameter_pattern(const char *js, size_t start,
    size_t end, size_t word_start)
{
    size_t pattern_start = word_start;
    size_t curly = 0;
    size_t round = 0;
    size_t square = 0;

    while (pattern_start > start) {
        pattern_start -= 1;
        if (js[pattern_start] == '}') {
            curly += 1;
        }
        else if (js[pattern_start] == ')') {
            round += 1;
        }
        else if (js[pattern_start] == ']') {
            square += 1;
        }
        else if (js[pattern_start] == '{') {
            if (curly == 0 && round == 0 && square == 0) {
                break;
            }
            curly -= curly > 0;
        }
        else if (js[pattern_start] == '(' || js[pattern_start] == ';') {
            return false;
        }
        else if (js[pattern_start] == '[') {
            square -= square > 0;
        }
    }
    if (js[pattern_start] != '{') {
        return false;
    }

    size_t pattern_end = js_find_matching(js, pattern_start, end, '{', '}');
    if (pattern_end >= end || word_start > pattern_end) {
        return false;
    }

    size_t before = pattern_start;
    while (before > start && is_whitespace(js[before - 1])) {
        before -= 1;
    }
    if (before <= start || (js[before - 1] != '(' && js[before - 1] != ',')) {
        return false;
    }

    size_t after = pattern_end + 1;
    while (after < end && is_whitespace(js[after])) {
        after += 1;
    }
    if (after >= end) {
        return false;
    }
    if (js[after] != ')' && js[after] != ',' && js[after] != '=') {
        return false;
    }

    size_t params_start = pattern_start;
    curly = 0;
    round = 0;
    square = 0;
    while (params_start > start) {
        params_start -= 1;
        if (js[params_start] == '}') {
            curly += 1;
        }
        else if (js[params_start] == ')') {
            round += 1;
        }
        else if (js[params_start] == ']') {
            square += 1;
        }
        else if (js[params_start] == '(') {
            if (curly == 0 && round == 0 && square == 0) {
                break;
            }
            round -= round > 0;
        }
        else if (js[params_start] == '{') {
            curly -= curly > 0;
        }
        else if (js[params_start] == '[') {
            square -= square > 0;
        }
        else if (js[params_start] == ';') {
            return false;
        }
    }
    if (js[params_start] != '(') {
        return false;
    }

    size_t params_end = js_find_matching(js, params_start, end, '(', ')');
    if (params_end >= end || pattern_end > params_end) {
        return false;
    }
    after = params_end + 1;
    while (after < end && is_whitespace(js[after])) {
        after += 1;
    }
    return (after < end && js[after] == '{') ||
        (after + 1 < end && js[after] == '=' && js[after + 1] == '>');
}

static char js_mangle_previous_punctuation(struct JsMangleProgram *program,
    size_t token_i, size_t min_start)
{
    while (token_i > 0) {
        token_i -= 1;
        struct JsMangleToken *token = &program->tokens[token_i];
        if (token->start < min_start) {
            return '\0';
        }
        if (token->kind == JS_MANGLE_TOKEN_PUNCTUATION) {
            return token->punctuation;
        }
        if (token->kind == JS_MANGLE_TOKEN_IDENTIFIER ||
            token->kind == JS_MANGLE_TOKEN_STRING ||
            token->kind == JS_MANGLE_TOKEN_REGEX)
        {
            return '\0';
        }
    }
    return '\0';
}

static char js_mangle_next_punctuation(struct JsMangleProgram *program,
    size_t token_i, size_t max_end)
{
    for (token_i += 1; token_i < program->tokens_length; ++token_i) {
        struct JsMangleToken *token = &program->tokens[token_i];
        if (token->start >= max_end) {
            return '\0';
        }
        if (token->kind == JS_MANGLE_TOKEN_PUNCTUATION) {
            return token->punctuation;
        }
        if (token->kind == JS_MANGLE_TOKEN_IDENTIFIER ||
            token->kind == JS_MANGLE_TOKEN_STRING ||
            token->kind == JS_MANGLE_TOKEN_REGEX)
        {
            return '\0';
        }
    }
    return '\0';
}

static size_t js_mangle_enclosing_open_punctuation_index(
    struct JsMangleProgram *program, size_t token_i, size_t min_start)
{
    size_t open_i = program->tokens[token_i].enclosing_open;
    if (open_i == SIZE_MAX || program->tokens[open_i].start < min_start) {
        return SIZE_MAX;
    }
    return open_i;
}

static char js_mangle_enclosing_close_punctuation(
    struct JsMangleProgram *program, size_t token_i, size_t max_end)
{
    size_t open_i = program->tokens[token_i].enclosing_open;
    if (open_i == SIZE_MAX) {
        return '\0';
    }
    size_t close_i = program->tokens[open_i].matching_token;
    if (close_i == SIZE_MAX || program->tokens[close_i].start >= max_end) {
        return '\0';
    }
    return program->tokens[close_i].punctuation;
}

static bool js_mangle_open_brace_starts_object_literal(const char *js,
    struct JsMangleProgram *program, size_t open_token_i, size_t min_start)
{
    struct JsMangleToken *open = &program->tokens[open_token_i];
    char previous = js_mangle_previous_punctuation(program, open_token_i,
        min_start);
    if (previous == '(' || previous == ',' || previous == '=' ||
        previous == '[' || previous == ':' || previous == '?')
    {
        return true;
    }
    return js_mangle_previous_word_equals(js, min_start, open->start,
        "return") || js_mangle_previous_word_equals(js, min_start,
        open->start, "yield");
}

static bool js_mangle_in_parameter_braces(const char *js, size_t params_start,
    size_t params_end, size_t word_start)
{
    size_t open = word_start;
    size_t curly = 0;

    while (open > params_start) {
        open -= 1;
        if (js[open] == '}') {
            curly += 1;
        }
        else if (js[open] == '{') {
            if (curly == 0) {
                break;
            }
            curly -= 1;
        }
    }
    if (js[open] != '{') {
        return false;
    }
    size_t close = js_find_matching(js, open, params_end, '{', '}');
    return close < params_end && word_start < close;
}

static bool js_mangle_token_starts_accessor(const char *js,
    struct JsMangleProgram *program, size_t token_i)
{
    if (!js_mangle_token_equals(js, &program->tokens[token_i], "get") &&
        !js_mangle_token_equals(js, &program->tokens[token_i], "set"))
    {
        return false;
    }
    if (token_i + 2 >= program->tokens_length) {
        return false;
    }
    if (program->tokens[token_i + 1].kind != JS_MANGLE_TOKEN_IDENTIFIER &&
        program->tokens[token_i + 1].kind != JS_MANGLE_TOKEN_STRING)
    {
        return false;
    }
    return program->tokens[token_i + 2].kind == JS_MANGLE_TOKEN_PUNCTUATION &&
        program->tokens[token_i + 2].punctuation == '(';
}

static bool js_mangle_identifier_token_edit(const char *js,
    struct JsMangleProgram *program, size_t token_i, size_t start, size_t end,
    struct JsMangleState *state, struct JsMangleBinding *bindings,
    size_t bindings_length, bool global)
{
    struct JsMangleToken *token = &program->tokens[token_i];
    if (token->start < start || token->start >= end ||
        token->kind != JS_MANGLE_TOKEN_IDENTIFIER)
    {
        return true;
    }
    if (!global && js_keyword(&js[token->start], token->length)) {
        return true;
    }
    if (js_mangle_token_starts_accessor(js, program, token_i)) {
        return true;
    }

    char previous = js_mangle_previous_punctuation(program, token_i, start);
    char next = js_mangle_next_punctuation(program, token_i, end);
    bool rest_param = token->start >= start + 3 &&
        js[token->start - 1] == '.' && js[token->start - 2] == '.' &&
        js[token->start - 3] == '.';
    bool label_reference =
        js_mangle_previous_word_equals(js, start, token->start, "break") ||
        js_mangle_previous_word_equals(js, start, token->start, "continue");
    if ((previous == '.' && !rest_param) || label_reference) {
        return true;
    }
    if (next == ':' && previous != '?') {
        return true;
    }

    struct JsMangleBinding *binding = token->name_index == NULL ? NULL :
        token->name_index->binding;
    if (binding == NULL || binding->replacement_length == 0) {
        return true;
    }

    bool pattern_alias = (previous == '{' || previous == ',') &&
        !rest_param &&
        (
            js_mangle_in_declaration_pattern(js, start, end, token->start) ||
            js_mangle_in_parameter_pattern(js, start, end, token->start)
        );
    if (pattern_alias) {
        return js_mangle_add_pattern_alias_edit(state, token->start,
            &js[token->start], token->length, binding->replacement,
            binding->replacement_length);
    }
    if (global && binding->import_bare &&
        js_mangle_in_import_specifier(js, token->start))
    {
        return js_mangle_add_import_alias_edit(state, token->start,
            &js[token->start], token->length, binding->replacement,
            binding->replacement_length);
    }
    if ((previous == '{' || previous == ',') &&
        (next == '}' || next == ','))
    {
        size_t enclosing_open_i = js_mangle_enclosing_open_punctuation_index(
            program, token_i, 0);
        char enclosing_open = enclosing_open_i == SIZE_MAX ? '\0' :
            program->tokens[enclosing_open_i].punctuation;
        char enclosing_close = js_mangle_enclosing_close_punctuation(program,
            token_i, program->tokens[program->tokens_length - 1].start + 1);
        if (enclosing_open == '{' && enclosing_close == '}' &&
            js_mangle_open_brace_starts_object_literal(js, program,
            enclosing_open_i, start))
        {
            return js_mangle_add_shorthand_edit(state, token->start,
                token->length, binding->replacement,
                binding->replacement_length);
        }
    }
    return js_mangle_add_edit(state, token->start, token->length,
        binding->replacement, binding->replacement_length);
}

static bool js_mangle_add_global_edits(const char *js, size_t end,
    struct JsMangleProgram *program, struct JsMangleState *state,
    struct JsMangleBinding *bindings, size_t bindings_length)
{
    bool success = false;
    if (!js_mangle_index_bindings(program, bindings, bindings_length)) {
        return false;
    }
    for (size_t t = 0; t < program->tokens_length; ++t) {
        if (program->tokens[t].start >= end) {
            break;
        }
        if (js_mangle_token_equals(js, &program->tokens[t], "function")) {
            char function_previous = js_mangle_previous_punctuation(program,
                t, 0);
            size_t i = program->tokens[t].start + sizeof "function" - 1;
            while (i < end && is_whitespace(js[i])) {
                i += 1;
            }
            if (js[i] == '*') {
                i += 1;
                while (i < end && is_whitespace(js[i])) {
                    i += 1;
                }
            }
            while (js_identifier_part(js[i])) {
                i += 1;
            }
            while (i < end && is_whitespace(js[i])) {
                i += 1;
            }
            if (js[i] == '(') {
                size_t params_start = i + 1;
                size_t params_end = js_find_matching(js, i, end, '(', ')');
                i = params_end + 1;
                while (i < end && is_whitespace(js[i])) {
                    i += 1;
                }
                if (params_end < end && js[i] == '{') {
                    size_t body_start = i + 1;
                    size_t body_end = js_find_matching(js, i, end, '{', '}');
                    if (function_previous == ':') {
                        while (t + 1 < program->tokens_length &&
                            program->tokens[t + 1].start < body_end)
                        {
                            t += 1;
                        }
                        continue;
                    }
                    struct JsMangleBinding *locals = NULL;
                    size_t locals_length = 0;
                    size_t locals_capacity = 0;
                    bool skip_function = false;
                    if (body_end >= end ||
                        !js_mangle_alloc_bindings(&locals, &locals_capacity,
                        js_mangle_count_identifiers(js, params_start,
                        params_end) + js_mangle_count_identifiers(js,
                        body_start, body_end)) ||
                        !js_mangle_collect_params(js, params_start,
                        params_end, &locals, &locals_length,
                        &locals_capacity) ||
                        !js_mangle_collect_declarations(js, body_start,
                        body_end, &locals, &locals_length, &locals_capacity))
                    {
                        free(locals);
                        goto done;
                    }
                    for (size_t l = 0; l < locals_length; ++l) {
                        if (js_mangle_find_binding(bindings, bindings_length,
                            locals[l].name, locals[l].length) != NULL)
                        {
                            skip_function = true;
                            break;
                        }
                    }
                    free(locals);
                    if (skip_function) {
                        while (t + 1 < program->tokens_length &&
                            program->tokens[t + 1].start < body_end)
                        {
                            t += 1;
                        }
                        continue;
                    }
                }
            }
        }
        if (!js_mangle_identifier_token_edit(js, program, t, 0, end, state,
            bindings, bindings_length, true))
        {
            goto done;
        }
    }
    success = true;

done:
    js_mangle_clear_binding_index(bindings, bindings_length);
    return success;
}

static bool js_mangle_add_function_edits(const char *js, size_t start,
    size_t end, size_t params_start, size_t params_end,
    struct JsMangleProgram *program, struct JsMangleState *state,
    struct JsMangleBinding *bindings, size_t bindings_length)
{
    bool success = false;
    if (!js_mangle_index_bindings(program, bindings, bindings_length)) {
        return false;
    }
    for (size_t t = js_mangle_find_token_start(program, start);
        t < program->tokens_length; ++t)
    {
        if (program->tokens[t].start >= end) {
            break;
        }
        size_t edits_length_before = state->edits_length;
        if (!js_mangle_identifier_token_edit(js, program, t, start, end,
            state, bindings, bindings_length, false))
        {
            goto done;
        }
        if (program->tokens[t].start >= params_start &&
            program->tokens[t].start < params_end &&
            program->tokens[t].kind == JS_MANGLE_TOKEN_IDENTIFIER)
        {
            char previous = js_mangle_previous_punctuation(program, t, start);
            char next = js_mangle_next_punctuation(program, t, end);
            bool rest_param = program->tokens[t].start >= start + 3 &&
                js[program->tokens[t].start - 1] == '.' &&
                js[program->tokens[t].start - 2] == '.' &&
                js[program->tokens[t].start - 3] == '.';
            if (previous != ':' && !rest_param &&
                (previous == '{' || previous == ',') &&
                (next == '}' || next == ',') &&
                js_mangle_in_parameter_braces(js, params_start, params_end,
                program->tokens[t].start))
            {
                struct JsMangleBinding *binding =
                    program->tokens[t].name_index == NULL ? NULL :
                    program->tokens[t].name_index->binding;
                if (binding != NULL && binding->replacement_length != 0 &&
                    state->edits_length == edits_length_before + 1)
                {
                    struct JsMangleEdit *edit =
                        &state->edits[state->edits_length - 1];
                    if (edit->start == program->tokens[t].start &&
                        edit->length == program->tokens[t].length)
                    {
                        js_mangle_free_edit(edit);
                        state->edits_length -= 1;
                        if (!js_mangle_add_pattern_alias_edit(state,
                            program->tokens[t].start,
                            &js[program->tokens[t].start],
                            program->tokens[t].length, binding->replacement,
                            binding->replacement_length))
                        {
                            goto done;
                        }
                    }
                }
            }
        }
    }
    success = true;

done:
    js_mangle_clear_binding_index(bindings, bindings_length);
    return success;
}

static bool js_mangle_function(const char *js, size_t params_start,
    size_t params_end, size_t body_start, size_t body_end,
    size_t name_start, size_t name_end, struct JsMangleProgram *program,
    struct JsMangleState *state)
{
    if (js_mangle_function_unsafe(js, body_start, body_end)) {
        return js_mangle_add_unsafe_range(state, body_start - 1, body_end);
    }

    struct JsMangleBinding *bindings = NULL;
    size_t bindings_length = 0;
    size_t bindings_capacity = 0;
    bool success = false;

    if (!js_mangle_alloc_bindings(&bindings, &bindings_capacity,
        js_mangle_count_identifiers(js, params_start, params_end) +
        js_mangle_count_identifiers(js, body_start, body_end) +
        (name_start < name_end)))
    {
        js_mangle_failure_reason = "binding allocation";
        goto done;
    }
    if (name_start < name_end &&
        !js_mangle_add_binding(&bindings, &bindings_length,
        &bindings_capacity, &js[name_start], name_end - name_start))
    {
        js_mangle_failure_reason = "function name binding";
        goto done;
    }
    if (!js_mangle_collect_params(js, params_start, params_end, &bindings,
        &bindings_length, &bindings_capacity))
    {
        js_mangle_failure_reason = "parameter collection";
        goto done;
    }
    if (!js_mangle_collect_declarations(js, body_start, body_end, &bindings,
        &bindings_length, &bindings_capacity))
    {
        js_mangle_failure_reason = "declaration collection";
        goto done;
    }
    size_t edit_start = name_start < name_end ? name_start : params_start;
    if (!js_mangle_assign_names(program, js, edit_start, body_end,
        params_end, state, bindings, bindings_length))
    {
        js_mangle_failure_reason = "name assignment";
        goto done;
    }
    if (!js_mangle_add_function_edits(js, edit_start, body_end, params_start,
        params_end, program, state, bindings, bindings_length))
    {
        js_mangle_failure_reason = "function edits";
        goto done;
    }
    success = true;

done:
    free(bindings);
    return success;
}

static size_t js_mangle_expression_end(const char *js, size_t start,
    size_t end)
{
    size_t round_nesting = 0;
    size_t square_nesting = 0;

    for (size_t i = start; i < end; ++i) {
        if (js[i] == '"' || js[i] == '\'' || js[i] == '`') {
            i = js_skip_quoted(js, i, end) - 1;
        }
        else if (js[i] == '/' && (js[i + 1] == '*' || js[i + 1] == '/')) {
            i = js_skip_comment(js, i, end) - 1;
        }
        else if (js[i] == '/' && js_regex_start(js, start, i))
        {
            i = js_skip_regex(js, i, end) - 1;
        }
        else if (js[i] == '(') {
            round_nesting += 1;
        }
        else if (js[i] == ')') {
            round_nesting -= round_nesting > 0;
        }
        else if (js[i] == '[') {
            square_nesting += 1;
        }
        else if (js[i] == ']') {
            square_nesting -= square_nesting > 0;
        }
        else if (round_nesting == 0 && square_nesting == 0 &&
            (js[i] == ';' || js[i] == ','))
        {
            return i;
        }
    }
    return end;
}

static bool js_mangle_arrow(const char *js, size_t params_start,
    size_t params_end, size_t arrow_i, size_t end,
    struct JsMangleProgram *program, struct JsMangleState *state,
    size_t *body_end)
{
    size_t body_start = arrow_i + 2;
    while (body_start < end && is_whitespace(js[body_start])) {
        body_start += 1;
    }
    if (js[body_start] == '{') {
        *body_end = js_find_matching(js, body_start, end, '{', '}');
        if (*body_end >= end) {
            return true;
        }
        return js_mangle_function(js, params_start, params_end,
            body_start + 1, *body_end, body_start, body_start, program,
            state);
    }
    *body_end = js_mangle_expression_end(js, body_start, end);
    return js_mangle_function(js, params_start, params_end, body_start,
        *body_end, body_start, body_start, program, state);
}

static struct Minification mangle_js_identifiers(const char *js,
    bool force_module)
{
    struct Minification m = {.result = NULL};
    struct JsMangleState state = {0};
    struct JsMangleProgram program = {0};
    struct JsMangleBinding *global_bindings = NULL;
    struct JsMangleBinding *exported_bindings = NULL;
    size_t global_bindings_length = 0;
    size_t global_bindings_capacity = 0;
    size_t exported_bindings_length = 0;
    size_t exported_bindings_capacity = 0;
    size_t length = strlen(js);
    struct JsMangleCounts counts = {0};

    js_mangle_count_program(js, length, &counts);
    if (!js_mangle_alloc_program(&program, counts) ||
        !js_mangle_alloc_state(&state, counts) ||
        !js_mangle_alloc_bindings(&global_bindings,
        &global_bindings_capacity, counts.identifiers) ||
        !js_mangle_alloc_bindings(&exported_bindings,
        &exported_bindings_capacity, counts.identifiers))
    {
        snprintf(m.error, sizeof m.error,
            "Cannot allocate memory (mangle setup)\n");
        goto error;
    }
    if (!js_mangle_build_program(js, length, &program)) {
        snprintf(m.error, sizeof m.error,
            "Cannot allocate memory (program build)\n");
        goto error;
    }

    if (js_mangle_top_level_unsafe(js, &program)) {
        m.result = malloc(length + 1);
        if (m.result == NULL) {
            snprintf(m.error, sizeof m.error, "Cannot allocate memory\n");
            goto error;
        }
        memcpy(m.result, js, length + 1);
        js_mangle_free_program(&program);
        return m;
    }

    if (force_module || js_mangle_is_module(js, &program)) {
        if (!js_mangle_collect_exported_declarations(js, length,
            &exported_bindings, &exported_bindings_length,
            &exported_bindings_capacity))
        {
            snprintf(m.error, sizeof m.error,
                "Cannot allocate memory (export collection)\n");
            goto error;
        }
        if (!js_mangle_collect_global_declarations(js, length,
            &global_bindings, &global_bindings_length,
            &global_bindings_capacity, exported_bindings,
            exported_bindings_length))
        {
            snprintf(m.error, sizeof m.error,
                "Cannot allocate memory (global collection)\n");
            goto error;
        }
        if (!js_mangle_assign_global_names(&program, js, length, &state,
            global_bindings, global_bindings_length))
        {
            snprintf(m.error, sizeof m.error,
                "Cannot allocate memory (global names)\n");
            goto error;
        }
        if (!js_mangle_add_global_edits(js, length, &program, &state,
            global_bindings, global_bindings_length))
        {
            snprintf(m.error, sizeof m.error,
                "Cannot allocate memory (global edits)\n");
            goto error;
        }
    }

    for (size_t i = 0; i < length; ++i) {
        if (js[i] == '"' || js[i] == '\'' || js[i] == '`') {
            i = js_skip_quoted(js, i, length) - 1;
        }
        else if (js[i] == '/' && (js[i + 1] == '*' || js[i + 1] == '/')) {
            i = js_skip_comment(js, i, length) - 1;
        }
        else if (js[i] == '/' && js_regex_start(js, 0, i))
        {
            i = js_skip_regex(js, i, length) - 1;
        }
        else if (js[i] == '(') {
            size_t params_start = i + 1;
            size_t params_end = js_find_matching(js, i, length, '(', ')');
            if (params_end >= length) {
                continue;
            }
            size_t arrow_i = params_end + 1;
            while (arrow_i < length && is_whitespace(js[arrow_i])) {
                arrow_i += 1;
            }
            if (js[arrow_i] == '=' && js[arrow_i + 1] == '>') {
                if (js_mangle_in_unsafe_range(&state, i)) {
                    continue;
                }
                size_t body_end;
                if (!js_mangle_arrow(js, params_start, params_end, arrow_i,
                    length, &program, &state, &body_end))
                {
                    snprintf(m.error, sizeof m.error,
                        "Cannot allocate memory (arrow mangling)\n");
                    goto error;
                }
                i = body_end;
            }
        }
        else if (js_identifier_start(js[i]) &&
            js_word_equals(js, i, "function"))
        {
            if (js_mangle_in_unsafe_range(&state, i)) {
                continue;
            }
            size_t function_start = i;
            i += sizeof "function" - 1;
            if (js[i] == '*') {
                i += 1;
            }
            while (is_whitespace(js[i])) {
                i += 1;
            }
            size_t name_start = i;
            while (js_identifier_part(js[i])) {
                i += 1;
            }
            size_t name_end = i;
            while (is_whitespace(js[i])) {
                i += 1;
            }
            if (js[i] != '(') {
                continue;
            }
            char previous, next;
            js_mangle_previous_next(js, 0, length, function_start,
                function_start + sizeof "function" - 1, &previous, &next);
            if (previous == '\0' || previous == ';' || previous == '{' ||
                previous == '}' || js_mangle_previous_word_equals(js, 0,
                function_start, "export") ||
                js_mangle_previous_word_equals(js, 0, function_start,
                "default"))
            {
                name_start = name_end;
            }
            size_t params_start = i + 1;
            size_t params_end = js_find_matching(js, i, length, '(', ')');
            if (params_end >= length) {
                continue;
            }
            i = params_end + 1;
            while (is_whitespace(js[i])) {
                i += 1;
            }
            if (js[i] != '{') {
                continue;
            }
            size_t body_start = i + 1;
            size_t body_end = js_find_matching(js, i, length, '{', '}');
            if (body_end >= length) {
                continue;
            }
            if (previous == ':') {
                i = body_end;
                continue;
            }
            if (!js_mangle_function(js, params_start, params_end, body_start,
                body_end, name_start, name_end, &program, &state))
            {
                snprintf(m.error, sizeof m.error,
                    "Cannot allocate memory (function mangling: %s)\n",
                    js_mangle_failure_reason != NULL ?
                    js_mangle_failure_reason : "unknown");
                goto error;
            }
        }
        else if (js_identifier_start(js[i])) {
            size_t params_start = i;
            while (js_identifier_part(js[i])) {
                i += 1;
            }
            size_t params_end = i;
            size_t arrow_i = i;
            while (arrow_i < length && is_whitespace(js[arrow_i])) {
                arrow_i += 1;
            }
            if (js[arrow_i] == '=' && js[arrow_i + 1] == '>') {
                if (js_mangle_in_unsafe_range(&state, params_start)) {
                    i = params_end;
                    continue;
                }
                size_t body_end;
                if (!js_mangle_arrow(js, params_start, params_end, arrow_i,
                    length, &program, &state, &body_end))
                {
                    snprintf(m.error, sizeof m.error,
                        "Cannot allocate memory (arrow mangling)\n");
                    goto error;
                }
                i = body_end;
            }
            else {
                i -= 1;
            }
        }
    }

    size_t result_length = length;
    qsort(state.edits, state.edits_length, sizeof *state.edits,
        js_mangle_compare_edits);
    js_mangle_rebase_edits(&state);
    size_t edits_length = 0;
    for (size_t i = 0; i < state.edits_length; ++i) {
        if (edits_length > 0 &&
            state.edits[i].start <
            state.edits[edits_length - 1].start +
            state.edits[edits_length - 1].length)
        {
            if (state.edits[i].start == state.edits[edits_length - 1].start &&
                state.edits[i].length == state.edits[edits_length - 1].length)
            {
                js_mangle_free_edit(&state.edits[i]);
                continue;
            }
            snprintf(m.error, sizeof m.error,
                "Cannot allocate memory (overlapping edits)\n");
            goto error;
        }
        if (edits_length < i) {
            js_mangle_free_edit(&state.edits[edits_length]);
        }
        state.edits[edits_length++] = state.edits[i];
        if (edits_length - 1 < i) {
            js_mangle_clear_edit(&state.edits[i]);
        }
    }
    state.edits_length = edits_length;
    js_mangle_rebase_edits(&state);
    for (size_t i = 0; i < state.edits_length; ++i) {
        result_length += state.edits[i].replacement_length -
            state.edits[i].length;
    }
    m.result = malloc(result_length + 1);
    if (m.result == NULL) {
        snprintf(m.error, sizeof m.error,
            "Cannot allocate memory (result buffer)\n");
        goto error;
    }

    size_t input_i = 0;
    size_t output_i = 0;
    for (size_t i = 0; i < state.edits_length; ++i) {
        memcpy(&m.result[output_i], &js[input_i],
            state.edits[i].start - input_i);
        output_i += state.edits[i].start - input_i;
        memcpy(&m.result[output_i], state.edits[i].replacement,
            state.edits[i].replacement_length);
        output_i += state.edits[i].replacement_length;
        input_i = state.edits[i].start + state.edits[i].length;
    }
    memcpy(&m.result[output_i], &js[input_i], length - input_i + 1);

    js_mangle_free_edits(&state);
    free(state.edits);
    free(state.maps);
    free(state.unsafe_ranges);
    js_mangle_free_program(&program);
    free(global_bindings);
    free(exported_bindings);
    return m;

error:
    js_mangle_free_edits(&state);
    free(state.edits);
    free(state.maps);
    free(state.unsafe_ranges);
    js_mangle_free_program(&program);
    free(global_bindings);
    free(exported_bindings);
    free(m.result);
    m.result = NULL;
    return m;
}

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

static struct Minification minify_js_with_options(const char *js)
{
    struct Minification m = minify_js(js);

    if (m.result != NULL && option_mangle_js_identifiers) {
        struct Minification mangled = mangle_js_identifiers(m.result, false);
        free(m.result);
        m = mangled;
    }

    return m;
}

static struct Minification minify_js_module_with_options(const char *js)
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

static void xmlhtml_correct_error_position(const char *encoded, const char *decoded, size_t *error_position,
    bool is_xml)
{
    size_t encoded_i = 0, decoded_i = 0;
    bool in_cdata = false;
    int (*tagncmp)(const char *, const char *, size_t) = (is_xml ? strncmp : strnicmp);
    while (true) {
        if (*error_position == decoded_i) {
            *error_position = encoded_i;
            return;
        }
        if (encoded[encoded_i] == '\0') {
            return;
        }
        if (!in_cdata) {
            if (is_xml && !tagncmp(&encoded[encoded_i], "<![CDATA[", sizeof "<![CDATA[" - 1)) {
                in_cdata = true;
                encoded_i += sizeof "<![CDATA[" - 1;
                decoded_i += 1;
                continue;
            }
            if (!tagncmp(&encoded[encoded_i], "&lt;", sizeof "&lt;" - 1)) {
                encoded_i += sizeof "&lt;" - 1;
                decoded_i += 1;
                continue;
            }
            if (!tagncmp(&encoded[encoded_i], "&gt;", sizeof "&gt;" - 1)) {
                encoded_i += sizeof "&gt;" - 1;
                decoded_i += 1;
                continue;
            }
            if (!tagncmp(&encoded[encoded_i], "&amp;", sizeof "&amp;" - 1)) {
                encoded_i += sizeof "&amp;" - 1;
                decoded_i += 1;
                continue;
            }
            if (!tagncmp(&encoded[encoded_i], "&apos;", sizeof "&apos;" - 1)) {
                encoded_i += sizeof "&apos;" - 1;
                decoded_i += 1;
                continue;
            }
            if (!tagncmp(&encoded[encoded_i], "&quot;", sizeof "&quot;" - 1)) {
                encoded_i += sizeof "&quot;" - 1;
                decoded_i += 1;
                continue;
            }
            if (!is_xml) {
                if (!tagncmp(&encoded[encoded_i], "&plus;", sizeof "&plus;" - 1)) {
                    encoded_i += sizeof "&quot;" - 1;
                    decoded_i += 1;
                    continue;
                }
                if (!tagncmp(&encoded[encoded_i], "&sol;", sizeof "&sol;" - 1)) {
                    encoded_i += sizeof "&sol;" - 1;
                    decoded_i += 1;
                    continue;
                }
            }
            if (encoded[encoded_i] == '&' && encoded[encoded_i + 1] == '#') {
                encoded_i += 2;
                if (encoded[encoded_i] == 'x' ||
                    (!is_xml && encoded[encoded_i] == 'X'))
                {
                    encoded_i += 1;
                }
                while (encoded[encoded_i] != ';' && encoded[encoded_i] != '\0') {
                    encoded_i += 1;
                }
                decoded_i += 1;
                continue;
            }
        }
        else if (is_xml && !strncmp(&encoded[encoded_i], "]]>", sizeof "]]>" - 1)) {
            in_cdata = false;
            encoded_i += sizeof "]]>" - 1;
            decoded_i += sizeof "]]>" - 1;
            continue;
        }
        encoded_i += 1;
        decoded_i += 1;
    }
}

static struct Minification xmlhtml_decode(const char *input, size_t length, bool is_xml)
{
    // This function helps minify inline scripts and styles in XML (e.g. SVG, MathML, XHTML)
    // documents. We need to decode XML entities and CDATA sections before feeding the tag content
    // into the JavaScript or CSS minifier. Note that XML indeed has only five named entities. In
    // HTML, entities are not effective inside script and style tags and CDATA sections are not
    // supported at all.
    //
    // The routine is based on the assumption that decoding will never make the string longer.
    //
    // The implementation of HTML decoding has only limited capability here, just enough to handle possible
    // encoding of the script type attribute.

    struct Minification m = {.result = malloc(length + 1)};
    if (m.result == NULL) {
        snprintf(m.error, sizeof m.error, "Cannot allocate memory\n");
        return m;
    }

    size_t i = 0;
    size_t result_length = 0;
    bool in_cdata = false;

    while (i < length) {
        if (!in_cdata) {
            if (is_xml && !strncmp(&input[i], "<![CDATA[", sizeof "<![CDATA[" - 1)) {
                in_cdata = true;
                i += sizeof "<![CDATA[" - 1;
                continue;
            }
            if (!strncmp(&input[i], "&lt;", sizeof "&lt;" - 1)) {
                m.result[result_length++] = '<';
                i += sizeof "&lt;" - 1;
                continue;
            }
            if (!strncmp(&input[i], "&gt;", sizeof "&gt;" - 1)) {
                m.result[result_length++] = '>';
                i += sizeof "&gt;" - 1;
                continue;
            }
            if (!strncmp(&input[i], "&amp;", sizeof "&amp;" - 1)) {
                m.result[result_length++] = '&';
                i += sizeof "&amp;" - 1;
                continue;
            }
            if (!strncmp(&input[i], "&apos;", sizeof "&apos;" - 1)) {
                m.result[result_length++] = '\'';
                i += sizeof "&apos;" - 1;
                continue;
            }
            if (!strncmp(&input[i], "&quot;", sizeof "&quot;" - 1)) {
                m.result[result_length++] = '"';
                i += sizeof "&quot;" - 1;
                continue;
            }
            if (!is_xml) {
                if (!strncmp(&input[i], "&plus;", sizeof "&plus;" - 1)) {
                    m.result[result_length++] = '+';
                    i += sizeof "&plus;" - 1;
                    continue;
                }
                if (!strncmp(&input[i], "&sol;", sizeof "&sol;" - 1)) {
                    m.result[result_length++] = '/';
                    i += sizeof "&sol;" - 1;
                    continue;
                }
            }
            if (input[i] == '&' && input[i + 1] == '#') {
                uint_fast32_t codepoint = 0;
                size_t k;
                if (input[i + 2] == 'x' ||
                    (!is_xml && input[i + 2] == 'X'))
                {
                    for (k = 3; input[i + k] != ';'; ++k) {
                        if (input[i + k] >= '0' && input[i + k] <= '9') {
                            codepoint = codepoint * 16 + (input[i + k] - '0');
                        }
                        else if (input[i + k] >= 'A' && input[i + k] <= 'F') {
                            codepoint = codepoint * 16 + (10 + input[i + k] - 'A');
                        }
                        else if (input[i + k] >= 'a' && input[i + k] <= 'f') {
                            codepoint = codepoint * 16 + (10 + input[i + k] - 'a');
                        }
                        else {
                            codepoint = -1;
                            break;
                        }
                    }
                }
                else {
                    for (k = 2; input[i + k] != ';'; ++k) {
                        if (input[i + k] >= '0' && input[i + k] <= '9') {
                            codepoint = codepoint * 10 + (input[i + k] - '0');
                        }
                        else {
                            codepoint = -1;
                            break;
                        }
                    }
                }
                if (codepoint > 0x7FFFFFFF) {
                    m.error_position = i;
                    snprintf(m.error, sizeof m.error,
                        "XML entity with invalid codepoint in line %%zu, column %%zu\n");
                    goto error;
                }

                i += k + 1;

                // See `man utf-8`

                if (codepoint <= 0x7F) {
                    m.result[result_length++] = codepoint;
                }
                else if (codepoint <= 0x7FF) {
                    m.result[result_length++] = 0b11000000 + (codepoint >> 6);
                    m.result[result_length++] = (10 << 6) + (codepoint & 0b111111);
                }
                else if (codepoint <= 0xFFFF) {
                    m.result[result_length++] = 0b11100000 + (codepoint >> 12);
                    m.result[result_length++] = (10 << 6) + ((codepoint >> 6) & 0b111111);
                    m.result[result_length++] = (10 << 6) + (codepoint & 0b111111);
                }
                else if (codepoint <= 0x1FFFFF) {
                    m.result[result_length++] = 0b11110000 + (codepoint >> 18);
                    m.result[result_length++] = (10 << 6) + ((codepoint >> 12) & 0b111111);
                    m.result[result_length++] = (10 << 6) + ((codepoint >> 6) & 0b111111);
                    m.result[result_length++] = (10 << 6) + (codepoint & 0b111111);
                }
                else if (codepoint <= 0x03FFFFFF) {
                    m.result[result_length++] = 0b11111000 + (codepoint >> 24);
                    m.result[result_length++] = (10 << 6) + ((codepoint >> 18) & 0b111111);
                    m.result[result_length++] = (10 << 6) + ((codepoint >> 12) & 0b111111);
                    m.result[result_length++] = (10 << 6) + ((codepoint >> 6) & 0b111111);
                    m.result[result_length++] = (10 << 6) + (codepoint & 0b111111);
                }
                else if (codepoint <= 0x7FFFFFFF) {
                    m.result[result_length++] = 0b1111110 + (codepoint >> 30);
                    m.result[result_length++] = (10 << 6) + ((codepoint >> 24) & 0b111111);
                    m.result[result_length++] = (10 << 6) + ((codepoint >> 18) & 0b111111);
                    m.result[result_length++] = (10 << 6) + ((codepoint >> 12) & 0b111111);
                    m.result[result_length++] = (10 << 6) + ((codepoint >> 6) & 0b111111);
                    m.result[result_length++] = (10 << 6) + (codepoint & 0b111111);
                }
                continue;
            }
            if (is_xml && input[i] == '&') {
                m.error_position = i;
                snprintf(m.error, sizeof m.error, "Invalid XML entity in line %%zu, column %%zu\n");
                goto error;
            }
        }
        else if (is_xml && !strncmp(&input[i], "]]>", sizeof "]]>" - 1)) {
            in_cdata = false;
            i += sizeof "]]>" - 1;
            continue;
        }
        m.result[result_length++] = input[i++];
    }
    m.result[result_length++] = '\0';
    return m;

error:
    free(m.result);
    m.result = NULL;
    return m;
}

static struct EncodedString
{
    char *data;
    size_t length;
}
xml_encode(const char *input, const size_t input_length)
{
    size_t added_length_with_cdata = sizeof "<![CDATA[]]>" - 1;
    size_t added_length_with_entities = 0;
    size_t i = 0;
    while (true) {
        if (input[i] == '\0') {
            break;
        }
        else if (input[i] == '<') {
            added_length_with_entities += sizeof "&lt;" - 2;
        }
        else if (input[i] == '>') {
            added_length_with_entities += sizeof "&gt;" - 2;
        }
        else if (input[i] == '&') {
            added_length_with_entities += sizeof "&amp;" - 2;
        }
        else if (input[i] == ']' && input[i + 1] == ']' && input[i + 2] == '>') {
            added_length_with_cdata += sizeof "]]><![CDATA[" - 1;
        }
        i += 1;
    }
    if (added_length_with_entities == 0) {
        struct EncodedString encoded = {
            malloc(input_length + 1),
            input_length
        };
        if (encoded.data == NULL) {
            return encoded;
        }
        memcpy(encoded.data, input, input_length);
        encoded.data[input_length] = '\0';
        return encoded;
    }
    if (added_length_with_cdata < added_length_with_entities + 1) {
        struct EncodedString encoded = {malloc(input_length + added_length_with_cdata + 1), 0};
        if (encoded.data == NULL) {
            return encoded;
        }
        strcpy(encoded.data, "<![CDATA[");
        encoded.length = sizeof "<![CDATA[" - 1;
        for (i = 0; input[i] != '\0'; ++i) {
            if (!strncmp(&input[i], "]]>", sizeof "]]>" - 1)) {
                strcpy(&encoded.data[encoded.length], "]]]]><![CDATA[>");
                encoded.length = sizeof "]]]]><![CDATA[>" - 1;
                i += 2;
            }
            else {
                encoded.data[encoded.length] = input[i];
                encoded.length += 1;
            }
        }
        strcpy(&encoded.data[encoded.length], "]]>");
        encoded.length = sizeof "]]>" - 1;
        return encoded;
    }
    else {
        struct EncodedString encoded = {malloc(input_length + added_length_with_cdata + 1), 0};
        if (encoded.data == NULL) {
            return encoded;
        }
        encoded.length = 0;
        for (i = 0; input[i] != '\0'; ++i) {
            if (input[i] == '<') {
                strcpy(&encoded.data[encoded.length], "&lt;");
                encoded.length += sizeof "&lt;" - 1;
            }
            else if (input[i] == '>') {
                strcpy(&encoded.data[encoded.length], "&gt;");
                encoded.length += sizeof "&gt;" - 1;
            }
            else if (input[i] == '&') {
                strcpy(&encoded.data[encoded.length], "&amp;");
                encoded.length += sizeof "&amp;" - 1;
            }
            else {
                encoded.data[encoded.length] = input[i];
                encoded.length += 1;
            }
        }
        encoded.data[encoded.length] = '\0';
        return encoded;
    }
}

static struct Minification minify_xmlhtml(const char *xmlhtml, bool is_xml)
{
    size_t input_strlen = strlen(xmlhtml);
    struct Minification m = {.result = malloc(input_strlen + 1)};
    if (m.result == NULL) {
        snprintf(m.error, sizeof m.error, "Cannot allocate memory\n");
        return m;
    }

    enum {
        SYNTAX_BLOCK_TAG,
        SYNTAX_BLOCK_CONTENT,
        SYNTAX_BLOCK_DOCTYPE,
    } syntax_block = SYNTAX_BLOCK_CONTENT;

    enum {
        SCRIPT_TYPE_JAVASCRIPT,
        SCRIPT_TYPE_MODULE,
        SCRIPT_TYPE_JSON,
        SCRIPT_TYPE_OTHER
    } script_type;

    size_t i = 0;
    const char *current_tag = NULL;
    size_t current_tag_length = 0;
    bool is_closing_tag = false;
    bool has_whitespace_before_tag = false;
    int (*tagncmp)(const char *, const char *, size_t) = (is_xml ? strncmp : strnicmp);
    const char *value = NULL, *attribute = NULL;
    size_t value_length = 0, attribute_length = 0;
    size_t result_length = 0;

    while (true) {
        // Beginning of inline minification

        const char *tag_content_delimiter = NULL;
        struct Minification (*tag_content_minify_callback)(const char *) = NULL;

        if (syntax_block == SYNTAX_BLOCK_CONTENT &&
            current_tag_length == sizeof "script" - 1 &&
            !tagncmp(current_tag, "script", sizeof "script" - 1))
        {
            tag_content_delimiter = "</script";
            if (script_type == SCRIPT_TYPE_JAVASCRIPT) {
                tag_content_minify_callback = minify_js_with_options;
            }
            else if (script_type == SCRIPT_TYPE_MODULE) {
                tag_content_minify_callback = minify_js_module_with_options;
            }
            else if (script_type == SCRIPT_TYPE_JSON) {
                tag_content_minify_callback = minify_json;
            }
            else if (script_type == SCRIPT_TYPE_OTHER) {
                tag_content_minify_callback = NULL;
            }
        }
        if (syntax_block == SYNTAX_BLOCK_CONTENT &&
            current_tag_length == sizeof "style" - 1 &&
            !tagncmp(current_tag, "style", sizeof "style" - 1))
        {
            tag_content_delimiter = "</style";
            tag_content_minify_callback = minify_css;
        }
        if (tag_content_delimiter != NULL) {
            size_t content_start_i = i;
            bool in_cdata = false;
            while (true) {
                if (xmlhtml[i] == '\0') {
                    do {
                        i -= 1;
                    } while (is_whitespace(xmlhtml[i - 1]));
                    m.error_position = i - 1;
                    snprintf(m.error, sizeof m.error,
                        "Unexpected end of document, expected `%s>` after line %%zu, column %%zu\n",
                        tag_content_delimiter);
                    goto error;
                }
                if (is_xml &&
                    !strncmp(&xmlhtml[i], "<![CDATA[", sizeof "<![CDATA[" - 1))
                {
                    in_cdata = true;
                    i += sizeof "<![CDATA[" - 1;
                    continue;
                }
                if (is_xml &&
                    !strncmp(&xmlhtml[i], "]]>", sizeof "]]>" - 1))
                {
                    in_cdata = false;
                    i += sizeof "]]>" - 1;
                    continue;
                }
                if (!in_cdata &&
                    !tagncmp(&xmlhtml[i], tag_content_delimiter, sizeof tag_content_delimiter - 1))
                {
                    current_tag_length = 0;
                    break;
                }
                i += 1;
            }
            if (tag_content_minify_callback == NULL) {
                memcpy(&m.result[result_length], &xmlhtml[content_start_i], i - content_start_i);
                result_length += i - content_start_i;
                continue;
            }

            struct Minification inline_m;
            if (is_xml) {
                inline_m = xmlhtml_decode(&xmlhtml[content_start_i], i - content_start_i, true);
                if (inline_m.result == NULL) {
                    inline_m.error_position += content_start_i;
                    goto error;
                }
                char *input_to_free = inline_m.result;
                inline_m = tag_content_minify_callback(inline_m.result);
                free(input_to_free);
                if (inline_m.result == NULL) {
                    xmlhtml_correct_error_position(&xmlhtml[content_start_i], inline_m.result,
                        &m.error_position, is_xml);
                    inline_m.error_position += content_start_i;
                    goto error;
                }
                struct EncodedString encoded = xml_encode(inline_m.result, i - content_start_i);
                free(inline_m.result);
                if (encoded.data == NULL) {
                    inline_m.result = NULL;
                    goto error;
                }
                inline_m.result = encoded.data;
                if (encoded.length > i - content_start_i) {
                    char *result_realloc = realloc(
                        inline_m.result, input_strlen + encoded.length - i + content_start_i
                    );
                    if (result_realloc == NULL) {
                        goto error;
                    }
                    inline_m.result = result_realloc;
                }
            }
            else {
                char *tag_content = malloc(i - content_start_i + 1);
                if (tag_content == NULL) {
                    goto error;
                }
                memcpy(tag_content, &xmlhtml[content_start_i], i - content_start_i);
                tag_content[i - content_start_i] = '\0';
                inline_m = tag_content_minify_callback(tag_content);
                free(tag_content);
                if (inline_m.result == NULL) {
                    inline_m.error_position += content_start_i;
                    goto error;
                }
            }
            size_t inline_result_length = strlen(inline_m.result);
            memcpy(&m.result[result_length], inline_m.result, inline_result_length);
            result_length += inline_result_length;
            free(inline_m.result);
            continue;
        }

        // End of inline minification

        if (xmlhtml[i] == '\0') {
            if (syntax_block == SYNTAX_BLOCK_TAG) {
                m.error_position = i;
                snprintf(m.error, sizeof m.error,
                    "Unexpected end of document expected `>` after line %%zu, column %%zu\n");
                goto error;
            }
            m.result[result_length] = '\0';
            break;
        }
        if (!strncmp(&xmlhtml[i], "<!--", 4)) {
            size_t comment_start_i = i;
            i += 4;
            while (xmlhtml[i] != '\0' && strncmp(&xmlhtml[i], "-->", 3)) {
                i += 1;
            }
            if (xmlhtml[i] == '\0') {
                m.error_position = comment_start_i;
                snprintf(m.error, sizeof m.error, "Unclosed comment starting in line %%zu, column %%zu\n");
                goto error;
            }
            i += 3;

            // Trim whitespace at the end of the document

            size_t k = i;
            while (is_whitespace(xmlhtml[k])) {
                k += 1;
            }
            if (xmlhtml[k] == '\0') {
                i = k;
            }
            continue;
        }
        if (is_xml && !strncmp(&xmlhtml[i], "<![CDATA[", sizeof "<![CDATA[" - 1)) {
            memcpy(&m.result[result_length], "<![CDATA[", sizeof "<![CDATA[" - 1);
            result_length += sizeof "<![CDATA[" - 1;
            i += sizeof "<![CDATA[" - 1;
            while (true) {
                if (!strncmp(&xmlhtml[i], "]]>", sizeof "]]>" - 1)) {
                    memcpy(&m.result[result_length], "]]>", sizeof "]]>" - 1);
                    result_length += sizeof "]]>" - 1;
                    i += sizeof "]]>" - 1;
                    break;
                }
                m.result[result_length++] = xmlhtml[i];
                i += 1;
            }
            continue;
        }
        if (xmlhtml[i] == '<') {
            // Consume `<` and tag name
            if (syntax_block == SYNTAX_BLOCK_TAG) {
                m.error_position = i;
                snprintf(m.error, sizeof m.error, "Illegal `<` in line %%zu, column %%zu\n");
                goto error;
            }
            has_whitespace_before_tag =
                result_length > 0 && is_whitespace(m.result[result_length - 1]);
            m.result[result_length++] = '<';
            i += 1;
            if (!strnicmp(&xmlhtml[i], "!DOCTYPE", sizeof "!DOCTYPE" - 1)) {
                syntax_block = SYNTAX_BLOCK_DOCTYPE;
                continue;
            }

            current_tag = &xmlhtml[i];
            is_closing_tag = (xmlhtml[i] == '/');
            if (is_closing_tag) {
                m.result[result_length++] = '/';
                i += 1;
            }
            if (!(
                (xmlhtml[i] >= 'a' && xmlhtml[i] <= 'z') ||
                (xmlhtml[i] >= 'A' && xmlhtml[i] <= 'Z') ||
                xmlhtml[i] == ':' || xmlhtml[i] == '_' ||
                xmlhtml[i] == '?'
            )) {
                m.error_position = i - 1;
                snprintf(m.error, sizeof m.error,
                    "`%c` in line %%zu, column %%zu is followed by an illegal character\n", xmlhtml[i - 1]);
                goto error;
            }
            m.result[result_length++] = xmlhtml[i];
            current_tag_length = 1;
            while (
                (xmlhtml[i + current_tag_length] >= 'a' &&
                 xmlhtml[i + current_tag_length] <= 'z') ||
                (xmlhtml[i + current_tag_length] >= 'A' &&
                 xmlhtml[i + current_tag_length] <= 'Z') ||
                (xmlhtml[i + current_tag_length] >= '0' &&
                 xmlhtml[i + current_tag_length] <= '9') ||
                xmlhtml[i + current_tag_length] == '-'
            ) {
                current_tag_length += 1;
                m.result[result_length++] = xmlhtml[i + current_tag_length - 1];
            }
            if (
                xmlhtml[i + current_tag_length] != '/' && xmlhtml[i + current_tag_length] != '>' &&
                !is_whitespace(xmlhtml[i + current_tag_length])
            ) {
                m.error_position = i + current_tag_length;
                snprintf(m.error, sizeof m.error,
                    "Illegal character in tag name in in line %%zu, column %%zu\n");
                goto error;
            }
            syntax_block = SYNTAX_BLOCK_TAG;
            script_type = SCRIPT_TYPE_JAVASCRIPT;
            attribute_length = 0;
            i += current_tag_length;
            continue;
        }
        if (xmlhtml[i] == '>') {
            if (syntax_block == SYNTAX_BLOCK_CONTENT) {
                m.error_position = i;
                snprintf(m.error, sizeof m.error, "Illegal `>` in line %%zu, column %%zu\n");
                goto error;
            }
            if (xmlhtml[i - 1] == '/' && m.result[result_length - 1] != '=') {
                is_closing_tag = true;
            }

            // Transform `<xmlhtml></xmlhtml>` to `<xmlhtml/>`. This would be illegal for HTML.

            else if (is_xml &&
                     syntax_block != SYNTAX_BLOCK_DOCTYPE &&
                     xmlhtml[i + 1] == '<' && xmlhtml[i + 2] == '/' &&
                     !strncmp(current_tag, &xmlhtml[i + 3], current_tag_length) &&
                     (is_whitespace(xmlhtml[i + 3 + current_tag_length]) ||
                     xmlhtml[i + 3 + current_tag_length] == '>'))
            {
                m.result[result_length++] = '/';
                m.result[result_length++] = '>';
                i += 3 + current_tag_length;
                while (xmlhtml[i] != '>' && xmlhtml[i] != '\0') {
                    i += 1;
                }
                syntax_block = SYNTAX_BLOCK_CONTENT;
                current_tag_length = 0;
                i += 1;
                continue;
            }

            i += 1;
            syntax_block = SYNTAX_BLOCK_CONTENT;

            if (is_xml) {
                // Ignore whitespace except between opening and closing tags

                size_t k = i;
                while (is_whitespace(xmlhtml[k])) {
                    k += 1;
                }
                if (xmlhtml[k] == '<' && (is_closing_tag || xmlhtml[k + 1] != '/')) {
                    i = k;
                }
            }

            m.result[result_length++] = '>';

            // Trim whitespace at the end of the document

            size_t k = i;
            while (is_whitespace(xmlhtml[k])) {
                k += 1;
            }
            if (xmlhtml[k] == '\0') {
                i = k;
            }
            continue;
        }
        if (syntax_block == SYNTAX_BLOCK_TAG && is_whitespace(xmlhtml[i])) {
            while (is_whitespace(xmlhtml[i])) {
                i += 1;
            }
            if (xmlhtml[i] != '=' && m.result[result_length - 1] != '=' && xmlhtml[i] != '>' &&
                xmlhtml[i] != '/')
            {
                m.result[result_length++] = ' ';
            }
            if (is_closing_tag && xmlhtml[i] != '>') {
                m.error_position = i;
                snprintf(m.error, sizeof m.error,
                    "Illegal content in line %%zu, column %%zu after whitespace in closing tag\n");
                goto error;
            }
            continue;
        }
        if (syntax_block == SYNTAX_BLOCK_TAG && xmlhtml[i] != '=') {
            // Consume attribute
            if (xmlhtml[i] == '"' || xmlhtml[i] == '\'') {
                m.error_position = i;
                snprintf(m.error, sizeof m.error,
                    "Illegal character `%c` in line %%zu, column %%zu\n", xmlhtml[i]);
                goto error;
            }
            attribute = &xmlhtml[i];
            attribute_length = 0;
            while (strchr("\"' \t\r\n<>=/", xmlhtml[i]) == NULL) {
                m.result[result_length++] = xmlhtml[i];
                attribute_length += 1;
                i += 1;
            }
            if (xmlhtml[i] == '/' && xmlhtml[i + 1] != '>') {
                m.error_position = i;
                snprintf(m.error, sizeof m.error, "`/` in line %%zu, column %%zu is not followed by `>` \n");
                goto error;
            }
            if (xmlhtml[i] == '/' &&xmlhtml[i + 1] == '>') {
                if (is_xml) {
                    m.result[result_length++] = '/';
                }
                i += 1;
                continue;
            }
            if (strchr("=> \r\t\n/", xmlhtml[i]) == NULL) {
                m.error_position = i;
                snprintf(m.error, sizeof m.error,
                    "Illegal character `%c` after attribute in line %%zu, column %%zu\n", xmlhtml[i]);
                goto error;
            }
            continue;
        }
        if (syntax_block == SYNTAX_BLOCK_TAG && xmlhtml[i] == '=') {
            // Consume `=` followed by quoted or unquoted value
            if (attribute_length == 0) {
                m.error_position = i;
                snprintf(m.error, sizeof m.error, "No attribute before `=` in line %%zu, column %%zu\n");
                goto error;
            }

            i += 1;
            while (is_whitespace(xmlhtml[i])) {
                i += 1;
            }
            if (xmlhtml[i] == '=' || xmlhtml[i] == '>') {
                m.error_position = i;
                snprintf(m.error, sizeof m.error, "No value after `=` in line %%zu, column %%zu\n");
                goto error;
            }
            if (is_xml && xmlhtml[i] != '"' && xmlhtml[i] != '\'') {
                m.error_position = i;
                snprintf(m.error, sizeof m.error,
                    "XML requires a quote after `=` in line %%zu, column %%zu\n");
                goto error;
            }

            m.result[result_length++] = '=';
            if (xmlhtml[i] == '"' || xmlhtml[i] == '\'') {
                char quote = xmlhtml[i];
                size_t string_start_i = i;
                i += 1;
                value = &xmlhtml[i];
                value_length = 0;
                bool need_quotes;
                if (is_xml || syntax_block == SYNTAX_BLOCK_DOCTYPE) {
                    need_quotes = true;
                }
                else if (xmlhtml[i] == quote) {
                    need_quotes = true;
                }
                else {
                    need_quotes = false;
                    size_t k = i;
                    while (xmlhtml[k] != quote) {
                        if (strchr(" \r\t\n=\"'/", xmlhtml[k]) != NULL) {
                            need_quotes = true;
                            break;
                        }
                        k += 1;
                    }
                }
                if (need_quotes) {
                    m.result[result_length++] = quote;
                }
                while (xmlhtml[i] != '\0' && xmlhtml[i] != quote) {
                    m.result[result_length++] = xmlhtml[i];
                    value_length += 1;
                    i += 1;
                }
                if (xmlhtml[i] == '\0') {
                    m.error_position = string_start_i;
                    snprintf(m.error, sizeof m.error, "Unclosed string starting in line %%zu, column %%zu\n");
                    goto error;
                }
                i += 1;
                if (
                    !is_whitespace(xmlhtml[i]) && xmlhtml[i] != '>' &&
                    (xmlhtml[i] != '/' || xmlhtml[i + 1] != '>') &&
                    (xmlhtml[i] != '?' || xmlhtml[i + 1] != '>')
                ) {
                    m.error_position = i;
                    snprintf(m.error, sizeof m.error,
                        "Illegal character after `%c` in line %%zu, column %%zu\n", quote);
                    goto error;
                }
                if (need_quotes) {
                    m.result[result_length++] = quote;
                }
            }
            else {
                value = &xmlhtml[i];
                value_length = 0;
                while (strchr(" \r\t\n >=\"'", xmlhtml[i]) == NULL) {
                    m.result[result_length++] = xmlhtml[i];
                    i += 1;
                    value_length += 1;
                }
            }

            // Checking script type

            struct Minification decoded_value = xmlhtml_decode(value, value_length, false);
            if (decoded_value.result == NULL) {
                m.error_position = i + decoded_value.error_position;
                m.result = decoded_value.result;
                goto error;
            }
            if (current_tag_length == sizeof "script" - 1 &&
                !tagncmp(current_tag, "script", sizeof "script" - 1) &&
                attribute_length == sizeof "type" - 1 && !tagncmp(attribute, "type", sizeof "type" - 1))
            {
                if (!strcmp(decoded_value.result, "application/json+ld")) {
                    script_type = SCRIPT_TYPE_JSON;
                }
                else if (!strcmp(decoded_value.result, "importmap")) {
                    script_type = SCRIPT_TYPE_JSON;
                }
                else if (!strcmp(decoded_value.result, "module")) {
                    script_type = SCRIPT_TYPE_MODULE;
                }
                else if (!strcmp(decoded_value.result, "text/javascript")) {
                    script_type = SCRIPT_TYPE_JAVASCRIPT;
                }
                else {
                    script_type = SCRIPT_TYPE_OTHER;
                }
            }
            free(decoded_value.result);
            continue;
        }
        if (!is_xml && syntax_block == SYNTAX_BLOCK_CONTENT && is_whitespace(xmlhtml[i])) {
            if (current_tag_length == sizeof "pre" - 1 &&
                !strnicmp(current_tag, "pre", sizeof "pre" - 1))
            {
                m.result[result_length++] = xmlhtml[i];
                i += 1;
                continue;
            }
            while (is_whitespace(xmlhtml[i])) {
                i += 1;
            }
            if (has_whitespace_before_tag && m.result[result_length - 1] == '>') {
                continue;
            }
            m.result[result_length++] = ' ';
            continue;
        }
        m.result[result_length++] = xmlhtml[i];
        i += 1;
    }
    return m;

error:
    free(m.result);
    m.result = NULL;
    return m;
}

struct Minification minify_xml(const char *xml)
{
    return minify_xmlhtml(xml, true);
}

struct Minification minify_html(const char *html)
{
    return minify_xmlhtml(html, false);
}

struct LineColumn
{
    size_t line;
    size_t column;
};

struct LineColumn position_to_line_column(const char *text, size_t position)
{
    struct LineColumn lc = {.line = 1, .column = 0};
    for (size_t i = 0; i <= position; ++i) {
        if (text[i] == '\n') {
            lc.line += 1;
            lc.column = 0;
        }
        else {
            lc.column += 1;
        }
    }
    return lc;
}

int main(int argc, const char *argv[])
{
    bool benchmark = false;
    bool print_usage = false;
    const char *format_str = NULL;
    const char *input_filename = NULL;
    enum {FORMAT_JS, FORMAT_CSS, FORMAT_XML, FORMAT_HTML, FORMAT_JSON} format;

    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--benchmark")) {
            benchmark = true;
        }
        else if (!strcmp(argv[i], "--mangle-js-identifiers")) {
            option_mangle_js_identifiers = true;
        }
        else if (format_str == NULL) {
            format_str = argv[i];
        }
        else if (input_filename == NULL) {
            input_filename = argv[i];
        }
        else {
            print_usage = true;
            break;
        }
    }
    if (format_str == NULL || input_filename == NULL) {
        print_usage = true;
    }
    else if (!strcmp(format_str, "js")) {
        format = FORMAT_JS;
    }
    else if (!strcmp(format_str, "css")) {
        format = FORMAT_CSS;
    }
    else if (!strcmp(format_str, "xml")) {
        format = FORMAT_XML;
    }
    else if (!strcmp(format_str, "html")) {
        format = FORMAT_HTML;
    }
    else if (!strcmp(format_str, "json")) {
        format = FORMAT_JSON;
    }
    else {
        fprintf(stderr, "Unsupported input format: %s\n", format_str);
        print_usage = true;
    }

    if (print_usage) {
        fputs("Usage: ", stderr);
        fputs(argv[0], stderr);
        fputs(" <css|js|xml|html|json> <input file|-> [--benchmark] "
            "[--mangle-js-identifiers]\n", stderr);
        return EXIT_FAILURE;
    }

    char *input = file_get_content(input_filename);
    if (input == NULL) {
        perror(input_filename);
        return EXIT_FAILURE;
    }
    struct Minification m;
    switch (format) {
    case FORMAT_JS:
        m = minify_js_with_options(input);
        break;
    case FORMAT_CSS:
        m = minify_css(input);
        break;
    case FORMAT_XML:
        m = minify_xml(input);
        break;
    case FORMAT_HTML:
        m = minify_html(input);
        break;
    case FORMAT_JSON:
    default:
        m = minify_json(input);
        break;
    }
    if (m.result == NULL) {
        struct LineColumn line_column = position_to_line_column(input, m.error_position);
        free(input);
        fprintf(stderr, m.error, line_column.line, line_column.column);
        free(m.result);
        return EXIT_FAILURE;
    }
    if (benchmark) {
        size_t strlen_input = strlen(input);
        size_t strlen_minification = strlen(m.result);
        printf(
            "Reduced the size by %.1f%% from %zu to %zu bytes\n",
            100.0 - 100.0 * strlen_minification / strlen_input, strlen_input, strlen_minification
        );
    }
    else {
        fputs(m.result, stdout);
    }
    free(m.result);
    free(input);
    return EXIT_SUCCESS;
}
