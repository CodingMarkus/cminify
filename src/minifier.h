/*
 * Copyright 2026 CodingMarkus
 * SPDX-License-Identifier: ISC
 */

#ifndef WEBMINCER_MINIFIER_H
#define WEBMINCER_MINIFIER_H

#include <stdbool.h>
#include <stddef.h>

struct Minification
{
    char *result;
    char error[256];
    size_t error_position;
};

enum CommentVariant {COMMENT_VARIANT_CSS, COMMENT_VARIANT_JS};

bool is_whitespace(char c);
int strnicmp(const char *s1, const char *s2, size_t length);
bool skip_whitespaces_comments(struct Minification *m, const char *input,
    size_t *i, char *min, size_t *min_length,
    enum CommentVariant comment_variant);
struct Minification minify_css(const char *css);
struct Minification minify_html(const char *html);
struct Minification minify_js(const char *js);
struct Minification minify_js_with_options(const char *js);
struct Minification minify_js_module_with_options(const char *js);
struct Minification minify_json(const char *json);
struct Minification minify_xml(const char *xml);

extern bool option_mangle_js_identifiers;

#endif
