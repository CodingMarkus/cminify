/*
 * Copyright 2026 CodingMarkus
 * Copyright 2024-2026 Jumping-Beaver
 *
 * SPDX-License-Identifier: ISC
 */

#include "js-mangler.h"
#include "minifier.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool IsJSIdentifierStart( const char c )
{
	return (
		((unsigned char)(c - 'A') <= 'Z' - 'A'
		|| (unsigned char)(c - 'a') <= 'z' - 'a' || c == '_' || c == '$')
	);
}


static bool IsJSIdentifierPart( const char c )
{
	return (IsJSIdentifierStart(c)
		|| (unsigned char)(c - '0') <= '9' - '0');
}


static bool IsJSRegexStart( const char * output, size_t outputLength )
{
	if (outputLength == 0
		|| strchr("^!&|([{><+-*%:?~,;=", output[outputLength - 1]) != NULL) {
		return (true);
	}
	if (!IsJSIdentifierPart(output[outputLength - 1])) {
		return (false);
	}
	size_t wordStart = outputLength - 1;
	while (wordStart > 0 && IsJSIdentifierPart(output[wordStart - 1])) {
		wordStart -= 1;
	}
	size_t wordLength = outputLength - wordStart;
	return ((wordLength == sizeof "return" - 1
			 && !strncmp(&output[wordStart], "return", wordLength))
		|| (wordLength == sizeof "throw" - 1
			&& !strncmp(&output[wordStart], "throw", wordLength))
		|| (wordLength == sizeof "case" - 1
			&& !strncmp(&output[wordStart], "case", wordLength))
		|| (wordLength == sizeof "delete" - 1
			&& !strncmp(&output[wordStart], "delete", wordLength))
		|| (wordLength == sizeof "typeof" - 1
			&& !strncmp(&output[wordStart], "typeof", wordLength))
		|| (wordLength == sizeof "void" - 1
			&& !strncmp(&output[wordStart], "void", wordLength)));
}


struct Minification MinifyJS( const char * js )
{
	struct Minification m = {.result = malloc(strlen(js) + 1)};

	size_t curlyBlocksCapacity = 64;

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

		size_t doNestingLevel;
	} * curlyBlocks = malloc(curlyBlocksCapacity * sizeof *curlyBlocks);

	size_t roundBlocksCapacity = 64;

	enum RoundBlockType {
		ROUND_BLOCK_DO_WHILE,
		ROUND_BLOCK_PREFIXED_CONDITION,
		ROUND_BLOCK_UNKNOWN,
		ROUND_BLOCK_CONDITION,
		ROUND_BLOCK_CATCH_SWITCH,
		ROUND_BLOCK_PARAM,
		ROUND_BLOCK_PARAM_STANDALONE,
		ROUND_BLOCK_PARAM_ARROWFUNC_SINGLE,
	} * roundBlocks
		= malloc(roundBlocksCapacity * sizeof *roundBlocks);

	if (m.result == NULL || curlyBlocks == NULL || roundBlocks == NULL) {
		snprintf(m.error, sizeof m.error, "Cannot allocate memory\n");
		goto error;
	}

	curlyBlocks[0] = (struct CurlyBlock){CURLY_BLOCK_GLOBAL, 0};
	size_t curlyNestingLevel = 1;
	size_t roundNestingLevel = 0;

	size_t resultLength = 0;
	size_t i = 0;
	size_t lastOpenCurlyBracketI = 0, lastOpenRoundBracketI = 0;
	const char * identifierDelimiters = "'\"`%<>+*/-=,(){}[]!~;|&^:? \t\r\n";

#define JS_SKIP_WHITESPACES_COMMENTS(js, ptrI, out, ptrOutLength)             \
	SkipWhitespacesComments(                                                  \
		&m, js, ptrI, out, ptrOutLength, JSCommentVariant);                   \
	if (m.result == NULL) {                                                   \
		goto error;                                                           \
	}

#define INCR_CURLY_NESTING_LEVEL                                              \
	if (++curlyNestingLevel > curlyBlocksCapacity) {                          \
		curlyBlocksCapacity += 512;                                           \
		struct CurlyBlock * curlyBlocksRealloc = realloc(                     \
			curlyBlocks, curlyBlocksCapacity * sizeof *curlyBlocks);          \
		if (curlyBlocksRealloc == NULL) {                                     \
			snprintf(m.error, sizeof m.error, "Cannot allocate memory\n");    \
			goto error;                                                       \
		}                                                                     \
		curlyBlocks = curlyBlocksRealloc;                                     \
	}                                                                         \
	curlyBlocks[curlyNestingLevel - 1].doNestingLevel = 0;                    \
	lastOpenCurlyBracketI = i;

#define INCR_ROUND_NESTING_LEVEL                                              \
	if (++roundNestingLevel > roundBlocksCapacity) {                          \
		roundBlocksCapacity += 512;                                           \
		enum RoundBlockType * roundBlocksRealloc = realloc(                   \
			roundBlocks, roundBlocksCapacity * sizeof *roundBlocks);          \
		if (roundBlocksRealloc == NULL) {                                     \
			snprintf(m.error, sizeof m.error, "Cannot allocate memory\n");    \
			goto error;                                                       \
		}                                                                     \
		roundBlocks = roundBlocksRealloc;                                     \
	}                                                                         \
	lastOpenRoundBracketI = i;

	while (true) {
		if (js[i] == '\0') {
			m.result[resultLength] = '\0';
			break;
		}

		size_t nextWordLength = strcspn(&js[i], identifierDelimiters);
		if (nextWordLength == 0) {
			goto afterKeywords;
		}

		// Keywords lose their meaning when used as object keys

		{
			size_t k = i + nextWordLength;
			JS_SKIP_WHITESPACES_COMMENTS(js, &k, NULL, NULL);
			if (js[k] == ':') {
				memcpy(&m.result[resultLength], &js[i], nextWordLength);
				resultLength += nextWordLength;
				i += nextWordLength;
				continue;
			}
		}

		// Next we handle keywords

		if ((nextWordLength == sizeof "switch" - 1
			 && !strncmp(&js[i], "switch", nextWordLength))
			|| (nextWordLength == sizeof "catch" - 1
				&& !strncmp(&js[i], "catch", nextWordLength))) {
			memcpy(&m.result[resultLength], &js[i], nextWordLength);
			resultLength += nextWordLength;
			i += nextWordLength;

			JS_SKIP_WHITESPACES_COMMENTS(js, &i, m.result, &resultLength);
			if (js[i] == '(') {
				INCR_ROUND_NESTING_LEVEL;
				roundBlocks[roundNestingLevel - 1] = ROUND_BLOCK_CATCH_SWITCH;
				m.result[resultLength++] = '(';
				i += 1;
			} else if (js[i] == '{') {
				INCR_CURLY_NESTING_LEVEL;
				curlyBlocks[curlyNestingLevel - 1].type
					= CURLY_BLOCK_CONDITION_BODY;
				m.result[resultLength++] = '{';
				i += 1;
			} else {
				m.errorPosition = i;
				snprintf(m.error,
						 sizeof m.error,
						 "Expected `(` or `{` in line %%zu, column %%zu\n");
				goto error;
			}
			continue;
		}
		if (nextWordLength == sizeof "do" - 1
			&& !strncmp(&js[i], "do", nextWordLength)) {
			memcpy(&m.result[resultLength], &js[i], nextWordLength);
			resultLength += nextWordLength;
			i += nextWordLength;
			JS_SKIP_WHITESPACES_COMMENTS(js, &i, m.result, &resultLength);
			if (js[i] == '{') {
				size_t k = i + 1;
				bool skippedAllComments
					= JS_SKIP_WHITESPACES_COMMENTS(js, &k, NULL, NULL);
				if (skippedAllComments && js[k] == '}') {
					curlyBlocks[curlyNestingLevel - 1].doNestingLevel += 1;
					m.result[resultLength++] = ';';
					i = k + 1;
					continue;
				}
				INCR_CURLY_NESTING_LEVEL;
				curlyBlocks[curlyNestingLevel - 1].type = CURLY_BLOCK_DO;
				m.result[resultLength++] = '{';
				i += 1;
				continue;
			}
			if (strchr(identifierDelimiters, js[i]) == NULL) {
				m.result[resultLength++] = ' ';
			}
			curlyBlocks[curlyNestingLevel - 1].doNestingLevel += 1;
			continue;
		}
		if ((nextWordLength == sizeof "try" - 1
			 && !strncmp(&js[i], "try", nextWordLength))
			|| (nextWordLength == sizeof "finally" - 1
				&& !strncmp(&js[i], "finally", nextWordLength))) {
			memcpy(&m.result[resultLength], &js[i], nextWordLength);
			resultLength += nextWordLength;
			i += nextWordLength;

			JS_SKIP_WHITESPACES_COMMENTS(js, &i, m.result, &resultLength);
			if (js[i] != '{') {
				m.errorPosition = i;
				snprintf(m.error,
						 sizeof m.error,
						 "Expected `{` in line %%zu, column %%zu\n");
				goto error;
			}
			INCR_CURLY_NESTING_LEVEL;
			curlyBlocks[curlyNestingLevel - 1].type = CURLY_BLOCK_TRY_FINALLY;
			m.result[resultLength++] = '{';
			i += 1;
			continue;
		}
		if (nextWordLength == sizeof "function" - 1
			&& !strncmp(&js[i], "function", nextWordLength)) {
			// We consume the input until `(` of the parameter list.
			//
			// Regular functions cannot be safely replaced by arrow functions.
			// Arrow functions cannot be used as constructors: `new
			// arrow_function()` where `arrow_function` is an arrow function is
			// invalid.
			//
			// `standalone` means that the function object is not assigned to a
			// variable. In this case it is possible to omit newlines and
			// semicolons after the function body.

			bool standalone = resultLength == 0
							  || m.result[resultLength - 1] == ';'
							  || m.result[resultLength - 1] == '}'
							  || m.result[resultLength - 1] == '{';

			memcpy(&m.result[resultLength], &js[i], nextWordLength);
			resultLength += nextWordLength;
			i += nextWordLength;

			JS_SKIP_WHITESPACES_COMMENTS(js, &i, m.result, &resultLength);

			if (js[i] == '*') {
				m.result[resultLength++] = '*';
				i += 1;
				JS_SKIP_WHITESPACES_COMMENTS(js, &i, m.result, &resultLength);
			}
			if (js[i] != '(') {
				m.result[resultLength++] = ' ';
				while (strchr(identifierDelimiters, js[i]) == NULL) {
					m.result[resultLength++] = js[i++];
				}
			}
			JS_SKIP_WHITESPACES_COMMENTS(js, &i, m.result, &resultLength);
			if (js[i] != '(') {
				m.errorPosition = i;
				snprintf(m.error,
						 sizeof m.error,
						 "Expected `(` in line %%zu, column %%zu\n");
				goto error;
			}
			INCR_ROUND_NESTING_LEVEL;
			roundBlocks[roundNestingLevel - 1]
				= standalone ? ROUND_BLOCK_PARAM_STANDALONE
							 : ROUND_BLOCK_PARAM;
			m.result[resultLength++] = '(';
			i += 1;
			continue;
		}
		if (nextWordLength == sizeof "while" - 1
			&& !strncmp(&js[i], "while", nextWordLength)) {
			char curlyBracketBeforeWhile
				= resultLength > 0 && m.result[resultLength - 1] == '}';
			memcpy(&m.result[resultLength], &js[i], nextWordLength);
			resultLength += nextWordLength;
			i += nextWordLength;
			JS_SKIP_WHITESPACES_COMMENTS(js, &i, m.result, &resultLength);
			if (js[i] != '(') {
				m.errorPosition = i;
				snprintf(m.error,
						 sizeof m.error,
						 "Expected `(` in line %%zu, column %%zu\n");
				goto error;
			}
			INCR_ROUND_NESTING_LEVEL;
			if (curlyBracketBeforeWhile
				&& curlyBlocks[curlyNestingLevel].type == CURLY_BLOCK_DO) {
				roundBlocks[roundNestingLevel - 1] = ROUND_BLOCK_DO_WHILE;
			} else if (curlyBlocks[curlyNestingLevel - 1].doNestingLevel > 0) {
				curlyBlocks[curlyNestingLevel - 1].doNestingLevel -= 1;
				roundBlocks[roundNestingLevel - 1] = ROUND_BLOCK_DO_WHILE;
			} else {
				roundBlocks[roundNestingLevel - 1]
					= ROUND_BLOCK_PREFIXED_CONDITION;
			}
			m.result[resultLength++] = '(';
			i += 1;
			continue;
		}
		if ((nextWordLength == sizeof "if" - 1
			 && !strncmp(&js[i], "if", nextWordLength))
			|| (nextWordLength == sizeof "for" - 1
				&& !strncmp(&js[i], "for", nextWordLength))) {
			memcpy(&m.result[resultLength], &js[i], nextWordLength);
			resultLength += nextWordLength;
			i += nextWordLength;
			JS_SKIP_WHITESPACES_COMMENTS(js, &i, m.result, &resultLength);
			if (js[i] != '(') {
				m.errorPosition = i;
				snprintf(m.error,
						 sizeof m.error,
						 "Expected `(` in line %%zu, column %%zu\n");
				goto error;
			}
			INCR_ROUND_NESTING_LEVEL;
			roundBlocks[roundNestingLevel - 1]
				= ROUND_BLOCK_PREFIXED_CONDITION;
			m.result[resultLength++] = '(';
			i += 1;
			continue;
		}
		if (nextWordLength == sizeof "else" - 1
			&& !strncmp(&js[i], "else", nextWordLength)) {
			memcpy(&m.result[resultLength], &js[i], nextWordLength);
			resultLength += nextWordLength;
			i += nextWordLength;
			size_t k = i;
			JS_SKIP_WHITESPACES_COMMENTS(js, &k, NULL, NULL);
			if (js[k] != '{') {
				continue;
			}
			JS_SKIP_WHITESPACES_COMMENTS(js, &i, m.result, &resultLength);
			i += 1;
			k = i;
			bool skippedAllComments
				= JS_SKIP_WHITESPACES_COMMENTS(js, &k, NULL, NULL);
			if (skippedAllComments && js[k] == '}') {
				JS_SKIP_WHITESPACES_COMMENTS(js, &i, m.result, &resultLength);
				m.result[resultLength++] = ';';
				do {
					i += 1;
					JS_SKIP_WHITESPACES_COMMENTS(
						js, &i, m.result, &resultLength);
				} while (js[i] == ';');
			} else {
				INCR_CURLY_NESTING_LEVEL;
				m.result[resultLength++] = '{';
				curlyBlocks[curlyNestingLevel - 1].type
					= CURLY_BLOCK_CONDITION_BODY;
			}
			continue;
		}
		// Cannot make this replacement because it is possible to redefine
		// `undefined` in the local scope. if (next_word_length == sizeof
		// "undefined" - 1 && !strncmp(&js[i], "undefined", next_word_length))
		// {
		//    strcpy(&m.result[result_length], "void 0");
		//    i += next_word_length;
		//    result_length += sizeof "void 0" - 1;
		//    continue;
		//}
		if ((nextWordLength == sizeof "true" - 1
			 && !strncmp(&js[i], "true", nextWordLength))
			|| (nextWordLength == sizeof "false" - 1
				&& !strncmp(&js[i], "false", nextWordLength))) {
			if (resultLength > 0 && m.result[resultLength - 1] == ' ') {
				resultLength -= 1;
			}
			m.result[resultLength++] = '!';
			m.result[resultLength++] = js[i] == 't' ? '0' : '1';
			i += nextWordLength;
			continue;
		}

		memcpy(&m.result[resultLength], &js[i], nextWordLength);
		resultLength += nextWordLength;
		i += nextWordLength;

	afterKeywords:

		if (js[i] == '{') {
			INCR_CURLY_NESTING_LEVEL;
			i += 1;
			if (resultLength > 0 && m.result[resultLength - 1] == ')'
				&& roundBlocks[roundNestingLevel]
					   == ROUND_BLOCK_PREFIXED_CONDITION) {
				// Replacing `if(1){}` by `if(1);`
				bool skippedAllComments = JS_SKIP_WHITESPACES_COMMENTS(
					js, &i, m.result, &resultLength);
				if (skippedAllComments && js[i] == '}') {
					m.result[resultLength++] = ';';
					do {
						i += 1;
						JS_SKIP_WHITESPACES_COMMENTS(
							js, &i, m.result, &resultLength);
					} while (js[i] == ';');
					curlyNestingLevel -= 1;
					continue;
				} else {
					curlyBlocks[curlyNestingLevel - 1].type
						= CURLY_BLOCK_CONDITION_BODY;
				}
			} else if (resultLength >= 1 && m.result[resultLength - 1] == ')'
					   && roundBlocks[roundNestingLevel]
							  == ROUND_BLOCK_CATCH_SWITCH) {
				curlyBlocks[curlyNestingLevel - 1].type
					= CURLY_BLOCK_CONDITION_BODY;
			} else if (resultLength >= 2 && m.result[resultLength - 2] == '='
					   && m.result[resultLength - 1] == '>') {
				curlyBlocks[curlyNestingLevel - 1].type
					= CURLY_BLOCK_ARROWFUNC_BODY;
			} else if (resultLength >= 1 && m.result[resultLength - 1] == ')'
					   && roundBlocks[roundNestingLevel]
							  == ROUND_BLOCK_PARAM) {
				curlyBlocks[curlyNestingLevel - 1].type
					= CURLY_BLOCK_FUNC_BODY;
			} else if (resultLength >= 1 && m.result[resultLength - 1] == ')'
					   && roundBlocks[roundNestingLevel]
							  == ROUND_BLOCK_PARAM_STANDALONE) {
				curlyBlocks[curlyNestingLevel - 1].type
					= CURLY_BLOCK_FUNC_BODY_STANDALONE;
			} else if (resultLength >= 1
					   && (m.result[resultLength - 1] == '}'
						   || m.result[resultLength - 1] == ';'
						   || m.result[resultLength - 1] == '{'
						   || m.result[resultLength - 1] == '\n')) {
				curlyBlocks[curlyNestingLevel - 1].type
					= CURLY_BLOCK_STANDALONE;
			} else {
				curlyBlocks[curlyNestingLevel - 1].type = CURLY_BLOCK_UNKNOWN;
			}
			m.result[resultLength++] = '{';
			continue;
		}
		if (js[i] == '(') {
			INCR_ROUND_NESTING_LEVEL;
			i += 1;

			// Removing round brackets around single parameters with no default
			// value of arrow functions.

			bool removeRoundBracketsAroundParam = false;

			size_t k = i;
			JS_SKIP_WHITESPACES_COMMENTS(js, &k, NULL, NULL);
			if (js[k]
				!= '.') { // Can't remove round brackets in `(...arg)=>{}`
				size_t argStart = k;
				while (strchr(identifierDelimiters, js[k]) == NULL) {
					k += 1;
				}
				JS_SKIP_WHITESPACES_COMMENTS(js, &k, NULL, NULL);
				if (k > argStart && js[k] == ')') {
					k += 1;
					JS_SKIP_WHITESPACES_COMMENTS(js, &k, NULL, NULL);
					if (js[k] == '=' && js[k + 1] == '>') {
						removeRoundBracketsAroundParam = true;
					}
				}
			}

			if (removeRoundBracketsAroundParam) {
				roundBlocks[roundNestingLevel - 1]
					= ROUND_BLOCK_PARAM_ARROWFUNC_SINGLE;
			} else {
				roundBlocks[roundNestingLevel - 1] = ROUND_BLOCK_UNKNOWN;
				m.result[resultLength++] = '(';
			}
			continue;
		}
		if (js[i] == ')') {
			if (roundBlocks[roundNestingLevel - 1]
				!= ROUND_BLOCK_PARAM_ARROWFUNC_SINGLE) {
				m.result[resultLength++] = ')';
			}
			if (roundNestingLevel-- == 0) {
				m.errorPosition = i;
				snprintf(m.error,
						 sizeof m.error,
						 "Unexpected `)` in line %%zu, column %%zu\n");
				goto error;
			}
			i += 1;
			continue;
		}
		if (js[i] == ';') {
			if (resultLength == 0) {
				i += 1;
				continue;
			}
			if (roundNestingLevel > 0
				&& roundBlocks[roundNestingLevel - 1]
					   == ROUND_BLOCK_PREFIXED_CONDITION) {
				// Do not remove `;` in `for(;;i++){…}`
				m.result[resultLength++] = ';';
				i += 1;
				continue;
			}
			char beforeSemicolon = m.result[resultLength - 1];
			bool emptyElseStatement = resultLength >= sizeof "else" - 1
				&& !strncmp(&m.result[resultLength - (sizeof "else" - 1)],
						"else",
						sizeof "else" - 1);
			do {
				i += 1;
				JS_SKIP_WHITESPACES_COMMENTS(js, &i, m.result, &resultLength);
			} while (js[i] == ';');

			// `;` can be removed before `}` and at the end of the document
			// except when it follows the `)` of a condition.

			if ((js[i] == '\0' || js[i] == '}')
				&& !(beforeSemicolon == ')'
					 && roundBlocks[roundNestingLevel]
							== ROUND_BLOCK_PREFIXED_CONDITION)
				&& !emptyElseStatement) {
				continue;
			}

			if (beforeSemicolon == '}'
				&& (curlyBlocks[curlyNestingLevel].type
						== CURLY_BLOCK_FUNC_BODY_STANDALONE
					|| curlyBlocks[curlyNestingLevel].type
						   == CURLY_BLOCK_STANDALONE)) {
				continue;
			}

			if (beforeSemicolon == ')'
				&& roundBlocks[roundNestingLevel] == ROUND_BLOCK_DO_WHILE) {
				continue;
			}

			m.result[resultLength++] = ';';
			continue;
		}
		if (js[i] == '/' && js[i + 1] != '/' && js[i + 1] != '*'
			&& (IsJSRegexStart(m.result, resultLength)
				|| (m.result[resultLength - 1] == ' '
					&& m.result[resultLength - 2] == '<'))) {
			// This is a regex object.

			size_t regexStartI = i;
			m.result[resultLength++] = '/';
			i += 1;
			bool activeBackslash = false;
			bool inAngularBrackets = false;
			while (js[i] != '\0'
				   && (js[i] != '/' || activeBackslash || inAngularBrackets)) {
				if (js[i] == '\n') {
					m.errorPosition = i - 1;
					snprintf(m.error,
							 sizeof m.error,
							 "Illegal line break in regex at the end of line "
							 "%%zu\n");
					goto error;
				}
				m.result[resultLength++] = js[i];
				if (js[i] == '[' && !activeBackslash) {
					inAngularBrackets = true;
				} else if (js[i] == ']' && !activeBackslash) {
					inAngularBrackets = false;
				}
				activeBackslash = js[i++] == '\\' && !activeBackslash;
			}
			if (js[i] != '/') {
				m.errorPosition = regexStartI;
				snprintf(
					m.error,
					sizeof m.error,
					"Unclosed regex starting in line %%zu, column %%zu\n");
				goto error;
			}
			m.result[resultLength++] = '/';
			i += 1;
			continue;
		}
		if (js[i] == '`' || js[i] == '"' || js[i] == '\''
			|| (js[i] == '}'
				&& curlyBlocks[curlyNestingLevel - 1].type
					   == CURLY_BLOCK_STRING_INTERPOLATION)) {
			if (js[i] == '}') {
				curlyNestingLevel -= 1;
			}
			m.result[resultLength++] = js[i];
			size_t quoteI;
		mergeStrings:
			quoteI = i;
			i += 1;
			bool activeBackslash = false;
			while (js[i] != '\0') {
				if (!activeBackslash && js[i] == js[quoteI]
					&& js[quoteI] != '}') {
					break;
				}
				if (!activeBackslash && js[quoteI] == '}' && js[i] == '`') {
					break;
				}
				if (js[i] == '\n') {
					if (activeBackslash) {
						i += 1;
						resultLength -= 1;
						continue;
					} else if (js[quoteI] != '`' && js[quoteI] != '}') {
						m.errorPosition = i;
						snprintf(m.error,
								 sizeof m.error,
								 "String contains unescaped line break in "
								 "line %%zu, column %%zu\n");
						goto error;
					}
				}
				m.result[resultLength++] = js[i];
				if (!activeBackslash
					&& (js[quoteI] == '}' || js[quoteI] == '`')
					&& js[i - 1] == '$' && js[i] == '{') {
					INCR_CURLY_NESTING_LEVEL;
					curlyBlocks[curlyNestingLevel - 1].type
						= CURLY_BLOCK_STRING_INTERPOLATION;
					break;
				}
				if (resultLength >= sizeof "</script" - 1
					&& i < quoteI + sizeof "</script" - 1
					&& !StrNICmp(
						&m.result[resultLength - sizeof "</script" + 1],
						"</script",
						sizeof "</script" - 1)) {
					strcpy(&m.result[resultLength - sizeof "</script" + 1],
						   "<\\/script");
					resultLength += 1;
				}
				activeBackslash = js[i++] == '\\' && !activeBackslash;
			}
			if (js[i] == '{') {
				i += 1;
				continue;
			}
			if (js[i] != js[quoteI] && !(js[quoteI] == '}' && js[i] == '`')) {
				while (IsWhitespace(js[i - 1])) {
					i -= 1;
				}
				m.errorPosition = i - 1;
				snprintf(m.error,
						 sizeof m.error,
						 "Unexpected end of script, expected `%c` after line "
						 "%%zu, column %%zu\n",
						 js[quoteI]);
				goto error;
			}
			i += 1;
			size_t k = i;
			bool skippedAllComments
				= JS_SKIP_WHITESPACES_COMMENTS(js, &k, NULL, NULL);
			if (!skippedAllComments || js[k] != '+') {
				m.result[resultLength++]
					= js[quoteI] == '}' ? '`' : js[quoteI];
				continue;
			}
			k += 1;
			skippedAllComments
				= JS_SKIP_WHITESPACES_COMMENTS(js, &k, NULL, NULL);
			if (!skippedAllComments
				|| (js[k] != js[quoteI]
					&& (js[quoteI] != '}' || js[k] != '`'))) {
				m.result[resultLength++]
					= js[quoteI] == '}' ? '`' : js[quoteI];
				continue;
			}

			JS_SKIP_WHITESPACES_COMMENTS(js, &i, m.result, &resultLength);
			i += 1; // Skipping the plus character
			JS_SKIP_WHITESPACES_COMMENTS(js, &i, m.result, &resultLength);

			goto mergeStrings;
		} else if (js[i] == '}') {
			if (curlyBlocks[curlyNestingLevel - 1].doNestingLevel != 0) {
				m.errorPosition = i;
				snprintf(m.error,
						 sizeof m.error,
						 "Unclosed `do` block before `}` in line %%zu, column "
						 "%%zu\n");
				goto error;
			}
			if (curlyNestingLevel-- == 1) {
				m.errorPosition = i;
				snprintf(m.error,
						 sizeof m.error,
						 "Unexpected `}` in line %%zu, column %%zu\n");
				goto error;
			}
			m.result[resultLength++] = '}';
			i += 1;
			continue;
		}
		if (IsWhitespace(js[i]) || (js[i] == '/' && js[i + 1] == '*')
			|| (js[i] == '/' && js[i + 1] == '/')) {
			size_t whitespaceCommentI = i;
			JS_SKIP_WHITESPACES_COMMENTS(js, &i, m.result, &resultLength);
			if (resultLength == 0) {
				continue;
			}
			if (js[i] == '}' || js[i] == '\0') {
				continue;
			}

			if ((js[i] == '+' && m.result[resultLength - 1] == '+')
				|| (js[i] == '-' && m.result[resultLength - 1] == '-')) {
				m.result[resultLength++] = ' ';
				continue;
			}
			if (m.result[resultLength - 1] == ')'
				&& (roundBlocks[roundNestingLevel]
						== ROUND_BLOCK_PREFIXED_CONDITION
					|| roundBlocks[roundNestingLevel] == ROUND_BLOCK_PARAM
					|| roundBlocks[roundNestingLevel]
						   == ROUND_BLOCK_PARAM_STANDALONE
					|| js[i] == '=')) {
				continue;
			}
			if (m.result[resultLength - 1] == '}'
				&& (curlyBlocks[curlyNestingLevel].type
						== CURLY_BLOCK_TRY_FINALLY
					|| curlyBlocks[curlyNestingLevel].type
						   == CURLY_BLOCK_CONDITION_BODY
					|| curlyBlocks[curlyNestingLevel].type
						   == CURLY_BLOCK_FUNC_BODY_STANDALONE
					|| curlyBlocks[curlyNestingLevel].type
						   == CURLY_BLOCK_STANDALONE)) {
				continue;
			}

			// Newlines terminate a preceding statement even when they are in a
			// comment. Try it out: `Math.sin(1)/*\n*/Math.sin(1)` is valid;
			// without `\n` it is invalid.

			bool hasLineBreak = false;
			do {
				if (js[whitespaceCommentI++] == '\n') {
					hasLineBreak = true;
					break;
				}
			} while (whitespaceCommentI < i);

			if (hasLineBreak) {
				if (((resultLength >= 2
					  && m.result[resultLength - 1]
							 == m.result[resultLength - 2]
					  && (m.result[resultLength - 1] == '+'
						  || m.result[resultLength - 1] == '-'))
					 || m.result[resultLength - 1] == '/')
					&& IsJSIdentifierStart(js[i])) {
					m.result[resultLength++] = '\n';
					continue;
				}

				// In JavaScript, `\n` can end a statement similar to `;`. We
				// only remove `\n` when we are sure that it neither ends a
				// statement nor is required as a whitespace between keywords
				// or identifiers. To keep this minifier simple, we accept to
				// miss some occasions were `\n` can be removed.
				//
				// Replacing `\n` between keywords and identifiers by `;` or `
				// ` would be quite difficult and just cosmetic, therefore we
				// preserve these newlines. How to replace depends not only on
				// the keyword but also on its context. For example, `await` is
				// only a keyword in async functions, `get` and `set` are
				// keywords only in object definition blocks, and `async` and
				// access modifiers can be used as variables inside function
				// blocks. Additionally we would need to consider backward and
				// forward compatibility with different JavaScript versions and
				// with TypeScript.

				const char trimNewlineAfter[] = ".([{;=*-+^!~?:,><-+/|&";
				if (strchr(trimNewlineAfter, m.result[resultLength - 1])
					!= NULL) {
					continue;
				}

				// Standalone lines may start with: +-~!"'`/ and more

				const char trimNewlineBefore[] = ")]}.;=*^?:,><|&";
				if (strchr(trimNewlineBefore, js[i]) == NULL) {
					m.result[resultLength++] = '\n';
				}
			} else {
				// Minifying a smaller-than comparison with a specific regex
				// may produce a `</script` tag that breaks inline JavaScript
				// in HTML. The HTML minifier could recognize such cases but
				// does not know how to escape it correctly. It is nontrivial
				// to distinguish such a smaller-than regex from a `</script`
				// tag created by merging strings. Therefore this is the right
				// place to handle the issue.

				const char trimSpaceAround[] = ".()[]{},=*;?!:><-+'\"/|&`";
				if ((strchr(trimSpaceAround, js[i]) == NULL
					 && strchr(trimSpaceAround, m.result[resultLength - 1])
							== NULL)
					|| (m.result[resultLength - 1] == '<'
						&& !StrNICmp(
							&js[i], "/script", sizeof "/script" - 1))) {
					m.result[resultLength++] = ' ';
				}
			}
			continue;
		}
		m.result[resultLength++] = js[i];
		i += 1;
	}
	if (roundNestingLevel != 0) {
		m.errorPosition = lastOpenRoundBracketI;
		snprintf(m.error,
				 sizeof m.error,
				 "Unclosed round bracket in line %%zu, column %%zu\n");
		goto error;
	}
	if (curlyNestingLevel != 1) {
		m.errorPosition = lastOpenCurlyBracketI;
		snprintf(m.error,
				 sizeof m.error,
				 "Unclosed curly bracket in line %%zu, column %%zu\n");
		goto error;
	}
	free(roundBlocks);
	free(curlyBlocks);
	return (m);

error:
	free(roundBlocks);
	free(curlyBlocks);
	free(m.result);
	m.result = NULL;
	return (m);
}


struct Minification MinifyJSWithOptions( const char * js )
{
	struct Minification m = MinifyJS(js);

	if (m.result != NULL && MangleOutputEnabled()) {
		struct Minification mangled = MangleJSIdentifiers(m.result, false);
		free(m.result);
		m = mangled;
	}

	return (m);
}


struct Minification MinifyJSModuleWithOptions( const char * js )
{
	struct Minification m = MinifyJS(js);

	if (m.result != NULL && MangleOutputEnabled()) {
		const char * moduleMarker = "import\"\";";
		size_t moduleMarkerLength = strlen(moduleMarker);
		size_t resultLength = strlen(m.result);
		char * moduleJs = malloc(moduleMarkerLength + resultLength + 1);
		if (moduleJs == NULL) {
			free(m.result);
			m.result = NULL;
			snprintf(m.error, sizeof m.error, "Cannot allocate memory\n");
			return (m);
		}
		memcpy(moduleJs, moduleMarker, moduleMarkerLength);
		memcpy(&moduleJs[moduleMarkerLength], m.result, resultLength + 1);

		struct Minification mangled = MangleJSIdentifiers(moduleJs, false);
		free(moduleJs);
		free(m.result);
		m = mangled;
		if (m.result != NULL) {
			memmove(m.result,
					&m.result[moduleMarkerLength],
					strlen(&m.result[moduleMarkerLength]) + 1);
		}
	}

	return (m);
}
