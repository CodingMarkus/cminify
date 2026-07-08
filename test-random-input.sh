#!/usr/bin/env bash

export LC_CTYPE=C
export LC_ALL=C

output=./build/cminify
for format in css js xml html json; do
	for i in {0..1000}; do
		input=$(LC_ALL=C tr -dc 'A-Za-z0-9!"#$%&'\''()*+,-./:;<=>?@[\]^_`{|}~' \
			</dev/urandom | head -c 1000)
		"$output" "$format" <(printf '%s' "$input") > /dev/null 2>&1
		if [ $? -gt 1 ]; then
			printf '%s' "$input" > input-causing-crash.txt
			echo Crash to reproduce: cminify "$format" input-causing-crash.txt
			exit
		fi
	done
done

echo Passed the test - no crash occurred
