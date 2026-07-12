#!/bin/sh

set -eu

binaryPath=${WEBMINCER_BINARY:-./.build/webmincer}


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
		"$binaryPath" js "$_tf_file" --mangle \
			> "$_tf_outputFile" || return 1
	else
		"$binaryPath" js "$_tf_file" > "$_tf_outputFile" || return 1
	fi
	_tf_outputSize=$( wc -c < "$_tf_outputFile" | tr -d ' ' ) || return 1
	if [ "$_tf_inputSize" = "0" ]
	then
		_tf_reduction=0.0
	else
		_tf_reduction=$( awk -v outputSize="$_tf_outputSize" \
			-v inputSize="$_tf_inputSize" -f - <<'AWK'
BEGIN {
	print 100.0 - 100.0 * outputSize / inputSize
}
AWK
		) || return 1
	fi
	printf 'Reduced the size by %.1f%% from %s to %s bytes\n' \
		"$_tf_reduction" "$_tf_inputSize" "$_tf_outputSize"
	node -c "$_tf_outputFile" || return 1
	benchmarkInputSize=$_tf_inputSize
	benchmarkOutputSize=$_tf_outputSize
	benchmarkReduction=$_tf_reduction
}


# $1 - File containing the expected consecutive lines.
# $2 - File that must contain those lines.
#
_assertContainsConsecutiveLines( )
{
	_accl_expectedFile=$1
	_accl_actualFile=$2

	if ! awk -f - "$_accl_expectedFile" "$_accl_actualFile" <<'AWK'
		NR == FNR {
			lines[++lineCount] = $0
			next
		}
		lineCount > 0 && $0 == lines[matchedLines + 1] {
			matchedLines += 1
			if (matchedLines == lineCount) {
				found = 1
			}
			next
		}
		$0 == lines[1] {
			matchedLines = 1
			next
		}
		{
			matchedLines = 0
		}
		END {
			exit(!found)
		}
AWK
	then
		printf 'Expected %s to contain the current benchmark table\n' \
			"$_accl_actualFile"
		return 1
	fi
}


# Downloads, minifies, and verifies the JavaScript library test files.
#
_main( )
{
	_main_tmpDir=$( mktemp -d \
		"${TMPDIR:-/tmp}/webmincer-js-libs.XXXXXX" ) || return 1
	trap 'rm -rf "$_main_tmpDir"' EXIT HUP INT TERM
	_main_tableFile=$_main_tmpDir/size-reduction-table.md
	{
		printf '%s%s\n' \
			'| Library | Original bytes | Minified bytes | Reduction |' \
			' Minified and mangled bytes | Reduction |'
		printf '%s\n' '| --- | ---: | ---: | ---: | ---: | ---: |'
	} > "$_main_tableFile" || return 1

	for file in .test/test-js-libs/*.js
	do
		baseName=$( basename "$file" .js )
		_testFile "$file" "plain" "$_main_tmpDir/$baseName.min.js" \
			|| return 1
		plainOutputSize=$benchmarkOutputSize
		plainReduction=$benchmarkReduction
		_testFile "$file" "mangled" \
			"$_main_tmpDir/$baseName.mangled.js" || return 1
		printf '| `%s.js` | %s | %s | %.1f%% | %s | %.1f%% |\n' \
			"$baseName" "$benchmarkInputSize" "$plainOutputSize" \
			"$plainReduction" "$benchmarkOutputSize" \
			"$benchmarkReduction" >> "$_main_tableFile" || return 1
	done
	if [ "${WEBMINCER_SKIP_SIZE_REDUCTION_BASELINE_CHECK:-}" != "1" ]
	then
		_assertContainsConsecutiveLines "$_main_tableFile" \
			doc/CurrentSizeReductionBaselines.md || return 1
		_main_generatedChart=$_main_tmpDir/JavaScriptLibrarySizeReduction.svg
		util/generate-size-reduction-chart.sh "$_main_generatedChart" \
			|| return 1
		if ! cmp -s "$_main_generatedChart" \
			doc/assets/JavaScriptLibrarySizeReduction.svg
		then
			printf 'Expected the SVG chart to match the current benchmark table\n'
			return 1
		fi
	fi
}


_download
_main
