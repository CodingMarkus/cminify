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

static bool ensure_result_capacity(char **result, size_t *capacity, size_t needed_length)
{
    if (needed_length + 1 <= *capacity) {
        return true;
    }
    size_t new_capacity = *capacity;
    while (needed_length + 1 > new_capacity) {
        new_capacity *= 2;
    }
    char *result_realloc = realloc(*result, new_capacity);
    if (result_realloc == NULL) {
        return false;
    }
    *result = result_realloc;
    *capacity = new_capacity;
    return true;
}

static bool xmlhtml_is_event_handler_attribute(const char *attribute, size_t attribute_length, bool is_xml)
{
    if (is_xml) {
        return false;
    }
    const char *event_handler_attributes[] = {
        "onabort",
        "onafterprint",
        "onauxclick",
        "onbeforematch",
        "onbeforeprint",
        "onbeforetoggle",
        "onbeforeunload",
        "onblur",
        "oncancel",
        "oncanplay",
        "oncanplaythrough",
        "onchange",
        "onclick",
        "onclose",
        "oncommand",
        "oncontextlost",
        "oncontextmenu",
        "oncontextrestored",
        "oncopy",
        "oncuechange",
        "oncut",
        "ondblclick",
        "ondrag",
        "ondragend",
        "ondragenter",
        "ondragleave",
        "ondragover",
        "ondragstart",
        "ondrop",
        "ondurationchange",
        "onemptied",
        "onended",
        "onerror",
        "onfocus",
        "onformdata",
        "onhashchange",
        "oninput",
        "oninvalid",
        "onkeydown",
        "onkeypress",
        "onkeyup",
        "onlanguagechange",
        "onload",
        "onloadeddata",
        "onloadedmetadata",
        "onloadstart",
        "onmessage",
        "onmessageerror",
        "onmousedown",
        "onmouseenter",
        "onmouseleave",
        "onmousemove",
        "onmouseout",
        "onmouseover",
        "onmouseup",
        "onoffline",
        "ononline",
        "onpagehide",
        "onpageshow",
        "onpaste",
        "onpause",
        "onplay",
        "onplaying",
        "onpopstate",
        "onprogress",
        "onratechange",
        "onrejectionhandled",
        "onreset",
        "onresize",
        "onscroll",
        "onscrollend",
        "onsecuritypolicyviolation",
        "onseeked",
        "onseeking",
        "onselect",
        "onslotchange",
        "onstalled",
        "onstorage",
        "onsubmit",
        "onsuspend",
        "ontimeupdate",
        "ontoggle",
        "onunhandledrejection",
        "onunload",
        "onvolumechange",
        "onwaiting",
        "onwheel",
    };
    for (size_t i = 0; i < sizeof event_handler_attributes /
        sizeof *event_handler_attributes; ++i)
    {
        if (attribute_length == strlen(event_handler_attributes[i]) &&
            !strnicmp(attribute, event_handler_attributes[i], attribute_length))
        {
            return true;
        }
    }
    return false;
}

static bool xmlhtml_is_javascript_url(const char *value, bool is_xml)
{
    return !is_xml && !strnicmp(value, "javascript:", sizeof "javascript:" - 1);
}

static struct Minification minify_js_handler(const char *js)
{
    const char prefix[] = "function a(){";
    const char suffix[] = "}";
    size_t js_length = strlen(js);
    char *wrapped_js = malloc(sizeof prefix - 1 + js_length + sizeof suffix);
    struct Minification m = {.result = NULL};
    if (wrapped_js == NULL) {
        snprintf(m.error, sizeof m.error, "Cannot allocate memory\n");
        return m;
    }
    memcpy(wrapped_js, prefix, sizeof prefix - 1);
    memcpy(&wrapped_js[sizeof prefix - 1], js, js_length);
    memcpy(&wrapped_js[sizeof prefix - 1 + js_length], suffix, sizeof suffix);

    m = minify_js(wrapped_js);
    free(wrapped_js);
    if (m.result == NULL) {
        if (m.error_position >= sizeof prefix - 1) {
            m.error_position -= sizeof prefix - 1;
        }
        else {
            m.error_position = 0;
        }
        return m;
    }

    size_t result_length = strlen(m.result);
    if (
        result_length < sizeof prefix + sizeof suffix - 2 ||
        strncmp(m.result, prefix, sizeof prefix - 1) ||
        strcmp(&m.result[result_length - (sizeof suffix - 1)], suffix)
    ) {
        free(m.result);
        m.result = NULL;
        m.error_position = 0;
        snprintf(m.error, sizeof m.error, "Unexpected internal JavaScript minification result\n");
        return m;
    }
    size_t body_length = result_length - (sizeof prefix - 1) - (sizeof suffix - 1);
    memmove(m.result, &m.result[sizeof prefix - 1], body_length);
    m.result[body_length] = '\0';
    return m;
}

static struct EncodedAttribute
{
    char *data;
    size_t length;
    char quote;
}
xmlhtml_encode_attribute(const char *input, char preferred_quote, bool is_xml, bool allow_unquoted)
{
    size_t input_length = strlen(input);
    bool need_quotes = is_xml || !allow_unquoted || input_length == 0;
    size_t single_quotes = 0, double_quotes = 0;
    for (size_t i = 0; i < input_length; ++i) {
        if (input[i] == '&' || input[i] == '<') {
            need_quotes = true;
        }
        if (!need_quotes &&
            (is_whitespace(input[i]) || strchr("\"'=<>`", input[i]) != NULL))
        {
            need_quotes = true;
        }
        if (input[i] == '\'') {
            single_quotes += 1;
        }
        else if (input[i] == '"') {
            double_quotes += 1;
        }
    }

    char quote = '\0';
    if (need_quotes) {
        if (preferred_quote == '"' || preferred_quote == '\'') {
            quote = preferred_quote;
        }
        else if (double_quotes <= single_quotes) {
            quote = '"';
        }
        else {
            quote = '\'';
        }
    }

    size_t output_length = input_length;
    for (size_t i = 0; i < input_length; ++i) {
        if (input[i] == '&') {
            output_length += sizeof "&amp;" - 2;
        }
        else if (input[i] == '<') {
            output_length += sizeof "&lt;" - 2;
        }
        else if (quote == '"' && input[i] == '"') {
            output_length += sizeof "&quot;" - 2;
        }
        else if (quote == '\'' && input[i] == '\'') {
            output_length += sizeof "&apos;" - 2;
        }
    }

    struct EncodedAttribute encoded = {
        .data = malloc(output_length + 1),
        .length = 0,
        .quote = quote,
    };
    if (encoded.data == NULL) {
        return encoded;
    }

    for (size_t i = 0; i < input_length; ++i) {
        if (input[i] == '&') {
            strcpy(&encoded.data[encoded.length], "&amp;");
            encoded.length += sizeof "&amp;" - 1;
        }
        else if (input[i] == '<') {
            strcpy(&encoded.data[encoded.length], "&lt;");
            encoded.length += sizeof "&lt;" - 1;
        }
        else if (quote == '"' && input[i] == '"') {
            strcpy(&encoded.data[encoded.length], "&quot;");
            encoded.length += sizeof "&quot;" - 1;
        }
        else if (quote == '\'' && input[i] == '\'') {
            strcpy(&encoded.data[encoded.length], "&apos;");
            encoded.length += sizeof "&apos;" - 1;
        }
        else {
            encoded.data[encoded.length++] = input[i];
        }
    }
    encoded.data[encoded.length] = '\0';
    return encoded;
}

static struct Minification minify_xmlhtml(const char *xmlhtml, bool is_xml)
{
    size_t input_strlen = strlen(xmlhtml);
    size_t result_capacity = input_strlen + 1;
    struct Minification m = {.result = malloc(result_capacity)};
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
            if (!ensure_result_capacity(&m.result, &result_capacity, result_length + inline_result_length)) {
                free(inline_m.result);
                goto error;
            }
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
            char original_quote = '\0';
            bool copy_raw_value = true;
            bool original_need_quotes = false;
            if (xmlhtml[i] == '"' || xmlhtml[i] == '\'') {
                char quote = xmlhtml[i];
                size_t string_start_i = i;
                i += 1;
                value = &xmlhtml[i];
                value_length = 0;
                original_quote = quote;
                if (is_xml || syntax_block == SYNTAX_BLOCK_DOCTYPE) {
                    original_need_quotes = true;
                }
                else if (xmlhtml[i] == quote) {
                    original_need_quotes = true;
                }
                else {
                    original_need_quotes = false;
                    size_t k = i;
                    while (xmlhtml[k] != quote) {
                        if (strchr(" \r\t\n=\"'/", xmlhtml[k]) != NULL) {
                            original_need_quotes = true;
                            break;
                        }
                        k += 1;
                    }
                }
                while (xmlhtml[i] != '\0' && xmlhtml[i] != quote) {
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
            }
            else {
                value = &xmlhtml[i];
                value_length = 0;
                while (strchr(" \r\t\n >=\"'", xmlhtml[i]) == NULL) {
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
            if (xmlhtml_is_event_handler_attribute(attribute, attribute_length, is_xml)) {
                copy_raw_value = false;
                struct Minification minified_js = minify_js_handler(decoded_value.result);
                if (minified_js.result == NULL) {
                    size_t error_position = minified_js.error_position;
                    xmlhtml_correct_error_position(value, decoded_value.result, &error_position, false);
                    m.error_position = value - xmlhtml + error_position;
                    strncpy(m.error, minified_js.error, sizeof m.error);
                    m.error[sizeof m.error - 1] = '\0';
                    free(decoded_value.result);
                    goto error;
                }
                struct EncodedAttribute encoded_value = xmlhtml_encode_attribute(
                    minified_js.result,
                    original_quote,
                    is_xml,
                    syntax_block != SYNTAX_BLOCK_DOCTYPE
                );
                free(minified_js.result);
                if (encoded_value.data == NULL) {
                    free(decoded_value.result);
                    goto error;
                }
                if (!ensure_result_capacity(
                    &m.result,
                    &result_capacity,
                    result_length + encoded_value.length + 2
                )) {
                    free(encoded_value.data);
                    free(decoded_value.result);
                    goto error;
                }
                if (encoded_value.quote != '\0') {
                    m.result[result_length++] = encoded_value.quote;
                }
                memcpy(&m.result[result_length], encoded_value.data, encoded_value.length);
                result_length += encoded_value.length;
                if (encoded_value.quote != '\0') {
                    m.result[result_length++] = encoded_value.quote;
                }
                free(encoded_value.data);
            }
            else if (xmlhtml_is_javascript_url(decoded_value.result, is_xml)) {
                copy_raw_value = false;
                struct Minification minified_js =
                    minify_js(&decoded_value.result[sizeof "javascript:" - 1]);
                if (minified_js.result == NULL) {
                    size_t error_position = sizeof "javascript:" - 1 + minified_js.error_position;
                    xmlhtml_correct_error_position(value, decoded_value.result, &error_position, false);
                    m.error_position = value - xmlhtml + error_position;
                    strncpy(m.error, minified_js.error, sizeof m.error);
                    m.error[sizeof m.error - 1] = '\0';
                    free(decoded_value.result);
                    goto error;
                }
                size_t minified_js_length = strlen(minified_js.result);
                char *javascript_url = malloc(sizeof "javascript:" - 1 + minified_js_length + 1);
                if (javascript_url == NULL) {
                    free(minified_js.result);
                    free(decoded_value.result);
                    goto error;
                }
                memcpy(javascript_url, "javascript:", sizeof "javascript:" - 1);
                memcpy(
                    &javascript_url[sizeof "javascript:" - 1],
                    minified_js.result,
                    minified_js_length + 1
                );
                free(minified_js.result);
                struct EncodedAttribute encoded_value = xmlhtml_encode_attribute(
                    javascript_url,
                    original_quote,
                    is_xml,
                    syntax_block != SYNTAX_BLOCK_DOCTYPE
                );
                free(javascript_url);
                if (encoded_value.data == NULL) {
                    free(decoded_value.result);
                    goto error;
                }
                if (!ensure_result_capacity(
                    &m.result,
                    &result_capacity,
                    result_length + encoded_value.length + 2
                )) {
                    free(encoded_value.data);
                    free(decoded_value.result);
                    goto error;
                }
                if (encoded_value.quote != '\0') {
                    m.result[result_length++] = encoded_value.quote;
                }
                memcpy(&m.result[result_length], encoded_value.data, encoded_value.length);
                result_length += encoded_value.length;
                if (encoded_value.quote != '\0') {
                    m.result[result_length++] = encoded_value.quote;
                }
                free(encoded_value.data);
            }
            if (copy_raw_value) {
                if (!ensure_result_capacity(
                    &m.result,
                    &result_capacity,
                    result_length + value_length + (original_need_quotes ? 2 : 0)
                )) {
                    free(decoded_value.result);
                    goto error;
                }
                if (original_need_quotes) {
                    m.result[result_length++] = original_quote;
                }
                memcpy(&m.result[result_length], value, value_length);
                result_length += value_length;
                if (original_need_quotes) {
                    m.result[result_length++] = original_quote;
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
