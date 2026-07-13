#!/bin/sh

#
# Copyright 2026 CodingMarkus
#
# SPDX-License-Identifier: ISC

set -eu

scriptDirectory=$(CDPATH='' cd "$(dirname "$0")" && pwd)
projectDirectory=$(CDPATH='' cd "$scriptDirectory/.." && pwd)
sizeReductionDocument=$projectDirectory/doc/CurrentSizeReductionBaselines.md
chartUtility=$projectDirectory/util/generate-size-reduction-chart.sh
testScript=$projectDirectory/test/stage3/test-js-libs.sh
temporaryDirectory=$(mktemp -d \
	"${TMPDIR:-/tmp}/webmincer-size-reduction.XXXXXX")
testOutput=$temporaryDirectory/test-output
tableFile=$temporaryDirectory/size-reduction-table.md
updatedDocument=$temporaryDirectory/CurrentSizeReductionBaselines.md

trap 'rm -rf "$temporaryDirectory"' EXIT HUP INT TERM

if ! (
	cd "$projectDirectory"
	"$testScript" --print-sizes
) > "$testOutput"
then
	cat "$testOutput"
	exit 1
fi

awk -f - "$testOutput" > "$tableFile" <<'AWK'
	/^\| Library \| Original bytes \| Minified bytes \| Reduction \|/ {
		inTable = 1
	}
	inTable && /^\|/ {
		sub(/ \([^)]*\)` \|/, "` |")
		print
		rows += 1
	}
	END { exit(rows < 2) }
AWK

awk -v tableFile="$tableFile" -f - "$sizeReductionDocument" \
	> "$updatedDocument" <<'AWK'
	BEGIN {
		while ((getline line < tableFile) > 0) {
			table[++tableLines] = line
		}
		close(tableFile)
	}
	$0 == "| Library | Original bytes | Minified bytes | Reduction |" \
		" Minified and mangled bytes | Reduction |" {
		for (line = 1; line <= tableLines; line += 1) {
			print table[line]
		}
		skipTable = 1
		next
	}
	skipTable && /^\|/ {
		next
	}
	skipTable {
		skipTable = 0
	}
	{
		print
	}
AWK

mv "$updatedDocument" "$sizeReductionDocument"
"$chartUtility"
