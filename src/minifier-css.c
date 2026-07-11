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

struct Color {
	const char * name;
	const char * hex;
};

struct IntegerUnitConversion {
	const char * sourceUnit;
	size_t divisor;
	const char * targetUnit;
};

struct DecimalUnitConversion {
	const char * sourceUnit;
	size_t divisor;
	size_t decimalPlaces;
};


static const struct Color CSS_COLORS[] = {
		{"black", "000000"},
		{"silver", "c0c0c0"},
		{"gray", "808080"},
		{"white", "ffffff"},
		{"maroon", "800000"},
		{"red", "ff0000"},
		{"rebeccapurple", "663399"},
		{"purple", "800080"},
		{"fuchsia", "ff00ff"},
		{"green", "008000"},
		{"lime", "00ff00"},
		{"olive", "808000"},
		{"yellow", "ffff00"},
		{"navy", "000080"},
		{"blue", "0000ff"},
		{"teal", "008080"},
		{"aqua", "00ffff"},
		{"aliceblue", "f0f8ff"},
		{"antiquewhite", "faebd7"},
		{"aquamarine", "7fffd4"},
		{"azure", "f0ffff"},
		{"beige", "f5f5dc"},
		{"bisque", "ffe4c4"},
		{"blanchedalmond", "ffebcd"},
		{"blueviolet", "8a2be2"},
		{"brown", "a52a2a"},
		{"burlywood", "deb887"},
		{"cadetblue", "5f9ea0"},
		{"chartreuse", "7fff00"},
		{"chocolate", "d2691e"},
		{"coral", "ff7f50"},
		{"cornflowerblue", "6495ed"},
		{"cornsilk", "fff8dc"},
		{"crimson", "dc143c"},
		{"cyan", "00ffff"},
		{"darkblue", "00008b"},
		{"darkcyan", "008b8b"},
		{"darkgoldenrod", "b8860b"},
		{"darkgray", "a9a9a9"},
		{"darkgreen", "006400"},
		{"darkgrey", "a9a9a9"},
		{"darkkhaki", "bdb76b"},
		{"darkmagenta", "8b008b"},
		{"darkolivegreen", "556b2f"},
		{"darkorange", "ff8c00"},
		{"darkorchid", "9932cc"},
		{"darkred", "8b0000"},
		{"darksalmon", "e9967a"},
		{"darkseagreen", "8fbc8f"},
		{"darkslateblue", "483d8b"},
		{"darkslategray", "2f4f4f"},
		{"darkslategrey", "2f4f4f"},
		{"darkturquoise", "00ced1"},
		{"darkviolet", "9400d3"},
		{"deeppink", "ff1493"},
		{"deepskyblue", "00bfff"},
		{"dimgray", "696969"},
		{"dimgrey", "696969"},
		{"dodgerblue", "1e90ff"},
		{"firebrick", "b22222"},
		{"floralwhite", "fffaf0"},
		{"forestgreen", "228b22"},
		{"gainsboro", "dcdcdc"},
		{"ghostwhite", "f8f8ff"},
		{"gold", "ffd700"},
		{"goldenrod", "daa520"},
		{"greenyellow", "adff2f"},
		{"grey", "808080"},
		{"honeydew", "f0fff0"},
		{"hotpink", "ff69b4"},
		{"indianred", "cd5c5c"},
		{"indigo", "4b0082"},
		{"ivory", "fffff0"},
		{"khaki", "f0e68c"},
		{"lavender", "e6e6fa"},
		{"lavenderblush", "fff0f5"},
		{"lawngreen", "7cfc00"},
		{"lemonchiffon", "fffacd"},
		{"lightblue", "add8e6"},
		{"lightcoral", "f08080"},
		{"lightcyan", "e0ffff"},
		{"lightgoldenrodyellow", "fafad2"},
		{"lightgray", "d3d3d3"},
		{"lightgreen", "90ee90"},
		{"lightgrey", "d3d3d3"},
		{"lightpink", "ffb6c1"},
		{"lightsalmon", "ffa07a"},
		{"lightseagreen", "20b2aa"},
		{"lightskyblue", "87cefa"},
		{"lightslategray", "778899"},
		{"lightslategrey", "778899"},
		{"lightsteelblue", "b0c4de"},
		{"lightyellow", "ffffe0"},
		{"limegreen", "32cd32"},
		{"linen", "faf0e6"},
		{"magenta", "ff00ff"},
		{"mediumaquamarine", "66cdaa"},
		{"mediumblue", "0000cd"},
		{"mediumorchid", "ba55d3"},
		{"mediumpurple", "9370db"},
		{"mediumseagreen", "3cb371"},
		{"mediumslateblue", "7b68ee"},
		{"mediumspringgreen", "00fa9a"},
		{"mediumturquoise", "48d1cc"},
		{"mediumvioletred", "c71585"},
		{"midnightblue", "191970"},
		{"mintcream", "f5fffa"},
		{"mistyrose", "ffe4e1"},
		{"moccasin", "ffe4b5"},
		{"navajowhite", "ffdead"},
		{"oldlace", "fdf5e6"},
		{"olivedrab", "6b8e23"},
		{"orange", "ffa500"},
		{"orangered", "ff4500"},
		{"orchid", "da70d6"},
		{"palegoldenrod", "eee8aa"},
		{"palegreen", "98fb98"},
		{"paleturquoise", "afeeee"},
		{"palevioletred", "db7093"},
		{"papayawhip", "ffefd5"},
		{"peachpuff", "ffdab9"},
		{"peru", "cd853f"},
		{"pink", "ffc0cb"},
		{"plum", "dda0dd"},
		{"powderblue", "b0e0e6"},
		{"rosybrown", "bc8f8f"},
		{"royalblue", "4169e1"},
		{"saddlebrown", "8b4513"},
		{"salmon", "fa8072"},
		{"sandybrown", "f4a460"},
		{"seagreen", "2e8b57"},
		{"seashell", "fff5ee"},
		{"sienna", "a0522d"},
		{"skyblue", "87ceeb"},
		{"slateblue", "6a5acd"},
		{"slategray", "708090"},
		{"slategrey", "708090"},
		{"snow", "fffafa"},
		{"springgreen", "00ff7f"},
		{"steelblue", "4682b4"},
		{"tan", "d2b48c"},
		{"thistle", "d8bfd8"},
		{"tomato", "ff6347"},
		{"turquoise", "40e0d0"},
		{"violet", "ee82ee"},
		{"wheat", "f5deb3"},
		{"whitesmoke", "f5f5f5"},
		{"yellowgreen", "9acd32"},
};

static const struct IntegerUnitConversion INTEGER_UNIT_CONVERSIONS[] = {
		{"hz", 1000, "kHz"},
		{"khz", 1000, "MHz"},
		{"deg", 360, "turn"},
		{"grad", 400, "turn"},
		{"px", 96, "in"},
		{"pt", 72, "in"},
		{"pc", 6, "in"},
		{"dpi", 96, "dppx"},
};

static const struct DecimalUnitConversion DECIMAL_UNIT_CONVERSIONS[] = {
		{"cm", 254, 2},
		{"mm", 254, 1},
};


static bool isHexadecimalDigit( const char c )
{
	return ((isdigit((unsigned char)c)
			|| (unsigned char)(c - 'a') <= 'f' - 'a'
			|| (unsigned char)(c - 'A') <= 'F' - 'A')
	);
}


static char lowercaseHexadecimalDigit( const char c )
{
	return ((unsigned char)(c - 'A') <= 'F' - 'A' ? c + 'a' - 'A' : c);
}


static bool isIdentifierCharacter( const char c )
{
	return (isalnum((unsigned char)c) || c == '-' || c == '_'
			|| (unsigned char)c >= 0x80);
}


static bool isColorProperty( const char * result, size_t resultLength )
{
	const char * property = &result[resultLength];
	while (property > result && property[-1] != '{' && property[-1] != '}'
		&& property[-1] != ';') {
		property -= 1;
	}
	size_t propertySpan = (size_t)(&result[resultLength] - property);
	const char * colon = memchr(
		property, ':', propertySpan);
	if (colon == NULL || property[0] == '-') {
		return (false);
	}
	const size_t propertyLength = (size_t)(colon - property);
	return ((propertyLength == sizeof "color" - 1
			&& !StrNICmp(property, "color", propertyLength))
		|| (propertyLength == sizeof "background" - 1
			&& !StrNICmp(property, "background", propertyLength))
		|| (propertyLength >= sizeof "background-" - 1
			&& !StrNICmp(property, "background-", sizeof "background-" - 1))
		|| (propertyLength >= sizeof "border" - 1
			&& !StrNICmp(property, "border", sizeof "border" - 1))
		|| (propertyLength == sizeof "box-shadow" - 1
			&& !StrNICmp(property, "box-shadow", propertyLength))
		|| (propertyLength == sizeof "caret-color" - 1
			&& !StrNICmp(property, "caret-color", propertyLength))
		|| (propertyLength >= sizeof "column-rule" - 1
			&& !StrNICmp(property, "column-rule", sizeof "column-rule" - 1))
		|| (propertyLength == sizeof "fill" - 1
			&& !StrNICmp(property, "fill", propertyLength))
		|| (propertyLength == sizeof "flood-color" - 1
			&& !StrNICmp(property, "flood-color", propertyLength))
		|| (propertyLength == sizeof "lighting-color" - 1
			&& !StrNICmp(property, "lighting-color", propertyLength))
		|| (propertyLength >= sizeof "outline" - 1
			&& !StrNICmp(property, "outline", sizeof "outline" - 1))
		|| (propertyLength == sizeof "scrollbar-color" - 1
			&& !StrNICmp(property, "scrollbar-color", propertyLength))
		|| (propertyLength == sizeof "stop-color" - 1
			&& !StrNICmp(property, "stop-color", propertyLength))
		|| (propertyLength == sizeof "stroke" - 1
			&& !StrNICmp(property, "stroke", propertyLength))
		|| (propertyLength >= sizeof "text-decoration" - 1
			&& !StrNICmp(
				property, "text-decoration", sizeof "text-decoration" - 1))
		|| (propertyLength == sizeof "text-emphasis-color" - 1
			&& !StrNICmp(property, "text-emphasis-color", propertyLength))
		|| (propertyLength == sizeof "text-shadow" - 1
			&& !StrNICmp(property, "text-shadow", propertyLength)));
}


static void shortenHexColor( char * color, size_t * colorLength )
{
	if (*colorLength != 6 && *colorLength != 8) {
		return;
	}
	for (size_t colorI = 0; colorI < *colorLength; colorI += 2) {
		if (color[colorI] != color[colorI + 1]) {
			return;
		}
	}
	for (size_t colorI = 0; colorI < *colorLength / 2; colorI += 1) {
		color[colorI] = color[colorI * 2];
	}
	*colorLength /= 2;
}


static const char * shorterColorName( const char * color, size_t colorLength )
{
	const char * shortestName = NULL;
	for (size_t i = 0; i < sizeof CSS_COLORS / sizeof *CSS_COLORS; i += 1) {
		char candidate[ 8 ];
		memcpy(candidate, CSS_COLORS[i].hex, sizeof "000000" - 1);
		size_t candidateLength = sizeof "000000" - 1;
		shortenHexColor(candidate, &candidateLength);

		if (candidateLength == colorLength
			&& !strncmp(color, candidate, colorLength)
			&& (shortestName == NULL
				|| strlen(CSS_COLORS[i].name) < strlen(shortestName))) {
			shortestName = CSS_COLORS[i].name;
		}
	}
	return (shortestName);
}


static size_t minifyHexColor(
	const char * css, size_t i, char * result, size_t * resultLength
)
{
	size_t colorLength = 0;
	while (isHexadecimalDigit(css[i + 1 + colorLength])) {
		colorLength += 1;
	}
	if ((colorLength != 3 && colorLength != 4 && colorLength != 6
			&& colorLength != 8)
		|| isIdentifierCharacter(css[i + 1 + colorLength])
		|| css[i + 1 + colorLength] == '\\') {
		return (0);
	}
	const size_t inputLength = colorLength;

	char color[ 8 ];
	for (size_t colorI = 0; colorI < colorLength; colorI += 1) {
		color[colorI] = lowercaseHexadecimalDigit(css[i + 1 + colorI]);
	}
	shortenHexColor(color, &colorLength);

	const char * colorName = shorterColorName(color, colorLength);
	if (colorName != NULL && strlen(colorName) < colorLength + 1) {
		memcpy(&result[*resultLength], colorName, strlen(colorName));
		*resultLength += strlen(colorName);
	} else {
		result[(*resultLength)++] = '#';
		memcpy(&result[*resultLength], color, colorLength);
		*resultLength += colorLength;
	}
	return (inputLength + 1);
}


static size_t minifyColorName(
	const char * css, size_t i, char * result, size_t * resultLength
)
{
	size_t inputLength = 0;
	while (isIdentifierCharacter(css[i + inputLength])) {
		inputLength += 1;
	}
	for (size_t colorI = 0; colorI < sizeof CSS_COLORS / sizeof *CSS_COLORS;
		 colorI += 1) {
		if (strlen(CSS_COLORS[colorI].name) != inputLength
			|| StrNICmp(&css[i], CSS_COLORS[colorI].name, inputLength)) {
			continue;
		}

		char color[ 8 ];
		memcpy(color, CSS_COLORS[colorI].hex, sizeof "000000" - 1);
		size_t colorLength = sizeof "000000" - 1;
		shortenHexColor(color, &colorLength);

		const char * colorName = shorterColorName(color, colorLength);
		if (colorName != NULL && strlen(colorName) < colorLength + 1) {
			memcpy(&result[*resultLength], colorName, strlen(colorName));
			*resultLength += strlen(colorName);
		} else {
			result[(*resultLength)++] = '#';
			memcpy(&result[*resultLength], color, colorLength);
			*resultLength += colorLength;
		}
		return (inputLength);
	}
	return (0);
}


static bool matchesUnit(
	const char * css, size_t i, const char * unit
)
{
	for (size_t unitI = 0; unit[unitI] != '\0'; unitI += 1) {
		if (tolower((unsigned char) css[i + unitI]) != unit[unitI]) {
			return (false);
		}
	}
	return (!isIdentifierCharacter(css[i + strlen(unit)])
		&& css[i + strlen(unit)] != '\\');
}


static size_t minifyIntegerUnit(
	const char * css,
	size_t i,
	const char * sourceUnit,
	size_t divisor,
	const char * targetUnit,
	char * result,
	size_t * resultLength
)
{
	if (i != 0 && isdigit(css[i - 1])) {
		return (0);
	}
	size_t end = i;
	while (isdigit(css[end])) {
		end += 1;
	}
	if (!matchesUnit(css, end, sourceUnit)) {
		return (0);
	}

	size_t remainder = 0;
	size_t quotientLength = 0;
	bool hasQuotientDigit = false;
	for (size_t digitI = i; digitI < end; digitI += 1) {
		remainder = remainder * 10 + (size_t) (css[digitI] - '0');
		size_t quotientDigit = remainder / divisor;
		remainder %= divisor;
		if (quotientDigit != 0 || hasQuotientDigit) {
			hasQuotientDigit = true;
			quotientLength += 1;
		}
	}
	if (remainder != 0) {
		return (0);
	}
	if (!hasQuotientDigit) {
		quotientLength = 1;
	}
	size_t sourceLength = end - i + strlen(sourceUnit);
	if (quotientLength + strlen(targetUnit) >= sourceLength) {
		return (0);
	}

	remainder = 0;
	hasQuotientDigit = false;
	for (size_t digitI = i; digitI < end; digitI += 1) {
		remainder = remainder * 10 + (size_t) (css[digitI] - '0');
		size_t quotientDigit = remainder / divisor;
		remainder %= divisor;
		if (quotientDigit != 0 || hasQuotientDigit) {
			hasQuotientDigit = true;
			result[(*resultLength)++] = (char) ('0' + quotientDigit);
		}
	}
	if (!hasQuotientDigit) {
		result[(*resultLength)++] = '0';
	}
	memcpy(&result[*resultLength], targetUnit, strlen(targetUnit));
	*resultLength += strlen(targetUnit);
	return (sourceLength);
}


static size_t minifyDecimalUnitToInteger(
	const char * css,
	size_t i,
	const char * sourceUnit,
	size_t divisor,
	size_t decimalPlaces,
	const char * targetUnit,
	char * result,
	size_t * resultLength
)
{
	if (i != 0 && isdigit(css[i - 1])) {
		return (0);
	}
	size_t dotI = i;
	while (isdigit(css[dotI])) {
		dotI += 1;
	}
	if (css[dotI] != '.') {
		return (0);
	}
	size_t end = dotI + 1;
	while (isdigit(css[end])) {
		end += 1;
	}
	if (end == dotI + 1 || !matchesUnit(css, end, sourceUnit)) {
		return (0);
	}

	size_t fractionalLength = end - dotI - 1;
	while (fractionalLength != 0
		&& css[dotI + fractionalLength] == '0') {
		fractionalLength -= 1;
	}
	if (fractionalLength > decimalPlaces) {
		return (0);
	}

	size_t remainder = 0;
	size_t quotientLength = 0;
	bool hasQuotientDigit = false;
	for (size_t digitI = i; digitI < dotI + fractionalLength + 1;
		 digitI += 1) {
		if (digitI == dotI) {
			continue;
		}
		remainder = remainder * 10 + (size_t) (css[digitI] - '0');
		size_t quotientDigit = remainder / divisor;
		remainder %= divisor;
		if (quotientDigit != 0 || hasQuotientDigit) {
			hasQuotientDigit = true;
			quotientLength += 1;
		}
	}
	for (size_t zeroI = fractionalLength; zeroI < decimalPlaces;
		 zeroI += 1) {
		remainder *= 10;
		size_t quotientDigit = remainder / divisor;
		remainder %= divisor;
		if (quotientDigit != 0 || hasQuotientDigit) {
			hasQuotientDigit = true;
			quotientLength += 1;
		}
	}
	if (remainder != 0) {
		return (0);
	}
	if (!hasQuotientDigit) {
		quotientLength = 1;
	}
	size_t sourceLength = end - i + strlen(sourceUnit);
	if (quotientLength + strlen(targetUnit) >= sourceLength) {
		return (0);
	}

	remainder = 0;
	hasQuotientDigit = false;
	for (size_t digitI = i; digitI < dotI + fractionalLength + 1;
		 digitI += 1) {
		if (digitI == dotI) {
			continue;
		}
		remainder = remainder * 10 + (size_t) (css[digitI] - '0');
		size_t quotientDigit = remainder / divisor;
		remainder %= divisor;
		if (quotientDigit != 0 || hasQuotientDigit) {
			hasQuotientDigit = true;
			result[(*resultLength)++] = (char) ('0' + quotientDigit);
		}
	}
	for (size_t zeroI = fractionalLength; zeroI < decimalPlaces;
		 zeroI += 1) {
		remainder *= 10;
		size_t quotientDigit = remainder / divisor;
		remainder %= divisor;
		if (quotientDigit != 0 || hasQuotientDigit) {
			hasQuotientDigit = true;
			result[(*resultLength)++] = (char) ('0' + quotientDigit);
		}
	}
	if (!hasQuotientDigit) {
		result[(*resultLength)++] = '0';
	}
	memcpy(&result[*resultLength], targetUnit, strlen(targetUnit));
	*resultLength += strlen(targetUnit);
	return (sourceLength);
}


static size_t minifyMilliseconds(
	const char * css, size_t i, char * result, size_t * resultLength
)
{
	if (i != 0 && isdigit(css[i - 1])) {
		return (0);
	}
	size_t end = i;
	while (isdigit(css[end])) {
		end += 1;
	}
	if (!matchesUnit(css, end, "ms")) {
		return (0);
	}

	size_t firstNonzero = i;
	while (css[firstNonzero] == '0') {
		firstNonzero += 1;
	}
	if (firstNonzero == end) {
		if (end + 2 - i <= sizeof "0s" - 1) {
			return (0);
		}
		memcpy(&result[*resultLength], "0s", sizeof "0s" - 1);
		*resultLength += sizeof "0s" - 1;
		return (end + 2 - i);
	}

	size_t significantLength = end - firstNonzero;
	size_t trailingZeros = 0;
	while (trailingZeros < 3
		&& css[end - 1 - trailingZeros] == '0') {
		trailingZeros += 1;
	}
	size_t integerLength
		= significantLength > 3 ? significantLength - 3 : 0;
	size_t fractionalLength = 3 - trailingZeros;
	size_t leadingFractionalZeros
		= significantLength < 3 ? 3 - significantLength : 0;
	size_t fractionalSourceLength = significantLength < 3
									 ? significantLength - trailingZeros
									 : fractionalLength;
	size_t outputLength = integerLength + sizeof "s" - 1;
	if (fractionalLength != 0) {
		outputLength += sizeof "." - 1 + fractionalLength;
	}
	if (outputLength >= end + 2 - i) {
		return (0);
	}

	if (integerLength != 0) {
		memcpy(&result[*resultLength], &css[firstNonzero], integerLength);
		*resultLength += integerLength;
	}
	if (fractionalLength != 0) {
		result[(*resultLength)++] = '.';
		for (size_t zeroI = 0; zeroI < leadingFractionalZeros;
			 zeroI += 1) {
			result[(*resultLength)++] = '0';
		}
		size_t fractionStart = firstNonzero + integerLength;
		memcpy(&result[*resultLength],
			   &css[fractionStart],
			   fractionalSourceLength);
		*resultLength += fractionalSourceLength;
	}
	result[(*resultLength)++] = 's';
	return (end + 2 - i);
}


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
		if (syntaxBlock == SYNTAX_BLOCK_STYLE && isdigit(css[i])) {
			bool minifiedValue = false;
			size_t timeLength
				= minifyMilliseconds(css, i, m.result, &resultLength);
			if (timeLength != 0) {
				i += timeLength;
				continue;
			}
			for (size_t conversionI = 0;
				 conversionI < sizeof INTEGER_UNIT_CONVERSIONS
							   / sizeof *INTEGER_UNIT_CONVERSIONS;
				 conversionI += 1) {
				size_t conversionLength = minifyIntegerUnit(
					css,
					i,
					INTEGER_UNIT_CONVERSIONS[conversionI].sourceUnit,
					INTEGER_UNIT_CONVERSIONS[conversionI].divisor,
					INTEGER_UNIT_CONVERSIONS[conversionI].targetUnit,
					m.result,
					&resultLength);
				if (conversionLength != 0) {
					i += conversionLength;
					minifiedValue = true;
					break;
				}
			}
			if (!minifiedValue) {
				for (size_t conversionI = 0;
					 conversionI < sizeof DECIMAL_UNIT_CONVERSIONS
								   / sizeof *DECIMAL_UNIT_CONVERSIONS;
					 conversionI += 1) {
					size_t conversionLength = minifyDecimalUnitToInteger(
						css,
						i,
						DECIMAL_UNIT_CONVERSIONS[conversionI].sourceUnit,
						DECIMAL_UNIT_CONVERSIONS[conversionI].divisor,
						DECIMAL_UNIT_CONVERSIONS[conversionI].decimalPlaces,
						"in",
						m.result,
						&resultLength);
					if (conversionLength != 0) {
						i += conversionLength;
						minifiedValue = true;
						break;
					}
				}
			}
			if (minifiedValue) {
				continue;
			}
		}
		if (css[i] == '#' && syntaxBlock == SYNTAX_BLOCK_STYLE
			&& isColorProperty(m.result, resultLength)) {
			size_t colorLength
				= minifyHexColor(css, i, m.result, &resultLength);
			if (colorLength != 0) {
				i += colorLength;
				continue;
			}
		}
		if (isColorProperty(m.result, resultLength)
			&& isIdentifierCharacter(css[i])) {
			size_t colorLength
				= minifyColorName(css, i, m.result, &resultLength);
			if (colorLength != 0) {
				i += colorLength;
				continue;
			}
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
