#!/bin/sh

set -eu

. test/lib/lib-assert.sh


# $1 - Expected minified output.
# $2 - Input to minify.
#
# Verifies JSON minification.
#
assert( )
{
	assertMinification "$1" "$2" json -
}

input=' { "false": false, "true": true } '
expected='{"false":false,"true":true}'
assert "$expected" "$input"

input=' false '
expected='false'
assert "$expected" "$input"

input='{"a":"\\u0041\\u000a\\u0022\\u005c\\u003c\\u00e4"}'
expected='{"a":"A\n\"\\\u003cä"}'
assert "$expected" "$input"

input='{"a":"\\ud83d\\ude00","b":"\\ud800"}'
expected='{"a":"😀","b":"\ud800"}'
assert "$expected" "$input"

testSuccess
