#!/usr/bin/env sh

export LC_CTYPE=C
export LC_ALL=C

output=./build/cminify
tmp_input="$(mktemp "${TMPDIR:-/tmp}/cminify-random-input.XXXXXX")" || exit 1
trap 'rm -f "$tmp_input"' EXIT HUP INT TERM

for format in css js xml html json; do
	i=0
	while [ "$i" -le 1000 ]; do
		input=$(LC_ALL=C tr -dc 'A-Za-z0-9!"#$%&'\''()*+,-./:;<=>?@[\]^_`{|}~' \
			</dev/urandom | head -c 1000)
		printf '%s' "$input" > "$tmp_input"
		"$output" "$format" "$tmp_input" > /dev/null 2>&1
		if [ $? -gt 1 ]; then
			printf '%s' "$input" > input-causing-crash.txt
			echo Crash to reproduce: cminify "$format" input-causing-crash.txt
			exit 1
		fi
		i=$((i + 1))
	done
done

echo Passed the test - no crash occurred
