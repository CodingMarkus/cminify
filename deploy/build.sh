#!/bin/sh

# Copyright 2026 CodingMarkus
#
# SPDX-License-Identifier: ISC

set -eu

deployDir=.deploy
archiveDir="$deployDir/archives"
archiveBinDir="$archiveDir/bin"
archiveDevDir="$archiveDir/dev"
buildDir="$deployDir/build"
licenseFile=$(pwd)/LICENSE
testScripts='test/stage1/test-build.sh test/stage1/test-cli.sh \
	test/stage2/test-css.sh test/stage2/test-html.sh \
	test/stage2/test-js-mangling.sh test/stage2/test-js.sh \
	test/stage2/test-json.sh test/stage2/test-xml.sh'
version=$(make --no-print-directory --silent version)

case "$version" in
	*.*.*.* | *..* | .* | *.)
		printf 'Invalid version: %s\n' "$version" >&2
		exit 1
		;;
	*.*.*)
		if [ "${version##*.}" -eq 0 ]; then
			version=${version%.*}
		fi
		;;
	*.*) ;;
	*) printf 'Invalid version: %s\n' "$version" >&2; exit 1 ;;
esac


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


addMacosSecurityWarning( )
{
	_targetDir=$1

	cp deploy/macOS_SecurityWarning.txt "$_targetDir/"
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
		i686-linux-gnu) _archiveName=Linux-x86 ;;
		i686-linux-musl) _archiveName=Linux-x86-static ;;
		x86_64-linux-gnu) _archiveName=Linux-x64 ;;
		x86_64-linux-musl) _archiveName=Linux-x64-static ;;
		aarch64-linux-gnu) _archiveName=Linux-arm64 ;;
		aarch64-linux-musl) _archiveName=Linux-arm64-static ;;
		x86_64-windows-gnu) _archiveName=Windows-x64 ;;
		aarch64-windows-gnu) _archiveName=Windows-arm64 ;;
		x86_64-macos) _archiveName=macOS-x64 ;;
		aarch64-macos) _archiveName=macOS-arm64 ;;
		*) echo "Unsupported archive target: $_target" >&2; exit 1 ;;
	esac

	if [ -n "$_devSuffix" ]; then
		_archiveContents=$_target
		_archiveName="webmincer_${version}_dev_${_target}"
		_archiveSubdir=dev
		cp "$licenseFile" "$buildDir/LICENSE"
	else
		_archiveName="WebMinCer_${version}_${_archiveName}"
		_archiveContents="$_target/webmincer"
		_archiveSubdir=bin
		case "$_target" in
			*-macos) _archiveFiles='webmincer macOS_SecurityWarning.txt LICENSE' ;;
			*) _archiveFiles='webmincer LICENSE' ;;
		esac
		cp "$licenseFile" "$buildDir/$_target/LICENSE"
	fi

	case "$_target" in
		*-windows-*)
			if [ -z "$_devSuffix" ]; then
				_archiveContents="$_target/webmincer.exe"
			fi
			if [ -n "$_devSuffix" ]; then
				(
					cd "$buildDir"
					rm -f "../archives/$_archiveSubdir/$_archiveName.zip"
					zip -9 --quiet --recurse-paths \
						"../archives/$_archiveSubdir/$_archiveName.zip" \
						"$_archiveContents" LICENSE \
						--exclude "$_target/obj/*"
				)
			else
				(
					cd "$buildDir/$_target"
					rm -f "../../archives/$_archiveSubdir/$_archiveName.zip"
					zip -9 --quiet \
						"../../archives/$_archiveSubdir/$_archiveName.zip" \
						webmincer.exe LICENSE
				)
			fi
			;;
		*)
			if [ -n "$_devSuffix" ]; then
				(
					cd "$buildDir"
					rm -f "../archives/$_archiveSubdir/$_archiveName.tar.xz"
					tar --create --xz \
						--file="../archives/$_archiveSubdir/$_archiveName.tar.xz" \
						--exclude="$_target/obj" "$_archiveContents" LICENSE
				)
			else
				(
					cd "$buildDir"
					rm -f "../archives/$_archiveSubdir/$_archiveName.tar.xz"
					tar --create --xz \
						--file="../archives/$_archiveSubdir/$_archiveName.tar.xz" \
						--directory="$_target" $_archiveFiles
				)
			fi
			;;
	esac

	case "$_target" in
		*-windows-*)
			verifyArchive="$archiveDir/$_archiveSubdir/$_archiveName.zip"
			if ! unzip -Z1 "$verifyArchive" |
				grep --fixed-strings --quiet --line-regexp LICENSE
			then
				printf 'Missing LICENSE in %s\n' "$verifyArchive" >&2
				exit 1
			fi
			;;
		*)
			verifyArchive="$archiveDir/$_archiveSubdir/$_archiveName.tar.xz"
			if ! tar --list --file="$verifyArchive" |
				grep --fixed-strings --quiet --line-regexp LICENSE
			then
				printf 'Missing LICENSE in %s\n' "$verifyArchive" >&2
				exit 1
			fi
			;;
		esac
}


verifyMacosArchive( )
{
	_archive=$1

	if ! tar --list --file="$_archive" |
		grep --fixed-strings --quiet macOS_SecurityWarning.txt
	then
		printf 'Missing macOS security warning in %s\n' "$_archive" >&2
		exit 1
	fi
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
addMacosSecurityWarning "$buildDir/x86_64-macos"
archiveTarget x86_64-macos
verifyMacosArchive "$archiveBinDir/WebMinCer_${version}_macOS-x64.tar.xz"
archiveTarget x86_64-macos -dev

buildTarget aarch64-macos aarch64-macos '' webmincer
extractMacosSymbols "$buildDir/aarch64-macos/webmincer"
addMacosSecurityWarning "$buildDir/aarch64-macos"
archiveTarget aarch64-macos
verifyMacosArchive "$archiveBinDir/WebMinCer_${version}_macOS-arm64.tar.xz"
archiveTarget aarch64-macos -dev
