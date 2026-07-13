#!/bin/sh

set -eu

. test/lib/lib-output.sh

binaryPath=${WEBMINCER_BINARY:-./.build/webmincer}
benchmark=0
printSizes=0
testDataDir=.test/stage3/test-js-libs
libraryVersions='react=19.2.7 typescript=6.0.3 vue=3.5.39 jquery=4.0.0'
libraryVersions="$libraryVersions ant-design=6.5.1 echarts=6.1.0"
libraryVersions="$libraryVersions victory=37.3.6 three=0.185.1"
libraryVersions="$libraryVersions terser=5.49.0 d3=7.9.0"
libraryVersions="$libraryVersions lodash=4.18.1 moment=2.30.1"

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

if command -v bun > /dev/null 2>&1
then
	jsSyntaxChecker=bun
	jsSyntaxCheckerName=Bun
elif command -v node > /dev/null 2>&1
then
	jsSyntaxChecker=node
	jsSyntaxCheckerName=Node.js
else
	testSkip 'Could not find Bun or Node.js for JavaScript verification.'
	exit 0
fi

if ! command -v wget > /dev/null 2>&1 && \
	! command -v curl > /dev/null 2>&1
then
	testSkip 'Could not find wget or curl to download JavaScript libraries.'
	exit 0
fi


# $1 - URL to download.
# $2 - Destination filename, optional.
#
# Downloads the file using wget or curl.
#
_downloadFile( )
{
	_df_url=$1
	_df_filename=${2:-}

	if command -v wget > /dev/null 2>&1
	then
		if [ -n "$_df_filename" ]
		then
			wget -q -O "$_df_filename" "$_df_url"
		else
			wget -q "$_df_url"
		fi
	elif command -v curl > /dev/null 2>&1
	then
		if [ -n "$_df_filename" ]
		then
			curl -fsSL -o "$_df_filename" "$_df_url"
		else
			curl -fsSLO "$_df_url"
		fi
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
	rm -f "$_d_testDir/antd.js"

	(
		cd "$_d_testDir" || exit 1
		if [ ! -f .versions ] || [ "$(cat .versions)" != "$libraryVersions" ]
		then
			rm -f -- *.js
			_downloadedNewVersions=1
		else
			_downloadedNewVersions=0
		fi

		if [ ! -f react.development.js ]
		then
			_downloadFile \
				https://unpkg.com/react@19.2.7/cjs/react.development.js
		fi
		if [ ! -f typescript.js ]
		then
			_downloadFile \
				https://unpkg.com/typescript@6.0.3/lib/typescript.js
		fi
		if [ ! -f vue.js ]
		then
			_downloadFile https://unpkg.com/vue@3.5.39/dist/vue.global.js \
				vue.js
		fi
		if [ ! -f jquery.js ]
		then
			_downloadFile https://unpkg.com/jquery@4.0.0/dist/jquery.js
		fi
		if [ ! -f ant-design.js ]
		then
			_downloadFile https://unpkg.com/antd@6.5.1/dist/antd.js \
				ant-design.js
		fi
		if [ ! -f echarts.js ]
		then
			_downloadFile https://unpkg.com/echarts@6.1.0/dist/echarts.js
		fi
		if [ ! -f victory.js ]
		then
			_downloadFile https://unpkg.com/victory@37.3.6/dist/victory.js
		fi
		if [ ! -f three.js ]
		then
			_downloadFile https://unpkg.com/three@0.185.1/build/three.module.js \
				three.js
		fi
		if [ ! -f bundle.min.js ]
		then
			_downloadFile https://unpkg.com/terser@5.49.0/dist/bundle.min.js
		fi
		if [ ! -f d3.js ]
		then
			_downloadFile https://unpkg.com/d3@7.9.0/dist/d3.js
		fi
		if [ ! -f lodash.js ]
		then
			_downloadFile https://unpkg.com/lodash@4.18.1/lodash.js
		fi
		if [ ! -f moment.js ]
		then
			_downloadFile https://unpkg.com/moment@2.30.1/moment.js
		fi
		if [ "$_downloadedNewVersions" = "1" ]
		then
			printf '%s\n' "$libraryVersions" > .versions
		fi
	) || return 1
}


# $1 - JavaScript library filename without its `.js` extension.
#
# Prints the library's display name.
#
_libraryName( )
{
	case "$1" in
	ant-design) printf '%s' 'Ant Design' ;;
	bundle.min) printf '%s' 'Terser' ;;
	d3) printf '%s' 'D3' ;;
	echarts) printf '%s' 'Apache ECharts' ;;
	jquery) printf '%s' 'jQuery' ;;
	lodash) printf '%s' 'Lodash' ;;
	moment) printf '%s' 'Moment.js' ;;
	react.development) printf '%s' 'React' ;;
	three) printf '%s' 'Three.js' ;;
	typescript) printf '%s' 'TypeScript' ;;
	victory) printf '%s' 'Victory' ;;
	vue) printf '%s' 'Vue.js' ;;
	*) return 1 ;;
	esac
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
	_main_baselineTableFile=$_main_tmpDir/size-reduction-baseline-table.md
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
		libraryName=$( _libraryName "$baseName" ) || return 1
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
			printf '| `%s (%s.js)` | %s | %s | %.1f%% | %s | %.1f%% |\n' \
				"$libraryName" "$baseName" "$benchmarkInputSize" \
				"$plainOutputSize" \
				"$plainReduction" "$benchmarkOutputSize" \
				"$benchmarkReduction" >> "$_main_tableFile" || return 1
		fi
	done
	if [ "$benchmark" = "1" ]
	then
		awk '{ sub(/ \([^)]*\)` \|/, "` |"); print }' \
			"$_main_tableFile" > "$_main_baselineTableFile" || return 1
		if ! _assertContainsConsecutiveLines "$_main_baselineTableFile" \
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
