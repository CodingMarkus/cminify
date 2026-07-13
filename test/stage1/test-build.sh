#!/bin/sh

set -eu

. test/lib/lib-output.sh

binaryPath=${WEBMINCER_BINARY:-./.build/webmincer}
objectDir=${WEBMINCER_OBJECT_DIR:-./.build/obj}

if [ ! -x "$binaryPath" ]
then
	testFail 'Expected built binary at %s\n' "$binaryPath"
fi

for sourceFile in ./src/*.c
do
	sourceName=${sourceFile##*/}
	objectFile=$objectDir/${sourceName%.c}.o
	if [ ! -f "$objectFile" ]
	then
		testFail 'Expected object file at %s\n' "$objectFile"
	fi
done

if find ./src -name '*.o' -print | grep . > /dev/null
then
	testFail 'Unexpected object files found in ./src\n'
fi

testSuccess
