#!/usr/bin/env sh

_download_file()
{
	if command -v wget > /dev/null 2>&1; then
		wget "$1"
	elif command -v curl > /dev/null 2>&1; then
		curl -LO "$1"
	else
		echo 'Error: neither wget nor curl is installed.' >&2
		return 1
	fi
}

_download()
{
	test_dir=.test/test-js-libs
	mkdir -p "$test_dir" || return 1
	(
		cd "$test_dir" || exit 1
		if [ ! -f react.development.js ]; then
			_download_file \
				https://unpkg.com/react@17.0.2/cjs/react.development.js
		fi
		if [ ! -f typescript.js ]; then
			_download_file \
				https://unpkg.com/typescript@5.2.2/lib/typescript.js
		fi
		if [ ! -f vue.js ]; then
			_download_file \
				https://unpkg.com/vue@2.6.12/dist/vue.js
		fi
		if [ ! -f jquery.js ]; then
			_download_file \
				https://unpkg.com/jquery@3.5.1/dist/jquery.js
		fi
		if [ ! -f antd.js ]; then
			_download_file \
				https://unpkg.com/antd@4.16.1/dist/antd.js
		fi
		if [ ! -f echarts.js ]; then
			_download_file \
				https://unpkg.com/echarts@5.1.1/dist/echarts.js
		fi
		if [ ! -f victory.js ]; then
			_download_file \
				https://unpkg.com/victory@35.8.4/dist/victory.js
		fi
		if [ ! -f three.js ]; then
			_download_file \
				https://unpkg.com/three@0.124.0/build/three.js
		fi
		if [ ! -f bundle.min.js ]; then
			_download_file \
				https://unpkg.com/terser@5.26.0/dist/bundle.min.js
		fi
		if [ ! -f d3.js ]; then
			_download_file \
				https://unpkg.com/d3@6.3.1/dist/d3.js
		fi
		if [ ! -f lodash.js ]; then
			_download_file \
				https://unpkg.com/lodash@4.17.21/lodash.js
		fi
		if [ ! -f moment.js ]; then
			_download_file \
				https://unpkg.com/moment@2.29.1/moment.js
		fi
	) || return 1
}

_test_file()
{
	file="$1"
	mode="$2"
	output_file="$3"
	input_size="$(wc -c < "$file" | tr -d ' ')" || return 1

	printf '%s (%s):\n   ' "$file" "$mode"
	if [ "$mode" = "mangled" ]; then
		.build/webmincer js "$file" --mangle-js-identifiers > "$output_file" ||
			return 1
	else
		.build/webmincer js "$file" > "$output_file" || return 1
	fi
	output_size="$(wc -c < "$output_file" | tr -d ' ')" || return 1
	if [ "$input_size" = "0" ]; then
		reduction=0.0
	else
		reduction="$(
			awk "BEGIN { print 100.0 - 100.0 * $output_size / $input_size }"
		)" || return 1
	fi
	printf 'Reduced the size by %.1f%% from %s to %s bytes\n' \
		"$reduction" "$input_size" "$output_size"
	node -c "$output_file" || return 1
}

_main()
{
	tmp_dir="$(mktemp -d "${TMPDIR:-/tmp}/cminify-js-libs.XXXXXX")" || return 1
	trap 'rm -rf "$tmp_dir"' EXIT HUP INT TERM

	for file in .test/test-js-libs/*.js; do
		base_name="$(basename "$file" .js)"
		_test_file "$file" "plain" "$tmp_dir/$base_name.min.js" ||
			return 1
		_test_file "$file" "mangled" "$tmp_dir/$base_name.mangled.js" ||
			return 1
	done
}

_download
_main
