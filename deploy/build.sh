#!/bin/sh

# Copyright 2026 CodingMarkus
#
# SPDX-License-Identifier: ISC

set -eu

deployDir=.deploy
archiveDir="$deployDir/archive"
archiveBinDir="$archiveDir/bin"
archiveDevDir="$archiveDir/dev"
buildDir="$deployDir/build"
testScripts='test/stage1/test-build.sh test/stage1/test-cli.sh \
test/stage2/test-css.sh test/stage2/test-html.sh \
test/stage2/test-js-mangling.sh test/stage2/test-js.sh \
test/stage2/test-json.sh test/stage2/test-xml.sh'


buildTarget( )
{
	_target=$1
	_zigTarget=$2
	_linkFlags=$3
	_output=$4
	_targetDir="$buildDir/$_target"

	mkdir -p "$_targetDir"
	{
		printf 'Zig version: '
		zig version
		printf 'Zig executable SHA-256: '
		sha256sum "$(command -v zig)" | cut -d ' ' -f 1
	} > "$_targetDir/build-info.txt"
	make \
		BUILD_DIR="$_targetDir" \
		CC="zig cc -target $_zigTarget" \
		LDFLAGS="$_linkFlags" \
		OUTPUT="$_output" \
		build
}


extractLinuxSymbols( )
{
	_binary=$1
	_debugFile="$1.debug"

	llvm-objcopy --only-keep-debug "$_binary" "$_debugFile"
	llvm-strip --strip-debug "$_binary"
	llvm-objcopy --add-gnu-debuglink="$_debugFile" "$_binary"
}


extractMacosSymbols( )
{
	_binary=$1

	dsymutil --out="$_binary.dSYM" "$_binary"
	llvm-strip --strip-debug "$_binary"
}


extractWindowsSymbols( )
{
	_binary=$1
	_debugFile="$1.debug"

	llvm-objcopy --only-keep-debug "$_binary" "$_debugFile"
	llvm-strip --strip-debug "$_binary"
}


archiveTarget( )
{
	_target=$1
	_devSuffix=${2-}

	case "$_target" in
		i686-linux-gnu) _archiveName=WebMinCer_Linux-x86 ;;
		i686-linux-musl) _archiveName=WebMinCer_Linux-x86-static ;;
		x86_64-linux-gnu) _archiveName=WebMinCer_Linux-x64 ;;
		x86_64-linux-musl) _archiveName=WebMinCer_Linux-x64-static ;;
		aarch64-linux-gnu) _archiveName=WebMinCer_Linux-arm64 ;;
		aarch64-linux-musl) _archiveName=WebMinCer_Linux-arm64-static ;;
		x86_64-windows-gnu) _archiveName=WebMinCer_Windows-x64 ;;
		aarch64-windows-gnu) _archiveName=WebMinCer_Windows-arm64 ;;
		x86_64-macos) _archiveName=WebMinCer_macOS-x64 ;;
		aarch64-macos) _archiveName=WebMinCer_macOS-arm64 ;;
		*) echo "Unsupported archive target: $_target" >&2; exit 1 ;;
	esac

	if [ -n "$_devSuffix" ]; then
		_archiveContents=$_target
		_archiveName=webmincer$_devSuffix_$_target
		_archiveSubdir=dev
	else
		_archiveContents="$_target/webmincer"
		_archiveSubdir=bin
	fi

	case "$_target" in
		*-windows-*)
			if [ -z "$_devSuffix" ]; then
				_archiveContents="$_target/webmincer.exe"
			fi
			(
				cd "$buildDir"
				rm -f "../archive/$_archiveSubdir/$_archiveName.zip"
				zip -9 --quiet --recurse-paths \
					"../archive/$_archiveSubdir/$_archiveName.zip" \
					"$_archiveContents" \
					--exclude "$_target/obj/*"
			)
			;;
		*)
			(
				cd "$buildDir"
				rm -f "../archive/$_archiveSubdir/$_archiveName.tar.xz"
				tar --create --xz \
					--file="../archive/$_archiveSubdir/$_archiveName.tar.xz" \
					--exclude="$_target/obj" "$_archiveContents"
			)
			;;
	esac
}


testStaticLinuxTarget( )
{
	_target=$1
	_emulator=$2
	_binary="$buildDir/$_target/webmincer"
	_wrapper="$buildDir/$_target/test-webmincer"

	printf '%s\n' '#!/bin/sh' > "$_wrapper"
	printf 'exec %s "%s" "$@"\n' "$_emulator" "$_binary" >> "$_wrapper"
	chmod +x "$_wrapper"
	make \
		BUILD_DIR="$buildDir/$_target" \
		OUTPUT=webmincer \
		TEST_BINARY="./$_wrapper" \
		TEST_OBJECT_DIR="./$buildDir/$_target/obj" \
		TEST_SCRIPTS="$testScripts" \
		test
	rm "$_wrapper"
}


rm -rf "$deployDir"
mkdir -p "$archiveBinDir" "$archiveDevDir" "$buildDir"

buildTarget i686-linux-musl x86-linux-musl -static webmincer
extractLinuxSymbols "$buildDir/i686-linux-musl/webmincer"
testStaticLinuxTarget i686-linux-musl qemu-i386
archiveTarget i686-linux-musl
archiveTarget i686-linux-musl -dev

buildTarget i686-linux-gnu x86-linux-gnu '' webmincer
extractLinuxSymbols "$buildDir/i686-linux-gnu/webmincer"
archiveTarget i686-linux-gnu
archiveTarget i686-linux-gnu -dev

buildTarget x86_64-linux-musl x86_64-linux-musl -static webmincer
extractLinuxSymbols "$buildDir/x86_64-linux-musl/webmincer"
testStaticLinuxTarget x86_64-linux-musl qemu-x86_64
archiveTarget x86_64-linux-musl
archiveTarget x86_64-linux-musl -dev

buildTarget x86_64-linux-gnu x86_64-linux-gnu '' webmincer
extractLinuxSymbols "$buildDir/x86_64-linux-gnu/webmincer"
archiveTarget x86_64-linux-gnu
archiveTarget x86_64-linux-gnu -dev

buildTarget aarch64-linux-musl aarch64-linux-musl -static webmincer
extractLinuxSymbols "$buildDir/aarch64-linux-musl/webmincer"
testStaticLinuxTarget aarch64-linux-musl qemu-aarch64
archiveTarget aarch64-linux-musl
archiveTarget aarch64-linux-musl -dev

buildTarget aarch64-linux-gnu aarch64-linux-gnu '' webmincer
extractLinuxSymbols "$buildDir/aarch64-linux-gnu/webmincer"
archiveTarget aarch64-linux-gnu
archiveTarget aarch64-linux-gnu -dev

buildTarget x86_64-windows-gnu x86_64-windows-gnu '' webmincer.exe
extractWindowsSymbols "$buildDir/x86_64-windows-gnu/webmincer.exe"
archiveTarget x86_64-windows-gnu
archiveTarget x86_64-windows-gnu -dev

buildTarget aarch64-windows-gnu aarch64-windows-gnu '' webmincer.exe
extractWindowsSymbols "$buildDir/aarch64-windows-gnu/webmincer.exe"
archiveTarget aarch64-windows-gnu
archiveTarget aarch64-windows-gnu -dev

buildTarget x86_64-macos x86_64-macos '' webmincer
extractMacosSymbols "$buildDir/x86_64-macos/webmincer"
archiveTarget x86_64-macos
archiveTarget x86_64-macos -dev

buildTarget aarch64-macos aarch64-macos '' webmincer
extractMacosSymbols "$buildDir/aarch64-macos/webmincer"
archiveTarget aarch64-macos
archiveTarget aarch64-macos -dev
