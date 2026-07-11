#!/bin/sh

set -eu

. test/lib_assert.sh


# $1 - Expected minified output.
# $2 - Input to minify.
#
# Verifies CSS minification.
#
assert( )
{
	assertMinification "$1" "$2" css -
}

input='/*! do not remove */'
expected='/*! do not remove */'
assert "$expected" "$input"

input='a /**/ + /*o/*o*/ a  #b { c : 1px/**//**/1px;/**/; ;;d:"/* "  3 ;; }'
expected='a+a #b{c:1px 1px;d:"/* " 3}'
assert "$expected" "$input"

input='@import url(/*o);'
expected='@import url(/*o);'
assert "$expected" "$input"

input='@import url(  "o"  );@import url(  o  );'
expected='@import url("o");@import url(o);'
assert "$expected" "$input"

input='a{b:c !important}'
expected='a{b:c!important}'
assert "$expected" "$input"

input='@media {a :hover{a :hover}}'
expected='@media{a :hover{a:hover}}'
assert "$expected" "$input"

input='@media all and (hover:hover){} @media (hover:hover){}'
expected='@media all and (hover:hover){}@media(hover:hover){}'
assert "$expected" "$input"

input='a [ padding = 0.1em 1em ] {}'
expected='a[padding=.1em 1em]{}'
assert "$expected" "$input"

input='@media ( width < 0.1em ) {}'
expected='@media(width<.1em){}'
assert "$expected" "$input"

input='@page :left { }'
expected='@page :left{}'
assert "$expected" "$input"

input='a\{b{}'
expected='a\{b{}'
assert "$expected" "$input"

input='a{color:#aabbcc;background:#AABBCCDD;border:#ff0000;'
input="${input}outline:#808000;fill:#f0f;stroke:#abcdefg;"
input="${input}stop-color:#aabbcc\\ d}"
expected='a{color:#abc;background:#abcd;border:red;outline:olive;'
expected="${expected}fill:#f0f;stroke:#abcdefg;stop-color:"
expected="${expected}#aabbcc\\ d}"
assert "$expected" "$input"

input='#aabbcc{color:#aabbcc}a{b:"#aabbcc";c:url(#aabbcc)}'
expected='#aabbcc{color:#abc}a{b:"#aabbcc";c:url(#aabbcc)}'
assert "$expected" "$input"

input='a{color:black;background:yellow;border:aqua 1px solid;'
input="${input}box-shadow:0 0 red;"
input="${input}font-family:black;animation:red}b{color:Grey}"
input="${input}c{color:#000000}"
input="${input}d{color:rebeccapurple}"
expected='a{color:#000;background:#ff0;border:#0ff 1px solid;'
expected="${expected}box-shadow:0 0 red;"
expected="${expected}font-family:black;animation:red}b{color:gray}"
expected="${expected}c{color:#000}"
expected="${expected}d{color:#639}"
assert "$expected" "$input"

printf 'Passed all tests\n'
