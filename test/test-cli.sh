#!/bin/sh

set -eu

binaryPath=${WEBMINCER_BINARY:-./.build/webmincer}
tmpDir=$( mktemp -d "${TMPDIR:-/tmp}/webmincer-cli.XXXXXX" ) || exit 1
trap 'rm -rf "$tmpDir"' EXIT HUP INT TERM


assertStdout( )
{
	if ! "$binaryPath" "$@" > "$tmpDir/stdout" 2> "$tmpDir/stderr"
	then
		printf 'Command failed unexpectedly: %s %s\n' \
			"$binaryPath" "$*"
		exit 1
	fi

	_as_stderr=$( cat "$tmpDir/stderr" )
	if [ -n "$_as_stderr" ]
	then
		printf 'Unexpected standard error for: %s %s\n' \
			"$binaryPath" "$*"
		printf '%s\n' "$_as_stderr"
		exit 1
	fi
}


assertContains( )
{
	_ac_pattern=$1
	_ac_file=$2

	if ! grep -F -- "$_ac_pattern" "$_ac_file" > /dev/null
	then
		printf 'Expected to find "%s" in %s\n' "$_ac_pattern" \
			"$_ac_file"
		exit 1
	fi
}


assertNoTabsOrLongLines( )
{
	_an_file=$1

	if grep '	' "$_an_file" > /dev/null
	then
		printf 'Found a tab character in %s\n' "$_an_file"
		exit 1
	fi
	if ! awk 'length > 80 { exit 1 }' "$_an_file"
	then
		printf 'Found a line longer than 80 characters in %s\n' \
			"$_an_file"
		exit 1
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
	assertContains '--version' "$_ah_file"
	assertContains 'Notes:' "$_ah_file"
	assertContains "Use '-' as the input file" "$_ah_file"
	assertContains 'Currently only JavaScript code is' "$_ah_file"
	assertContains 'the format is js or the input is HTML' "$_ah_file"
}


assertStdout
	cp "$tmpDir/stdout" "$tmpDir/help-no-args"
assertHelpOutput "$tmpDir/help-no-args"

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

printf 'Passed all tests\n'
