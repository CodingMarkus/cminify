#!/bin/sh

set -eu

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
	shift 2

	if ! _am_result=$( printf '%b' "$_am_input" | "$_am_binary" "$@" )
	then
		printf 'Crashed on:\n%s\n' "$_am_input"
		printf 'Standard output:\n%s\n' "$_am_result"
		exit 1
	fi

	if [ "$_am_expected" != "$_am_result" ]
	then
		printf 'Error: expected:\n%s\n' "$_am_expected"
		printf 'got:\n%s\n' "$_am_result"
		exit 1
	fi
}
