#!/bin/sh

#
# Copyright 2026 CodingMarkus
#
# SPDX-License-Identifier: ISC

set -eu

[ -n "${__included_test_output_sh:-}" ] && return 0
__included_test_output_sh=1


case "${LC_ALL:-${LC_CTYPE:-${LANG:-}}}" in
*UTF-8*|*UTF8*|*utf-8*|*utf8*)
	testOutputIsUtf8=1
	;;
*)
	testOutputIsUtf8=0
	;;
esac

if [ -t 1 ] && [ "${TERM:-}" != "dumb" ]
then
	testOutputHasColors=1
else
	testOutputHasColors=0
fi


# $1 - Whether the status is successful.
#
# Prints a short, optionally coloured test status.
#
_testPrintStatus( )
{
	_tps_success=$1
	if [ "$_tps_success" = "1" ]
	then
		_tps_text=OK
		_tps_color=32
	else
		_tps_text=FAIL
		_tps_color=31
	fi
	if [ "$testOutputIsUtf8" = "1" ]
	then
		if [ "$_tps_success" = "1" ]
		then
			_tps_text='✓'
		else
			_tps_text='✗'
		fi
	fi
	if [ "$testOutputHasColors" = "1" ]
	then
		printf '\033[%sm%s\033[0m' "$_tps_color" "$_tps_text"
	else
		printf '%s' "$_tps_text"
	fi
}


# $1 - Successful test message.
#
# Prints the only output produced by a successful test script.
#
testSuccess( )
{
	printf '%s ' "$1"
	_testPrintStatus 1
	printf '\n'
}


# $1 - printf format string.
# $2... - printf format arguments.
#
# Prints a failure status and diagnostic output, then exits with failure.
#
testFail( )
{
	printf '\n'
	_testPrintStatus 0
	printf '\n'
	printf "$@"
	exit 1
}
