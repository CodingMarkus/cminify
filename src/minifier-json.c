/*
 * Copyright 2026 CodingMarkus
 * Copyright 2024-2026 Jumping-Beaver
 *
 * SPDX-License-Identifier: ISC
 */

#include "minifier.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t jsonLiteralLength( const char * json )
{
	static const char * const LITERALS[] = {"true", "false", "null"};

	for (size_t i = 0; i < sizeof LITERALS / sizeof *LITERALS; ++i) {
		size_t length = strlen(LITERALS[i]);
		if (!strncmp(json, LITERALS[i], length)
			&& (json[length] == '\0'
				|| strchr(" \r\t\n],}", json[length]) != NULL)) {
			return (length);
		}
	}
	return (0);
}


static unsigned int unicodeEscapeCodepoint( const char * escape )
{
	unsigned int codepoint = 0;
	for (size_t i = 2; i < 6; i += 1) {
		codepoint *= 16;
		if (escape[i] >= '0' && escape[i] <= '9') {
			codepoint += (unsigned int) (escape[i] - '0');
		} else if (escape[i] >= 'a' && escape[i] <= 'f') {
			codepoint += (unsigned int) (escape[i] - 'a' + 10);
		} else {
			codepoint += (unsigned int) (escape[i] - 'A' + 10);
		}
	}
	return (codepoint);
}


static bool isUnicodeEscape( const char * escape )
{
	if (escape[0] != '\\' || escape[1] != 'u') {
		return (false);
	}
	for (size_t i = 2; i < 6; i += 1) {
		if (!((escape[i] >= '0' && escape[i] <= '9')
			  || (escape[i] >= 'a' && escape[i] <= 'f')
			  || (escape[i] >= 'A' && escape[i] <= 'F'))) {
			return (false);
		}
	}
	return (true);
}


static size_t appendShortenedUnicodeEscape(
	const char * escape, char * result, size_t * resultLength
)
{
	unsigned int codepoint = unicodeEscapeCodepoint(escape);
	char escapedCharacter = '\0';
	if (codepoint == '\b') {
		escapedCharacter = 'b';
	} else if (codepoint == '\t') {
		escapedCharacter = 't';
	} else if (codepoint == '\n') {
		escapedCharacter = 'n';
	} else if (codepoint == '\f') {
		escapedCharacter = 'f';
	} else if (codepoint == '\r') {
		escapedCharacter = 'r';
	} else if (codepoint == '"' || codepoint == '\\') {
		escapedCharacter = (char) codepoint;
	}
	if (escapedCharacter != '\0') {
		result[(*resultLength)++] = '\\';
		result[(*resultLength)++] = escapedCharacter;
		return (6);
	}

	if (codepoint >= ' ' && codepoint <= '~') {
		if (strchr("&/<>", (int) codepoint) != NULL) {
			return (0);
		}
		result[(*resultLength)++] = (char) codepoint;
		return (6);
	}
	if (codepoint >= 0xD800u && codepoint <= 0xDBFFu) {
		if (!isUnicodeEscape(&escape[6])) {
			return (0);
		}
		const unsigned int lowSurrogate = unicodeEscapeCodepoint(&escape[6]);
		if (lowSurrogate < 0xDC00u || lowSurrogate > 0xDFFFu) {
			return (0);
		}
		codepoint = 0x10000u + (codepoint - 0xD800u) * 0x400u
			+ lowSurrogate - 0xDC00u;
	} else if (codepoint >= 0xDC00u && codepoint <= 0xDFFFu) {
		return (0);
	}

	if (codepoint <= 0x7FFu) {
		result[(*resultLength)++] = (char) (0xC0u + (codepoint >> 6));
		result[(*resultLength)++] = (char) (0x80u + (codepoint & 0x3Fu));
	} else if (codepoint <= 0xFFFFu) {
		result[(*resultLength)++] = (char) (0xE0u + (codepoint >> 12));
		result[(*resultLength)++]
			= (char) (0x80u + ((codepoint >> 6) & 0x3Fu));
		result[(*resultLength)++] = (char) (0x80u + (codepoint & 0x3Fu));
	} else {
		result[(*resultLength)++] = (char) (0xF0u + (codepoint >> 18));
		result[(*resultLength)++]
			= (char) (0x80u + ((codepoint >> 12) & 0x3Fu));
		result[(*resultLength)++]
			= (char) (0x80u + ((codepoint >> 6) & 0x3Fu));
		result[(*resultLength)++] = (char) (0x80u + (codepoint & 0x3Fu));
	}
	return (codepoint > 0xFFFFu ? 12 : 6);
}


struct Minification MinifyJSON( const char * json )
{
	struct Minification m = {.result = malloc(strlen(json) + 1)};

	size_t bracketTypesCapacity = 512;
	char * bracketTypes = malloc(bracketTypesCapacity * sizeof *bracketTypes);

	if (m.result == NULL || bracketTypes == NULL) {
		snprintf(m.error, sizeof m.error, "Cannot allocate memory\n");
		goto error;
	}

	size_t nestingLevel = 0;
	size_t resultLength = 0;
	size_t i = 0;

	while (true) {
		while (IsWhitespace(json[i])) {
			i += 1;
		}
		if (json[i] == '\0') {
			m.result[resultLength] = '\0';
			break;
		}
		if ((json[i] == ',' || json[i] == '}')
			&& m.result[resultLength - 1] == ':') {
			m.errorPosition = i;
			snprintf(m.error,
					 sizeof m.error,
					 "No value after `:` in line %%zu, column %%zu\n");
			goto error;
		}
		if (json[i] == '[' || json[i] == '{') {
			if (++nestingLevel > bracketTypesCapacity) {
				bracketTypesCapacity += 512;
				char * bracketTypesRealloc = realloc(
					bracketTypes, bracketTypesCapacity * sizeof *bracketTypes);
				if (bracketTypesRealloc == NULL) {
					goto error;
				}
				bracketTypes = bracketTypesRealloc;
			}
			bracketTypes[nestingLevel - 1] = json[i];
			m.result[resultLength++] = json[i];
			i += 1;
			continue;
		}
		if (json[i] == ']' || json[i] == '}') {
			if (m.result[resultLength - 1] == ',') {
				m.errorPosition = i;
				snprintf(
					m.error,
					sizeof m.error,
					"Illegal `,` before bracket in line %%zu, column %%zu\n");
				goto error;
			}
			if (nestingLevel == 0
				|| bracketTypes[nestingLevel - 1] != json[i] - 2) {
				m.errorPosition = i;
				snprintf(m.error,
						 sizeof m.error,
						 "Unexpected `%c` in line %%zu, column %%zu\n",
						 json[i]);
				goto error;
			}
			nestingLevel -= 1;
			m.result[resultLength++] = json[i];
			i += 1;
			continue;
		}

		bool isKey = nestingLevel > 0
					 && (bracketTypes[nestingLevel - 1] == '{'
						 && (m.result[resultLength - 1] == ','
							 || m.result[resultLength - 1] == '{'));

		if (json[i] == '"') {
			i += 1;
			m.result[resultLength++] = '"';
			bool activeBackslash = false;
			while (json[i] != '\0' && (json[i] != '"' || activeBackslash)) {
				if (json[i] == '\n') {
					m.errorPosition = i - 1;
					snprintf(
						m.error,
						sizeof m.error,
						"Illegal line break in JSON string after line %%zu\n");
					goto error;
				}
				activeBackslash = (json[i] == '\\') * !activeBackslash;
				if (activeBackslash
					&& strchr("\"\\/bfnrtu", json[i + 1]) == NULL) {
					m.errorPosition = i;
					snprintf(m.error,
							 sizeof m.error,
							 "Invalid JSON escape sequence `\\%c` in line "
							 "%%zu, column %%zu\n",
							 json[i + 1]);
					goto error;
				}
				if (activeBackslash && json[i + 1] == 'u') {
					bool invalidUnicode = false;
					size_t k;
					for (k = i + 2; k <= i + 5; ++k) {
						if (!((json[k] >= '0' && json[k] <= '9')
							  || (json[k] >= 'a' && json[k] <= 'f')
							  || (json[k] >= 'A' && json[k] <= 'F'))) {
							invalidUnicode = true;
						}
						if (json[k] == '"' || json[k] == '\n'
							|| json[k] == '\0') {
							break;
						}
					}
					if (invalidUnicode) {
						m.errorPosition = i - 1;
						snprintf(m.error,
								 sizeof m.error,
								 "Invalid JSON escape sequence `%.*s` in line "
								 "%%zu, column %%zu\n",
								 (int)(k - i),
								 &json[i]);
						goto error;
					}
					size_t shortenedLength = appendShortenedUnicodeEscape(
						&json[i], m.result, &resultLength);
					if (shortenedLength != 0) {
						i += shortenedLength;
						activeBackslash = false;
						continue;
					}
				}
				m.result[resultLength++] = json[i];
				i += 1;
			}
			if (json[i] == '\0') {
				m.errorPosition = i - 1;
				snprintf(m.error,
						 sizeof m.error,
						 "Unexpected end of JSON document, expected `\"` "
						 "after line %%zu, column %%zu\n");
				goto error;
			}
			m.result[resultLength++] = '"';
			i += 1;
			if (!isKey) {
				continue;
			}
			while (IsWhitespace(json[i])) {
				i += 1;
			}
			if (json[i] != ':') {
				m.errorPosition = i;
				snprintf(
					m.error,
					sizeof m.error,
					"Expected `:` instead of `%c` in line %%zu, column %%zu\n",
					json[i]);
				goto error;
			}
			m.result[resultLength++] = ':';
			i += 1;
			continue;
		}

		if (isKey) {
			m.errorPosition = i;
			snprintf(m.error,
					 sizeof m.error,
					 "Expected `\"`, `[` or `{` instead of `%c` in line %%zu, "
					 "column %%zu\n",
					 json[i]);
			goto error;
		}

		size_t literalLength = jsonLiteralLength(&json[i]);
		if (literalLength > 0) {
			memcpy(&m.result[resultLength], &json[i], literalLength);
			resultLength += literalLength;
			i += literalLength;
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
			if ((json[k] == 'e' || json[k] == 'E')
				&& (json[k + 1] == '+' || json[k + 1] == '-')
				&& json[k + 2] >= '0' && json[k + 2] <= '9') {
				k += 2;
			}
			while (json[k] >= '0' && json[k] <= '9') {
				k += 1;
			}
			memcpy(&m.result[resultLength], &json[i], k - i);
			resultLength += k - i;
			i = k;
			continue;
		}
		if (nestingLevel > 0 && json[i] == ','
			&& m.result[resultLength - 1] != ',') {
			m.result[resultLength++] = ',';
			i += 1;
			continue;
		}
		if (json[i] != '\0' && !IsWhitespace(json[i])) {
			m.errorPosition = i;
			snprintf(m.error,
					 sizeof m.error,
					 "Unexpected data starting with `%c` in line %%zu, column "
					 "%%zu\n",
					 json[i]);
			goto error;
		}
	}
	if (nestingLevel != 0) {
		do {
			i -= 1;
		} while (IsWhitespace(json[i]));
		m.errorPosition = i;
		snprintf(m.error,
				 sizeof m.error,
				 "Missing `%c` after line %%zu, column %%zu\n",
				 bracketTypes[nestingLevel - 1]);
		goto error;
	}
	free(bracketTypes);
	return (m);

error:
	free(bracketTypes);
	free(m.result);
	m.result = NULL;
	return (m);
}
