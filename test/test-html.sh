#!/bin/sh

set -eu

. test/lib_assert.sh


# $1 - Expected minified output.
# $2 - Input to minify.
#
# Verifies HTML minification.
#
assert( )
{
	_am_expected=$1
	_am_input=$2
	shift 2
	assertMinification "$_am_expected" "$_am_input" html - "$@"
}

input='<script>"<"+"/script>"</script>'
expected='<script>"<\/script>"</script>'
assert "$expected" "$input"

input='<script>3 < /script>/</script>'
expected='<script>3< /script>/</script>'
assert "$expected" "$input"

input='<script>a="</scri" + "pt>"</script>'
expected='<script>a="<\/script>"</script>'
assert "$expected" "$input"

input='  a  b  '
expected='  a  b  '
assert "$expected" "$input"

input='  a  b  '
expected=' a b '
assert "$expected" "$input" --compact-ws

input='<div>  text  <span>  more  </span>  </div>'
expected='<div>  text  <span>  more  </span>  </div>'
assert "$expected" "$input"

input='<div>  text  <span>  more  </span>  </div>'
expected='<div> text <span>more </span></div>'
assert "$expected" "$input" --compact-ws

input='<pre>  text  </pre>'
expected='<pre>  text  </pre>'
assert "$expected" "$input"

input='<pre>  text  </pre>'
expected='<pre> text </pre>'
assert "$expected" "$input" --compact-ws

input='<scrIpT TyPe=application/json&plus;ld> { "key" : true } </script>'
expected='<scrIpT TyPe=application/json&plus;ld>{"key":true}</script>'
assert "$expected" "$input"

input='<script type="text&sol;javascript"> { "key" : true } </script>'
expected='<script type=text&sol;javascript>{"key":!0}</script>'
assert "$expected" "$input"

input='<a onclick=" return false "></a>'
expected='<a onclick=return!1></a>'
assert "$expected" "$input"

input='<a onclick="&quot;a&quot; &amp;&amp; b"></a>'
expected='<a onclick="&quot;a&quot;&amp;&amp;b"></a>'
assert "$expected" "$input"

input='<a onclick="if (a)\n b()"></a>'
expected='<a onclick=if(a)b()></a>'
assert "$expected" "$input"

input='<body onload=" return false "></body>'
expected='<body onload=return!1></body>'
assert "$expected" "$input"

input='<a onfoo=" return false "></a>'
expected='<a onfoo=" return false "></a>'
assert "$expected" "$input"

input='<a href="javascript: alert(1) "></a>'
expected='<a href=javascript:alert(1)></a>'
assert "$expected" "$input"

input='<a href="javascript: a < b &amp;&amp; c&lt;d"></a>'
expected='<a href="javascript:a&lt;b&amp;&amp;c&lt;d"></a>'
assert "$expected" "$input"

input='<html prop=/>'
expected='<html prop=/>'
assert "$expected" "$input"

input='<html data-yes/>'
expected='<html data-yes>'
assert "$expected" "$input"

input='<html prop="abc"/>'
expected='<html prop=abc>'
assert "$expected" "$input"

input='<html></html>'
expected='<html></html>'
assert "$expected" "$input"

input='<html> <html> </html> <html> </html> </html>'
expected='<html> <html> </html> <html> </html> </html>'
assert "$expected" "$input"

input='<html> <html> </html> <html> </html> </html>'
expected='<html> <html></html> <html></html> </html>'
assert "$expected" "$input" --compact-ws

input='<html>  <!---->'
expected='<html>  '
assert "$expected" "$input"

input='<html></html>  '
expected='<html></html>'
assert "$expected" "$input"

printf 'Passed all tests\n'
