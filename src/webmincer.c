/*
 * Copyright 2026 CodingMarkus
 * Copyright 2024-2026 Jumping-Beaver
 *
 * SPDX-License-Identifier: ISC
 */

#include "minifier.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool optionMangleJSIdentifiers = false;

static char * fileGetContent( const char * filename )
{
	FILE * fp;
	if (filename[0] == '-' && filename[1] == '\0') {
		fp = stdin;
	} else {
		fp = fopen(filename, "r");
		if (fp == NULL) {
			return (NULL);
		}
	}
	size_t bufferSize = BUFSIZ + 1;
	char * buffer = malloc(bufferSize);
	if (buffer == NULL) {
		fclose(fp);
		return (NULL);
	}
	size_t read = 0;
	char * largerBuffer;
	do {
		read += fread(&buffer[read], 1, bufferSize - read - 1, fp);
		if (ferror(fp) != 0) {
			free(buffer);
			fclose(fp);
			return (NULL);
		}
		if (feof(fp) != 0) {
			break;
		}
		bufferSize += BUFSIZ;
		largerBuffer = realloc(buffer, bufferSize);
		if (largerBuffer == NULL) {
			free(buffer);
			fclose(fp);
			return (NULL);
		}
		buffer = largerBuffer;
	} while (true);
	buffer[read] = '\0';
	return (buffer);
}


bool MangleJSIdentifiersEnabled(  )
{
	return (optionMangleJSIdentifiers);
}

struct LineColumn {
	size_t line;
	size_t column;
};

struct LineColumn positionToLineColumn( const char * text, size_t position )
{
	struct LineColumn lc = {.line = 1, .column = 0};
	for (size_t i = 0; i <= position; ++i) {
		if (text[i] == '\n') {
			lc.line += 1;
			lc.column = 0;
		} else {
			lc.column += 1;
		}
	}
	return (lc);
}


int main( int argc, const char * argv[] )
{
	bool benchmark = false;
	bool printUsage = false;
	const char * formatStr = NULL;
	const char * inputFilename = NULL;

	enum {
		FORMAT_JS,
		FORMAT_CSS,
		FORMAT_XML,
		FORMAT_HTML,
		FORMAT_JSON
	} format;

	for (int i = 1; i < argc; ++i) {
		if (!strcmp(argv[i], "--benchmark")) {
			benchmark = true;
		} else if (!strcmp(argv[i], "--mangle-js-identifiers")) {
			optionMangleJSIdentifiers = true;
		} else if (formatStr == NULL) {
			formatStr = argv[i];
		} else if (inputFilename == NULL) {
			inputFilename = argv[i];
		} else {
			printUsage = true;
			break;
		}
	}
	if (formatStr == NULL || inputFilename == NULL) {
		printUsage = true;
	} else if (!strcmp(formatStr, "js")) {
		format = FORMAT_JS;
	} else if (!strcmp(formatStr, "css")) {
		format = FORMAT_CSS;
	} else if (!strcmp(formatStr, "xml")) {
		format = FORMAT_XML;
	} else if (!strcmp(formatStr, "html")) {
		format = FORMAT_HTML;
	} else if (!strcmp(formatStr, "json")) {
		format = FORMAT_JSON;
	} else {
		fprintf(stderr, "Unsupported input format: %s\n", formatStr);
		printUsage = true;
	}

	if (printUsage) {
		fputs("Usage: ", stderr);
		fputs(argv[0], stderr);
		fputs(" <css|js|xml|html|json> <input file|-> [--benchmark] "
			  "[--mangle-js-identifiers]\n",
			  stderr);
		return (EXIT_FAILURE);
	}

	char * input = fileGetContent(inputFilename);
	if (input == NULL) {
		perror(inputFilename);
		return (EXIT_FAILURE);
	}
	struct Minification m;
	switch (format) {
	case FORMAT_JS:
		m = MinifyJSWithOptions(input);
		break;
	case FORMAT_CSS:
		m = MinifyCSS(input);
		break;
	case FORMAT_XML:
		m = MinifyXML(input);
		break;
	case FORMAT_HTML:
		m = MinifyHTML(input);
		break;
	case FORMAT_JSON:
	default:
		m = MinifyJSON(input);
		break;
	}
	if (m.result == NULL) {
		struct LineColumn lineColumn
			= positionToLineColumn(input, m.errorPosition);
		free(input);
		fprintf(stderr, m.error, lineColumn.line, lineColumn.column);
		free(m.result);
		return (EXIT_FAILURE);
	}
	if (benchmark) {
		size_t strlenInput = strlen(input);
		size_t strlenMinification = strlen(m.result);
		printf("Reduced the size by %.1f%% from %zu to %zu bytes\n",
			   100.0 - 100.0 * strlenMinification / strlenInput,
			   strlenInput,
			   strlenMinification);
	} else {
		fputs(m.result, stdout);
	}
	free(m.result);
	free(input);
	return (EXIT_SUCCESS);
}
