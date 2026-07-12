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

printf 'Passed all tests\n'
