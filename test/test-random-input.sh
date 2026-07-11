#!/bin/sh

set -eu

export LC_CTYPE=C
export LC_ALL=C

outputPath=${WEBMINCER_BINARY:-./.build/webmincer}
tmpInput=$( mktemp "${TMPDIR:-/tmp}/webmincer-random-input.XXXXXX" ) || exit 1
trap 'rm -f "$tmpInput"' EXIT HUP INT TERM

for format in css js xml html json
do
	i=0
	while [ "$i" -le 1000 ]
	do
		input=$( LC_ALL=C tr -dc 'A-Za-z0-9!"#$%&'\''()*+,-./:;<=>?@[\]^_`{|}~' \
			< /dev/urandom | head -c 1000 )
		printf '%s' "$input" > "$tmpInput"
		if "$outputPath" "$format" "$tmpInput" > /dev/null 2>&1
		then
			:
		else
			exitStatus=$?
			if [ "$exitStatus" -gt 1 ]
			then
				printf '%s' "$input" > input-causing-crash.txt
				printf 'Crash to reproduce: webmincer %s input-causing-crash.txt\n' \
					"$format"
				exit 1
			fi
		fi
		i=$((i + 1))
	done
done

printf 'Passed the test - no crash occurred\n'
