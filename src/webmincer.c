/*
 * Copyright 2026 CodingMarkus
 * Copyright 2024-2026 Jumping-Beaver
 *
 * SPDX-License-Identifier: ISC
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "minifier.h"

bool option_mangle_js_identifiers = false;


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
