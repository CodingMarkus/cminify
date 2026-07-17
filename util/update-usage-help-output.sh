#!/bin/sh

#
# Copyright 2026 CodingMarkus
#
# SPDX-License-Identifier: ISC

set -eu

scriptDirectory=$(CDPATH='' cd "$(dirname "$0")" && pwd)
projectDirectory=$(CDPATH='' cd "$scriptDirectory/.." && pwd)
binaryPath=${WEBMINCER_BINARY:-$projectDirectory/.build/webmincer}
usagePath=${1:-$projectDirectory/doc/Usage.md}
temporaryDirectory=$(mktemp -d \
	"${TMPDIR:-/tmp}/webmincer-usage-help.XXXXXX")
trimLeadingBlankLinesAwk=$temporaryDirectory/trim-leading-blank-lines.awk
helpOutput=$temporaryDirectory/help-output
updatedUsage=$temporaryDirectory/Usage.md

trap 'rm -rf "$temporaryDirectory"' EXIT HUP INT TERM

if [ ! -x "$binaryPath" ]
then
	printf 'Expected executable WebMinCer binary at %s\n' "$binaryPath" \
		>&2
	printf 'Build it first, for example with "make build".\n' >&2
	exit 1
fi

if [ ! -f "$usagePath" ]
then
	printf 'Usage file not found: %s\n' "$usagePath" >&2
	exit 1
fi

cat > "$trimLeadingBlankLinesAwk" <<'AWK'
	started || $0 !~ /^[[:space:]]*$/ {
		print
		started = 1
	}
AWK
"$binaryPath" | awk -f "$trimLeadingBlankLinesAwk" > "$helpOutput"

if ! awk -v helpFile="$helpOutput" -f - "$usagePath" \
	> "$updatedUsage" <<'AWK'
	BEGIN {
		while ((getline line < helpFile) > 0) {
			help[++helpLines] = line
		}
		close(helpFile)
	}
	skipOriginalBlock && $0 == "```" {
		print
		skipOriginalBlock = 0
		next
	}
	skipOriginalBlock {
		next
	}
	{
		print
		if ($0 == "Command-line reference") {
			inCommandLineReference = 1
			next
		}
		if (inCommandLineReference && $0 == "```") {
			for (line = 1; line <= helpLines; line += 1) {
				print help[line]
			}
			replaced = 1
			skipOriginalBlock = 1
			inCommandLineReference = 0
			next
		}
	}
	END {
		if (!replaced || skipOriginalBlock) {
			exit 1
		}
	}
AWK
then
	printf 'Could not locate the help code block in %s\n' "$usagePath" \
		>&2
	exit 1
fi

mv "$updatedUsage" "$usagePath"
