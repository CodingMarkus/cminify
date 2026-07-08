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
	mkdir -p test-js-libs || return 1
	(
		cd test-js-libs || exit 1
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

	printf '%s (%s):\n   ' "$file" "$mode"
	if [ "$mode" = "mangled" ]; then
		build/cminify js "$file" --mangle-js-identifiers | node -c ||
			return 1
		build/cminify js --benchmark "$file" \
			--mangle-js-identifiers || return 1
	else
		build/cminify js "$file" | node -c || return 1
		build/cminify js --benchmark "$file" || return 1
	fi
}

_main()
{
	for file in test-js-libs/*.js; do
		_test_file "$file" "plain" || return 1
		_test_file "$file" "mangled" || return 1
	done
}

_download
_main
