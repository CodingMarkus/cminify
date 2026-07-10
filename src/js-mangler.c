/*
 * Copyright 2026 CodingMarkus
 *
 * SPDX-License-Identifier: ISC
 */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "js-mangler.h"

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
        (c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z');
}

static bool js_identifier_part(char c)
{
    return js_identifier_start(c) || (c >= '0' && c <= '9');
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
            binding = binding || (
                containers[containers_length - 1] == '{' &&
                (previous == ':' || previous == '{' || previous == ',' ||
                rest_element));
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

struct Minification mangle_js_identifiers(const char *js,
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
