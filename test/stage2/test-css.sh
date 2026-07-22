#!/bin/sh

set -eu

. test/lib/lib-assert.sh


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

input='a{animation:0ms 1ms 10ms 100ms 250ms 500ms 750MS 1000ms -1500ms;'
input="${input}transition-delay:000ms}"
expected='a{animation:0s 1ms 10ms .1s .25s .5s .75s 1s -1.5s;'
expected="${expected}transition-delay:0s}"
assert "$expected" "$input"

input='a[foo=1000Hz]{b:1000HZ 1000kHz 360deg 400grad 96px 72pt 12pc;'
input="${input}192dpi -192dpi 2.54cm 5.080cm 25.4mm}"
expected='a[foo=1000Hz]{b:1kHz 1MHz 1turn 1turn 1in 1in 2in;'
expected="${expected}2dppx -2dppx 1in 2in 1in}"
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

input='a{color:rgb(255,0,0);background:rgb(0, 0, 255);'
input="${input}border:rgb(170, 187, 204);outline:rgb(0, 0, 0);"
input="${input}fill:rgb(128,128,128);stroke:rgb(100%,0%,0%)}"
expected='a{color:red;background:#00f;border:#abc;outline:#000;'
expected="${expected}fill:gray;stroke:red}"
assert "$expected" "$input"

input='a{color:rgb(50%,0%,0%);background:rgb(255 0 0);'
input="${input}border:rgb(256,0,0);outline:rgba(255,0,0,1)}"
expected='a{color:rgb(50%,0%,0%);background:rgb(255 0 0);'
expected="${expected}border:rgb(256,0,0);outline:rgba(255,0,0,1)}"
assert "$expected" "$input"

testSuccess
