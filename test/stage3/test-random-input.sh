#!/bin/sh

set -eu

. test/lib/lib-output.sh

export LC_CTYPE=C
export LC_ALL=C

outputPath=${WEBMINCER_BINARY:-./.build/webmincer}
tmpDir=$( mktemp -d "${TMPDIR:-/tmp}/webmincer-random-input.XXXXXX" ) \
	|| testFail 'Could not create a temporary directory\n'
trap 'rm -rf "$tmpDir"' EXIT HUP INT TERM

sampleCount=1001
sampleLength=1000
randomInputPath="$tmpDir/random-input"

for format in css js xml html json
do
	LC_ALL=C tr -dc 'A-Za-z0-9!"#$%&'\''()*+,-./:;<=>?@[\]^_`{|}~' \
		< /dev/urandom | head -c $((sampleCount * sampleLength)) \
		> "$randomInputPath"
	split -a 4 -b "$sampleLength" "$randomInputPath" "$tmpDir/input-"
	for inputPath in "$tmpDir"/input-*
	do
		if "$outputPath" "$format" "$inputPath" > /dev/null 2>&1
		then
			:
		else
			exitStatus=$?
			if [ "$exitStatus" -gt 1 ]
			then
				cp "$inputPath" input-causing-crash.txt
				testFail \
					'Crash to reproduce: webmincer %s input-causing-crash.txt\n' \
					"$format"
			fi
		fi
	done
	rm -f "$tmpDir"/input-*
done

testSuccess
