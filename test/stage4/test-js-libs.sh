#!/bin/sh

set -eu

. test/lib/lib-output.sh

binaryPath=${WEBMINCER_BINARY:-./.build/webmincer}
benchmark=0
printSizes=0
testDataDir=.test/stage4/test-js-libs

if command -v bun > /dev/null 2>&1
then
	jsSyntaxChecker=bun
	jsSyntaxCheckerName=Bun
elif command -v node > /dev/null 2>&1
then
	jsSyntaxChecker=node
	jsSyntaxCheckerName=Node.js
else
	testFail 'Could not find Bun or Node.js for JavaScript verification\n'
fi


_printUsage( )
{
	printf '%s\n' 'Usage: test-js-libs.sh [--bench] [--print-sizes]'
	printf '%s\n' ''
	printf '%s\n' 'Options:'
	printf '%s\n' '    --bench             Verify the documented size-reduction baseline.'
	printf '%s\n' '    --print-sizes       Print sizes without verifying the documented baseline.'
	printf '%s\n' '    -h, -help, --help   Show this help page.'
}

for argument in "$@"
do
	case "$argument" in
	--bench)
		benchmark=1
		;;
	--print-sizes)
		printSizes=1
		;;
	-h|-help|--help)
		_printUsage
		exit 0
		;;
	*)
		printf 'Unknown option: %s\n' "$argument" >&2
		_printUsage >&2
		exit 2
		;;
	esac
done


# $1 - URL to download.
#
# Downloads the file using wget or curl.
#
_downloadFile( )
{
	if command -v wget > /dev/null 2>&1
	then
		wget -q "$1"
	elif command -v curl > /dev/null 2>&1
	then
		curl -fsSLO "$1"
	else
		return 1
	fi
}


# Downloads all JavaScript library test files.
#
_download( )
{
	_d_testDir=$testDataDir
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


# $1 - JavaScript file to verify.
#
# Verifies the JavaScript syntax without executing the file.
#
_verifyJavaScriptSyntax( )
{
	_vjs_file=$1

	if [ "$jsSyntaxChecker" = "bun" ]
	then
		bun build "$_vjs_file" --no-bundle --outfile /dev/null
	else
		node -c "$_vjs_file"
	fi
}


# $1 - Input file.
# $2 - Minification mode.
# $3 - Output file.
#
# Minifies the file and verifies the resulting JavaScript syntax and size.
#
_testFile( )
{
	_tf_file=$1
	_tf_mode=$2
	_tf_outputFile=$3
	_tf_inputSize=$( wc -c < "$_tf_file" | tr -d ' ' ) || return 1

	if [ "$_tf_mode" = "mangled" ]
	then
		"$binaryPath" js "$_tf_file" --mangle \
			> "$_tf_outputFile" 2> "$_tf_outputFile.stderr" || {
			testFail 'Could not minify %s (%s):\n%s' "$_tf_file" \
				"$_tf_mode" "$(cat "$_tf_outputFile.stderr")"
		}
	else
		"$binaryPath" js "$_tf_file" > "$_tf_outputFile" \
			2> "$_tf_outputFile.stderr" || {
			testFail 'Could not minify %s (%s):\n%s' "$_tf_file" \
				"$_tf_mode" "$(cat "$_tf_outputFile.stderr")"
		}
	fi
	_tf_outputSize=$( wc -c < "$_tf_outputFile" | tr -d ' ' ) || return 1
	if [ "$_tf_outputSize" -gt "$_tf_inputSize" ]
	then
		testFail 'Minified output is larger than %s (%s): %s > %s bytes\n' \
			"$_tf_file" "$_tf_mode" "$_tf_outputSize" "$_tf_inputSize"
	fi
	if ! _verifyJavaScriptSyntax "$_tf_outputFile" > /dev/null \
		2> "$_tf_outputFile.stderr"
	then
		testFail 'Generated JavaScript does not verify for %s (%s):\n%s' \
			"$_tf_file" "$_tf_mode" "$(cat "$_tf_outputFile.stderr")"
	fi
	if [ "$reportBenchmark" = "0" ] || [ "$_tf_inputSize" = "0" ]
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
		return 1
	fi
}


# Downloads, minifies, and verifies the JavaScript library test files.
#
_main( )
{
	_main_tmpDir=$( mktemp -d \
		"${TMPDIR:-/tmp}/webmincer-js-libs.XXXXXX" ) || testFail \
		'Could not create a temporary JavaScript library test directory\n'
	trap 'rm -rf "$_main_tmpDir"' EXIT HUP INT TERM
	_main_tableFile=$_main_tmpDir/size-reduction-table.md
	if [ "$reportBenchmark" = "1" ]
	then
		{
			printf '%s%s\n' \
				'| Library | Original bytes | Minified bytes | Reduction |' \
				' Minified and mangled bytes | Reduction |'
			printf '%s\n' '| --- | ---: | ---: | ---: | ---: | ---: |'
		} > "$_main_tableFile" || return 1
	fi

	for file in "$testDataDir"/*.js
	do
		baseName=$( basename "$file" .js )
		_testFile "$file" "plain" "$_main_tmpDir/$baseName.min.js" \
			|| return 1
		plainOutputSize=$benchmarkOutputSize
		plainReduction=$benchmarkReduction
		_testFile "$file" "mangled" \
			"$_main_tmpDir/$baseName.mangled.js" || return 1
		if [ "$benchmarkOutputSize" -gt "$plainOutputSize" ]
		then
			testFail 'Mangled output is larger than unmangled output for %s: %s > %s bytes\n' \
				"$file" "$benchmarkOutputSize" \
				"$plainOutputSize"
		fi
		if [ "$reportBenchmark" = "1" ]
		then
			# shellcheck disable=SC2016
			printf '| `%s.js` | %s | %s | %.1f%% | %s | %.1f%% |\n' \
				"$baseName" "$benchmarkInputSize" "$plainOutputSize" \
				"$plainReduction" "$benchmarkOutputSize" \
				"$benchmarkReduction" >> "$_main_tableFile" || return 1
		fi
	done
	if [ "$benchmark" = "1" ]
	then
		if ! _assertContainsConsecutiveLines "$_main_tableFile" \
			doc/CurrentSizeReductionBaselines.md
		then
			testFail 'Size-reduction benchmark differs from the baseline:\n\n%s' \
				"$(cat "$_main_tableFile")"
		fi
		_main_generatedChart=$_main_tmpDir/JavaScriptLibrarySizeReduction.svg
		util/generate-size-reduction-chart.sh "$_main_generatedChart" \
			> /dev/null 2> "$_main_tmpDir/chart.stderr" || testFail \
			'Could not generate the size-reduction chart:\n%s' \
			"$(cat "$_main_tmpDir/chart.stderr")"
		if ! cmp -s "$_main_generatedChart" \
			doc/assets/JavaScriptLibrarySizeReduction.svg
		then
			testFail 'Size-reduction benchmark differs from the baseline:\n\n%s' \
				"$(cat "$_main_tableFile")"
		fi
	fi
	if [ "$printSizes" = "1" ]
	then
		cat "$_main_tableFile"
	fi
}


reportBenchmark=$benchmark
if [ "$printSizes" = "1" ]
then
	reportBenchmark=1
fi

_download || testFail 'Could not download the JavaScript library test files\n'
_main || testFail 'Could not run the JavaScript library test\n'
testSuccessWithLabel "$jsSyntaxCheckerName"
