#!/bin/sh

set -eu


# $1 - URL to download.
#
# Downloads the file using wget or curl.
#
_downloadFile( )
{
	if command -v wget > /dev/null 2>&1
	then
		wget "$1"
	elif command -v curl > /dev/null 2>&1
	then
		curl -LO "$1"
	else
		printf '%s\n' "Error: neither wget nor curl is installed." >&2
		return 1
	fi
}


# Downloads all JavaScript library test files.
#
_download( )
{
	_d_testDir=.test/test-js-libs
	mkdir -p "$_d_testDir" || return 1

	(
		cd "$_d_testDir" || exit 1

		if [ ! -f react.development.js ]
		then
			_downloadFile \
				https://unpkg.com/react@17.0.2/cjs/react.development.js
		fi
		if [ ! -f typescript.js ]
		then
			_downloadFile \
				https://unpkg.com/typescript@5.2.2/lib/typescript.js
		fi
		if [ ! -f vue.js ]
		then
			_downloadFile https://unpkg.com/vue@2.6.12/dist/vue.js
		fi
		if [ ! -f jquery.js ]
		then
			_downloadFile https://unpkg.com/jquery@3.5.1/dist/jquery.js
		fi
		if [ ! -f antd.js ]
		then
			_downloadFile https://unpkg.com/antd@4.16.1/dist/antd.js
		fi
		if [ ! -f echarts.js ]
		then
			_downloadFile https://unpkg.com/echarts@5.1.1/dist/echarts.js
		fi
		if [ ! -f victory.js ]
		then
			_downloadFile https://unpkg.com/victory@35.8.4/dist/victory.js
		fi
		if [ ! -f three.js ]
		then
			_downloadFile https://unpkg.com/three@0.124.0/build/three.js
		fi
		if [ ! -f bundle.min.js ]
		then
			_downloadFile https://unpkg.com/terser@5.26.0/dist/bundle.min.js
		fi
		if [ ! -f d3.js ]
		then
			_downloadFile https://unpkg.com/d3@6.3.1/dist/d3.js
		fi
		if [ ! -f lodash.js ]
		then
			_downloadFile https://unpkg.com/lodash@4.17.21/lodash.js
		fi
		if [ ! -f moment.js ]
		then
			_downloadFile https://unpkg.com/moment@2.29.1/moment.js
		fi
	) || return 1
}


# $1 - Input file.
# $2 - Minification mode.
# $3 - Output file.
#
# Minifies the file and verifies the resulting JavaScript syntax.
#
_testFile( )
{
	_tf_file=$1
	_tf_mode=$2
	_tf_outputFile=$3
	_tf_inputSize=$( wc -c < "$_tf_file" | tr -d ' ' ) || return 1

	printf '%s (%s):\n   ' "$_tf_file" "$_tf_mode"
	if [ "$_tf_mode" = "mangled" ]
	then
		.build/webmincer js "$_tf_file" --mangle-js-identifiers \
			> "$_tf_outputFile" || return 1
	else
		.build/webmincer js "$_tf_file" > "$_tf_outputFile" || return 1
	fi
	_tf_outputSize=$( wc -c < "$_tf_outputFile" | tr -d ' ' ) || return 1
	if [ "$_tf_inputSize" = "0" ]
	then
		_tf_reduction=0.0
	else
		_tf_reduction=$( \
			awk "BEGIN { print 100.0 - 100.0 * $_tf_outputSize / $_tf_inputSize }"
		) || return 1
	fi
	printf 'Reduced the size by %.1f%% from %s to %s bytes\n' \
		"$_tf_reduction" "$_tf_inputSize" "$_tf_outputSize"
	node -c "$_tf_outputFile" || return 1
}


# Downloads, minifies, and verifies the JavaScript library test files.
#
_main( )
{
	_main_tmpDir=$( mktemp -d \
		"${TMPDIR:-/tmp}/webmincer-js-libs.XXXXXX" ) || return 1
	trap 'rm -rf "$_main_tmpDir"' EXIT HUP INT TERM

	for file in .test/test-js-libs/*.js
	do
		baseName=$( basename "$file" .js )
		_testFile "$file" "plain" "$_main_tmpDir/$baseName.min.js" \
			|| return 1
		_testFile "$file" "mangled" \
			"$_main_tmpDir/$baseName.mangled.js" || return 1
	done
}


_download
_main
