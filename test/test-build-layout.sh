#!/bin/sh

set -eu

. test/lib/lib-output.sh

binaryPath=${WEBMINCER_BINARY:-./.build/webmincer}
objectDir=${WEBMINCER_OBJECT_DIR:-./.build/objects}

if [ ! -x "$binaryPath" ]
then
	testFail 'Expected built binary at %s\n' "$binaryPath"
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
		testFail 'Expected object file at %s\n' "$objectFile"
	fi
done

if find ./src -name '*.o' -print | grep . > /dev/null
then
	testFail 'Unexpected object files found in ./src\n'
fi

testSuccess 'Passed the build layout test'
