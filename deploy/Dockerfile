# Copyright 2026 CodingMarkus
#
# SPDX-License-Identifier: ISC

FROM debian:trixie-slim

ARG TARGETARCH

RUN apt-get update \
	&& apt-get install --yes --no-install-recommends \
		ca-certificates \
		curl \
		jq \
		llvm \
		make \
		qemu-user \
		zip \
		xz-utils \
	&& rm -rf /var/lib/apt/lists/*

RUN case "${TARGETARCH}" in \
	amd64) zigArch=x86_64 ;; \
	arm64) zigArch=aarch64 ;; \
	*) echo "Unsupported Docker architecture: ${TARGETARCH}" >&2; exit 1 ;; \
	esac \
	&& zigUrl=$(curl --fail --location --silent \
			https://ziglang.org/download/index.json \
		| jq --raw-output --arg arch "${zigArch}-linux" \
			'[to_entries[] | select(.key != "master") | .value] \
			| max_by(.date) | .[$arch].tarball') \
	&& zigFile=${zigUrl##*/} \
	&& curl --fail --location --output "/tmp/${zigFile}" "${zigUrl}" \
	&& tar --extract --file "/tmp/${zigFile}" --directory /opt \
	&& ln --symbolic "/opt/${zigFile%.tar.xz}/zig" \
		/usr/local/bin/zig \
	&& rm "/tmp/${zigFile}"

WORKDIR /proj
