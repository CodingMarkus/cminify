/*
 * Copyright 2026 CodingMarkus
 * Copyright 2024-2026 Jumping-Beaver
 *
 * SPDX-License-Identifier: ISC
 */

#include "minifier.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Minification MinifyCSS( const char * css )
{
	struct Minification m = {.result = malloc(strlen(css) + 1)};
	if (m.result == NULL) {
		snprintf(m.error, sizeof m.error, "Cannot allocate memory\n");
		return (m);
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
	} syntaxBlock
		= SYNTAX_BLOCK_RULE_START;

	size_t resultLength = 0;
	const char * atrule = NULL;
	size_t atruleLength;
	size_t i = 0;
	size_t nestingLevel = 0;

#define CSS_SKIP_WHITESPACES_COMMENTS(css, ptrI, out, ptrOutLength)           \
	SkipWhitespacesComments(                                                  \
		&m, css, ptrI, out, ptrOutLength, CSSCommentVariant);                 \
	if (m.result == NULL) {                                                   \
		goto error;                                                           \
	}

	CSS_SKIP_WHITESPACES_COMMENTS(css, &i, m.result, &resultLength);
	while (true) {
		if (css[i] == '\0') {
			if (syntaxBlock != SYNTAX_BLOCK_RULE_START) {
				while (i > 0 && IsWhitespace(css[i - 1])) {
					i -= 1;
				}
				if (syntaxBlock == SYNTAX_BLOCK_STYLE) {
					snprintf(m.error,
							 sizeof m.error,
							 "Unexpected end of stylesheet, expected `}` "
							 "after line %%zu, column %%zu\n");
				} else if (syntaxBlock == SYNTAX_BLOCK_QRULE) {
					snprintf(m.error,
							 sizeof m.error,
							 "Unexpected end of stylesheet, expected `{…}` "
							 "after line %%zu, column %%zu\n");
				} else if (syntaxBlock == SYNTAX_BLOCK_ATRULE) {
					snprintf(m.error,
							 sizeof m.error,
							 "Unexpected end of stylesheet, expected `;` or "
							 "`{…}` after line %%zu, column %%zu\n");
				} else {
					snprintf(m.error,
							 sizeof m.error,
							 "Unexpected end of stylesheet after line %%zu, "
							 "column %%zu\n");
				}
				m.errorPosition = i - 1;
				goto error;
			}
			m.result[resultLength] = '\0';
			break;
		}
		if (css[i] == '}') {
			do {
				if (nestingLevel == 0) {
					m.errorPosition = i;
					snprintf(m.error,
							 sizeof m.error,
							 "Unexpected `}` in line %%zu, column %%zu\n");
					goto error;
				}
				m.result[resultLength++] = '}';
				nestingLevel -= 1;
				i += 1;
				CSS_SKIP_WHITESPACES_COMMENTS(
					css, &i, m.result, &resultLength);
			} while (css[i] == '}');
			syntaxBlock = SYNTAX_BLOCK_RULE_START;
			continue;
		}
		if (syntaxBlock == SYNTAX_BLOCK_RULE_START) {
			if (css[i] == '{' || css[i] == '}' || css[i] == '"'
				|| css[i] == '\'') {
				m.errorPosition = i;
				snprintf(m.error,
						 sizeof m.error,
						 "Unexpected `%c` in line %%zu, column %%zu\n",
						 css[i]);
				goto error;
			}
			m.result[resultLength++] = css[i];
			if (css[i] == '@') {
				syntaxBlock = SYNTAX_BLOCK_ATRULE;
				atrule = &css[i];
				i += 1;
				atruleLength = 1;
				while (isalnum(css[i])) {
					m.result[resultLength++] = css[i];
					atruleLength += 1;
					i += 1;
				}
			} else {
				syntaxBlock = SYNTAX_BLOCK_QRULE;
				i += 1;
			}
			continue;
		}
		if (i >= 3 && !strncmp(&css[i - 3], "url(", 4)) {
			m.result[resultLength++] = '(';
			i += 1;
			while (IsWhitespace(css[i])) {
				i += 1;
			}
			if (css[i] == '"' || css[i] == '\'') {
				size_t quoteStartI = i;
				char quot = css[i];
				bool activeBackslash = false;
				do {
					activeBackslash = (css[i] == '\\') * !activeBackslash;
					m.result[resultLength++] = css[i];
					i += 1;
				} while ((css[i] != quot || activeBackslash)
						 && css[i] != '\0');
				if (css[i] == '\0') {
					m.errorPosition = quoteStartI;
					snprintf(m.error,
							 sizeof m.error,
							 "Unclosed string starting in line %%zu, column "
							 "%%zu\n");
					goto error;
				}
				m.result[resultLength++] = quot;
				i += 1;
				while (IsWhitespace(css[i])) {
					i += 1;
				}
				if (css[i] != ')') {
					m.errorPosition = i;
					snprintf(m.error,
							 sizeof m.error,
							 "Expected `)` in line %%zu, column %%zu\n");
					goto error;
				}
			} else {
				while ((css[i] != ')' || css[i - 1] == '\\') && css[i] != '\0'
					   && !IsWhitespace(css[i])) {
					m.result[resultLength++] = css[i];
					i += 1;
				}
				while (IsWhitespace(css[i])) {
					i += 1;
				}
				if (css[i] != ')') {
					if (css[i] == '\0') {
						m.errorPosition = i;
						snprintf(m.error,
								 sizeof m.error,
								 "Unexpected end of stylesheet, expected `)` "
								 "in line %%zu, column %%zu\n");
						goto error;
					} else if (IsWhitespace(css[i - 1])) {
						m.errorPosition = i;
						snprintf(m.error,
								 sizeof m.error,
								 "Illegal whitespace in URL in line %%zu, "
								 "column %%zu\n");
						goto error;
					}
				}
			}
			m.result[resultLength++] = ')';
			i += 1;
			continue;
		}
		if (css[i] == '\\') {
			m.result[resultLength++] = css[i++];
			bool activeBackslash = true;
			while (css[i] == '\\') {
				activeBackslash = !activeBackslash;
				m.result[resultLength++] = css[i++];
			}
			if (activeBackslash) {
				m.result[resultLength++] = css[i++];
			}
			continue;
		}
		if (css[i] == '"' || css[i] == '\'') {
			size_t quoteStartI = i;
			m.result[resultLength++] = css[i++];
			bool activeBackslash = false;
			while (css[i] != '\0'
				   && (css[i] != css[quoteStartI] || activeBackslash)) {
				activeBackslash = (css[i] == '\\') * !activeBackslash;
				m.result[resultLength++] = css[i];
				i += 1;
			}
			if (css[i] == '\0') {
				m.errorPosition = quoteStartI;
				snprintf(
					m.error,
					sizeof m.error,
					"Unclosed string starting in line %%zu, column %%zu\n");
				goto error;
			}
			m.result[resultLength++] = css[quoteStartI];
			i += 1;
			continue;
		}
		if (css[i] == ';' && syntaxBlock != SYNTAX_BLOCK_QRULE) {
			do {
				i += 1;
				CSS_SKIP_WHITESPACES_COMMENTS(
					css, &i, m.result, &resultLength);
			} while (css[i] == ';');
			if (css[i] != '}') {
				m.result[resultLength++] = ';';
			}
			if (syntaxBlock == SYNTAX_BLOCK_ATRULE) {
				syntaxBlock = SYNTAX_BLOCK_RULE_START;
			}
			continue;
		}
		if (css[i] == '{') {
			nestingLevel += 1;
			if (syntaxBlock == SYNTAX_BLOCK_STYLE) {
				m.errorPosition = i;
				snprintf(m.error,
						 sizeof m.error,
						 "Unexpected `{` in line %%zu, column %%zu\n");
				goto error;
			}
			m.result[resultLength++] = '{';
			i += 1;
			CSS_SKIP_WHITESPACES_COMMENTS(css, &i, m.result, &resultLength);
			if (syntaxBlock == SYNTAX_BLOCK_QRULE) {
				syntaxBlock = SYNTAX_BLOCK_STYLE;
			} else if (syntaxBlock == SYNTAX_BLOCK_ATRULE) {
				bool isNestableAtrule
					= (sizeof "@media" - 1 == atruleLength
					   && !StrNICmp(atrule, "@media", atruleLength))
					  ||

					  (sizeof "@layer " - 1 == atruleLength
					   && !StrNICmp(atrule, "@layer", atruleLength))
					  ||

					  (sizeof "@container" - 1 == atruleLength
					   && !StrNICmp(atrule, "@container", atruleLength))
					  ||

					  (sizeof "@keyframes" - 1 == atruleLength
					   && !StrNICmp(atrule, "@keyframes", atruleLength));

				syntaxBlock = isNestableAtrule ? SYNTAX_BLOCK_RULE_START
											   : SYNTAX_BLOCK_STYLE;
			}
			continue;
		}
		if (css[i] == '0' && css[i + 1] == '.'
			&& (i == 0 || css[i - 1] < '0' || css[i - 1] > '9')) {
			// Converting for example `0.1` to `.1`
			i += 1;
			continue;
		}
		if (css[i] == '(' && syntaxBlock == SYNTAX_BLOCK_ATRULE) {
			syntaxBlock = SYNTAX_BLOCK_ATRULE_ROUND_BRACKETS;
			m.result[resultLength++] = '(';
			i += 1;
			continue;
		}
		if (css[i] == '[' && syntaxBlock == SYNTAX_BLOCK_ATRULE) {
			syntaxBlock = SYNTAX_BLOCK_ATRULE_SQUARE_BRACKETS;
			m.result[resultLength++] = '[';
			i += 1;
			continue;
		}
		if (css[i] == ')'
			&& syntaxBlock == SYNTAX_BLOCK_ATRULE_ROUND_BRACKETS) {
			syntaxBlock = SYNTAX_BLOCK_ATRULE;
			m.result[resultLength++] = ')';
			i += 1;
			continue;
		}
		if (css[i] == ']'
			&& syntaxBlock == SYNTAX_BLOCK_ATRULE_SQUARE_BRACKETS) {
			syntaxBlock = SYNTAX_BLOCK_ATRULE;
			m.result[resultLength++] = ']';
			i += 1;
			continue;
		}
		if (css[i] == '(' && syntaxBlock == SYNTAX_BLOCK_QRULE) {
			syntaxBlock = SYNTAX_BLOCK_QRULE_ROUND_BRACKETS;
			m.result[resultLength++] = '(';
			i += 1;
			continue;
		}
		if (css[i] == '[' && syntaxBlock == SYNTAX_BLOCK_QRULE) {
			syntaxBlock = SYNTAX_BLOCK_QRULE_SQUARE_BRACKETS;
			m.result[resultLength++] = '[';
			i += 1;
			continue;
		}
		if (css[i] == ')'
			&& syntaxBlock == SYNTAX_BLOCK_QRULE_ROUND_BRACKETS) {
			syntaxBlock = SYNTAX_BLOCK_QRULE;
			m.result[resultLength++] = ')';
			i += 1;
			continue;
		}
		if (css[i] == ']'
			&& syntaxBlock == SYNTAX_BLOCK_QRULE_SQUARE_BRACKETS) {
			syntaxBlock = SYNTAX_BLOCK_QRULE;
			m.result[resultLength++] = ']';
			i += 1;
			continue;
		}
		if (IsWhitespace(css[i]) || (css[i] == '/' && css[i + 1] == '*')) {
			if (syntaxBlock == SYNTAX_BLOCK_ATRULE_ROUND_BRACKETS
				|| syntaxBlock == SYNTAX_BLOCK_QRULE_ROUND_BRACKETS) {
				// Removing whitespace around `:` in `@media (with : 3 px){}`
				// but not in `@page :left{}`

				CSS_SKIP_WHITESPACES_COMMENTS(
					css, &i, m.result, &resultLength);
				if (strchr("(,<>:", m.result[resultLength - 1]) == NULL
					&& strchr("),<>:", css[i]) == NULL) {
					m.result[resultLength++] = ' ';
				}
			} else if (syntaxBlock == SYNTAX_BLOCK_ATRULE_SQUARE_BRACKETS
					   || syntaxBlock == SYNTAX_BLOCK_QRULE_SQUARE_BRACKETS) {
				CSS_SKIP_WHITESPACES_COMMENTS(
					css, &i, m.result, &resultLength);
				if (strchr("[=,", m.result[resultLength - 1]) == NULL
					&& strchr("]=,*$^-|", css[i]) == NULL) {
					m.result[resultLength++] = ' ';
				}
			} else if (syntaxBlock == SYNTAX_BLOCK_ATRULE) {
				size_t beforeWhitespace = i;
				CSS_SKIP_WHITESPACES_COMMENTS(
					css, &i, m.result, &resultLength);

				// Removing whitespace before `(` in `@media (...){}` but not
				// in `@media all and (...){}`

				if ((css[i] != '('
					 || &atrule[atruleLength - 1]
							!= &css[beforeWhitespace] - 1)
					&& strchr(",)(", m.result[resultLength - 1]) == NULL
					&& strchr(",);{", css[i]) == NULL) {
					m.result[resultLength++] = ' ';
				}
			} else if (syntaxBlock == SYNTAX_BLOCK_QRULE) {
				CSS_SKIP_WHITESPACES_COMMENTS(
					css, &i, m.result, &resultLength);
				if (strchr("~>+,]", m.result[resultLength - 1]) == NULL
					&& strchr("~>+,[{", css[i]) == NULL) {
					m.result[resultLength++] = ' ';
				}
			} else if (syntaxBlock == SYNTAX_BLOCK_STYLE) {
				CSS_SKIP_WHITESPACES_COMMENTS(
					css, &i, m.result, &resultLength);
				if (strchr("{:,", m.result[resultLength - 1]) == NULL
					&& strchr("}:,;!", css[i]) == NULL) {
					m.result[resultLength++] = ' ';
				}
			}
			continue;
		}
		m.result[resultLength++] = css[i];
		i += 1;
	}
	return (m);

error:
	free(m.result);
	m.result = NULL;
	return (m);
}
