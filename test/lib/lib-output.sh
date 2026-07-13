#!/bin/sh

#
# Copyright 2026 CodingMarkus
#
# SPDX-License-Identifier: ISC

set -eu

[ -n "${__included_test_output_sh:-}" ] && return 0
__included_test_output_sh=1

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
		_tps_text=PASSED
		_tps_color=32
	else
		_tps_text=FAILED
		_tps_color=31
	fi
	if [ "$testOutputHasColors" = "1" ]
	then
		printf '\033[%sm%s\033[0m' "$_tps_color" "$_tps_text"
	else
		printf '%s' "$_tps_text"
	fi
}


# Prints the name of the calling test script.
#
# The name is the filename without its `.sh` extension.
#
_testPrintName( )
{
	_tpn_name=${0##*/}
	_tpn_name=${_tpn_name%.sh}
	printf '%s' "$_tpn_name"
}


# Prints the successful test status.
#
testSuccess( )
{
	_testPrintName
	printf ': '
	_testPrintStatus 1
	printf '\n'
}


# $1 - Test label.
#
# Prints the successful test status with a label.
#
testSuccessWithLabel( )
{
	_testPrintName
	printf ' (%s): ' "$1"
	_testPrintStatus 1
	printf '\n'
}


# $1 - Reason the test was skipped.
#
# Prints a skipped test status and its reason.
#
testSkip( )
{
	_testPrintName
	printf ': SKIPPED\n'
	printf '%s\n' "$1"
}


# $1 - printf format string.
# $2... - printf format arguments.
#
# Prints a failure status and diagnostic output, then exits with failure.
#
testFail( )
{
	_testPrintName
	printf ': '
	_testPrintStatus 0
	printf '\n\n'
	printf "$@"
	exit 1
}
