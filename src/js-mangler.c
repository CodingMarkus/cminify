/*
 * Copyright 2026 CodingMarkus
 *
 * SPDX-License-Identifier: ISC
 */

#include "js-mangler.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct JsMangleEdit {
	size_t start;
	size_t length;
	const char * replacement;
	char replacementStorage[32];
	size_t replacementLength;
	bool replacementDynamic;
};

struct JsMangleBinding {
	const char * name;
	size_t length;
	struct JsMangleNameIndex * nameIndex;
	char replacement[16];
	size_t replacementLength;
	bool importBare;
};

struct JsMangleMap {
	const char * name;
	size_t length;
	struct JsMangleNameIndex * nameIndex;
	char replacement[16];
	size_t replacementLength;
	size_t scopeStart;
	size_t scopeEnd;
	size_t visibleGeneration;
	struct JsMangleMap * nextForName;
};

struct JsMangleRange {
	size_t start;
	size_t end;
};

enum JsMangleTokenKind {
	JS_MANGLE_TOKEN_IDENTIFIER,
	JS_MANGLE_TOKEN_PUNCTUATION,
	JS_MANGLE_TOKEN_STRING,
	JS_MANGLE_TOKEN_REGEX,
};

struct JsMangleNameIndex;

struct JsMangleToken {
	enum JsMangleTokenKind kind;
	size_t start;
	size_t length;
	char punctuation;
	struct JsMangleNameIndex * nameIndex;
	size_t enclosingOpen;
	size_t matchingToken;
};

struct JsMangleScope {
	size_t parent;
	size_t tokenStart;
	size_t tokenEnd;
};

struct JsMangleDeclaration {
	size_t token;
	size_t scope;
};

struct JsMangleReference {
	size_t token;
	size_t scope;
};

struct JsMangleNameIndex {
	const char * name;
	size_t length;
	size_t occurrencesStart;
	size_t occurrencesLength;
	size_t visibleGeneration;
	struct JsMangleBinding * binding;
	struct JsMangleMap * firstMap;
};

struct JsMangleProgram {
	struct JsMangleToken * tokens;
	size_t tokensLength;
	size_t tokensCapacity;

	struct JsMangleScope * scopes;
	size_t scopesLength;
	size_t scopesCapacity;

	struct JsMangleDeclaration * declarations;
	size_t declarationsLength;
	size_t declarationsCapacity;

	struct JsMangleReference * references;
	size_t referencesLength;
	size_t referencesCapacity;

	struct JsMangleNameIndex * nameIndices;
	size_t nameIndicesCapacity;
	size_t * nameOccurrences;
	size_t nameOccurrencesCapacity;
	size_t visibleGeneration;
};

struct JsMangleCounts {
	size_t tokens;
	size_t identifiers;
	size_t scopes;
};

struct JsMangleState {
	struct JsMangleEdit * edits;
	size_t editsLength;
	size_t editsCapacity;

	struct JsMangleMap * maps;
	size_t mapsLength;
	size_t mapsCapacity;

	struct JsMangleRange * unsafeRanges;
	size_t unsafeRangesLength;
	size_t unsafeRangesCapacity;
};

static const char * jsMangleFailureReason = NULL;

static void jsManglePreviousNext(
	const char * js,
	size_t start,
	size_t end,
	size_t wordStart,
	size_t wordEnd,
	char * previous,
	char * next
);

static bool jsIdentifierStart( char c )
{
	return (c == '_' || c == '$' || (c >= 'a' && c <= 'z')
			|| (c >= 'A' && c <= 'Z'));
}


static bool jsIdentifierPart( char c )
{
	return (jsIdentifierStart(c) || (c >= '0' && c <= '9'));
}


static bool jsWordEquals( const char * js, size_t i, const char * word )
{
	size_t length = strlen(word);
	return (!strncmp(&js[i], word, length)
			&& !jsIdentifierPart(js[i + length]));
}


static bool jsKeyword( const char * name, size_t length )
{
	static const char * keywords[] = {
		"break",	"case",	   "catch",		 "class", "const",	  "continue",
		"debugger", "default", "delete",	 "do",	  "else",	  "export",
		"extends",	"false",   "finally",	 "for",	  "function", "if",
		"import",	"in",	   "instanceof", "let",	  "new",	  "null",
		"return",	"super",   "switch",	 "this",  "throw",	  "true",
		"try",		"typeof",  "var",		 "void",  "while",	  "with",
		"yield"};
	for (size_t i = 0; i < sizeof keywords / sizeof keywords[0]; ++i) {
		if (strlen(keywords[i]) == length
			&& !strncmp(name, keywords[i], length)) {
			return (true);
		}
	}
	return (false);
}


static size_t jsDeclarationKeywordLength( const char * js, size_t i )
{
	if (jsWordEquals(js, i, "var") || jsWordEquals(js, i, "let")) {
		return (3);
	}
	if (jsWordEquals(js, i, "const")) {
		return (5);
	}
	return (0);
}


static size_t jsSkipQuoted( const char * js, size_t i, size_t end )
{
	char quote = js[i++];
	bool activeBackslash = false;
	while (i < end) {
		if (!activeBackslash && js[i] == quote) {
			return (i + 1);
		}
		activeBackslash = js[i++] == '\\' && !activeBackslash;
	}
	return (end);
}


static size_t jsSkipRegex( const char * js, size_t i, size_t end )
{
	bool activeBackslash = false;
	bool inBrackets = false;
	i += 1;
	while (i < end) {
		if (!activeBackslash && !inBrackets && js[i] == '/') {
			return (i + 1);
		}
		if (!activeBackslash && js[i] == '[') {
			inBrackets = true;
		} else if (!activeBackslash && js[i] == ']') {
			inBrackets = false;
		}
		activeBackslash = js[i++] == '\\' && !activeBackslash;
	}
	return (end);
}


static size_t jsSkipComment( const char * js, size_t i, size_t end )
{
	if (js[i + 1] == '*') {
		i += 2;
		while (i < end && (js[i] != '*' || js[i + 1] != '/')) {
			i += 1;
		}
		return (i < end ? i + 2 : end);
	}
	i += 2;
	while (i < end && js[i] != '\n') {
		i += 1;
	}
	return (i);
}


static bool jsRegexStart( const char * js, size_t start, size_t i )
{
	size_t k = i;
	while (k > start && IsWhitespace(js[k - 1])) {
		k -= 1;
	}
	if (k == start || strchr("^!&|([{><+-*%:?~,;=", js[k - 1]) != NULL) {
		return (true);
	}
	if (!jsIdentifierPart(js[k - 1])) {
		return (false);
	}
	size_t wordStart = k - 1;
	while (wordStart > start && jsIdentifierPart(js[wordStart - 1])) {
		wordStart -= 1;
	}
	return (jsWordEquals(js, wordStart, "return")
			|| jsWordEquals(js, wordStart, "throw")
			|| jsWordEquals(js, wordStart, "case")
			|| jsWordEquals(js, wordStart, "delete")
			|| jsWordEquals(js, wordStart, "typeof")
			|| jsWordEquals(js, wordStart, "void"));
}


static size_t jsFindMatching(
	const char * js, size_t i, size_t end, char open, char close
)
{
	size_t nesting = 1;
	for (i += 1; i < end; ++i) {
		if (js[i] == '"' || js[i] == '\'' || js[i] == '`') {
			i = jsSkipQuoted(js, i, end) - 1;
		} else if (js[i] == '/' && (js[i + 1] == '*' || js[i + 1] == '/')) {
			i = jsSkipComment(js, i, end) - 1;
		} else if (js[i] == '/' && js[i + 1] != '/' && js[i + 1] != '*'
				   && jsRegexStart(js, 0, i)) {
			i = jsSkipRegex(js, i, end) - 1;
		} else if (js[i] == open) {
			nesting += 1;
		} else if (js[i] == close && --nesting == 0) {
			return (i);
		}
	}
	return (end);
}


static bool jsMangleAddToken(
	struct JsMangleProgram * program, enum JsMangleTokenKind kind, size_t start,
	size_t length, char punctuation
)
{
	if (program->tokensLength >= program->tokensCapacity) {
		return (false);
	}
	program->tokens[program->tokensLength++] = (struct JsMangleToken){
		kind, start, length, punctuation, NULL, SIZE_MAX, SIZE_MAX};
	return (true);
}


static bool jsMangleTokenize(
	const char * js, size_t end, struct JsMangleProgram * program
)
{
	for (size_t i = 0; i < end; ++i) {
		if (IsWhitespace(js[i])) {
			continue;
		}
		if (js[i] == '"' || js[i] == '\'' || js[i] == '`') {
			size_t tokenStart = i;
			i = jsSkipQuoted(js, i, end) - 1;
			if (!jsMangleAddToken(program,
								  JS_MANGLE_TOKEN_STRING,
								  tokenStart,
								  i - tokenStart + 1,
								  '\0')) {
				return (false);
			}
		} else if (js[i] == '/' && (js[i + 1] == '*' || js[i + 1] == '/')) {
			i = jsSkipComment(js, i, end) - 1;
		} else if (js[i] == '/' && jsRegexStart(js, 0, i)) {
			size_t tokenStart = i;
			i = jsSkipRegex(js, i, end) - 1;
			if (!jsMangleAddToken(program,
								  JS_MANGLE_TOKEN_REGEX,
								  tokenStart,
								  i - tokenStart + 1,
								  '\0')) {
				return (false);
			}
		} else if (jsIdentifierStart(js[i])) {
			size_t tokenStart = i;
			while (jsIdentifierPart(js[i])) {
				i += 1;
			}
			if (!jsMangleAddToken(program,
								  JS_MANGLE_TOKEN_IDENTIFIER,
								  tokenStart,
								  i - tokenStart,
								  '\0')) {
				return (false);
			}
			i -= 1;
		} else {
			if (!jsMangleAddToken(
					program, JS_MANGLE_TOKEN_PUNCTUATION, i, 1, js[i])) {
				return (false);
			}
		}
	}
	return (true);
}


static void jsMangleCountProgram(
	const char * js, size_t end, struct JsMangleCounts * counts
)
{
	counts->scopes = 1;
	for (size_t i = 0; i < end; ++i) {
		if (IsWhitespace(js[i])) {
			continue;
		}
		counts->tokens += 1;
		if (js[i] == '"' || js[i] == '\'' || js[i] == '`') {
			i = jsSkipQuoted(js, i, end) - 1;
		} else if (js[i] == '/' && (js[i + 1] == '*' || js[i + 1] == '/')) {
			counts->tokens -= 1;
			i = jsSkipComment(js, i, end) - 1;
		} else if (js[i] == '/' && jsRegexStart(js, 0, i)) {
			i = jsSkipRegex(js, i, end) - 1;
		} else if (jsIdentifierStart(js[i])) {
			counts->identifiers += 1;
			while (jsIdentifierPart(js[i])) {
				i += 1;
			}
			i -= 1;
		} else if (js[i] == '{') {
			counts->scopes += 1;
		}
	}
}


static bool jsMangleAllocProgram(
	struct JsMangleProgram * program, struct JsMangleCounts counts
)
{
	program->tokensCapacity = counts.tokens;
	program->scopesCapacity = counts.scopes;
	program->declarationsCapacity = counts.identifiers;
	program->referencesCapacity = counts.identifiers;
	program->nameIndicesCapacity = counts.identifiers * 2 + 1;
	program->nameOccurrencesCapacity = counts.identifiers;

	if (program->tokensCapacity != 0) {
		program->tokens
			= malloc(program->tokensCapacity * sizeof *program->tokens);
	}
	if (program->scopesCapacity != 0) {
		program->scopes
			= malloc(program->scopesCapacity * sizeof *program->scopes);
	}
	if (program->declarationsCapacity != 0) {
		program->declarations = malloc(program->declarationsCapacity
									   * sizeof *program->declarations);
	}
	if (program->referencesCapacity != 0) {
		program->references = malloc(program->referencesCapacity
									 * sizeof *program->references);
	}
	if (program->nameIndicesCapacity != 0) {
		program->nameIndices = calloc(program->nameIndicesCapacity,
									  sizeof *program->nameIndices);
	}
	if (program->nameOccurrencesCapacity != 0) {
		program->nameOccurrences = malloc(program->nameOccurrencesCapacity
										  * sizeof *program->nameOccurrences);
	}
	return (program->tokensCapacity == 0 || program->tokens != NULL)
		   && (program->scopesCapacity == 0 || program->scopes != NULL)
		   && (program->declarationsCapacity == 0
			   || program->declarations != NULL)
		   && (program->referencesCapacity == 0 || program->references != NULL)
		   && (program->nameIndicesCapacity == 0
			   || program->nameIndices != NULL)
		   && (program->nameOccurrencesCapacity == 0
			   || program->nameOccurrences != NULL);
}


static bool jsMangleAllocState(
	struct JsMangleState * state, struct JsMangleCounts counts
)
{
	state->editsCapacity = counts.identifiers * 2 + 1;
	state->mapsCapacity = counts.identifiers + 1;
	state->unsafeRangesCapacity = counts.identifiers + 1;

	state->edits = malloc(state->editsCapacity * sizeof *state->edits);
	state->maps = malloc(state->mapsCapacity * sizeof *state->maps);
	state->unsafeRanges
		= malloc(state->unsafeRangesCapacity * sizeof *state->unsafeRanges);
	return (state->edits != NULL && state->maps != NULL
			&& state->unsafeRanges != NULL);
}


static size_t jsMangleCountIdentifiers(
	const char * js, size_t start, size_t end
)
{
	size_t count = 0;
	for (size_t i = start; i < end; ++i) {
		if (js[i] == '"' || js[i] == '\'' || js[i] == '`') {
			i = jsSkipQuoted(js, i, end) - 1;
		} else if (js[i] == '/' && (js[i + 1] == '*' || js[i + 1] == '/')) {
			i = jsSkipComment(js, i, end) - 1;
		} else if (js[i] == '/' && jsRegexStart(js, start, i)) {
			i = jsSkipRegex(js, i, end) - 1;
		} else if (jsIdentifierStart(js[i])) {
			count += 1;
			while (jsIdentifierPart(js[i])) {
				i += 1;
			}
			i -= 1;
		}
	}
	return (count);
}


static bool jsMangleAllocBindings(
	struct JsMangleBinding ** bindings, size_t * capacity, size_t count
)
{
	*capacity = count;
	if (*capacity == 0) {
		*bindings = NULL;
		return (true);
	}
	*bindings = malloc(*capacity * sizeof **bindings);
	return (*bindings != NULL);
}


static bool jsMangleAddScope(
	struct JsMangleProgram * program, size_t parent, size_t tokenStart
)
{
	if (program->scopesLength >= program->scopesCapacity) {
		return (false);
	}
	program->scopes[program->scopesLength++]
		= (struct JsMangleScope){parent, tokenStart, tokenStart};
	return (true);
}


static bool jsMangleAddDeclaration(
	struct JsMangleProgram * program, size_t token, size_t scope
)
{
	if (program->declarationsLength >= program->declarationsCapacity) {
		return (false);
	}
	program->declarations[program->declarationsLength++]
		= (struct JsMangleDeclaration){token, scope};
	return (true);
}


static bool jsMangleAddReference(
	struct JsMangleProgram * program, size_t token, size_t scope
)
{
	if (program->referencesLength >= program->referencesCapacity) {
		return (false);
	}
	program->references[program->referencesLength++]
		= (struct JsMangleReference){token, scope};
	return (true);
}


static bool jsMangleTokenEquals(
	const char * js, const struct JsMangleToken * token, const char * word
)
{
	size_t length = strlen(word);
	return (token->kind == JS_MANGLE_TOKEN_IDENTIFIER
			&& token->length == length
			&& !strncmp(&js[token->start], word, length));
}


static size_t jsMangleNameHash( const char * name, size_t length )
{
	size_t hash = 1469598103934665603ull;
	for (size_t i = 0; i < length; ++i) {
		hash ^= (unsigned char)name[i];
		hash *= 1099511628211ull;
	}
	return (hash);
}


static struct JsMangleNameIndex * jsMangleFindNameIndex(
	struct JsMangleProgram * program, const char * name, size_t length )
{
	if (program->nameIndicesCapacity == 0) {
		return (NULL);
	}
	size_t i = jsMangleNameHash(name, length) % program->nameIndicesCapacity;
	for (size_t probes = 0; probes < program->nameIndicesCapacity; ++probes) {
		struct JsMangleNameIndex * index = &program->nameIndices[i];
		if (index->name == NULL) {
			return (index);
		}
		if (index->length == length && !strncmp(index->name, name, length)) {
			return (index);
		}
		i += 1;
		if (i == program->nameIndicesCapacity) {
			i = 0;
		}
	}
	return (NULL);
}


static bool jsMangleBuildNameIndex(
	struct JsMangleProgram * program, const char * js
)
{
	for (size_t i = 0; i < program->tokensLength; ++i) {
		struct JsMangleToken * token = &program->tokens[i];
		if (token->kind != JS_MANGLE_TOKEN_IDENTIFIER) {
			continue;
		}
		struct JsMangleNameIndex * index
			= jsMangleFindNameIndex(program, &js[token->start], token->length);
		if (index == NULL) {
			return (false);
		}
		if (index->name == NULL) {
			index->name = &js[token->start];
			index->length = token->length;
		}
		index->occurrencesLength += 1;
	}

	size_t occurrencesStart = 0;
	for (size_t i = 0; i < program->nameIndicesCapacity; ++i) {
		struct JsMangleNameIndex * index = &program->nameIndices[i];
		if (index->name == NULL) {
			continue;
		}
		index->occurrencesStart = occurrencesStart;
		occurrencesStart += index->occurrencesLength;
		index->occurrencesLength = 0;
	}
	if (occurrencesStart > program->nameOccurrencesCapacity) {
		return (false);
	}

	for (size_t i = 0; i < program->tokensLength; ++i) {
		struct JsMangleToken * token = &program->tokens[i];
		if (token->kind != JS_MANGLE_TOKEN_IDENTIFIER) {
			continue;
		}
		struct JsMangleNameIndex * index
			= jsMangleFindNameIndex(program, &js[token->start], token->length);
		if (index == NULL || index->name == NULL) {
			return (false);
		}
		token->nameIndex = index;
		program->nameOccurrences[index->occurrencesStart
								 + index->occurrencesLength++]
			= i;
	}
	return (true);
}


static bool jsMangleMatchingPunctuation( char open, char close )
{
	return (open == '{' && close == '}') || (open == '(' && close == ')')
		   || (open == '[' && close == ']');
}


static bool jsMangleBuildPunctuationIndex( struct JsMangleProgram * program )
{
	if (program->tokensLength == 0) {
		return (true);
	}
	size_t * stack = malloc(program->tokensLength * sizeof *stack);
	if (stack == NULL) {
		return (false);
	}
	size_t stackLength = 0;
	for (size_t i = 0; i < program->tokensLength; ++i) {
		struct JsMangleToken * token = &program->tokens[i];
		token->enclosingOpen
			= stackLength == 0 ? SIZE_MAX : stack[stackLength - 1];
		if (token->kind != JS_MANGLE_TOKEN_PUNCTUATION) {
			continue;
		}
		if (token->punctuation == '{' || token->punctuation == '('
			|| token->punctuation == '[') {
			stack[stackLength++] = i;
		} else if (token->punctuation == '}' || token->punctuation == ')'
				   || token->punctuation == ']') {
			while (stackLength > 0) {
				size_t openI = stack[--stackLength];
				struct JsMangleToken * open = &program->tokens[openI];
				if (jsMangleMatchingPunctuation(open->punctuation,
												token->punctuation)) {
					open->matchingToken = i;
					token->matchingToken = openI;
					token->enclosingOpen
						= stackLength == 0 ? SIZE_MAX : stack[stackLength - 1];
					break;
				}
			}
		}
	}
	free(stack);
	return (true);
}


static size_t jsMangleFindTokenStart(
	struct JsMangleProgram * program, size_t start
)
{
	size_t left = 0;
	size_t right = program->tokensLength;
	while (left < right) {
		size_t middle = left + (right - left) / 2;
		if (program->tokens[middle].start < start) {
			left = middle + 1;
		} else {
			right = middle;
		}
	}
	return (left);
}


static size_t jsMangleFindOccurrenceStart(
	struct JsMangleProgram * program, struct JsMangleNameIndex * index,
	size_t start
)
{
	size_t left = 0;
	size_t right = index->occurrencesLength;
	while (left < right) {
		size_t middle = left + (right - left) / 2;
		size_t tokenI
			= program->nameOccurrences[index->occurrencesStart + middle];
		if (program->tokens[tokenI].start < start) {
			left = middle + 1;
		} else {
			right = middle;
		}
	}
	return (left);
}


static bool jsMangleBuildProgram(
	const char * js, size_t end, struct JsMangleProgram * program
)
{
	if (!jsMangleTokenize(js, end, program)) {
		return (false);
	}
	if (!jsMangleBuildPunctuationIndex(program)) {
		return (false);
	}
	if (!jsMangleBuildNameIndex(program, js)) {
		return (false);
	}
	if (!jsMangleAddScope(program, 0, 0)) {
		return (false);
	}

	size_t scope = 0;
	for (size_t i = 0; i < program->tokensLength; ++i) {
		struct JsMangleToken * token = &program->tokens[i];
		if (token->kind == JS_MANGLE_TOKEN_PUNCTUATION
			&& token->punctuation == '{') {
			if (!jsMangleAddScope(program, scope, i)) {
				return (false);
			}
			scope = program->scopesLength - 1;
		} else if (token->kind == JS_MANGLE_TOKEN_PUNCTUATION
				   && token->punctuation == '}' && scope != 0) {
			program->scopes[scope].tokenEnd = i;
			scope = program->scopes[scope].parent;
		} else if (jsMangleTokenEquals(js, token, "var")
				   || jsMangleTokenEquals(js, token, "let")
				   || jsMangleTokenEquals(js, token, "const")) {
			if (i + 1 < program->tokensLength
				&& program->tokens[i + 1].kind == JS_MANGLE_TOKEN_IDENTIFIER) {
				if (!jsMangleAddDeclaration(program, i + 1, scope)) {
					return (false);
				}
			}
		} else if (token->kind == JS_MANGLE_TOKEN_IDENTIFIER
				   && !jsKeyword(&js[token->start], token->length)) {
			if (!jsMangleAddReference(program, i, scope)) {
				return (false);
			}
		}
	}
	program->scopes[0].tokenEnd = program->tokensLength;
	return (true);
}


static void jsMangleFreeProgram( struct JsMangleProgram * program )
{
	free(program->tokens);
	free(program->scopes);
	free(program->declarations);
	free(program->references);
	free(program->nameIndices);
	free(program->nameOccurrences);
}


static void jsMangleRebaseEdits( struct JsMangleState * state )
{
	for (size_t i = 0; i < state->editsLength; ++i) {
		if (!state->edits[i].replacementDynamic) {
			state->edits[i].replacement = state->edits[i].replacementStorage;
		}
	}
}


static bool jsMangleEnsureEditCapacity( struct JsMangleState * state )
{
	if (state->editsLength < state->editsCapacity) {
		return (true);
	}
	size_t capacity = state->editsCapacity > 0 ? state->editsCapacity * 2 : 1;
	struct JsMangleEdit * edits
		= realloc(state->edits, capacity * sizeof *state->edits);
	if (edits == NULL) {
		return (false);
	}
	state->edits = edits;
	state->editsCapacity = capacity;
	jsMangleRebaseEdits(state);
	return (true);
}


static void jsMangleFreeEdit( struct JsMangleEdit * edit )
{
	if (edit->replacementDynamic) {
		free((void *)edit->replacement);
		edit->replacement = NULL;
		edit->replacementDynamic = false;
	}
}


static void jsMangleClearEdit( struct JsMangleEdit * edit )
{
	edit->replacement = NULL;
	edit->replacementLength = 0;
	edit->replacementDynamic = false;
}


static bool jsMangleSetEditReplacement(
	struct JsMangleEdit * edit, const char * replacement, size_t replacementLength
)
{
	if (replacementLength < sizeof edit->replacementStorage) {
		edit->replacement = edit->replacementStorage;
		memcpy(edit->replacementStorage, replacement, replacementLength);
		edit->replacementDynamic = false;
		return (true);
	}
	char * dynamicReplacement = malloc(replacementLength);
	if (dynamicReplacement == NULL) {
		return (false);
	}
	memcpy(dynamicReplacement, replacement, replacementLength);
	edit->replacement = dynamicReplacement;
	edit->replacementDynamic = true;
	return (true);
}


static void jsMangleFreeEdits( struct JsMangleState * state )
{
	for (size_t i = 0; i < state->editsLength; ++i) {
		jsMangleFreeEdit(&state->edits[i]);
	}
}


static bool jsMangleAddEdit(
	struct JsMangleState * state, size_t start, size_t length,
	const char * replacement, size_t replacementLength
)
{
	if (!jsMangleEnsureEditCapacity(state)) {
		return (false);
	}
	struct JsMangleEdit * edit = &state->edits[state->editsLength++];
	edit->start = start;
	edit->length = length;
	edit->replacementLength = replacementLength;
	if (!jsMangleSetEditReplacement(edit, replacement, replacementLength)) {
		state->editsLength -= 1;
		return (false);
	}
	return (true);
}


static bool jsMangleAddShorthandEdit(
	struct JsMangleState * state, size_t start, size_t length,
	const char * replacement, size_t replacementLength
)
{
	if (!jsMangleEnsureEditCapacity(state)) {
		return (false);
	}
	struct JsMangleEdit * edit = &state->edits[state->editsLength++];
	size_t editReplacementLength = replacementLength * 2 + 1;
	char * replacementBuffer = malloc(editReplacementLength);
	if (replacementBuffer == NULL) {
		state->editsLength -= 1;
		return (false);
	}
	edit->start = start;
	edit->length = length;
	memcpy(replacementBuffer, replacement, replacementLength);
	replacementBuffer[replacementLength] = ':';
	memcpy(&replacementBuffer[replacementLength + 1],
		   replacement,
		   replacementLength);
	edit->replacementLength = editReplacementLength;
	if (!jsMangleSetEditReplacement(
			edit, replacementBuffer, editReplacementLength)) {
		free(replacementBuffer);
		state->editsLength -= 1;
		return (false);
	}
	free(replacementBuffer);
	return (true);
}


static bool jsMangleAddImportAliasEdit(
	struct JsMangleState * state, size_t start, const char * original,
	size_t length, const char * replacement, size_t replacementLength
)
{
	const char as[] = " as ";
	if (!jsMangleEnsureEditCapacity(state)) {
		return (false);
	}
	struct JsMangleEdit * edit = &state->edits[state->editsLength++];
	size_t editReplacementLength = length + sizeof as - 1 + replacementLength;
	char * replacementBuffer = malloc(editReplacementLength);
	if (replacementBuffer == NULL) {
		state->editsLength -= 1;
		return (false);
	}
	edit->start = start;
	edit->length = length;
	memcpy(replacementBuffer, original, length);
	memcpy(&replacementBuffer[length], as, sizeof as - 1);
	memcpy(&replacementBuffer[length + sizeof as - 1],
		   replacement,
		   replacementLength);
	edit->replacement = replacementBuffer;
	edit->replacementLength = editReplacementLength;
	edit->replacementDynamic = true;
	return (true);
}


static bool jsMangleAddPatternAliasEdit(
	struct JsMangleState * state, size_t start, const char * original,
	size_t length, const char * replacement, size_t replacementLength
)
{
	if (!jsMangleEnsureEditCapacity(state)) {
		return (false);
	}
	struct JsMangleEdit * edit = &state->edits[state->editsLength++];
	size_t editReplacementLength = length + 1 + replacementLength;
	char * replacementBuffer = malloc(editReplacementLength);
	if (replacementBuffer == NULL) {
		state->editsLength -= 1;
		return (false);
	}
	edit->start = start;
	edit->length = length;
	memcpy(replacementBuffer, original, length);
	replacementBuffer[length] = ':';
	memcpy(&replacementBuffer[length + 1], replacement, replacementLength);
	edit->replacement = replacementBuffer;
	edit->replacementLength = editReplacementLength;
	edit->replacementDynamic = true;
	return (true);
}


static int jsMangleCompareEdits( const void * a, const void * b )
{
	const struct JsMangleEdit * editA = a;
	const struct JsMangleEdit * editB = b;
	return (editA->start > editB->start) - (editA->start < editB->start);
}


static bool jsMangleAddMap(
	struct JsMangleProgram * program, struct JsMangleState * state,
	struct JsMangleBinding binding, size_t scopeStart, size_t scopeEnd
)
{
	if (state->mapsLength >= state->mapsCapacity) {
		return (false);
	}
	struct JsMangleNameIndex * index
		= jsMangleFindNameIndex(program, binding.name, binding.length);
	struct JsMangleMap * map = &state->maps[state->mapsLength++];
	*map = (struct JsMangleMap){binding.name,
								binding.length,
								index,
								"",
								binding.replacementLength,
								scopeStart,
								scopeEnd,
								0,
								NULL};
	memcpy(
		map->replacement, binding.replacement, binding.replacementLength + 1);
	if (index != NULL) {
		map->nextForName = index->firstMap;
		index->firstMap = map;
	}
	return (true);
}


static bool jsMangleAddUnsafeRange(
	struct JsMangleState * state, size_t start, size_t end
)
{
	if (state->unsafeRangesLength >= state->unsafeRangesCapacity) {
		return (false);
	}
	state->unsafeRanges[state->unsafeRangesLength++]
		= (struct JsMangleRange){start, end};
	return (true);
}


static bool jsMangleInUnsafeRange( struct JsMangleState * state, size_t i )
{
	for (size_t r = 0; r < state->unsafeRangesLength; ++r) {
		if (i > state->unsafeRanges[r].start
			&& i < state->unsafeRanges[r].end) {
			return (true);
		}
	}
	return (false);
}


static bool jsMangleSameName(
	const char * a, size_t aLength, const char * b, size_t bLength
)
{
	return (aLength == bLength && !strncmp(a, b, aLength));
}


static bool jsMangleAddBinding(
	struct JsMangleBinding ** bindings, size_t * bindingsLength,
	size_t * bindingsCapacity, const char * name, size_t length
)
{
	if (length <= 1 || jsKeyword(name, length)) {
		return (true);
	}
	for (size_t i = 0; i < *bindingsLength; ++i) {
		if (jsMangleSameName(
				(*bindings)[i].name, (*bindings)[i].length, name, length)) {
			return (true);
		}
	}
	if (*bindingsLength >= *bindingsCapacity) {
		return (false);
	}
	(*bindings)[*bindingsLength]
		= (struct JsMangleBinding){name, length, NULL, "", 0, false};
	*bindingsLength += 1;
	return (true);
}


static bool jsMangleAddImportBinding(
	struct JsMangleBinding ** bindings, size_t * bindingsLength,
	size_t * bindingsCapacity, const char * name, size_t length, bool importBare
)
{
	if (!jsMangleAddBinding(
			bindings, bindingsLength, bindingsCapacity, name, length)) {
		return (false);
	}
	for (size_t i = 0; i < *bindingsLength; ++i) {
		if (jsMangleSameName(
				(*bindings)[i].name, (*bindings)[i].length, name, length)) {
			(*bindings)[i].importBare = importBare;
			break;
		}
	}
	return (true);
}


static bool jsMangleBindingListContains(
	struct JsMangleBinding * bindings, size_t bindingsLength, const char * name,
	size_t length
)
{
	for (size_t i = 0; i < bindingsLength; ++i) {
		if (jsMangleSameName(
				bindings[i].name, bindings[i].length, name, length)) {
			return (true);
		}
	}
	return (false);
}


static bool jsMangleAddGlobalBinding(
	struct JsMangleBinding ** bindings,
	size_t * bindingsLength,
	size_t * bindingsCapacity,
	struct JsMangleBinding * excluded,
	size_t excludedLength,
	const char * name,
	size_t length
)
{
	if (jsMangleBindingListContains(excluded, excludedLength, name, length)) {
		return (true);
	}
	return (jsMangleAddBinding(
		bindings, bindingsLength, bindingsCapacity, name, length));
}


static bool jsMangleAddGlobalImportBinding(
	struct JsMangleBinding ** bindings,
	size_t * bindingsLength,
	size_t * bindingsCapacity,
	struct JsMangleBinding * excluded,
	size_t excludedLength,
	const char * name,
	size_t length,
	bool importBare
)
{
	if (jsMangleBindingListContains(excluded, excludedLength, name, length)) {
		return (true);
	}
	return (jsMangleAddImportBinding(
		bindings, bindingsLength, bindingsCapacity, name, length, importBare));
}


static bool jsMangleCollectPatternBindings(
	const char * js, size_t start, size_t end, struct JsMangleBinding ** bindings,
	size_t * bindingsLength, size_t * bindingsCapacity
)
{
	char containers[64];
	size_t containersLength = 0;

	for (size_t i = start; i < end; ++i) {
		if (js[i] == '"' || js[i] == '\'' || js[i] == '`') {
			i = jsSkipQuoted(js, i, end) - 1;
		} else if ((js[i] == '[' || js[i] == '{')
				   && containersLength < sizeof containers) {
			containers[containersLength++] = js[i];
		} else if ((js[i] == ']' || js[i] == '}') && containersLength > 0) {
			containersLength -= 1;
		} else if (containersLength > 0 && jsIdentifierStart(js[i])) {
			size_t wordStart = i;
			while (jsIdentifierPart(js[i])) {
				i += 1;
			}
			char previous, next;
			jsManglePreviousNext(
				js, start, end, wordStart, i, &previous, &next);
			bool restElement
				= wordStart >= start + 3 && js[wordStart - 1] == '.'
				  && js[wordStart - 2] == '.' && js[wordStart - 3] == '.';
			bool binding
				= containers[containersLength - 1] == '['
				  && (previous == '[' || previous == ',' || restElement);
			binding = binding
					  || (containers[containersLength - 1] == '{'
						  && (previous == ':' || previous == '{'
							  || previous == ',' || restElement));
			if (binding && next != ':') {
				if (!jsMangleAddBinding(bindings,
										bindingsLength,
										bindingsCapacity,
										&js[wordStart],
										i - wordStart)) {
					return (false);
				}
			}
			i -= 1;
		}
	}
	return (true);
}


static bool jsMangleCollectGlobalPatternBindings(
	const char * js,
	size_t start,
	size_t end,
	struct JsMangleBinding ** bindings,
	size_t * bindingsLength,
	size_t * bindingsCapacity,
	struct JsMangleBinding * excluded,
	size_t excludedLength
)
{
	struct JsMangleBinding * patternBindings = NULL;
	size_t patternBindingsLength = 0;
	size_t patternBindingsCapacity = 0;
	bool success = false;

	if (!jsMangleAllocBindings(&patternBindings,
							   &patternBindingsCapacity,
							   jsMangleCountIdentifiers(js, start, end))) {
		goto done;
	}
	if (!jsMangleCollectPatternBindings(js,
										start,
										end,
										&patternBindings,
										&patternBindingsLength,
										&patternBindingsCapacity)) {
		goto done;
	}
	for (size_t i = 0; i < patternBindingsLength; ++i) {
		if (!jsMangleAddGlobalBinding(bindings,
									  bindingsLength,
									  bindingsCapacity,
									  excluded,
									  excludedLength,
									  patternBindings[i].name,
									  patternBindings[i].length)) {
			goto done;
		}
	}
	success = true;

done:
	free(patternBindings);
	return (success);
}


static size_t jsMangleNameCount(
	struct JsMangleProgram * program, const char * js, size_t start, size_t end,
	const char * name, size_t length
)
{
	struct JsMangleNameIndex * index
		= jsMangleFindNameIndex(program, name, length);
	if (index == NULL || index->name == NULL) {
		return (0);
	}
	size_t count = 0;
	for (size_t i = jsMangleFindOccurrenceStart(program, index, start);
		 i < index->occurrencesLength;
		 ++i) {
		struct JsMangleToken * token
			= &program->tokens[program->nameOccurrences[index->occurrencesStart
														+ i]];
		if (token->start >= end) {
			break;
		}
		if (token->start + token->length > end) {
			continue;
		}
		count += 1;
	}
	return (count);
}


static bool jsMangleNameUsed(
	struct JsMangleProgram * program, const char * js, size_t start, size_t end,
	const char * name, size_t length
)
{
	return (jsMangleNameCount(program, js, start, end, name, length) != 0);
}


static size_t jsMangleCollectVisibleMaps(
	struct JsMangleProgram * program, const char * js, size_t start, size_t end,
	struct JsMangleState * state, struct JsMangleMap * visibleMaps
)
{
	size_t visibleMapsLength = 0;
	size_t generation = ++program->visibleGeneration;
	for (size_t t = jsMangleFindTokenStart(program, start);
		 t < program->tokensLength;
		 ++t) {
		struct JsMangleToken * token = &program->tokens[t];
		if (token->start >= end) {
			break;
		}
		if (token->kind != JS_MANGLE_TOKEN_IDENTIFIER
			|| token->start + token->length > end
			|| token->nameIndex == NULL) {
			continue;
		}
		if (token->nameIndex->visibleGeneration == generation) {
			continue;
		}
		token->nameIndex->visibleGeneration = generation;
		for (struct JsMangleMap * map = token->nameIndex->firstMap;
			 map != NULL;
			 map = map->nextForName) {
			if (map->visibleGeneration == generation || map->scopeStart > start
				|| map->scopeEnd < end) {
				continue;
			}
			map->visibleGeneration = generation;
			visibleMaps[visibleMapsLength++] = *map;
		}
	}
	return (visibleMapsLength);
}


static bool jsMangleReplacementVisible(
	struct JsMangleMap * visibleMaps, size_t visibleMapsLength, const char * name,
	size_t length
)
{
	for (size_t i = 0; i < visibleMapsLength; ++i) {
		if (jsMangleSameName(visibleMaps[i].replacement,
							 visibleMaps[i].replacementLength,
							 name,
							 length)) {
			return (true);
		}
	}
	return (false);
}


static void jsMangleWriteNameSuffix(
	size_t index, size_t width, const char * alphabet, size_t alphabetLength,
	char * name, size_t * length
)
{
	size_t start = *length;
	for (size_t i = 0; i < width; ++i) {
		name[start + width - i - 1] = alphabet[index % alphabetLength];
		index /= alphabetLength;
	}
	*length += width;
	name[*length] = '\0';
}


static void jsMangleMakeName( size_t index, char * name, size_t * length )
{
	const char alphabet[]
		= "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
	size_t alphabetLength = sizeof alphabet - 1;
	if (index < alphabetLength) {
		name[0] = alphabet[index];
		name[1] = '\0';
		*length = 1;
		return;
	}
	index -= alphabetLength;
	name[0] = '_';
	*length = 1;
	size_t width = 1;
	size_t groupSize = alphabetLength;
	while (index >= groupSize && *length + width < 15) {
		index -= groupSize;
		width += 1;
		groupSize *= alphabetLength;
	}
	jsMangleWriteNameSuffix(
		index, width, alphabet, alphabetLength, name, length);
}


static void jsMangleMakeGlobalName(
	size_t index, char * name, size_t * length
)
{
	const char alphabet[]
		= "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
	size_t alphabetLength = sizeof alphabet - 1;
	name[0] = 'G';
	*length = 1;
	size_t width = 1;
	size_t groupSize = alphabetLength;
	while (index >= groupSize && *length + width < 15) {
		index -= groupSize;
		width += 1;
		groupSize *= alphabetLength;
	}
	jsMangleWriteNameSuffix(
		index, width, alphabet, alphabetLength, name, length);
}


static void jsManglePreviousNext(
	const char * js,
	size_t start,
	size_t end,
	size_t wordStart,
	size_t wordEnd,
	char * previous,
	char * next
)
{
	size_t i = wordStart;
	*previous = '\0';
	*next = '\0';
	while (i > start && IsWhitespace(js[i - 1])) {
		i -= 1;
	}
	if (i > start) {
		*previous = js[i - 1];
	}
	i = wordEnd;
	while (i < end && IsWhitespace(js[i])) {
		i += 1;
	}
	if (i < end) {
		*next = js[i];
	}
}


static bool jsManglePreviousWordEquals(
	const char * js, size_t start, size_t wordStart, const char * word
)
{
	size_t i = wordStart;
	while (i > start && IsWhitespace(js[i - 1])) {
		i -= 1;
	}
	if (i == start || !jsIdentifierPart(js[i - 1])) {
		return (false);
	}
	size_t previousEnd = i;
	while (i > start && jsIdentifierPart(js[i - 1])) {
		i -= 1;
	}
	return (strlen(word) == previousEnd - i
			&& !strncmp(&js[i], word, previousEnd - i));
}


static bool jsMangleFunctionUnsafe( const char * js, size_t start, size_t end )
{
	for (size_t i = start; i < end; ++i) {
		if (js[i] == '"' || js[i] == '\'') {
			i = jsSkipQuoted(js, i, end) - 1;
		} else if (js[i] == '/' && (js[i + 1] == '*' || js[i + 1] == '/')) {
			i = jsSkipComment(js, i, end) - 1;
		} else if (js[i] == '/' && jsRegexStart(js, start, i)) {
			i = jsSkipRegex(js, i, end) - 1;
		} else if (js[i] == '`') {
			return (true);
		} else if (js[i] == '=' && js[i + 1] == '>') {
			return (true);
		} else if (jsIdentifierStart(js[i])) {
			size_t wordStart = i;
			while (jsIdentifierPart(js[i])) {
				i += 1;
			}
			char previous, next;
			jsManglePreviousNext(
				js, start, end, wordStart, i, &previous, &next);
			if (jsWordEquals(js, wordStart, "function")) {
				while (i < end && IsWhitespace(js[i])) {
					i += 1;
				}
				if (js[i] == '*') {
					i += 1;
					while (i < end && IsWhitespace(js[i])) {
						i += 1;
					}
				}
				while (jsIdentifierPart(js[i])) {
					i += 1;
				}
				while (i < end && IsWhitespace(js[i])) {
					i += 1;
				}
				if (js[i] == '(') {
					size_t paramsEnd = jsFindMatching(js, i, end, '(', ')');
					i = paramsEnd + 1;
					while (i < end && IsWhitespace(js[i])) {
						i += 1;
					}
					if (paramsEnd < end && js[i] == '{') {
						i = jsFindMatching(js, i, end, '{', '}');
					}
				}
				continue;
			}
			if (previous != '.' && jsWordEquals(js, wordStart, "with")) {
				return (true);
			}
			if (previous != '.' && jsWordEquals(js, wordStart, "eval")) {
				size_t k = i;
				while (k < end && IsWhitespace(js[k])) {
					k += 1;
				}
				if (js[k] == '(') {
					return (true);
				}
			}
			i -= 1;
		}
	}
	return (false);
}


static bool jsMangleIsModule(
	const char * js, struct JsMangleProgram * program
)
{
	for (size_t i = 0; i < program->tokensLength; ++i) {
		if (jsMangleTokenEquals(js, &program->tokens[i], "import")
			|| jsMangleTokenEquals(js, &program->tokens[i], "export")) {
			return (true);
		}
	}
	return (false);
}


static char jsManglePreviousPunctuation(
	struct JsMangleProgram * program, size_t tokenI, size_t minStart
);

static bool jsMangleTopLevelUnsafe(
	const char * js, struct JsMangleProgram * program
)
{
	size_t curlyNesting = 0;
	size_t roundNesting = 0;
	size_t squareNesting = 0;

	for (size_t i = 0; i < program->tokensLength; ++i) {
		struct JsMangleToken * token = &program->tokens[i];
		if (token->kind == JS_MANGLE_TOKEN_PUNCTUATION
			&& token->punctuation == '{') {
			curlyNesting += 1;
		} else if (token->kind == JS_MANGLE_TOKEN_PUNCTUATION
				   && token->punctuation == '}') {
			curlyNesting -= curlyNesting > 0;
		} else if (token->kind == JS_MANGLE_TOKEN_PUNCTUATION
				   && token->punctuation == '(') {
			roundNesting += 1;
		} else if (token->kind == JS_MANGLE_TOKEN_PUNCTUATION
				   && token->punctuation == ')') {
			roundNesting -= roundNesting > 0;
		} else if (token->kind == JS_MANGLE_TOKEN_PUNCTUATION
				   && token->punctuation == '[') {
			squareNesting += 1;
		} else if (token->kind == JS_MANGLE_TOKEN_PUNCTUATION
				   && token->punctuation == ']') {
			squareNesting -= squareNesting > 0;
		} else if (curlyNesting == 0 && roundNesting == 0
				   && squareNesting == 0) {
			char previous = jsManglePreviousPunctuation(program, i, 0);
			if (previous != '.' && jsMangleTokenEquals(js, token, "with")) {
				return (true);
			}
			if (previous != '.' && jsMangleTokenEquals(js, token, "eval")
				&& i + 1 < program->tokensLength
				&& program->tokens[i + 1].kind == JS_MANGLE_TOKEN_PUNCTUATION
				&& program->tokens[i + 1].punctuation == '(') {
				return (true);
			}
		}
	}
	return (false);
}


static bool jsMangleCollectParams(
	const char * js, size_t start, size_t end, struct JsMangleBinding ** bindings,
	size_t * bindingsLength, size_t * bindingsCapacity
)
{
	bool expectName = true;
	size_t nesting = 0;

	for (size_t i = start; i < end; ++i) {
		if (js[i] == '"' || js[i] == '\'' || js[i] == '`') {
			i = jsSkipQuoted(js, i, end) - 1;
		} else if (nesting == 0 && expectName
				   && (js[i] == '[' || js[i] == '{')) {
			size_t patternEnd
				= jsFindMatching(js, i, end, js[i], js[i] == '[' ? ']' : '}');
			if (patternEnd >= end) {
				return (true);
			}
			if (!jsMangleCollectPatternBindings(js,
												i,
												patternEnd + 1,
												bindings,
												bindingsLength,
												bindingsCapacity)) {
				return (false);
			}
			i = patternEnd;
			expectName = false;
		} else if (js[i] == '(' || js[i] == '[' || js[i] == '{') {
			nesting += 1;
		} else if (js[i] == ')' || js[i] == ']' || js[i] == '}') {
			nesting -= nesting > 0;
		} else if (nesting == 0 && js[i] == ',') {
			expectName = true;
		} else if (nesting == 0 && expectName && js[i] == '.'
				   && js[i + 1] == '.' && js[i + 2] == '.') {
			i += 2;
		} else if (nesting == 0 && expectName && jsIdentifierStart(js[i])) {
			size_t wordStart = i;
			while (jsIdentifierPart(js[i])) {
				i += 1;
			}
			if (!jsMangleAddBinding(bindings,
									bindingsLength,
									bindingsCapacity,
									&js[wordStart],
									i - wordStart)) {
				return (false);
			}
			expectName = false;
			i -= 1;
		}
	}
	return (true);
}


static bool jsMangleCollectExportClause(
	const char * js, size_t start, size_t end, struct JsMangleBinding ** bindings,
	size_t * bindingsLength, size_t * bindingsCapacity
)
{
	size_t close = jsFindMatching(js, start, end, '{', '}');
	if (close >= end) {
		return (true);
	}

	size_t i = close + 1;
	while (i < end && IsWhitespace(js[i])) {
		i += 1;
	}
	if (jsWordEquals(js, i, "from")) {
		return (true);
	}

	for (i = start + 1; i < close; ++i) {
		while (i < close && !jsIdentifierStart(js[i])) {
			if (js[i] == '"' || js[i] == '\'') {
				i = jsSkipQuoted(js, i, close) - 1;
			}
			i += 1;
		}
		if (i >= close) {
			break;
		}
		size_t wordStart = i;
		while (jsIdentifierPart(js[i])) {
			i += 1;
		}
		if (jsWordEquals(js, wordStart, "as")) {
			continue;
		}
		if (!jsMangleAddBinding(bindings,
								bindingsLength,
								bindingsCapacity,
								&js[wordStart],
								i - wordStart)) {
			return (false);
		}
		while (i < close && js[i] != ',') {
			i += 1;
		}
	}
	return (true);
}


static bool jsMangleCollectExportedDeclarations(
	const char * js, size_t end, struct JsMangleBinding ** bindings,
	size_t * bindingsLength, size_t * bindingsCapacity
)
{
	size_t curlyNesting = 0;
	size_t roundNesting = 0;
	size_t squareNesting = 0;

	for (size_t i = 0; i < end; ++i) {
		if (js[i] == '"' || js[i] == '\'' || js[i] == '`') {
			i = jsSkipQuoted(js, i, end) - 1;
		} else if (js[i] == '/' && (js[i + 1] == '*' || js[i + 1] == '/')) {
			i = jsSkipComment(js, i, end) - 1;
		} else if (js[i] == '/' && jsRegexStart(js, 0, i)) {
			i = jsSkipRegex(js, i, end) - 1;
		} else if (js[i] == '{') {
			curlyNesting += 1;
		} else if (js[i] == '}') {
			curlyNesting -= curlyNesting > 0;
		} else if (js[i] == '(') {
			roundNesting += 1;
		} else if (js[i] == ')') {
			roundNesting -= roundNesting > 0;
		} else if (js[i] == '[') {
			squareNesting += 1;
		} else if (js[i] == ']') {
			squareNesting -= squareNesting > 0;
		} else if (curlyNesting == 0 && roundNesting == 0 && squareNesting == 0
				   && jsWordEquals(js, i, "export")) {
			i += sizeof "export" - 1;
			while (i < end && IsWhitespace(js[i])) {
				i += 1;
			}
			if (jsWordEquals(js, i, "default")) {
				i += sizeof "default" - 1;
				while (i < end && IsWhitespace(js[i])) {
					i += 1;
				}
				if (jsIdentifierStart(js[i])
					&& !jsWordEquals(js, i, "function")
					&& !jsWordEquals(js, i, "class")) {
					size_t wordStart = i;
					while (jsIdentifierPart(js[i])) {
						i += 1;
					}
					if (!jsMangleAddBinding(bindings,
											bindingsLength,
											bindingsCapacity,
											&js[wordStart],
											i - wordStart)) {
						return (false);
					}
				}
			} else if (js[i] == '{') {
				if (!jsMangleCollectExportClause(js,
												 i,
												 end,
												 bindings,
												 bindingsLength,
												 bindingsCapacity)) {
					return (false);
				}
				i = jsFindMatching(js, i, end, '{', '}');
			} else if (jsWordEquals(js, i, "function")) {
				i += sizeof "function" - 1;
				while (i < end && IsWhitespace(js[i])) {
					i += 1;
				}
				if (js[i] == '*') {
					i += 1;
					while (i < end && IsWhitespace(js[i])) {
						i += 1;
					}
				}
				if (jsIdentifierStart(js[i])) {
					size_t wordStart = i;
					while (jsIdentifierPart(js[i])) {
						i += 1;
					}
					if (!jsMangleAddBinding(bindings,
											bindingsLength,
											bindingsCapacity,
											&js[wordStart],
											i - wordStart)) {
						return (false);
					}
				}
			} else if (jsWordEquals(js, i, "var") || jsWordEquals(js, i, "let")
					   || jsWordEquals(js, i, "const")) {
				i += jsDeclarationKeywordLength(js, i);
				while (i < end && IsWhitespace(js[i])) {
					i += 1;
				}
				int nesting = 0;
				bool expectName = true;
				while (i < end && js[i] != ';') {
					if (js[i] == '(' || js[i] == '[' || js[i] == '{') {
						if (nesting == 0 && expectName
							&& (js[i] == '[' || js[i] == '{')) {
							size_t patternEnd = jsFindMatching(
								js, i, end, js[i], js[i] == '[' ? ']' : '}');
							if (patternEnd >= end) {
								break;
							}
							if (!jsMangleCollectPatternBindings(
									js,
									i,
									patternEnd + 1,
									bindings,
									bindingsLength,
									bindingsCapacity)) {
								return (false);
							}
							i = patternEnd;
							expectName = false;
							continue;
						}
						nesting += 1;
					} else if (js[i] == ')' || js[i] == ']' || js[i] == '}') {
						nesting -= 1;
					} else if (nesting == 0 && js[i] == ',') {
						expectName = true;
					} else if (expectName && jsIdentifierStart(js[i])) {
						size_t wordStart = i;
						while (jsIdentifierPart(js[i])) {
							i += 1;
						}
						if (!jsMangleAddBinding(bindings,
												bindingsLength,
												bindingsCapacity,
												&js[wordStart],
												i - wordStart)) {
							return (false);
						}
						expectName = false;
						i -= 1;
					}
					i += 1;
				}
			}
		}
	}
	return (true);
}


static bool jsMangleCollectGlobalDeclarations(
	const char * js,
	size_t end,
	struct JsMangleBinding ** bindings,
	size_t * bindingsLength,
	size_t * bindingsCapacity,
	struct JsMangleBinding * excluded,
	size_t excludedLength
)
{
	size_t curlyNesting = 0;
	size_t roundNesting = 0;
	size_t squareNesting = 0;

	for (size_t i = 0; i < end; ++i) {
		if (js[i] == '"' || js[i] == '\'' || js[i] == '`') {
			i = jsSkipQuoted(js, i, end) - 1;
		} else if (js[i] == '/' && (js[i + 1] == '*' || js[i + 1] == '/')) {
			i = jsSkipComment(js, i, end) - 1;
		} else if (js[i] == '/' && jsRegexStart(js, 0, i)) {
			i = jsSkipRegex(js, i, end) - 1;
		} else if (js[i] == '{') {
			curlyNesting += 1;
		} else if (js[i] == '}') {
			curlyNesting -= curlyNesting > 0;
		} else if (js[i] == '(') {
			roundNesting += 1;
		} else if (js[i] == ')') {
			roundNesting -= roundNesting > 0;
		} else if (js[i] == '[') {
			squareNesting += 1;
		} else if (js[i] == ']') {
			squareNesting -= squareNesting > 0;
		} else if (curlyNesting == 0 && roundNesting == 0 && squareNesting == 0
				   && jsIdentifierStart(js[i])) {
			if (jsWordEquals(js, i, "import")) {
				i += sizeof "import" - 1;
				while (i < end && IsWhitespace(js[i])) {
					i += 1;
				}
				if (js[i] == '"' || js[i] == '\'') {
					i = jsSkipQuoted(js, i, end) - 1;
					continue;
				}
				if (jsIdentifierStart(js[i])) {
					size_t wordStart = i;
					while (jsIdentifierPart(js[i])) {
						i += 1;
					}
					if (!jsMangleAddGlobalImportBinding(bindings,
														bindingsLength,
														bindingsCapacity,
														excluded,
														excludedLength,
														&js[wordStart],
														i - wordStart,
														false)) {
						return (false);
					}
					while (i < end && js[i] != ';' && js[i] != ',') {
						i += 1;
					}
					if (js[i] != ',') {
						continue;
					}
					i += 1;
				}
				while (i < end && IsWhitespace(js[i])) {
					i += 1;
				}
				if (js[i] == '*') {
					i += 1;
					while (i < end && IsWhitespace(js[i])) {
						i += 1;
					}
					if (jsWordEquals(js, i, "as")) {
						i += sizeof "as" - 1;
					}
					while (i < end && IsWhitespace(js[i])) {
						i += 1;
					}
					if (jsIdentifierStart(js[i])) {
						size_t wordStart = i;
						while (jsIdentifierPart(js[i])) {
							i += 1;
						}
						if (!jsMangleAddGlobalImportBinding(bindings,
															bindingsLength,
															bindingsCapacity,
															excluded,
															excludedLength,
															&js[wordStart],
															i - wordStart,
															false)) {
							return (false);
						}
					}
					continue;
				}
				if (js[i] == '{') {
					i += 1;
					while (i < end && js[i] != '}') {
						while (i < end && !jsIdentifierStart(js[i])
							   && js[i] != '}') {
							i += 1;
						}
						if (js[i] == '}') {
							break;
						}
						size_t sourceStart = i;
						while (jsIdentifierPart(js[i])) {
							i += 1;
						}
						size_t sourceLength = i - sourceStart;
						while (i < end && IsWhitespace(js[i])) {
							i += 1;
						}
						bool importBare = true;
						if (jsWordEquals(js, i, "as")) {
							i += sizeof "as" - 1;
							while (i < end && IsWhitespace(js[i])) {
								i += 1;
							}
							importBare = false;
							sourceStart = i;
							while (jsIdentifierPart(js[i])) {
								i += 1;
							}
							sourceLength = i - sourceStart;
						}
						if (!jsMangleAddGlobalImportBinding(bindings,
															bindingsLength,
															bindingsCapacity,
															excluded,
															excludedLength,
															&js[sourceStart],
															sourceLength,
															importBare)) {
							return (false);
						}
					}
					continue;
				}
			}
			if (jsWordEquals(js, i, "export")) {
				i += sizeof "export" - 2;
				continue;
			}
			if (jsWordEquals(js, i, "function")) {
				char previous, next;
				while (jsIdentifierPart(js[i])) {
					i += 1;
				}
				jsManglePreviousNext(js,
									 0,
									 end,
									 i - sizeof "function" + 1,
									 i,
									 &previous,
									 &next);
				if (previous != '\0' && previous != ';' && previous != '}') {
					i -= 1;
					continue;
				}
				while (i < end && IsWhitespace(js[i])) {
					i += 1;
				}
				if (js[i] == '*') {
					i += 1;
					while (i < end && IsWhitespace(js[i])) {
						i += 1;
					}
				}
				if (jsIdentifierStart(js[i])) {
					size_t wordStart = i;
					while (jsIdentifierPart(js[i])) {
						i += 1;
					}
					if (!jsMangleAddGlobalBinding(bindings,
												  bindingsLength,
												  bindingsCapacity,
												  excluded,
												  excludedLength,
												  &js[wordStart],
												  i - wordStart)) {
						return (false);
					}
					i -= 1;
				}
				continue;
			}
			if (!(jsWordEquals(js, i, "var") || jsWordEquals(js, i, "let")
				  || jsWordEquals(js, i, "const"))) {
				while (jsIdentifierPart(js[i])) {
					i += 1;
				}
				i -= 1;
				continue;
			}
			i += jsDeclarationKeywordLength(js, i);
			while (i < end && IsWhitespace(js[i])) {
				i += 1;
			}
			int nesting = 0;
			bool expectName = true;
			while (i < end && js[i] != ';') {
				if (js[i] == '(' || js[i] == '[' || js[i] == '{') {
					if (nesting == 0 && expectName
						&& (js[i] == '[' || js[i] == '{')) {
						size_t patternEnd = jsFindMatching(
							js, i, end, js[i], js[i] == '[' ? ']' : '}');
						if (patternEnd >= end) {
							break;
						}
						if (!jsMangleCollectGlobalPatternBindings(
								js,
								i,
								patternEnd + 1,
								bindings,
								bindingsLength,
								bindingsCapacity,
								excluded,
								excludedLength)) {
							return (false);
						}
						i = patternEnd;
						expectName = false;
						continue;
					}
					nesting += 1;
					if (expectName && (js[i] == '[' || js[i] == '{')) {
						break;
					}
				} else if (js[i] == ')' || js[i] == ']' || js[i] == '}') {
					nesting -= 1;
				} else if (nesting == 0 && js[i] == ',') {
					expectName = true;
				} else if (expectName && jsIdentifierStart(js[i])) {
					size_t wordStart = i;
					while (jsIdentifierPart(js[i])) {
						i += 1;
					}
					if (!jsMangleAddGlobalBinding(bindings,
												  bindingsLength,
												  bindingsCapacity,
												  excluded,
												  excludedLength,
												  &js[wordStart],
												  i - wordStart)) {
						return (false);
					}
					expectName = false;
					i -= 1;
				}
				i += 1;
			}
		}
	}
	return (true);
}


static bool jsMangleCollectDeclarations(
	const char * js, size_t start, size_t end, struct JsMangleBinding ** bindings,
	size_t * bindingsLength, size_t * bindingsCapacity
)
{
	for (size_t i = start; i < end; ++i) {
		if (js[i] == '"' || js[i] == '\'' || js[i] == '`') {
			i = jsSkipQuoted(js, i, end) - 1;
		} else if (js[i] == '/' && (js[i + 1] == '*' || js[i + 1] == '/')) {
			i = jsSkipComment(js, i, end) - 1;
		} else if (js[i] == '/' && jsRegexStart(js, start, i)) {
			i = jsSkipRegex(js, i, end) - 1;
		} else if (jsIdentifierStart(js[i])
				   && jsWordEquals(js, i, "function")) {
			i += sizeof "function" - 1;
			while (i < end && IsWhitespace(js[i])) {
				i += 1;
			}
			if (js[i] == '*') {
				i += 1;
				while (i < end && IsWhitespace(js[i])) {
					i += 1;
				}
			}
			if (jsIdentifierStart(js[i])) {
				size_t wordStart = i;
				while (jsIdentifierPart(js[i])) {
					i += 1;
				}
				size_t nameEnd = i;
				while (i < end && IsWhitespace(js[i])) {
					i += 1;
				}
				if (js[i] == '(') {
					size_t paramsEnd = jsFindMatching(js, i, end, '(', ')');
					i = paramsEnd + 1;
					while (i < end && IsWhitespace(js[i])) {
						i += 1;
					}
					if (paramsEnd < end && js[i] == '{') {
						size_t bodyStart = i + 1;
						size_t bodyEnd = jsFindMatching(js, i, end, '{', '}');
						if (bodyEnd < end
							&& jsMangleFunctionUnsafe(
								js, bodyStart, bodyEnd)) {
							i = nameEnd - 1;
							continue;
						}
					}
				}
				if (!jsMangleAddBinding(bindings,
										bindingsLength,
										bindingsCapacity,
										&js[wordStart],
										nameEnd - wordStart)) {
					return (false);
				}
				i = nameEnd - 1;
			}
		} else if (jsIdentifierStart(js[i]) && jsWordEquals(js, i, "catch")) {
			i += sizeof "catch" - 1;
			while (i < end && IsWhitespace(js[i])) {
				i += 1;
			}
			if (js[i] == '(') {
				i += 1;
				while (i < end && IsWhitespace(js[i])) {
					i += 1;
				}
				if (jsIdentifierStart(js[i])) {
					size_t wordStart = i;
					while (jsIdentifierPart(js[i])) {
						i += 1;
					}
					if (!jsMangleAddBinding(bindings,
											bindingsLength,
											bindingsCapacity,
											&js[wordStart],
											i - wordStart)) {
						return (false);
					}
				}
			}
		} else if (jsIdentifierStart(js[i])
				   && (jsWordEquals(js, i, "var") || jsWordEquals(js, i, "let")
					   || jsWordEquals(js, i, "const"))) {
			i += jsDeclarationKeywordLength(js, i);
			while (i < end && IsWhitespace(js[i])) {
				i += 1;
			}
			int nesting = 0;
			bool expectName = true;
			while (i < end && js[i] != ';') {
				if (js[i] == '(' || js[i] == '[' || js[i] == '{') {
					if (nesting == 0 && expectName
						&& (js[i] == '[' || js[i] == '{')) {
						size_t patternEnd = jsFindMatching(
							js, i, end, js[i], js[i] == '[' ? ']' : '}');
						if (patternEnd >= end) {
							break;
						}
						if (!jsMangleCollectPatternBindings(
								js,
								i,
								patternEnd + 1,
								bindings,
								bindingsLength,
								bindingsCapacity)) {
							return (false);
						}
						i = patternEnd;
						expectName = false;
						continue;
					}
					nesting += 1;
					if (expectName && (js[i] == '[' || js[i] == '{')) {
						break;
					}
				} else if (js[i] == ')' || js[i] == ']' || js[i] == '}') {
					nesting -= 1;
				} else if (nesting == 0 && js[i] == ',') {
					expectName = true;
				} else if (expectName && jsIdentifierStart(js[i])) {
					size_t wordStart = i;
					while (jsIdentifierPart(js[i])) {
						i += 1;
					}
					if (!jsMangleAddBinding(bindings,
											bindingsLength,
											bindingsCapacity,
											&js[wordStart],
											i - wordStart)) {
						return (false);
					}
					expectName = false;
					i -= 1;
				}
				i += 1;
			}
		}
	}
	return (true);
}


static bool jsMangleAssignGlobalNames(
	struct JsMangleProgram * program, const char * js, size_t end,
	struct JsMangleState * state, struct JsMangleBinding * bindings,
	size_t bindingsLength
)
{
	size_t nameIndex = 0;
	for (size_t i = 0; i < bindingsLength; ++i) {
		if (bindings[i].length <= sizeof "g0" - 1
			|| jsMangleNameCount(
				   program, js, 0, end, bindings[i].name, bindings[i].length)
				   <= 1) {
			bindings[i].replacementLength = 0;
			continue;
		}
		do {
			jsMangleMakeGlobalName(nameIndex++,
								   bindings[i].replacement,
								   &bindings[i].replacementLength);
		} while (jsMangleNameUsed(program,
								  js,
								  0,
								  end,
								  bindings[i].replacement,
								  bindings[i].replacementLength));

		if (bindings[i].replacementLength >= bindings[i].length) {
			bindings[i].replacementLength = 0;
			continue;
		}
		if (!jsMangleAddMap(program, state, bindings[i], 0, end)) {
			return (false);
		}
	}
	return (true);
}


static struct JsMangleBinding *
jsMangleFindBinding( struct JsMangleBinding * bindings,
					size_t bindingsLength,
					const char * name,
					size_t length )
{
	for (size_t i = 0; i < bindingsLength; ++i) {
		if (jsMangleSameName(
				bindings[i].name, bindings[i].length, name, length)) {
			return (&bindings[i]);
		}
	}
	return (NULL);
}


static bool jsMangleIndexBindings(
	struct JsMangleProgram * program, struct JsMangleBinding * bindings,
	size_t bindingsLength
)
{
	for (size_t i = 0; i < bindingsLength; ++i) {
		if (bindings[i].nameIndex == NULL) {
			bindings[i].nameIndex = jsMangleFindNameIndex(
				program, bindings[i].name, bindings[i].length);
		}
		if (bindings[i].nameIndex == NULL) {
			return (false);
		}
		bindings[i].nameIndex->binding = &bindings[i];
	}
	return (true);
}


static void jsMangleClearBindingIndex(
	struct JsMangleBinding * bindings, size_t bindingsLength
)
{
	for (size_t i = 0; i < bindingsLength; ++i) {
		if (bindings[i].nameIndex != NULL
			&& bindings[i].nameIndex->binding == &bindings[i]) {
			bindings[i].nameIndex->binding = NULL;
		}
	}
}


static bool jsMangleNameUsedRaw(
	const char * js, size_t start, size_t end, const char * name, size_t length
)
{
	for (size_t i = start; i < end; ++i) {
		if (js[i] == '"' || js[i] == '\'' || js[i] == '`') {
			i = jsSkipQuoted(js, i, end) - 1;
		} else if (js[i] == '/' && (js[i + 1] == '*' || js[i + 1] == '/')) {
			i = jsSkipComment(js, i, end) - 1;
		} else if (js[i] == '/' && jsRegexStart(js, start, i)) {
			i = jsSkipRegex(js, i, end) - 1;
		} else if (jsIdentifierStart(js[i])) {
			size_t wordStart = i;
			while (jsIdentifierPart(js[i])) {
				i += 1;
			}
			if (jsMangleSameName(
					&js[wordStart], i - wordStart, name, length)) {
				return (true);
			}
			i -= 1;
		}
	}
	return (false);
}


static bool jsMangleReplacementUsedInScope(
	struct JsMangleBinding * bindings, size_t bindingsLength, const char * name,
	size_t length
)
{
	for (size_t i = 0; i < bindingsLength; ++i) {
		if (bindings[i].replacementLength != 0
			&& jsMangleSameName(bindings[i].replacement,
								bindings[i].replacementLength,
								name,
								length)) {
			return (true);
		}
	}
	return (false);
}


static bool jsMangleAssignNames(
	struct JsMangleProgram * program,
	const char * js,
	size_t start,
	size_t end,
	size_t signatureEnd,
	struct JsMangleState * state,
	struct JsMangleBinding * bindings,
	size_t bindingsLength
)
{
	struct JsMangleMap * visibleMaps = NULL;
	size_t visibleMapsLength = 0;
	size_t nameIndex = 0;

	if (state->mapsLength != 0) {
		visibleMaps = malloc(state->mapsLength * sizeof *visibleMaps);
		if (visibleMaps == NULL) {
			return (false);
		}
		visibleMapsLength = jsMangleCollectVisibleMaps(
			program, js, start, end, state, visibleMaps);
	}
	for (size_t i = 0; i < bindingsLength; ++i) {
		do {
			jsMangleMakeName(nameIndex++,
							 bindings[i].replacement,
							 &bindings[i].replacementLength);
		} while (jsMangleReplacementUsedInScope(bindings,
												i,
												bindings[i].replacement,
												bindings[i].replacementLength)
				 || jsMangleNameUsedRaw(js,
										start,
										signatureEnd,
										bindings[i].replacement,
										bindings[i].replacementLength)
				 || jsMangleNameUsed(program,
									 js,
									 start,
									 end,
									 bindings[i].replacement,
									 bindings[i].replacementLength)
				 || jsMangleReplacementVisible(visibleMaps,
											   visibleMapsLength,
											   bindings[i].replacement,
											   bindings[i].replacementLength));

		if (bindings[i].replacementLength >= bindings[i].length) {
			bindings[i].replacementLength = 0;
			continue;
		}
		if (!jsMangleAddMap(program, state, bindings[i], start, end)) {
			free(visibleMaps);
			return (false);
		}
	}
	free(visibleMaps);
	return (true);
}


static bool jsMangleInImportSpecifier( const char * js, size_t wordStart )
{
	size_t statementStart = wordStart;
	while (statementStart > 0 && js[statementStart - 1] != ';') {
		statementStart -= 1;
	}
	size_t i = statementStart;
	while (IsWhitespace(js[i])) {
		i += 1;
	}
	if (!jsWordEquals(js, i, "import")) {
		return (false);
	}
	bool inBraces = false;
	for (; i < wordStart; ++i) {
		if (js[i] == '"' || js[i] == '\'') {
			i = jsSkipQuoted(js, i, wordStart) - 1;
		} else if (js[i] == '{') {
			inBraces = true;
		} else if (js[i] == '}') {
			inBraces = false;
		}
	}
	return (inBraces);
}


static bool jsMangleInDeclarationPattern(
	const char * js, size_t start, size_t end, size_t wordStart
)
{
	size_t search = wordStart;
	while (search > start) {
		size_t open = search;
		while (open > start && js[open] != '{' && js[open] != ';') {
			open -= 1;
		}
		if (js[open] != '{') {
			return (false);
		}

		size_t close = jsFindMatching(js, open, end, '{', '}');
		if (close >= end || wordStart > close) {
			return (false);
		}
		size_t i = close + 1;
		while (i < end && IsWhitespace(js[i])) {
			i += 1;
		}
		if (js[i] == '=') {
			i = open;
			while (i > start && IsWhitespace(js[i - 1])) {
				i -= 1;
			}
			while (i > start && js[i - 1] != ';' && js[i - 1] != '{'
				   && js[i - 1] != '}') {
				i -= 1;
			}
			while (i < open && IsWhitespace(js[i])) {
				i += 1;
			}
			return (jsWordEquals(js, i, "let") || jsWordEquals(js, i, "const")
					|| jsWordEquals(js, i, "var"));
		}
		if (open == start) {
			break;
		}
		search = open - 1;
	}
	return (false);
}


static bool jsMangleInParameterPattern(
	const char * js, size_t start, size_t end, size_t wordStart
)
{
	size_t patternStart = wordStart;
	size_t curly = 0;
	size_t round = 0;
	size_t square = 0;

	while (patternStart > start) {
		patternStart -= 1;
		if (js[patternStart] == '}') {
			curly += 1;
		} else if (js[patternStart] == ')') {
			round += 1;
		} else if (js[patternStart] == ']') {
			square += 1;
		} else if (js[patternStart] == '{') {
			if (curly == 0 && round == 0 && square == 0) {
				break;
			}
			curly -= curly > 0;
		} else if (js[patternStart] == '(' || js[patternStart] == ';') {
			return (false);
		} else if (js[patternStart] == '[') {
			square -= square > 0;
		}
	}
	if (js[patternStart] != '{') {
		return (false);
	}

	size_t patternEnd = jsFindMatching(js, patternStart, end, '{', '}');
	if (patternEnd >= end || wordStart > patternEnd) {
		return (false);
	}

	size_t before = patternStart;
	while (before > start && IsWhitespace(js[before - 1])) {
		before -= 1;
	}
	if (before <= start || (js[before - 1] != '(' && js[before - 1] != ',')) {
		return (false);
	}

	size_t after = patternEnd + 1;
	while (after < end && IsWhitespace(js[after])) {
		after += 1;
	}
	if (after >= end) {
		return (false);
	}
	if (js[after] != ')' && js[after] != ',' && js[after] != '=') {
		return (false);
	}

	size_t paramsStart = patternStart;
	curly = 0;
	round = 0;
	square = 0;
	while (paramsStart > start) {
		paramsStart -= 1;
		if (js[paramsStart] == '}') {
			curly += 1;
		} else if (js[paramsStart] == ')') {
			round += 1;
		} else if (js[paramsStart] == ']') {
			square += 1;
		} else if (js[paramsStart] == '(') {
			if (curly == 0 && round == 0 && square == 0) {
				break;
			}
			round -= round > 0;
		} else if (js[paramsStart] == '{') {
			curly -= curly > 0;
		} else if (js[paramsStart] == '[') {
			square -= square > 0;
		} else if (js[paramsStart] == ';') {
			return (false);
		}
	}
	if (js[paramsStart] != '(') {
		return (false);
	}

	size_t paramsEnd = jsFindMatching(js, paramsStart, end, '(', ')');
	if (paramsEnd >= end || patternEnd > paramsEnd) {
		return (false);
	}
	after = paramsEnd + 1;
	while (after < end && IsWhitespace(js[after])) {
		after += 1;
	}
	return (after < end && js[after] == '{')
		   || (after + 1 < end && js[after] == '=' && js[after + 1] == '>');
}


static char jsManglePreviousPunctuation(
	struct JsMangleProgram * program, size_t tokenI, size_t minStart
)
{
	while (tokenI > 0) {
		tokenI -= 1;
		struct JsMangleToken * token = &program->tokens[tokenI];
		if (token->start < minStart) {
			return '\0';
		}
		if (token->kind == JS_MANGLE_TOKEN_PUNCTUATION) {
			return (token->punctuation);
		}
		if (token->kind == JS_MANGLE_TOKEN_IDENTIFIER
			|| token->kind == JS_MANGLE_TOKEN_STRING
			|| token->kind == JS_MANGLE_TOKEN_REGEX) {
			return '\0';
		}
	}
	return '\0';
}


static char jsMangleNextPunctuation(
	struct JsMangleProgram * program, size_t tokenI, size_t maxEnd
)
{
	for (tokenI += 1; tokenI < program->tokensLength; ++tokenI) {
		struct JsMangleToken * token = &program->tokens[tokenI];
		if (token->start >= maxEnd) {
			return '\0';
		}
		if (token->kind == JS_MANGLE_TOKEN_PUNCTUATION) {
			return (token->punctuation);
		}
		if (token->kind == JS_MANGLE_TOKEN_IDENTIFIER
			|| token->kind == JS_MANGLE_TOKEN_STRING
			|| token->kind == JS_MANGLE_TOKEN_REGEX) {
			return '\0';
		}
	}
	return '\0';
}


static size_t jsMangleEnclosingOpenPunctuationIndex(
	struct JsMangleProgram * program, size_t tokenI, size_t minStart
)
{
	size_t openI = program->tokens[tokenI].enclosingOpen;
	if (openI == SIZE_MAX || program->tokens[openI].start < minStart) {
		return (SIZE_MAX);
	}
	return (openI);
}


static char jsMangleEnclosingClosePunctuation(
	struct JsMangleProgram * program, size_t tokenI, size_t maxEnd
)
{
	size_t openI = program->tokens[tokenI].enclosingOpen;
	if (openI == SIZE_MAX) {
		return '\0';
	}
	size_t closeI = program->tokens[openI].matchingToken;
	if (closeI == SIZE_MAX || program->tokens[closeI].start >= maxEnd) {
		return '\0';
	}
	return (program->tokens[closeI].punctuation);
}


static bool jsMangleOpenBraceStartsObjectLiteral(
	const char * js, struct JsMangleProgram * program, size_t openTokenI,
	size_t minStart
)
{
	struct JsMangleToken * open = &program->tokens[openTokenI];
	char previous = jsManglePreviousPunctuation(program, openTokenI, minStart);
	if (previous == '(' || previous == ',' || previous == '='
		|| previous == '[' || previous == ':' || previous == '?') {
		return (true);
	}
	return (jsManglePreviousWordEquals(js, minStart, open->start, "return")
			|| jsManglePreviousWordEquals(js, minStart, open->start, "yield"));
}


static bool jsMangleInParameterBraces(
	const char * js, size_t paramsStart, size_t paramsEnd, size_t wordStart
)
{
	size_t open = wordStart;
	size_t curly = 0;

	while (open > paramsStart) {
		open -= 1;
		if (js[open] == '}') {
			curly += 1;
		} else if (js[open] == '{') {
			if (curly == 0) {
				break;
			}
			curly -= 1;
		}
	}
	if (js[open] != '{') {
		return (false);
	}
	size_t close = jsFindMatching(js, open, paramsEnd, '{', '}');
	return (close < paramsEnd && wordStart < close);
}


static bool jsMangleTokenStartsAccessor(
	const char * js, struct JsMangleProgram * program, size_t tokenI
)
{
	if (!jsMangleTokenEquals(js, &program->tokens[tokenI], "get")
		&& !jsMangleTokenEquals(js, &program->tokens[tokenI], "set")) {
		return (false);
	}
	if (tokenI + 2 >= program->tokensLength) {
		return (false);
	}
	if (program->tokens[tokenI + 1].kind != JS_MANGLE_TOKEN_IDENTIFIER
		&& program->tokens[tokenI + 1].kind != JS_MANGLE_TOKEN_STRING) {
		return (false);
	}
	return (program->tokens[tokenI + 2].kind == JS_MANGLE_TOKEN_PUNCTUATION
			&& program->tokens[tokenI + 2].punctuation == '(');
}


static bool jsMangleIdentifierTokenEdit(
	const char * js,
	struct JsMangleProgram * program,
	size_t tokenI,
	size_t start,
	size_t end,
	struct JsMangleState * state,
	struct JsMangleBinding * bindings,
	size_t bindingsLength,
	bool global
)
{
	struct JsMangleToken * token = &program->tokens[tokenI];
	if (token->start < start || token->start >= end
		|| token->kind != JS_MANGLE_TOKEN_IDENTIFIER) {
		return (true);
	}
	if (!global && jsKeyword(&js[token->start], token->length)) {
		return (true);
	}
	if (jsMangleTokenStartsAccessor(js, program, tokenI)) {
		return (true);
	}

	char previous = jsManglePreviousPunctuation(program, tokenI, start);
	char next = jsMangleNextPunctuation(program, tokenI, end);
	bool restParam = token->start >= start + 3 && js[token->start - 1] == '.'
					 && js[token->start - 2] == '.'
					 && js[token->start - 3] == '.';
	bool labelReference
		= jsManglePreviousWordEquals(js, start, token->start, "break")
		  || jsManglePreviousWordEquals(js, start, token->start, "continue");
	if ((previous == '.' && !restParam) || labelReference) {
		return (true);
	}
	if (next == ':' && previous != '?') {
		return (true);
	}

	struct JsMangleBinding * binding
		= token->nameIndex == NULL ? NULL : token->nameIndex->binding;
	if (binding == NULL || binding->replacementLength == 0) {
		return (true);
	}

	bool patternAlias
		= (previous == '{' || previous == ',') && !restParam
		  && (jsMangleInDeclarationPattern(js, start, end, token->start)
			  || jsMangleInParameterPattern(js, start, end, token->start));
	if (patternAlias) {
		return (jsMangleAddPatternAliasEdit(state,
											token->start,
											&js[token->start],
											token->length,
											binding->replacement,
											binding->replacementLength));
	}
	if (global && binding->importBare
		&& jsMangleInImportSpecifier(js, token->start)) {
		return (jsMangleAddImportAliasEdit(state,
										   token->start,
										   &js[token->start],
										   token->length,
										   binding->replacement,
										   binding->replacementLength));
	}
	if ((previous == '{' || previous == ',') && (next == '}' || next == ',')) {
		size_t enclosingOpenI
			= jsMangleEnclosingOpenPunctuationIndex(program, tokenI, 0);
		char enclosingOpen = enclosingOpenI == SIZE_MAX
								 ? '\0'
								 : program->tokens[enclosingOpenI].punctuation;
		char enclosingClose = jsMangleEnclosingClosePunctuation(
			program,
			tokenI,
			program->tokens[program->tokensLength - 1].start + 1);
		if (enclosingOpen == '{' && enclosingClose == '}'
			&& jsMangleOpenBraceStartsObjectLiteral(
				js, program, enclosingOpenI, start)) {
			return (jsMangleAddShorthandEdit(state,
											 token->start,
											 token->length,
											 binding->replacement,
											 binding->replacementLength));
		}
	}
	return (jsMangleAddEdit(state,
							token->start,
							token->length,
							binding->replacement,
							binding->replacementLength));
}


static bool jsMangleAddGlobalEdits(
	const char * js, size_t end, struct JsMangleProgram * program,
	struct JsMangleState * state, struct JsMangleBinding * bindings,
	size_t bindingsLength
)
{
	bool success = false;
	if (!jsMangleIndexBindings(program, bindings, bindingsLength)) {
		return (false);
	}
	for (size_t t = 0; t < program->tokensLength; ++t) {
		if (program->tokens[t].start >= end) {
			break;
		}
		if (jsMangleTokenEquals(js, &program->tokens[t], "function")) {
			char functionPrevious = jsManglePreviousPunctuation(program, t, 0);
			size_t i = program->tokens[t].start + sizeof "function" - 1;
			while (i < end && IsWhitespace(js[i])) {
				i += 1;
			}
			if (js[i] == '*') {
				i += 1;
				while (i < end && IsWhitespace(js[i])) {
					i += 1;
				}
			}
			while (jsIdentifierPart(js[i])) {
				i += 1;
			}
			while (i < end && IsWhitespace(js[i])) {
				i += 1;
			}
			if (js[i] == '(') {
				size_t paramsStart = i + 1;
				size_t paramsEnd = jsFindMatching(js, i, end, '(', ')');
				i = paramsEnd + 1;
				while (i < end && IsWhitespace(js[i])) {
					i += 1;
				}
				if (paramsEnd < end && js[i] == '{') {
					size_t bodyStart = i + 1;
					size_t bodyEnd = jsFindMatching(js, i, end, '{', '}');
					if (functionPrevious == ':') {
						while (t + 1 < program->tokensLength
							   && program->tokens[t + 1].start < bodyEnd) {
							t += 1;
						}
						continue;
					}
					struct JsMangleBinding * locals = NULL;
					size_t localsLength = 0;
					size_t localsCapacity = 0;
					bool skipFunction = false;
					if (bodyEnd >= end
						|| !jsMangleAllocBindings(
							&locals,
							&localsCapacity,
							jsMangleCountIdentifiers(
								js, paramsStart, paramsEnd)
								+ jsMangleCountIdentifiers(
									js, bodyStart, bodyEnd))
						|| !jsMangleCollectParams(js,
												  paramsStart,
												  paramsEnd,
												  &locals,
												  &localsLength,
												  &localsCapacity)
						|| !jsMangleCollectDeclarations(js,
														bodyStart,
														bodyEnd,
														&locals,
														&localsLength,
														&localsCapacity)) {
						free(locals);
						goto done;
					}
					for (size_t l = 0; l < localsLength; ++l) {
						if (jsMangleFindBinding(bindings,
												bindingsLength,
												locals[l].name,
												locals[l].length)
							!= NULL) {
							skipFunction = true;
							break;
						}
					}
					free(locals);
					if (skipFunction) {
						while (t + 1 < program->tokensLength
							   && program->tokens[t + 1].start < bodyEnd) {
							t += 1;
						}
						continue;
					}
				}
			}
		}
		if (!jsMangleIdentifierTokenEdit(js,
										 program,
										 t,
										 0,
										 end,
										 state,
										 bindings,
										 bindingsLength,
										 true)) {
			goto done;
		}
	}
	success = true;

done:
	jsMangleClearBindingIndex(bindings, bindingsLength);
	return (success);
}


static bool jsMangleAddFunctionEdits(
	const char * js,
	size_t start,
	size_t end,
	size_t paramsStart,
	size_t paramsEnd,
	struct JsMangleProgram * program,
	struct JsMangleState * state,
	struct JsMangleBinding * bindings,
	size_t bindingsLength
)
{
	bool success = false;
	if (!jsMangleIndexBindings(program, bindings, bindingsLength)) {
		return (false);
	}
	for (size_t t = jsMangleFindTokenStart(program, start);
		 t < program->tokensLength;
		 ++t) {
		if (program->tokens[t].start >= end) {
			break;
		}
		size_t editsLengthBefore = state->editsLength;
		if (!jsMangleIdentifierTokenEdit(js,
										 program,
										 t,
										 start,
										 end,
										 state,
										 bindings,
										 bindingsLength,
										 false)) {
			goto done;
		}
		if (program->tokens[t].start >= paramsStart
			&& program->tokens[t].start < paramsEnd
			&& program->tokens[t].kind == JS_MANGLE_TOKEN_IDENTIFIER) {
			char previous = jsManglePreviousPunctuation(program, t, start);
			char next = jsMangleNextPunctuation(program, t, end);
			bool restParam = program->tokens[t].start >= start + 3
							 && js[program->tokens[t].start - 1] == '.'
							 && js[program->tokens[t].start - 2] == '.'
							 && js[program->tokens[t].start - 3] == '.';
			if (previous != ':' && !restParam
				&& (previous == '{' || previous == ',')
				&& (next == '}' || next == ',')
				&& jsMangleInParameterBraces(
					js, paramsStart, paramsEnd, program->tokens[t].start)) {
				struct JsMangleBinding * binding
					= program->tokens[t].nameIndex == NULL
						  ? NULL
						  : program->tokens[t].nameIndex->binding;
				if (binding != NULL && binding->replacementLength != 0
					&& state->editsLength == editsLengthBefore + 1) {
					struct JsMangleEdit * edit
						= &state->edits[state->editsLength - 1];
					if (edit->start == program->tokens[t].start
						&& edit->length == program->tokens[t].length) {
						jsMangleFreeEdit(edit);
						state->editsLength -= 1;
						if (!jsMangleAddPatternAliasEdit(
								state,
								program->tokens[t].start,
								&js[program->tokens[t].start],
								program->tokens[t].length,
								binding->replacement,
								binding->replacementLength)) {
							goto done;
						}
					}
				}
			}
		}
	}
	success = true;

done:
	jsMangleClearBindingIndex(bindings, bindingsLength);
	return (success);
}


static bool jsMangleFunction(
	const char * js,
	size_t paramsStart,
	size_t paramsEnd,
	size_t bodyStart,
	size_t bodyEnd,
	size_t nameStart,
	size_t nameEnd,
	struct JsMangleProgram * program,
	struct JsMangleState * state
)
{
	if (jsMangleFunctionUnsafe(js, bodyStart, bodyEnd)) {
		return (jsMangleAddUnsafeRange(state, bodyStart - 1, bodyEnd));
	}

	struct JsMangleBinding * bindings = NULL;
	size_t bindingsLength = 0;
	size_t bindingsCapacity = 0;
	bool success = false;

	if (!jsMangleAllocBindings(
			&bindings,
			&bindingsCapacity,
			jsMangleCountIdentifiers(js, paramsStart, paramsEnd)
				+ jsMangleCountIdentifiers(js, bodyStart, bodyEnd)
				+ (nameStart < nameEnd))) {
		jsMangleFailureReason = "binding allocation";
		goto done;
	}
	if (nameStart < nameEnd
		&& !jsMangleAddBinding(&bindings,
							   &bindingsLength,
							   &bindingsCapacity,
							   &js[nameStart],
							   nameEnd - nameStart)) {
		jsMangleFailureReason = "function name binding";
		goto done;
	}
	if (!jsMangleCollectParams(js,
							   paramsStart,
							   paramsEnd,
							   &bindings,
							   &bindingsLength,
							   &bindingsCapacity)) {
		jsMangleFailureReason = "parameter collection";
		goto done;
	}
	if (!jsMangleCollectDeclarations(js,
									 bodyStart,
									 bodyEnd,
									 &bindings,
									 &bindingsLength,
									 &bindingsCapacity)) {
		jsMangleFailureReason = "declaration collection";
		goto done;
	}
	size_t editStart = nameStart < nameEnd ? nameStart : paramsStart;
	if (!jsMangleAssignNames(program,
							 js,
							 editStart,
							 bodyEnd,
							 paramsEnd,
							 state,
							 bindings,
							 bindingsLength)) {
		jsMangleFailureReason = "name assignment";
		goto done;
	}
	if (!jsMangleAddFunctionEdits(js,
								  editStart,
								  bodyEnd,
								  paramsStart,
								  paramsEnd,
								  program,
								  state,
								  bindings,
								  bindingsLength)) {
		jsMangleFailureReason = "function edits";
		goto done;
	}
	success = true;

done:
	free(bindings);
	return (success);
}


static size_t jsMangleExpressionEnd(
	const char * js, size_t start, size_t end
)
{
	size_t roundNesting = 0;
	size_t squareNesting = 0;

	for (size_t i = start; i < end; ++i) {
		if (js[i] == '"' || js[i] == '\'' || js[i] == '`') {
			i = jsSkipQuoted(js, i, end) - 1;
		} else if (js[i] == '/' && (js[i + 1] == '*' || js[i + 1] == '/')) {
			i = jsSkipComment(js, i, end) - 1;
		} else if (js[i] == '/' && jsRegexStart(js, start, i)) {
			i = jsSkipRegex(js, i, end) - 1;
		} else if (js[i] == '(') {
			roundNesting += 1;
		} else if (js[i] == ')') {
			roundNesting -= roundNesting > 0;
		} else if (js[i] == '[') {
			squareNesting += 1;
		} else if (js[i] == ']') {
			squareNesting -= squareNesting > 0;
		} else if (roundNesting == 0 && squareNesting == 0
				   && (js[i] == ';' || js[i] == ',')) {
			return (i);
		}
	}
	return (end);
}


static bool jsMangleArrow(
	const char * js,
	size_t paramsStart,
	size_t paramsEnd,
	size_t arrowI,
	size_t end,
	struct JsMangleProgram * program,
	struct JsMangleState * state,
	size_t * bodyEnd
)
{
	size_t bodyStart = arrowI + 2;
	while (bodyStart < end && IsWhitespace(js[bodyStart])) {
		bodyStart += 1;
	}
	if (js[bodyStart] == '{') {
		*bodyEnd = jsFindMatching(js, bodyStart, end, '{', '}');
		if (*bodyEnd >= end) {
			return (true);
		}
		return (jsMangleFunction(js,
								 paramsStart,
								 paramsEnd,
								 bodyStart + 1,
								 *bodyEnd,
								 bodyStart,
								 bodyStart,
								 program,
								 state));
	}
	*bodyEnd = jsMangleExpressionEnd(js, bodyStart, end);
	return (jsMangleFunction(js,
							 paramsStart,
							 paramsEnd,
							 bodyStart,
							 *bodyEnd,
							 bodyStart,
							 bodyStart,
							 program,
							 state));
}


struct Minification MangleJSIdentifiers( const char * js, bool forceModule )
{
	struct Minification m = {.result = NULL};
	struct JsMangleState state = {0};
	struct JsMangleProgram program = {0};
	struct JsMangleBinding * globalBindings = NULL;
	struct JsMangleBinding * exportedBindings = NULL;
	size_t globalBindingsLength = 0;
	size_t globalBindingsCapacity = 0;
	size_t exportedBindingsLength = 0;
	size_t exportedBindingsCapacity = 0;
	size_t length = strlen(js);
	struct JsMangleCounts counts = {0};

	jsMangleCountProgram(js, length, &counts);
	if (!jsMangleAllocProgram(&program, counts)
		|| !jsMangleAllocState(&state, counts)
		|| !jsMangleAllocBindings(
			&globalBindings, &globalBindingsCapacity, counts.identifiers)
		|| !jsMangleAllocBindings(&exportedBindings,
								  &exportedBindingsCapacity,
								  counts.identifiers)) {
		snprintf(m.error,
				 sizeof m.error,
				 "Cannot allocate memory (mangle setup)\n");
		goto error;
	}
	if (!jsMangleBuildProgram(js, length, &program)) {
		snprintf(m.error,
				 sizeof m.error,
				 "Cannot allocate memory (program build)\n");
		goto error;
	}

	if (jsMangleTopLevelUnsafe(js, &program)) {
		m.result = malloc(length + 1);
		if (m.result == NULL) {
			snprintf(m.error, sizeof m.error, "Cannot allocate memory\n");
			goto error;
		}
		memcpy(m.result, js, length + 1);
		jsMangleFreeProgram(&program);
		return (m);
	}

	if (forceModule || jsMangleIsModule(js, &program)) {
		if (!jsMangleCollectExportedDeclarations(js,
												 length,
												 &exportedBindings,
												 &exportedBindingsLength,
												 &exportedBindingsCapacity)) {
			snprintf(m.error,
					 sizeof m.error,
					 "Cannot allocate memory (export collection)\n");
			goto error;
		}
		if (!jsMangleCollectGlobalDeclarations(js,
											   length,
											   &globalBindings,
											   &globalBindingsLength,
											   &globalBindingsCapacity,
											   exportedBindings,
											   exportedBindingsLength)) {
			snprintf(m.error,
					 sizeof m.error,
					 "Cannot allocate memory (global collection)\n");
			goto error;
		}
		if (!jsMangleAssignGlobalNames(&program,
									   js,
									   length,
									   &state,
									   globalBindings,
									   globalBindingsLength)) {
			snprintf(m.error,
					 sizeof m.error,
					 "Cannot allocate memory (global names)\n");
			goto error;
		}
		if (!jsMangleAddGlobalEdits(js,
									length,
									&program,
									&state,
									globalBindings,
									globalBindingsLength)) {
			snprintf(m.error,
					 sizeof m.error,
					 "Cannot allocate memory (global edits)\n");
			goto error;
		}
	}

	for (size_t i = 0; i < length; ++i) {
		if (js[i] == '"' || js[i] == '\'' || js[i] == '`') {
			i = jsSkipQuoted(js, i, length) - 1;
		} else if (js[i] == '/' && (js[i + 1] == '*' || js[i + 1] == '/')) {
			i = jsSkipComment(js, i, length) - 1;
		} else if (js[i] == '/' && jsRegexStart(js, 0, i)) {
			i = jsSkipRegex(js, i, length) - 1;
		} else if (js[i] == '(') {
			size_t paramsStart = i + 1;
			size_t paramsEnd = jsFindMatching(js, i, length, '(', ')');
			if (paramsEnd >= length) {
				continue;
			}
			size_t arrowI = paramsEnd + 1;
			while (arrowI < length && IsWhitespace(js[arrowI])) {
				arrowI += 1;
			}
			if (js[arrowI] == '=' && js[arrowI + 1] == '>') {
				if (jsMangleInUnsafeRange(&state, i)) {
					continue;
				}
				size_t bodyEnd;
				if (!jsMangleArrow(js,
								   paramsStart,
								   paramsEnd,
								   arrowI,
								   length,
								   &program,
								   &state,
								   &bodyEnd)) {
					snprintf(m.error,
							 sizeof m.error,
							 "Cannot allocate memory (arrow mangling)\n");
					goto error;
				}
				i = bodyEnd;
			}
		} else if (jsIdentifierStart(js[i])
				   && jsWordEquals(js, i, "function")) {
			if (jsMangleInUnsafeRange(&state, i)) {
				continue;
			}
			size_t functionStart = i;
			i += sizeof "function" - 1;
			if (js[i] == '*') {
				i += 1;
			}
			while (IsWhitespace(js[i])) {
				i += 1;
			}
			size_t nameStart = i;
			while (jsIdentifierPart(js[i])) {
				i += 1;
			}
			size_t nameEnd = i;
			while (IsWhitespace(js[i])) {
				i += 1;
			}
			if (js[i] != '(') {
				continue;
			}
			char previous, next;
			jsManglePreviousNext(js,
								 0,
								 length,
								 functionStart,
								 functionStart + sizeof "function" - 1,
								 &previous,
								 &next);
			if (previous == '\0' || previous == ';' || previous == '{'
				|| previous == '}'
				|| jsManglePreviousWordEquals(js, 0, functionStart, "export")
				|| jsManglePreviousWordEquals(
					js, 0, functionStart, "default")) {
				nameStart = nameEnd;
			}
			size_t paramsStart = i + 1;
			size_t paramsEnd = jsFindMatching(js, i, length, '(', ')');
			if (paramsEnd >= length) {
				continue;
			}
			i = paramsEnd + 1;
			while (IsWhitespace(js[i])) {
				i += 1;
			}
			if (js[i] != '{') {
				continue;
			}
			size_t bodyStart = i + 1;
			size_t bodyEnd = jsFindMatching(js, i, length, '{', '}');
			if (bodyEnd >= length) {
				continue;
			}
			if (previous == ':') {
				i = bodyEnd;
				continue;
			}
			if (!jsMangleFunction(js,
								  paramsStart,
								  paramsEnd,
								  bodyStart,
								  bodyEnd,
								  nameStart,
								  nameEnd,
								  &program,
								  &state)) {
				snprintf(m.error,
						 sizeof m.error,
						 "Cannot allocate memory (function mangling: %s)\n",
						 jsMangleFailureReason != NULL ? jsMangleFailureReason
													   : "unknown");
				goto error;
			}
		} else if (jsIdentifierStart(js[i])) {
			size_t paramsStart = i;
			while (jsIdentifierPart(js[i])) {
				i += 1;
			}
			size_t paramsEnd = i;
			size_t arrowI = i;
			while (arrowI < length && IsWhitespace(js[arrowI])) {
				arrowI += 1;
			}
			if (js[arrowI] == '=' && js[arrowI + 1] == '>') {
				if (jsMangleInUnsafeRange(&state, paramsStart)) {
					i = paramsEnd;
					continue;
				}
				size_t bodyEnd;
				if (!jsMangleArrow(js,
								   paramsStart,
								   paramsEnd,
								   arrowI,
								   length,
								   &program,
								   &state,
								   &bodyEnd)) {
					snprintf(m.error,
							 sizeof m.error,
							 "Cannot allocate memory (arrow mangling)\n");
					goto error;
				}
				i = bodyEnd;
			} else {
				i -= 1;
			}
		}
	}

	size_t resultLength = length;
	qsort(state.edits,
		  state.editsLength,
		  sizeof *state.edits,
		  jsMangleCompareEdits);
	jsMangleRebaseEdits(&state);
	size_t editsLength = 0;
	for (size_t i = 0; i < state.editsLength; ++i) {
		if (editsLength > 0
			&& state.edits[i].start
				   < state.edits[editsLength - 1].start
						 + state.edits[editsLength - 1].length) {
			if (state.edits[i].start == state.edits[editsLength - 1].start
				&& state.edits[i].length
					   == state.edits[editsLength - 1].length) {
				jsMangleFreeEdit(&state.edits[i]);
				continue;
			}
			snprintf(m.error,
					 sizeof m.error,
					 "Cannot allocate memory (overlapping edits)\n");
			goto error;
		}
		if (editsLength < i) {
			jsMangleFreeEdit(&state.edits[editsLength]);
		}
		state.edits[editsLength++] = state.edits[i];
		if (editsLength - 1 < i) {
			jsMangleClearEdit(&state.edits[i]);
		}
	}
	state.editsLength = editsLength;
	jsMangleRebaseEdits(&state);
	for (size_t i = 0; i < state.editsLength; ++i) {
		resultLength
			+= state.edits[i].replacementLength - state.edits[i].length;
	}
	m.result = malloc(resultLength + 1);
	if (m.result == NULL) {
		snprintf(m.error,
				 sizeof m.error,
				 "Cannot allocate memory (result buffer)\n");
		goto error;
	}

	size_t inputI = 0;
	size_t outputI = 0;
	for (size_t i = 0; i < state.editsLength; ++i) {
		memcpy(&m.result[outputI], &js[inputI], state.edits[i].start - inputI);
		outputI += state.edits[i].start - inputI;
		memcpy(&m.result[outputI],
			   state.edits[i].replacement,
			   state.edits[i].replacementLength);
		outputI += state.edits[i].replacementLength;
		inputI = state.edits[i].start + state.edits[i].length;
	}
	memcpy(&m.result[outputI], &js[inputI], length - inputI + 1);

	jsMangleFreeEdits(&state);
	free(state.edits);
	free(state.maps);
	free(state.unsafeRanges);
	jsMangleFreeProgram(&program);
	free(globalBindings);
	free(exportedBindings);
	return (m);

error:
	jsMangleFreeEdits(&state);
	free(state.edits);
	free(state.maps);
	free(state.unsafeRanges);
	jsMangleFreeProgram(&program);
	free(globalBindings);
	free(exportedBindings);
	free(m.result);
	m.result = NULL;
	return (m);
}
