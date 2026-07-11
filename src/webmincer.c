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

static bool optionMangleOutput = false;
static const char * const VERSION = "1.0";

struct Format {
	const char * name;
	struct Minification (* minify)( const char * );
};


static const struct Format FORMATS[] = {
	{"js", MinifyJSWithOptions},
	{"css", MinifyCSS},
	{"xml", MinifyXML},
	{"html", MinifyHTML},
	{"json", MinifyJSON},
};

static const char * programName( const char * argv0 )
{
	const char * lastSlash = strrchr(argv0, '/');
	if (lastSlash == NULL) {
		return (argv0);
	}
	return (lastSlash + 1);
}


static void printVersion( FILE * stream )
{
	fprintf(stream, "%s\n", VERSION);
}


static void printHelp( FILE * stream, const char * argv0 )
{
	const char * name = programName(argv0);

	fprintf(
		stream,
		"\n"
		"Usage:\n"
		"    %s <format> <input-file|-> [options]\n"
		"\n\n"
		"Formats:\n"
		"    css    Minify CSS input.\n"
		"    js     Minify JavaScript input.\n"
		"    xml    Minify XML input.\n"
		"    html   Minify HTML input.\n"
		"    json   Minify JSON input.\n"
		"\n\n"
		"Options:\n"
		"    -h, -help, --help       Show this help page.\n"
		"\n"
		"    --benchmark             Print size reduction statistics.\n"
		"\n"
		"    --mangle                Enable output mangling.\n"
		"\n"
		"                            Currently only JavaScript code is\n"
		"                            mangled, so this has no effect unless\n"
		"                            the format is js or the input is HTML\n"
		"                            with embedded JavaScript.\n"
		"\n"
		"    --version               Print version %s.\n"
		"\n\n"
		"Notes:\n"
		"    Use '-' as the input file to read from standard input.\n",
		name,
		VERSION);
}


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


bool MangleOutputEnabled( void )
{
	return (optionMangleOutput);
}


struct LineColumn {
	size_t line;
	size_t column;
};

static struct LineColumn positionToLineColumn(
	const char * text, size_t position
)
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
	const char * formatStr = NULL;
	const char * inputFilename = NULL;

	if (argc == 1) {
		printHelp(stdout, argv[0]);
		return (EXIT_SUCCESS);
	}

	for (int i = 1; i < argc; ++i) {
		if (!strcmp(argv[i], "--benchmark")) {
			benchmark = true;
		} else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "-help")
			   || !strcmp(argv[i], "--help")) {
			printHelp(stdout, argv[0]);
			return (EXIT_SUCCESS);
		} else if (!strcmp(argv[i], "--version")) {
			printVersion(stdout);
			return (EXIT_SUCCESS);
		} else if (!strcmp(argv[i], "--mangle")) {
			optionMangleOutput = true;
		} else if (formatStr == NULL) {
			formatStr = argv[i];
		} else if (inputFilename == NULL) {
			inputFilename = argv[i];
		} else {
			printHelp(stderr, argv[0]);
			return (EXIT_FAILURE);
		}
	}
	if (formatStr == NULL || inputFilename == NULL) {
		printHelp(stderr, argv[0]);
		return (EXIT_FAILURE);
	}
	const struct Format * format = NULL;
	for (size_t i = 0; i < sizeof FORMATS / sizeof *FORMATS; ++i)
		if (!strcmp(formatStr, FORMATS[i].name)) {
			format = &FORMATS[i];
			break;
		}
	if (format == NULL) {
		fprintf(stderr, "Unsupported input format: %s\n\n", formatStr);
		printHelp(stderr, argv[0]);
		return (EXIT_FAILURE);
	}

	char * input = fileGetContent(inputFilename);
	if (input == NULL) {
		perror(inputFilename);
		return (EXIT_FAILURE);
	}
	struct Minification m = format->minify(input);
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
