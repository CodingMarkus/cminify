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
testScript=$projectDirectory/test/test-js-libs.sh
temporaryDirectory=$(mktemp -d \
	"${TMPDIR:-/tmp}/webmincer-size-reduction.XXXXXX")
testOutput=$temporaryDirectory/test-output
tableFile=$temporaryDirectory/size-reduction-table.md
updatedDocument=$temporaryDirectory/CurrentSizeReductionBaselines.md

trap 'rm -rf "$temporaryDirectory"' EXIT HUP INT TERM

if ! (
	cd "$projectDirectory"
	WEBMINCER_SKIP_SIZE_REDUCTION_BASELINE_CHECK=1 "$testScript"
) > "$testOutput"
then
	cat "$testOutput"
	exit 1
fi

awk -f - "$testOutput" > "$tableFile" <<'AWK'
	BEGIN {
		print "| Library | Original bytes | Minified bytes | Reduction |" \
			" Minified and mangled bytes | Reduction |"
		print "| --- | ---: | ---: | ---: | ---: | ---: |"
	}
	/^\.test\/test-js-libs\/.* \(plain\):$/ {
		file = $1
		sub(/^\.test\/test-js-libs\//, "", file)
		mode = "plain"
		next
	}
	/^\.test\/test-js-libs\/.* \(mangled\):$/ {
		mode = "mangled"
		next
	}
	$1 == "Reduced" && mode == "plain" {
		plainReduction = $5
		plainInputSize = $7
		plainOutputSize = $9
		next
	}
	$1 == "Reduced" && mode == "mangled" {
		printf "| `%s` | %s | %s | %s | %s | %s |\n", file, \
			plainInputSize, plainOutputSize, plainReduction, $9, $5
		mode = ""
		rows += 1
	}
	END {
		if (rows == 0) {
			exit 1
		}
	}
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
