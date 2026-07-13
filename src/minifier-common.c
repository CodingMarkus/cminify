/*
 * Copyright 2026 CodingMarkus
 * Copyright 2024-2026 Jumping-Beaver
 *
 * SPDX-License-Identifier: ISC
 */

#include "minifier.h"

#include <stdio.h>
#include <string.h>

bool IsWhitespace( const char c )
{
	return (c == ' ' || c == '\t' || c == '\n' || c == '\r');
}


bool SkipWhitespacesComments(
	struct Minification * m, const char * input, size_t * i, char * min,
	size_t * minLength, enum CommentVariant commentVariant
)
{
	bool skippedAllComments = true;
	do {
		while (IsWhitespace(input[*i])) {
			*i += 1;
		}
		const char * preservedComment = NULL;
		if (input[*i] == '\0') {
			break;
		} else if (input[*i] == '/' && input[*i + 1] == '*') {
			size_t commentStart = *i;
			if (input[*i + 2] == '!') {
				preservedComment = &input[*i];
			}
			*i += 2;
			while (input[*i] != '\0'
				   && (input[*i] != '*' || input[*i + 1] != '/')) {
				*i += 1;
			}
			if (input[*i] == '\0') {
				m->result = NULL;
				m->errorPosition = commentStart;
				snprintf(m->error,
						 sizeof m->error,
						 "Unclosed multi-line comment starting in line %%zu, "
						 "column %%zu\n");
				return (false);
			}
			*i += 2;
		} else if (commentVariant == JSCommentVariant && input[*i] == '/'
				   && input[*i + 1] == '/') {
			*i += 2;
			while (input[*i] != '\0' && input[*i] != '\n') {
				*i += 1;
			}
		} else {
			break;
		}
		if (preservedComment != NULL) {
			skippedAllComments = false;
			if (min != NULL) {
				size_t preservedCommentLength =
					(size_t)(&input[*i] - preservedComment);
				memcpy(&min[*minLength],
					preservedComment,
					preservedCommentLength);
				*minLength += preservedCommentLength;
			}
		}
	} while (true);
	return (skippedAllComments);
}


int StrNICmp( const char * s1, const char * s2, size_t length )
{
	int diff = 0;
	while (length--) {
		if ((diff = *s1 - *s2)) {
			if ((unsigned char)(*s1 - 'A') <= 'Z' - 'A') {
				diff += 'a' - 'A';
			}
			if ((unsigned char)(*s2 - 'A') <= 'Z' - 'A') {
				diff -= 'a' - 'A';
			}
			if (diff != 0) {
				break;
			}
		}
		if (*s1 == '\0') {
			break;
		}
		s1++;
		s2++;
	}
	return (diff);
}
