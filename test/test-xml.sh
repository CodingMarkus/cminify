#!/bin/sh

set -eu

. test/lib_assert.sh


# $1 - Expected minified output.
# $2 - Input to minify.
#
# Verifies XML minification.
#
assert( )
{
	assertMinification "$1" "$2" xml -
}

input='<?xml version="1.0" encoding="iso-8859-1"?>'
expected='<?xml version="1.0" encoding="iso-8859-1"?>'
assert "$expected" "$input"

input='<script>"&#174;&#x2115;&#8475;&#x2126;"</script>'
expected='<script>"®ℕℛΩ"</script>'
assert "$expected" "$input"

input='<![CDATA[ OO <>& OO ]]>'
expected='<![CDATA[ OO <>& OO ]]>'
assert "$expected" "$input"

input='<script><![CDATA[ let a = "</script>" ]]></script>'
expected='<script>let a="&lt;/script&gt;"</script>'
assert "$expected" "$input"

input='<script>"<<<<<<"</script>'
expected='<script><![CDATA["<<<<<<"]]></script>'
assert "$expected" "$input"

input='<style> * { font-weight : bold ; } </style>'
expected='<style>*{font-weight:bold}</style>'
assert "$expected" "$input"

input='<script>;let  a  =  b;</script>'
expected='<script>let a=b</script>'
assert "$expected" "$input"

input='<xml></xml><xml></xmll>'
expected='<xml/><xml></xmll>'
assert "$expected" "$input"

input='<xml> <xml> </xml> <xml> </xml> </xml>'
expected='<xml><xml> </xml><xml> </xml></xml>'
assert "$expected" "$input"

input=' <xml a = " b " > <b>  </b><a /><a b="c"/> </xml>'
expected=' <xml a=" b "><b>  </b><a/><a b="c"/></xml>'
assert "$expected" "$input"

printf 'Passed all tests\n'
