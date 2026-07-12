#!/bin/sh

set -eu

. test/lib/lib-output.sh

[ -n "${__included_test_assert_sh:-}" ] && return 0
__included_test_assert_sh=1


# $1 - Expected minified output.
# $2 - Input to minify.
# $3... - WebMinCer arguments.
#
# Exits with failure if minification crashes or produces unexpected output.
#
assertMinification( )
{
	_am_expected=$1
	_am_input=$2
	_am_binary=${WEBMINCER_BINARY:-./.build/webmincer}
	_am_stderrFile=$( mktemp "${TMPDIR:-/tmp}/webmincer-assert.XXXXXX" ) \
		|| testFail 'Could not create a temporary assertion file\n'
	shift 2

	if ! _am_result=$( printf '%b' "$_am_input" | "$_am_binary" "$@" \
		2> "$_am_stderrFile" )
	then
		_am_stderr=$( cat "$_am_stderrFile" )
		rm -f "$_am_stderrFile"
		testFail 'Crashed on:\n%s\nStandard output:\n%s\nStandard error:\n%s\n' \
			"$_am_input" "$_am_result" "$_am_stderr"
	fi
	_am_stderr=$( cat "$_am_stderrFile" )
	rm -f "$_am_stderrFile"
	if [ -n "$_am_stderr" ]
	then
		testFail 'Unexpected standard error:\n%s\n' "$_am_stderr"
	fi

	if [ "$_am_expected" != "$_am_result" ]
	then
		testFail 'Error: expected:\n%s\ngot:\n%s\n' \
			"$_am_expected" "$_am_result"
	fi
}
