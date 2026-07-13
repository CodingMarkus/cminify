#!/bin/sh

set -eu

. test/lib/lib-output.sh

export LC_CTYPE=C
export LC_ALL=C

outputPath=${WEBMINCER_BINARY:-./.build/webmincer}
tmpDir=$( mktemp -d "${TMPDIR:-/tmp}/webmincer-random-input.XXXXXX" ) \
	|| testFail 'Could not create a temporary directory\n'
trap 'rm -rf "$tmpDir"' EXIT HUP INT TERM

sampleCount=1000
sampleLength=1000

runFormat( )
{
	_rfi_format=$1
	_rfi_dir="$tmpDir/$_rfi_format"
	_rfi_randomInputPath="$_rfi_dir/random-input"
	mkdir "$_rfi_dir"
	LC_ALL=C tr -dc 'A-Za-z0-9!"#$%&'\''()*+,-./:;<=>?@[\]^_`{|}~' \
		< /dev/urandom | head -c $((sampleCount * sampleLength)) \
		> "$_rfi_randomInputPath"
	split -a 4 -b "$sampleLength" "$_rfi_randomInputPath" \
		"$_rfi_dir/input-"
	for _rfi_inputPath in "$_rfi_dir"/input-*
	do
		if "$outputPath" "$_rfi_format" "$_rfi_inputPath" > /dev/null 2>&1
		then
			:
		else
			exitStatus=$?
			if [ "$exitStatus" -gt 1 ]
			then
				cp "$_rfi_inputPath" \
					"input-causing-crash-$_rfi_format.txt"
				return 1
			fi
		fi
	done
	return 0
}

workerPids=
for format in css js xml html json
do
	(
		trap - EXIT HUP INT TERM
		runFormat "$format"
	) &
	workerPids="$workerPids $!"
done

testFailed=0
for workerPid in $workerPids
do
	if ! wait "$workerPid"
	then
		testFailed=1
	fi
done
if [ "$testFailed" -ne 0 ]
then
	testFail \
		'A random input caused a crash. See input-causing-crash-*.txt.\n'
fi

testSuccess
