#!/bin/sh

#
# Copyright 2026 CodingMarkus
#
# SPDX-License-Identifier: ISC

# This utility intentionally emits SVG only. Rendering an SVG as PNG requires
# a non-POSIX rasterizer, which is not a project requirement.

set -eu

scriptDirectory=$(CDPATH='' cd "$(dirname "$0")" && pwd)
projectDirectory=$(CDPATH='' cd "$scriptDirectory/.." && pwd)
sizeReductionDocument=$projectDirectory/doc/CurrentSizeReductionBaselines.md
assetDirectory=$projectDirectory/doc/assets
outputImage=${1:-$assetDirectory/JavaScriptLibrarySizeReduction.svg}

awk -f - "$sizeReductionDocument" > "$outputImage" <<'AWK'
	/^[|] `/ {
		split($0, fields, "|")
		count += 1
		name[count] = fields[2]
		gsub(/^[[:space:]]*`|`[[:space:]]*$/, "", name[count])
		original[count] = fields[3] + 0
		plain[count] = fields[4] + 0
		mangled[count] = fields[6] + 0
	}
	END {
		width = 1400
		left = 280
		top = 130
		rowHeight = 48
		plotWidth = 980
		height = top + count * rowHeight + 100

		print "<svg xmlns='http://www.w3.org/2000/svg' width='" \
			width "' height='" height "' viewBox='0 0 " \
			width " " height "'>"
		print "<rect width='100%' height='100%' fill='#ffffff'/>"
		print "<style>text{font-family:-apple-system,BlinkMacSystemFont," \
			"'Segoe UI',sans-serif;fill:#202124}</style>"
		print "<text x='50' y='48' font-size='28' " \
			"font-weight='600'>JavaScript library benchmark: output size" \
			"</text>"
		print "<text x='50' y='78' font-size='17'>Output size " \
			"as a percentage of the original. Lower is better.</text>"

		for (tick = 0; tick <= 100; tick += 20) {
			x = left + plotWidth * tick / 100
			print "<line x1='" x "' y1='" top - 12 "' x2='" \
				x "' y2='" height - 60 "' stroke='#d8d8d8'/>"
			print "<text x='" x "' y='" height - 35 \
				"' font-size='15' text-anchor='middle'>" \
				tick "%</text>"
		}

		for (rowIndex = 1; rowIndex <= count; rowIndex += 1) {
			y = top + (rowIndex - 1) * rowHeight
			plainPercent = 100 * plain[rowIndex] / original[rowIndex]
			mangledPercent = 100 * mangled[rowIndex] / original[rowIndex]
			plainWidth = plotWidth * plainPercent / 100
			mangledWidth = plotWidth * mangledPercent / 100
			print "<text x='" left - 16 "' y='" y + 30 \
				"' font-size='16' text-anchor='end'>" \
				name[rowIndex] "</text>"
			print "<rect x='" left "' y='" y + 4 "' width='" \
				plainWidth "' height='16' fill='#4c78a8'/>"
			print "<rect x='" left "' y='" y + 25 "' width='" \
				mangledWidth "' height='16' fill='#f58518'/>"
			printf "<text x='%.1f' y='%.1f' font-size='14'>", \
				left + plainWidth + 8, y + 18
			printf "%.1f%%</text>\n", plainPercent
			printf "<text x='%.1f' y='%.1f' font-size='14'>", \
				left + mangledWidth + 8, y + 39
			printf "%.1f%%</text>\n", mangledPercent
		}

		print "<rect x='" left "' y='" height - 23 \
			"' width='16' height='16' fill='#4c78a8'/>"
		print "<text x='" left + 24 "' y='" height - 10 \
			"' font-size='15'>Minified</text>"
		print "<rect x='" left + 140 "' y='" height - 23 \
			"' width='16' height='16' fill='#f58518'/>"
		print "<text x='" left + 164 "' y='" height - 10 \
			"' font-size='15'>Minified and mangled</text>"
		print "</svg>"
	}
AWK
