#!/bin/sh

set -eu

. test/lib/lib-output.sh

binaryPath=${WEBMINCER_BINARY:-./.build/webmincer}
updateReadmeHelpScript=./util/update-readme-help-output.sh
tmpDir=$( mktemp -d "${TMPDIR:-/tmp}/webmincer-cli.XXXXXX" ) \
	|| testFail 'Could not create a temporary test directory\n'
trap 'rm -rf "$tmpDir"' EXIT HUP INT TERM


assertStdout( )
{
	if ! "$binaryPath" "$@" > "$tmpDir/stdout" 2> "$tmpDir/stderr"
	then
		testFail 'Command failed unexpectedly: %s %s\n' \
			"$binaryPath" "$*"
	fi

	_as_stderr=$( cat "$tmpDir/stderr" )
	if [ -n "$_as_stderr" ]
	then
		testFail 'Unexpected standard error for: %s %s\n%s\n' \
			"$binaryPath" "$*" "$_as_stderr"
	fi
}


assertContains( )
{
	_ac_pattern=$1
	_ac_file=$2

	if ! grep -F -- "$_ac_pattern" "$_ac_file" > /dev/null
	then
		testFail 'Expected to find "%s" in %s\n' "$_ac_pattern" \
			"$_ac_file"
	fi
}


assertNoTabsOrLongLines( )
{
	_an_file=$1

	if grep '	' "$_an_file" > /dev/null
	then
		testFail 'Found a tab character in %s\n' "$_an_file"
	fi
	if ! awk -f - "$_an_file" <<'AWK'
length > 80 { exit 1 }
AWK
	then
		testFail 'Found a line longer than 80 characters in %s\n' \
			"$_an_file"
	fi
}


assertHelpOutput( )
{
	_ah_file=$1

	assertNoTabsOrLongLines "$_ah_file"
	assertContains 'Usage:' "$_ah_file"
	assertContains 'webmincer <format> <input-file|-> [options]' "$_ah_file"
	assertContains 'Formats:' "$_ah_file"
	assertContains 'css' "$_ah_file"
	assertContains 'js' "$_ah_file"
	assertContains 'xml' "$_ah_file"
	assertContains 'html' "$_ah_file"
	assertContains 'json' "$_ah_file"
	assertContains 'Options:' "$_ah_file"
	assertContains '-h, -help, --help' "$_ah_file"
	assertContains '--benchmark' "$_ah_file"
	assertContains '--mangle' "$_ah_file"
	assertContains '--compact-ws' "$_ah_file"
	assertContains '--version' "$_ah_file"
	assertContains 'Notes:' "$_ah_file"
	assertContains "Use '-' as the input file" "$_ah_file"
	assertContains 'Currently only JavaScript code is' "$_ah_file"
	assertContains 'the format is js or the input is HTML' "$_ah_file"
}


assertReadmeContainsHelpOutput( )
{
	_archo_help_file=$1
	_archo_readme_file=$2

	if ! awk -f - "$_archo_help_file" "$_archo_readme_file" <<'AWK'
		NR == FNR {
			if ($0 !~ /^[[:space:]]*$/ || started) {
				lines[++lineCount] = $0
				if ($0 !~ /^[[:space:]]*$/) {
					lastContentLine = lineCount
				}
				started = 1
			}
			next
		}
		lineCount > 0 && $0 == lines[matchedLines + 1] {
			matchedLines += 1
			if (matchedLines == lastContentLine) {
				found = 1
			}
			next
		}
		$0 == lines[1] {
			matchedLines = 1
			next
		}
		{
			matchedLines = 0
		}
		END {
			exit(!found)
		}
AWK
	then
		testFail 'Expected README.md to contain the complete help output\n'
	fi
}


assertStdout
cp "$tmpDir/stdout" "$tmpDir/help-no-args"
assertHelpOutput "$tmpDir/help-no-args"
assertReadmeContainsHelpOutput "$tmpDir/help-no-args" README.md

cp README.md "$tmpDir/README.md"
awk -f - "$tmpDir/README.md" > "$tmpDir/README.updated" <<'AWK'
	!updated && /Show this help page\./ {
		sub(/Show this help page\./, "Show outdated help text.")
		updated = 1
	}
	{
		print
	}
	END {
		exit(!updated)
	}
AWK
mv "$tmpDir/README.updated" "$tmpDir/README.md"
WEBMINCER_BINARY="$binaryPath" "$updateReadmeHelpScript" \
	"$tmpDir/README.md"
assertReadmeContainsHelpOutput "$tmpDir/help-no-args" "$tmpDir/README.md"

assertStdout -h
	cp "$tmpDir/stdout" "$tmpDir/help-short"
assertHelpOutput "$tmpDir/help-short"

assertStdout -help
	cp "$tmpDir/stdout" "$tmpDir/help-single-dash"
assertHelpOutput "$tmpDir/help-single-dash"

assertStdout --help
	cp "$tmpDir/stdout" "$tmpDir/help-double-dash"
assertHelpOutput "$tmpDir/help-double-dash"

assertStdout --version
	cp "$tmpDir/stdout" "$tmpDir/version"
assertNoTabsOrLongLines "$tmpDir/version"
assertContains '1.0' "$tmpDir/version"

testSuccess 'Passed all tests'
