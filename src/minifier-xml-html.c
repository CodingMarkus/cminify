/*
 * Copyright 2026 CodingMarkus
 * Copyright 2024-2026 Jumping-Beaver
 *
 * SPDX-License-Identifier: ISC
 */

#include "minifier.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static char utf8Byte( uint_fast32_t byte )
{
	return ((char)(unsigned char)byte);
}

static void xmlhtmlCorrectErrorPosition(
	const char * encoded, size_t * errorPosition, bool isXml
)
{
	size_t encodedI = 0, decodedI = 0;
	bool inCdata = false;
	int (* tagncmp)(const char *, const char *, size_t) = (isXml ?
		strncmp : StrNICmp
	);
	while (true) {
		if (*errorPosition == decodedI) {
			*errorPosition = encodedI;
			return;
		}
		if (encoded[encodedI] == '\0') {
			return;
		}
		if (!inCdata) {
			if (isXml
				&& !tagncmp(
					&encoded[encodedI], "<![CDATA[", sizeof "<![CDATA[" - 1)) {
				inCdata = true;
				encodedI += sizeof "<![CDATA[" - 1;
				decodedI += 1;
				continue;
			}
			if (!tagncmp(&encoded[encodedI], "&lt;", sizeof "&lt;" - 1)) {
				encodedI += sizeof "&lt;" - 1;
				decodedI += 1;
				continue;
			}
			if (!tagncmp(&encoded[encodedI], "&gt;", sizeof "&gt;" - 1)) {
				encodedI += sizeof "&gt;" - 1;
				decodedI += 1;
				continue;
			}
			if (!tagncmp(&encoded[encodedI], "&amp;", sizeof "&amp;" - 1)) {
				encodedI += sizeof "&amp;" - 1;
				decodedI += 1;
				continue;
			}
			if (!tagncmp(&encoded[encodedI], "&apos;", sizeof "&apos;" - 1)) {
				encodedI += sizeof "&apos;" - 1;
				decodedI += 1;
				continue;
			}
			if (!tagncmp(&encoded[encodedI], "&quot;", sizeof "&quot;" - 1)) {
				encodedI += sizeof "&quot;" - 1;
				decodedI += 1;
				continue;
			}
			if (!isXml) {
				if (!tagncmp(
						&encoded[encodedI], "&plus;", sizeof "&plus;" - 1)) {
					encodedI += sizeof "&quot;" - 1;
					decodedI += 1;
					continue;
				}
				if (!tagncmp(
						&encoded[encodedI], "&sol;", sizeof "&sol;" - 1)) {
					encodedI += sizeof "&sol;" - 1;
					decodedI += 1;
					continue;
				}
			}
			if (encoded[encodedI] == '&' && encoded[encodedI + 1] == '#') {
				encodedI += 2;
				if (encoded[encodedI] == 'x'
					|| (!isXml && encoded[encodedI] == 'X')) {
					encodedI += 1;
				}
				while (encoded[encodedI] != ';' && encoded[encodedI] != '\0') {
					encodedI += 1;
				}
				decodedI += 1;
				continue;
			}
		} else if (isXml
				   && !strncmp(&encoded[encodedI], "]]>", sizeof "]]>" - 1)) {
			inCdata = false;
			encodedI += sizeof "]]>" - 1;
			decodedI += sizeof "]]>" - 1;
			continue;
		}
		encodedI += 1;
		decodedI += 1;
	}
}


static struct Minification xmlhtmlDecode(
	const char * input, size_t length, bool isXml
)
{
	// This function helps minify inline scripts and styles in XML (e.g. SVG,
	// MathML, XHTML) documents. We need to decode XML entities and CDATA
	// sections before feeding the tag content into the JavaScript or CSS
	// minifier. Note that XML indeed has only five named entities. In HTML,
	// entities are not effective inside script and style tags and CDATA
	// sections are not supported at all.
	//
	// The routine is based on the assumption that decoding will never make the
	// string longer.
	//
	// The implementation of HTML decoding has only limited capability here,
	// just enough to handle possible encoding of the script type attribute.

	struct Minification m = {.result = malloc(length + 1)};
	if (m.result == NULL) {
		snprintf(m.error, sizeof m.error, "Cannot allocate memory\n");
		return (m);
	}

	size_t i = 0;
	size_t resultLength = 0;
	bool inCdata = false;

	while (i < length) {
		if (!inCdata) {
			if (isXml
				&& !strncmp(&input[i], "<![CDATA[", sizeof "<![CDATA[" - 1)) {
				inCdata = true;
				i += sizeof "<![CDATA[" - 1;
				continue;
			}
			if (!strncmp(&input[i], "&lt;", sizeof "&lt;" - 1)) {
				m.result[resultLength++] = '<';
				i += sizeof "&lt;" - 1;
				continue;
			}
			if (!strncmp(&input[i], "&gt;", sizeof "&gt;" - 1)) {
				m.result[resultLength++] = '>';
				i += sizeof "&gt;" - 1;
				continue;
			}
			if (!strncmp(&input[i], "&amp;", sizeof "&amp;" - 1)) {
				m.result[resultLength++] = '&';
				i += sizeof "&amp;" - 1;
				continue;
			}
			if (!strncmp(&input[i], "&apos;", sizeof "&apos;" - 1)) {
				m.result[resultLength++] = '\'';
				i += sizeof "&apos;" - 1;
				continue;
			}
			if (!strncmp(&input[i], "&quot;", sizeof "&quot;" - 1)) {
				m.result[resultLength++] = '"';
				i += sizeof "&quot;" - 1;
				continue;
			}
			if (!isXml) {
				if (!strncmp(&input[i], "&plus;", sizeof "&plus;" - 1)) {
					m.result[resultLength++] = '+';
					i += sizeof "&plus;" - 1;
					continue;
				}
				if (!strncmp(&input[i], "&sol;", sizeof "&sol;" - 1)) {
					m.result[resultLength++] = '/';
					i += sizeof "&sol;" - 1;
					continue;
				}
			}
			if (input[i] == '&' && input[i + 1] == '#') {
				uint_fast32_t codepoint = 0;
				bool validCodepoint = true;
				size_t k;
				if (input[i + 2] == 'x' || (!isXml && input[i + 2] == 'X')) {
					for (k = 3; input[i + k] != ';'; ++k) {
						if (input[i + k] >= '0' && input[i + k] <= '9') {
							codepoint = codepoint * 16
								+ (uint_fast32_t)(input[i + k] - '0');
						} else if (input[i + k] >= 'A'
								   && input[i + k] <= 'F') {
							codepoint = codepoint * 16
								+ (uint_fast32_t)(10 + input[i + k] - 'A');
						} else if (input[i + k] >= 'a'
								   && input[i + k] <= 'f') {
							codepoint = codepoint * 16
								+ (uint_fast32_t)(10 + input[i + k] - 'a');
						} else {
							validCodepoint = false;
							break;
						}
					}
				} else {
					for (k = 2; input[i + k] != ';'; ++k) {
						if (input[i + k] >= '0' && input[i + k] <= '9') {
							codepoint = codepoint * 10
								+ (uint_fast32_t)(input[i + k] - '0');
						} else {
							validCodepoint = false;
							break;
						}
					}
				}
				if (!validCodepoint || codepoint > UINT32_C(0x7FFFFFFF)) {
					m.errorPosition = i;
					snprintf(m.error,
							 sizeof m.error,
							 "XML entity with invalid codepoint in line %%zu, "
							 "column %%zu\n");
					goto error;
				}

				i += k + 1;

				// See `man utf-8`

					if (codepoint <= 0x7F) {
						m.result[resultLength++] = utf8Byte(codepoint);
					} else if (codepoint <= 0x7FF) {
						m.result[resultLength++] =
							utf8Byte(0xC0u + (codepoint >> 6));
						m.result[resultLength++] =
							utf8Byte(0x80u + (codepoint & 0x3Fu));
					} else if (codepoint <= 0xFFFF) {
						m.result[resultLength++] =
							utf8Byte(0xE0u + (codepoint >> 12));
						m.result[resultLength++] =
							utf8Byte(0x80u + ((codepoint >> 6) & 0x3Fu));
						m.result[resultLength++] =
							utf8Byte(0x80u + (codepoint & 0x3Fu));
					} else if (codepoint <= 0x1FFFFF) {
						m.result[resultLength++] =
							utf8Byte(0xF0u + (codepoint >> 18));
						m.result[resultLength++] =
							utf8Byte(0x80u + ((codepoint >> 12) & 0x3Fu));
						m.result[resultLength++] =
							utf8Byte(0x80u + ((codepoint >> 6) & 0x3Fu));
						m.result[resultLength++] =
							utf8Byte(0x80u + (codepoint & 0x3Fu));
					} else if (codepoint <= 0x03FFFFFF) {
						m.result[resultLength++] =
							utf8Byte(0xF8u + (codepoint >> 24));
						m.result[resultLength++] =
							utf8Byte(0x80u + ((codepoint >> 18) & 0x3Fu));
						m.result[resultLength++] =
							utf8Byte(0x80u + ((codepoint >> 12) & 0x3Fu));
						m.result[resultLength++] =
							utf8Byte(0x80u + ((codepoint >> 6) & 0x3Fu));
						m.result[resultLength++] =
							utf8Byte(0x80u + (codepoint & 0x3Fu));
					} else if (codepoint <= 0x7FFFFFFF) {
						m.result[resultLength++] =
							utf8Byte(0xFCu + (codepoint >> 30));
						m.result[resultLength++] =
							utf8Byte(0x80u + ((codepoint >> 24) & 0x3Fu));
						m.result[resultLength++] =
							utf8Byte(0x80u + ((codepoint >> 18) & 0x3Fu));
						m.result[resultLength++] =
							utf8Byte(0x80u + ((codepoint >> 12) & 0x3Fu));
						m.result[resultLength++] =
							utf8Byte(0x80u + ((codepoint >> 6) & 0x3Fu));
						m.result[resultLength++] =
							utf8Byte(0x80u + (codepoint & 0x3Fu));
					}
				continue;
			}
			if (isXml && input[i] == '&') {
				m.errorPosition = i;
				snprintf(m.error,
						 sizeof m.error,
						 "Invalid XML entity in line %%zu, column %%zu\n");
				goto error;
			}
		} else if (isXml && !strncmp(&input[i], "]]>", sizeof "]]>" - 1)) {
			inCdata = false;
			i += sizeof "]]>" - 1;
			continue;
		}
		m.result[resultLength++] = input[i++];
	}
	m.result[resultLength++] = '\0';
	return (m);

error:
	free(m.result);
	m.result = NULL;
	return (m);
}

struct EncodedString {
	char * data;
	size_t length;
};


static struct EncodedString xmlEncode(
	const char * input, const size_t inputLength
)
{
	size_t addedLengthWithCdata = sizeof "<![CDATA[]]>" - 1;
	size_t addedLengthWithEntities = 0;
	size_t i = 0;
	while (true) {
		if (input[i] == '\0') {
			break;
		} else if (input[i] == '<') {
			addedLengthWithEntities += sizeof "&lt;" - 2;
		} else if (input[i] == '>') {
			addedLengthWithEntities += sizeof "&gt;" - 2;
		} else if (input[i] == '&') {
			addedLengthWithEntities += sizeof "&amp;" - 2;
		} else if (input[i] == ']' && input[i + 1] == ']'
				   && input[i + 2] == '>') {
			addedLengthWithCdata += sizeof "]]><![CDATA[" - 1;
		}
		i += 1;
	}
	if (addedLengthWithEntities == 0) {
		struct EncodedString encoded = {malloc(inputLength + 1), inputLength};
		if (encoded.data == NULL) {
			return (encoded);
		}
		memcpy(encoded.data, input, inputLength);
		encoded.data[inputLength] = '\0';
		return (encoded);
	}
	if (addedLengthWithCdata < addedLengthWithEntities + 1) {
		struct EncodedString encoded
			= {malloc(inputLength + addedLengthWithCdata + 1), 0};
		if (encoded.data == NULL) {
			return (encoded);
		}
		strcpy(encoded.data, "<![CDATA[");
		encoded.length = sizeof "<![CDATA[" - 1;
		for (i = 0; input[i] != '\0'; ++i) {
			if (!strncmp(&input[i], "]]>", sizeof "]]>" - 1)) {
				strcpy(&encoded.data[encoded.length], "]]]]><![CDATA[>");
				encoded.length += sizeof "]]]]><![CDATA[>" - 1;
				i += 2;
			} else {
				encoded.data[encoded.length] = input[i];
				encoded.length += 1;
			}
		}
		strcpy(&encoded.data[encoded.length], "]]>");
		encoded.length += sizeof "]]>" - 1;
		encoded.data[encoded.length] = '\0';
		return (encoded);
	} else {
		struct EncodedString encoded
			= {malloc(inputLength + addedLengthWithEntities + 1), 0};
		if (encoded.data == NULL) {
			return (encoded);
		}
		encoded.length = 0;
		for (i = 0; input[i] != '\0'; ++i) {
			if (input[i] == '<') {
				strcpy(&encoded.data[encoded.length], "&lt;");
				encoded.length += sizeof "&lt;" - 1;
			} else if (input[i] == '>') {
				strcpy(&encoded.data[encoded.length], "&gt;");
				encoded.length += sizeof "&gt;" - 1;
			} else if (input[i] == '&') {
				strcpy(&encoded.data[encoded.length], "&amp;");
				encoded.length += sizeof "&amp;" - 1;
			} else {
				encoded.data[encoded.length] = input[i];
				encoded.length += 1;
			}
		}
		encoded.data[encoded.length] = '\0';
		return (encoded);
	}
}


static bool ensureResultCapacity(
	char ** result, size_t * capacity, size_t neededLength
)
{
	if (neededLength + 1 <= *capacity) {
		return (true);
	}
	size_t newCapacity = *capacity;
	while (neededLength + 1 > newCapacity) {
		newCapacity *= 2;
	}
	char * resultRealloc = realloc(*result, newCapacity);
	if (resultRealloc == NULL) {
		return (false);
	}
	*result = resultRealloc;
	*capacity = newCapacity;
	return (true);
}


static void xmlhtmlCopyError(
	struct Minification * destination, const struct Minification * source,
	size_t positionOffset
)
{
	destination->errorPosition = positionOffset + source->errorPosition;
	memcpy(destination->error, source->error, sizeof destination->error);
}


static bool xmlhtmlIsEventHandlerAttribute(
	const char * attribute, size_t attributeLength, bool isXml
)
{
	if (isXml) {
		return (false);
	}
	const char * eventHandlerAttributes[] = {
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
	for (size_t i = 0;
		 i < sizeof eventHandlerAttributes / sizeof *eventHandlerAttributes;
		 ++i) {
		if (attributeLength == strlen(eventHandlerAttributes[i])
			&& !StrNICmp(
				attribute, eventHandlerAttributes[i], attributeLength)) {
			return (true);
		}
	}
	return (false);
}


static bool xmlhtmlIsJavascriptUrl( const char * value, bool isXml )
{
	return (!isXml
			&& !StrNICmp(value, "javascript:", sizeof "javascript:" - 1));
}


static struct Minification minifyJsHandler( const char * js )
{
	const char prefix[] = "function a(){";
	const char suffix[] = "}";
	size_t jsLength = strlen(js);
	char * wrappedJs = malloc(sizeof prefix - 1 + jsLength + sizeof suffix);
	struct Minification m = {.result = NULL};
	if (wrappedJs == NULL) {
		snprintf(m.error, sizeof m.error, "Cannot allocate memory\n");
		return (m);
	}
	memcpy(wrappedJs, prefix, sizeof prefix - 1);
	memcpy(&wrappedJs[sizeof prefix - 1], js, jsLength);
	memcpy(&wrappedJs[sizeof prefix - 1 + jsLength], suffix, sizeof suffix);

	m = MinifyJS(wrappedJs);
	free(wrappedJs);
	if (m.result == NULL) {
		if (m.errorPosition >= sizeof prefix - 1) {
			m.errorPosition -= sizeof prefix - 1;
		} else {
			m.errorPosition = 0;
		}
		return (m);
	}

	size_t resultLength = strlen(m.result);
	if (resultLength < sizeof prefix + sizeof suffix - 2
		|| strncmp(m.result, prefix, sizeof prefix - 1)
		|| strcmp(&m.result[resultLength - (sizeof suffix - 1)], suffix)) {
		free(m.result);
		m.result = NULL;
		m.errorPosition = 0;
		snprintf(m.error,
				 sizeof m.error,
				 "Unexpected internal JavaScript minification result\n");
		return (m);
	}
	size_t bodyLength
		= resultLength - (sizeof prefix - 1) - (sizeof suffix - 1);
	memmove(m.result, &m.result[sizeof prefix - 1], bodyLength);
	m.result[bodyLength] = '\0';
	return (m);
}

struct EncodedAttribute {
	char * data;
	size_t length;
	char quote;
};


static struct EncodedAttribute xmlhtmlEncodeAttribute(
	const char * input, char preferredQuote, bool isXml, bool allowUnquoted
)
{
	size_t inputLength = strlen(input);
	bool needQuotes = isXml || !allowUnquoted || inputLength == 0;
	size_t singleQuotes = 0, doubleQuotes = 0;
	for (size_t i = 0; i < inputLength; ++i) {
		if (input[i] == '&' || input[i] == '<') {
			needQuotes = true;
		}
		if (!needQuotes
			&& (IsWhitespace(input[i])
				|| strchr("\"'=<>`", input[i]) != NULL)) {
			needQuotes = true;
		}
		if (input[i] == '\'') {
			singleQuotes += 1;
		} else if (input[i] == '"') {
			doubleQuotes += 1;
		}
	}

	char quote = '\0';
	if (needQuotes) {
		if (preferredQuote == '"' || preferredQuote == '\'') {
			quote = preferredQuote;
		} else if (doubleQuotes <= singleQuotes) {
			quote = '"';
		} else {
			quote = '\'';
		}
	}

	size_t outputLength = inputLength;
	for (size_t i = 0; i < inputLength; ++i) {
		if (input[i] == '&') {
			outputLength += sizeof "&amp;" - 2;
		} else if (input[i] == '<') {
			outputLength += sizeof "&lt;" - 2;
		} else if (quote == '"' && input[i] == '"') {
			outputLength += sizeof "&quot;" - 2;
		} else if (quote == '\'' && input[i] == '\'') {
			outputLength += sizeof "&apos;" - 2;
		}
	}

	struct EncodedAttribute encoded = {
		.data = malloc(outputLength + 1),
		.length = 0,
		.quote = quote,
	};
	if (encoded.data == NULL) {
		return (encoded);
	}

	for (size_t i = 0; i < inputLength; ++i) {
		if (input[i] == '&') {
			strcpy(&encoded.data[encoded.length], "&amp;");
			encoded.length += sizeof "&amp;" - 1;
		} else if (input[i] == '<') {
			strcpy(&encoded.data[encoded.length], "&lt;");
			encoded.length += sizeof "&lt;" - 1;
		} else if (quote == '"' && input[i] == '"') {
			strcpy(&encoded.data[encoded.length], "&quot;");
			encoded.length += sizeof "&quot;" - 1;
		} else if (quote == '\'' && input[i] == '\'') {
			strcpy(&encoded.data[encoded.length], "&apos;");
			encoded.length += sizeof "&apos;" - 1;
		} else {
			encoded.data[encoded.length++] = input[i];
		}
	}
	encoded.data[encoded.length] = '\0';
	return (encoded);
}


static struct Minification minifyXmlhtml( const char * xmlhtml, bool isXml )
{
	size_t inputStrlen = strlen(xmlhtml);
	size_t resultCapacity = inputStrlen + 1;
	struct Minification m = {.result = malloc(resultCapacity)};
	if (m.result == NULL) {
		snprintf(m.error, sizeof m.error, "Cannot allocate memory\n");
		return (m);
	}

	enum {
		SYNTAX_BLOCK_TAG,
		SYNTAX_BLOCK_CONTENT,
		SYNTAX_BLOCK_DOCTYPE,
	} syntaxBlock
		= SYNTAX_BLOCK_CONTENT;

	enum {
		SCRIPT_TYPE_JAVASCRIPT,
		SCRIPT_TYPE_MODULE,
		SCRIPT_TYPE_JSON,
		SCRIPT_TYPE_OTHER
	} scriptType;

	size_t i = 0;
	const char * currentTag = NULL;
	size_t currentTagLength = 0;
	bool isClosingTag = false;
	bool hasWhitespaceBeforeTag = false;
	int (*tagncmp)(const char *, const char *, size_t)
		= (isXml ? strncmp : StrNICmp);
	const char *value = NULL, *attribute = NULL;
	size_t valueLength = 0, attributeLength = 0;
	size_t resultLength = 0;

	while (true) {
		// Beginning of inline minification

		const char * tagContentDelimiter = NULL;
		struct Minification (*tagContentMinifyCallback)(const char *) = NULL;

		if (syntaxBlock == SYNTAX_BLOCK_CONTENT
			&& currentTagLength == sizeof "script" - 1
			&& !tagncmp(currentTag, "script", sizeof "script" - 1)) {
			tagContentDelimiter = "</script";
			if (scriptType == SCRIPT_TYPE_JAVASCRIPT) {
				tagContentMinifyCallback = MinifyJSWithOptions;
			} else if (scriptType == SCRIPT_TYPE_MODULE) {
				tagContentMinifyCallback = MinifyJSModuleWithOptions;
			} else if (scriptType == SCRIPT_TYPE_JSON) {
				tagContentMinifyCallback = MinifyJSON;
			} else if (scriptType == SCRIPT_TYPE_OTHER) {
				tagContentMinifyCallback = NULL;
			}
		}
		if (syntaxBlock == SYNTAX_BLOCK_CONTENT
			&& currentTagLength == sizeof "style" - 1
			&& !tagncmp(currentTag, "style", sizeof "style" - 1)) {
			tagContentDelimiter = "</style";
			tagContentMinifyCallback = MinifyCSS;
		}
		if (tagContentDelimiter != NULL) {
			size_t contentStartI = i;
			bool inCdata = false;
			while (true) {
				if (xmlhtml[i] == '\0') {
					do {
						i -= 1;
					} while (IsWhitespace(xmlhtml[i - 1]));
					m.errorPosition = i - 1;
					snprintf(m.error,
							 sizeof m.error,
							 "Unexpected end of document, expected `%s>` "
							 "after line %%zu, column %%zu\n",
							 tagContentDelimiter);
					goto error;
				}
				if (isXml
					&& !strncmp(
						&xmlhtml[i], "<![CDATA[", sizeof "<![CDATA[" - 1)) {
					inCdata = true;
					i += sizeof "<![CDATA[" - 1;
					continue;
				}
				if (isXml && !strncmp(&xmlhtml[i], "]]>", sizeof "]]>" - 1)) {
					inCdata = false;
					i += sizeof "]]>" - 1;
					continue;
				}
				if (!inCdata
					&& !tagncmp(&xmlhtml[i],
							tagContentDelimiter,
							strlen(tagContentDelimiter))) {
					currentTagLength = 0;
					break;
				}
				i += 1;
			}
			if (tagContentMinifyCallback == NULL) {
				memcpy(&m.result[resultLength],
					   &xmlhtml[contentStartI],
					   i - contentStartI);
				resultLength += i - contentStartI;
				continue;
			}

			struct Minification inlineM;
			if (isXml) {
				inlineM = xmlhtmlDecode(
					&xmlhtml[contentStartI], i - contentStartI, true);
				if (inlineM.result == NULL) {
					xmlhtmlCopyError(&m, &inlineM, contentStartI);
					goto error;
				}
				char * inputToFree = inlineM.result;
				inlineM = tagContentMinifyCallback(inlineM.result);
				free(inputToFree);
				if (inlineM.result == NULL) {
					xmlhtmlCorrectErrorPosition(&xmlhtml[contentStartI],
						&inlineM.errorPosition,
						isXml);
					xmlhtmlCopyError(&m, &inlineM, contentStartI);
					goto error;
				}
				struct EncodedString encoded
					= xmlEncode(inlineM.result, i - contentStartI);
				free(inlineM.result);
				if (encoded.data == NULL) {
					inlineM.result = NULL;
					goto error;
				}
				inlineM.result = encoded.data;
				if (encoded.length > i - contentStartI) {
					char * resultRealloc = realloc(inlineM.result,
												   inputStrlen + encoded.length
													   - i + contentStartI);
					if (resultRealloc == NULL) {
						goto error;
					}
					inlineM.result = resultRealloc;
				}
			} else {
				char * tagContent = malloc(i - contentStartI + 1);
				if (tagContent == NULL) {
					goto error;
				}
				memcpy(tagContent, &xmlhtml[contentStartI], i - contentStartI);
				tagContent[i - contentStartI] = '\0';
				inlineM = tagContentMinifyCallback(tagContent);
				free(tagContent);
				if (inlineM.result == NULL) {
					xmlhtmlCopyError(&m, &inlineM, contentStartI);
					goto error;
				}
			}
			size_t inlineResultLength = strlen(inlineM.result);
			if (!ensureResultCapacity(&m.result,
									  &resultCapacity,
									  resultLength + inlineResultLength)) {
				free(inlineM.result);
				goto error;
			}
			memcpy(
				&m.result[resultLength], inlineM.result, inlineResultLength);
			resultLength += inlineResultLength;
			free(inlineM.result);
			continue;
		}

		// End of inline minification

		if (xmlhtml[i] == '\0') {
			if (syntaxBlock == SYNTAX_BLOCK_TAG) {
				m.errorPosition = i;
				snprintf(m.error,
						 sizeof m.error,
						 "Unexpected end of document expected `>` after line "
						 "%%zu, column %%zu\n");
				goto error;
			}
			m.result[resultLength] = '\0';
			break;
		}
		if (!strncmp(&xmlhtml[i], "<!--", 4)) {
			size_t commentStartI = i;
			i += 4;
			while (xmlhtml[i] != '\0' && strncmp(&xmlhtml[i], "-->", 3)) {
				i += 1;
			}
			if (xmlhtml[i] == '\0') {
				m.errorPosition = commentStartI;
				snprintf(
					m.error,
					sizeof m.error,
					"Unclosed comment starting in line %%zu, column %%zu\n");
				goto error;
			}
			i += 3;

			// Trim whitespace at the end of the document

			size_t k = i;
			while (IsWhitespace(xmlhtml[k])) {
				k += 1;
			}
			if (xmlhtml[k] == '\0') {
				i = k;
			}
			continue;
		}
		if (isXml
			&& !strncmp(&xmlhtml[i], "<![CDATA[", sizeof "<![CDATA[" - 1)) {
			memcpy(
				&m.result[resultLength], "<![CDATA[", sizeof "<![CDATA[" - 1);
			resultLength += sizeof "<![CDATA[" - 1;
			i += sizeof "<![CDATA[" - 1;
			while (true) {
				if (xmlhtml[i] == '\0') {
					m.errorPosition = i;
					snprintf(m.error,
						sizeof m.error,
						"Unclosed CDATA section before line %%zu, column "
						"%%zu\n");
					goto error;
				}
				if (!strncmp(&xmlhtml[i], "]]>", sizeof "]]>" - 1)) {
					memcpy(&m.result[resultLength], "]]>", sizeof "]]>" - 1);
					resultLength += sizeof "]]>" - 1;
					i += sizeof "]]>" - 1;
					break;
				}
				m.result[resultLength++] = xmlhtml[i];
				i += 1;
			}
			continue;
		}
		if (xmlhtml[i] == '<') {
			// Consume `<` and tag name
			if (syntaxBlock == SYNTAX_BLOCK_TAG) {
				m.errorPosition = i;
				snprintf(m.error,
						 sizeof m.error,
						 "Illegal `<` in line %%zu, column %%zu\n");
				goto error;
			}
			hasWhitespaceBeforeTag
				= resultLength > 0 && IsWhitespace(m.result[resultLength - 1]);
			m.result[resultLength++] = '<';
			i += 1;
			if (!StrNICmp(&xmlhtml[i], "!DOCTYPE", sizeof "!DOCTYPE" - 1)) {
				syntaxBlock = SYNTAX_BLOCK_DOCTYPE;
				continue;
			}

			currentTag = &xmlhtml[i];
			isClosingTag = (xmlhtml[i] == '/');
			if (isClosingTag) {
				m.result[resultLength++] = '/';
				i += 1;
			}
			if (!((xmlhtml[i] >= 'a' && xmlhtml[i] <= 'z')
				  || (xmlhtml[i] >= 'A' && xmlhtml[i] <= 'Z')
				  || xmlhtml[i] == ':' || xmlhtml[i] == '_'
				  || xmlhtml[i] == '?')) {
				m.errorPosition = i - 1;
				snprintf(m.error,
						 sizeof m.error,
						 "`%c` in line %%zu, column %%zu is followed by an "
						 "illegal character\n",
						 xmlhtml[i - 1]);
				goto error;
			}
			m.result[resultLength++] = xmlhtml[i];
			currentTagLength = 1;
			while ((xmlhtml[i + currentTagLength] >= 'a'
					&& xmlhtml[i + currentTagLength] <= 'z')
				   || (xmlhtml[i + currentTagLength] >= 'A'
					   && xmlhtml[i + currentTagLength] <= 'Z')
				   || (xmlhtml[i + currentTagLength] >= '0'
					   && xmlhtml[i + currentTagLength] <= '9')
				   || xmlhtml[i + currentTagLength] == '-') {
				currentTagLength += 1;
				m.result[resultLength++] = xmlhtml[i + currentTagLength - 1];
			}
			if (xmlhtml[i + currentTagLength] != '/'
				&& xmlhtml[i + currentTagLength] != '>'
				&& !IsWhitespace(xmlhtml[i + currentTagLength])) {
				m.errorPosition = i + currentTagLength;
				snprintf(m.error,
						 sizeof m.error,
						 "Illegal character in tag name in in line %%zu, "
						 "column %%zu\n");
				goto error;
			}
			syntaxBlock = SYNTAX_BLOCK_TAG;
			scriptType = SCRIPT_TYPE_JAVASCRIPT;
			attributeLength = 0;
			i += currentTagLength;
			continue;
		}
		if (xmlhtml[i] == '>') {
			if (syntaxBlock == SYNTAX_BLOCK_CONTENT) {
				m.errorPosition = i;
				snprintf(m.error,
						 sizeof m.error,
						 "Illegal `>` in line %%zu, column %%zu\n");
				goto error;
			}
			if (xmlhtml[i - 1] == '/' && m.result[resultLength - 1] != '=') {
				isClosingTag = true;
			}

			// Transform `<xmlhtml></xmlhtml>` to `<xmlhtml/>`. This would be
			// illegal for HTML.

			else if (isXml && syntaxBlock != SYNTAX_BLOCK_DOCTYPE
					 && xmlhtml[i + 1] == '<' && xmlhtml[i + 2] == '/'
					 && !strncmp(currentTag, &xmlhtml[i + 3], currentTagLength)
					 && (IsWhitespace(xmlhtml[i + 3 + currentTagLength])
						 || xmlhtml[i + 3 + currentTagLength] == '>')) {
				m.result[resultLength++] = '/';
				m.result[resultLength++] = '>';
				i += 3 + currentTagLength;
				while (xmlhtml[i] != '>' && xmlhtml[i] != '\0') {
					i += 1;
				}
				syntaxBlock = SYNTAX_BLOCK_CONTENT;
				currentTagLength = 0;
				i += 1;
				continue;
			}

			i += 1;
			syntaxBlock = SYNTAX_BLOCK_CONTENT;

			if (isXml) {
				// Ignore whitespace except between opening and closing tags

				size_t k = i;
				while (IsWhitespace(xmlhtml[k])) {
					k += 1;
				}
				if (xmlhtml[k] == '<'
					&& (isClosingTag || xmlhtml[k + 1] != '/')) {
					i = k;
				}
			}

			m.result[resultLength++] = '>';

			// Trim whitespace at the end of the document

			size_t k = i;
			while (IsWhitespace(xmlhtml[k])) {
				k += 1;
			}
			if (xmlhtml[k] == '\0') {
				i = k;
			}
			continue;
		}
		if (syntaxBlock == SYNTAX_BLOCK_TAG && IsWhitespace(xmlhtml[i])) {
			while (IsWhitespace(xmlhtml[i])) {
				i += 1;
			}
			if (xmlhtml[i] != '=' && m.result[resultLength - 1] != '='
				&& xmlhtml[i] != '>' && xmlhtml[i] != '/') {
				m.result[resultLength++] = ' ';
			}
			if (isClosingTag && xmlhtml[i] != '>') {
				m.errorPosition = i;
				snprintf(m.error,
						 sizeof m.error,
						 "Illegal content in line %%zu, column %%zu after "
						 "whitespace in closing tag\n");
				goto error;
			}
			continue;
		}
		if (syntaxBlock == SYNTAX_BLOCK_TAG && xmlhtml[i] != '=') {
			// Consume attribute
			if (xmlhtml[i] == '"' || xmlhtml[i] == '\'') {
				m.errorPosition = i;
				snprintf(m.error,
						 sizeof m.error,
						 "Illegal character `%c` in line %%zu, column %%zu\n",
						 xmlhtml[i]);
				goto error;
			}
			attribute = &xmlhtml[i];
			attributeLength = 0;
			while (strchr("\"' \t\r\n<>=/", xmlhtml[i]) == NULL) {
				m.result[resultLength++] = xmlhtml[i];
				attributeLength += 1;
				i += 1;
			}
			if (xmlhtml[i] == '/' && xmlhtml[i + 1] != '>') {
				m.errorPosition = i;
				snprintf(
					m.error,
					sizeof m.error,
					"`/` in line %%zu, column %%zu is not followed by `>` \n");
				goto error;
			}
			if (xmlhtml[i] == '/' && xmlhtml[i + 1] == '>') {
				if (isXml) {
					m.result[resultLength++] = '/';
				}
				i += 1;
				continue;
			}
			if (strchr("=> \r\t\n/", xmlhtml[i]) == NULL) {
				m.errorPosition = i;
				snprintf(m.error,
						 sizeof m.error,
						 "Illegal character `%c` after attribute in line "
						 "%%zu, column %%zu\n",
						 xmlhtml[i]);
				goto error;
			}
			continue;
		}
		if (syntaxBlock == SYNTAX_BLOCK_TAG && xmlhtml[i] == '=') {
			// Consume `=` followed by quoted or unquoted value
			if (attributeLength == 0) {
				m.errorPosition = i;
				snprintf(
					m.error,
					sizeof m.error,
					"No attribute before `=` in line %%zu, column %%zu\n");
				goto error;
			}

			i += 1;
			while (IsWhitespace(xmlhtml[i])) {
				i += 1;
			}
			if (xmlhtml[i] == '=' || xmlhtml[i] == '>') {
				m.errorPosition = i;
				snprintf(m.error,
						 sizeof m.error,
						 "No value after `=` in line %%zu, column %%zu\n");
				goto error;
			}
			if (isXml && xmlhtml[i] != '"' && xmlhtml[i] != '\'') {
				m.errorPosition = i;
				snprintf(m.error,
						 sizeof m.error,
						 "XML requires a quote after `=` in line %%zu, column "
						 "%%zu\n");
				goto error;
			}

			m.result[resultLength++] = '=';
			char originalQuote = '\0';
			bool copyRawValue = true;
			bool originalNeedQuotes = false;
			if (xmlhtml[i] == '"' || xmlhtml[i] == '\'') {
				char quote = xmlhtml[i];
				size_t stringStartI = i;
				i += 1;
				value = &xmlhtml[i];
				valueLength = 0;
				originalQuote = quote;
				if (isXml || syntaxBlock == SYNTAX_BLOCK_DOCTYPE) {
					originalNeedQuotes = true;
				} else if (xmlhtml[i] == quote) {
					originalNeedQuotes = true;
				} else {
					originalNeedQuotes = false;
					size_t k = i;
					while (xmlhtml[k] != quote) {
						if (strchr(" \r\t\n=\"'/", xmlhtml[k]) != NULL) {
							originalNeedQuotes = true;
							break;
						}
						k += 1;
					}
				}
				while (xmlhtml[i] != '\0' && xmlhtml[i] != quote) {
					valueLength += 1;
					i += 1;
				}
				if (xmlhtml[i] == '\0') {
					m.errorPosition = stringStartI;
					snprintf(m.error,
							 sizeof m.error,
							 "Unclosed string starting in line %%zu, column "
							 "%%zu\n");
					goto error;
				}
				i += 1;
				if (!IsWhitespace(xmlhtml[i]) && xmlhtml[i] != '>'
					&& (xmlhtml[i] != '/' || xmlhtml[i + 1] != '>')
					&& (xmlhtml[i] != '?' || xmlhtml[i + 1] != '>')) {
					m.errorPosition = i;
					snprintf(m.error,
							 sizeof m.error,
							 "Illegal character after `%c` in line %%zu, "
							 "column %%zu\n",
							 quote);
					goto error;
				}
			} else {
				value = &xmlhtml[i];
				valueLength = 0;
				while (strchr(" \r\t\n >=\"'", xmlhtml[i]) == NULL) {
					i += 1;
					valueLength += 1;
				}
			}

			// Checking script type

			struct Minification decodedValue
				= xmlhtmlDecode(value, valueLength, false);
			if (decodedValue.result == NULL) {
				m.errorPosition = i + decodedValue.errorPosition;
				m.result = decodedValue.result;
				goto error;
			}
			if (currentTagLength == sizeof "script" - 1
				&& !tagncmp(currentTag, "script", sizeof "script" - 1)
				&& attributeLength == sizeof "type" - 1
				&& !tagncmp(attribute, "type", sizeof "type" - 1)) {
				if (!strcmp(decodedValue.result, "application/json+ld")) {
					scriptType = SCRIPT_TYPE_JSON;
				} else if (!strcmp(decodedValue.result, "importmap")) {
					scriptType = SCRIPT_TYPE_JSON;
				} else if (!strcmp(decodedValue.result, "module")) {
					scriptType = SCRIPT_TYPE_MODULE;
				} else if (!strcmp(decodedValue.result, "text/javascript")) {
					scriptType = SCRIPT_TYPE_JAVASCRIPT;
				} else {
					scriptType = SCRIPT_TYPE_OTHER;
				}
			}
			if (xmlhtmlIsEventHandlerAttribute(
					attribute, attributeLength, isXml)) {
				copyRawValue = false;
				struct Minification minifiedJs
					= minifyJsHandler(decodedValue.result);
					if (minifiedJs.result == NULL) {
						size_t errorPosition = minifiedJs.errorPosition;
						size_t valueOffset = (size_t)(value - xmlhtml);
						xmlhtmlCorrectErrorPosition(
							value, &errorPosition, false);
						m.errorPosition = valueOffset + errorPosition;
						strncpy(m.error, minifiedJs.error, sizeof m.error);
						m.error[sizeof m.error - 1] = '\0';
						free(decodedValue.result);
					goto error;
				}
				struct EncodedAttribute encodedValue = xmlhtmlEncodeAttribute(
					minifiedJs.result,
					originalQuote,
					isXml,
					syntaxBlock != SYNTAX_BLOCK_DOCTYPE);
				free(minifiedJs.result);
				if (encodedValue.data == NULL) {
					free(decodedValue.result);
					goto error;
				}
				if (!ensureResultCapacity(&m.result,
										  &resultCapacity,
										  resultLength + encodedValue.length
											  + 2)) {
					free(encodedValue.data);
					free(decodedValue.result);
					goto error;
				}
				if (encodedValue.quote != '\0') {
					m.result[resultLength++] = encodedValue.quote;
				}
				memcpy(&m.result[resultLength],
					   encodedValue.data,
					   encodedValue.length);
				resultLength += encodedValue.length;
				if (encodedValue.quote != '\0') {
					m.result[resultLength++] = encodedValue.quote;
				}
				free(encodedValue.data);
			} else if (xmlhtmlIsJavascriptUrl(decodedValue.result, isXml)) {
				copyRawValue = false;
				struct Minification minifiedJs
					= MinifyJS(&decodedValue.result[sizeof "javascript:" - 1]);
					if (minifiedJs.result == NULL) {
						size_t errorPosition
							= sizeof "javascript:" - 1 + minifiedJs.errorPosition;
						size_t valueOffset = (size_t)(value - xmlhtml);
						xmlhtmlCorrectErrorPosition(
							value, &errorPosition, false);
						m.errorPosition = valueOffset + errorPosition;
						strncpy(m.error, minifiedJs.error, sizeof m.error);
						m.error[sizeof m.error - 1] = '\0';
						free(decodedValue.result);
					goto error;
				}
				size_t minifiedJsLength = strlen(minifiedJs.result);
				char * javascriptUrl
					= malloc(sizeof "javascript:" - 1 + minifiedJsLength + 1);
				if (javascriptUrl == NULL) {
					free(minifiedJs.result);
					free(decodedValue.result);
					goto error;
				}
				memcpy(javascriptUrl, "javascript:", sizeof "javascript:" - 1);
				memcpy(&javascriptUrl[sizeof "javascript:" - 1],
					   minifiedJs.result,
					   minifiedJsLength + 1);
				free(minifiedJs.result);
				struct EncodedAttribute encodedValue = xmlhtmlEncodeAttribute(
					javascriptUrl,
					originalQuote,
					isXml,
					syntaxBlock != SYNTAX_BLOCK_DOCTYPE);
				free(javascriptUrl);
				if (encodedValue.data == NULL) {
					free(decodedValue.result);
					goto error;
				}
				if (!ensureResultCapacity(&m.result,
										  &resultCapacity,
										  resultLength + encodedValue.length
											  + 2)) {
					free(encodedValue.data);
					free(decodedValue.result);
					goto error;
				}
				if (encodedValue.quote != '\0') {
					m.result[resultLength++] = encodedValue.quote;
				}
				memcpy(&m.result[resultLength],
					   encodedValue.data,
					   encodedValue.length);
				resultLength += encodedValue.length;
				if (encodedValue.quote != '\0') {
					m.result[resultLength++] = encodedValue.quote;
				}
				free(encodedValue.data);
			}
			if (copyRawValue) {
				if (!ensureResultCapacity(
						&m.result,
						&resultCapacity,
						resultLength + valueLength
							+ (originalNeedQuotes ? 2 : 0))) {
					free(decodedValue.result);
					goto error;
				}
				if (originalNeedQuotes) {
					m.result[resultLength++] = originalQuote;
				}
				memcpy(&m.result[resultLength], value, valueLength);
				resultLength += valueLength;
				if (originalNeedQuotes) {
					m.result[resultLength++] = originalQuote;
				}
			}
			free(decodedValue.result);
			continue;
		}
		if (!isXml && syntaxBlock == SYNTAX_BLOCK_CONTENT
			&& IsWhitespace(xmlhtml[i])) {
			if (currentTagLength == sizeof "pre" - 1
				&& !StrNICmp(currentTag, "pre", sizeof "pre" - 1)) {
				m.result[resultLength++] = xmlhtml[i];
				i += 1;
				continue;
			}
			while (IsWhitespace(xmlhtml[i])) {
				i += 1;
			}
			if (hasWhitespaceBeforeTag && m.result[resultLength - 1] == '>') {
				continue;
			}
			m.result[resultLength++] = ' ';
			continue;
		}
		m.result[resultLength++] = xmlhtml[i];
		i += 1;
	}
	return (m);

error:
	free(m.result);
	m.result = NULL;
	return (m);
}


struct Minification MinifyXML( const char * xml )
{
	return (minifyXmlhtml(xml, true));
}


struct Minification MinifyHTML( const char * html )
{
	return (minifyXmlhtml(html, false));
}
