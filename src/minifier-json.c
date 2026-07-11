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

		if (!strncmp(&json[i], "true", sizeof "true" - 1)
			&& (json[i + sizeof "true" - 1] == '\0'
				|| strchr(" \r\t\n],}", json[i + sizeof "true" - 1]))) {
			strcpy(&m.result[resultLength], "true");
			resultLength += sizeof "true" - 1;
			i += sizeof "true" - 1;
			continue;
		}
		if (!strncmp(&json[i], "false", sizeof "false" - 1)
			&& (json[i + sizeof "false" - 1] == '\0'
				|| strchr("\r\t\n],}", json[i + sizeof "false" - 1]))) {
			strcpy(&m.result[resultLength], "false");
			resultLength += sizeof "false" - 1;
			i += sizeof "false" - 1;
			continue;
		}
		if (!strncmp(&json[i], "null", sizeof "null" - 1)
			&& (json[i + sizeof "null" - 1] == '\0'
				|| strchr("\r\t\n],}", json[i + sizeof "null" - 1]))) {
			strcpy(&m.result[resultLength], "null");
			resultLength += sizeof "null" - 1;
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
