/*
 * Copyright 2026 CodingMarkus
 * SPDX-License-Identifier: ISC
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>

struct Minification {
	char * result;
	char error[256];
	size_t errorPosition;
};

enum CommentVariant {
	CSSCommentVariant,
	JSCommentVariant,
};

bool MangleOutputEnabled( void );

bool CompactWhitespaceEnabled( void );


bool IsWhitespace( char c );

int StrNICmp( const char * s1, const char * s2, size_t length );

bool SkipWhitespacesComments( struct Minification * m,
	const char * input,
	size_t * i,
	char * min,
	size_t * minLength,
	enum CommentVariant commentVariant );

struct Minification MinifyCSS( const char * css );

struct Minification MinifyHTML( const char * html );

struct Minification MinifyJS( const char * js );

struct Minification MinifyJSWithOptions( const char * js );

struct Minification MinifyJSModuleWithOptions( const char * js );

struct Minification MinifyJSON( const char * json );

struct Minification MinifyXML( const char * xml );
