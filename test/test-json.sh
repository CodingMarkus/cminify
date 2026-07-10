#!/usr/bin/env sh

assert()
{
    result="$(printf '%b' "$2" | ./.build/webmincer json -)"
	if [ "$?" != "0" ]; then
		echo Crashed on:
		echo "$2"
		echo Standard output:
		echo "$result"
		exit 1
	elif [ "$1" != "$result" ]; then
		echo 'Error: expected:'
		echo "$1"
		echo got:
		echo "$result"
		exit 1
	fi
}

input=' { "false": false, "true": true } '
expected='{"false":false,"true":true}'
assert "$expected" "$input"

echo 'Passed all tests'
