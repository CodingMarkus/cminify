#!/bin/sh

set -eu

binaryPath=${WEBMINCER_BINARY:-./.build/webmincer}
objectDir=${WEBMINCER_OBJECT_DIR:-./.build/objects}

if [ ! -x "$binaryPath" ]
then
	printf 'Expected built binary at %s\n' "$binaryPath" >&2
	exit 1
fi

for objectFile in \
	"$objectDir"/js-mangler.o \
	"$objectDir"/minifier-common.o \
	"$objectDir"/minifier-css.o \
	"$objectDir"/minifier-js.o \
	"$objectDir"/minifier-json.o \
	"$objectDir"/minifier-xml-html.o \
	"$objectDir"/webmincer.o
do
	if [ ! -f "$objectFile" ]
	then
		printf 'Expected object file at %s\n' "$objectFile" >&2
		exit 1
	fi
done

if find ./src -name '*.o' -print | grep .
then
	printf 'Unexpected object files found in ./src\n' >&2
	exit 1
fi

printf 'Passed the build layout test\n'
