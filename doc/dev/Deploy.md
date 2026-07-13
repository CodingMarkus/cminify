Deployment
==========

`make deploy` creates release archives for Linux, Windows, and macOS. It builds a Docker image from `deploy/Dockerfile`, mounts the repository read/write at `/proj`, and writes all generated files to `.deploy/`.

Run `make deploy-clean` to remove `.deploy/`.

The image is based on `debian:trixie-slim`. It downloads the latest stable Zig compiler and contains Zip, XZ, LLVM tools, Make, and QEMU user-mode emulators. It deliberately does not contain Bun or Node.js.


Release targets
---------------

Build output is written to `.deploy/build/<target>`. Binary archives are written
to `.deploy/archive/bin/`, and developer archives are written to
`.deploy/archive/dev/`.

- `i686-linux-musl` and `i686-linux-gnu`

- `x86_64-linux-musl` and `x86_64-linux-gnu`

- `aarch64-linux-musl` and `aarch64-linux-gnu`

- `x86_64-windows-gnu` and `aarch64-windows-gnu`

- `x86_64-macos` and `aarch64-macos`

Linux musl releases are statically linked. Linux GNU releases are dynamically linked against glibc. Windows releases use the GNU Windows target supplied by Zig.

Binary-only archives use friendly names such as `WebMinCer_Linux-x64.tar.xz` and `WebMinCer_Windows-arm64.zip`. Static Linux builds add the `-static` suffix. Developer archives retain the technical target name and use the `-dev` suffix. Each target directory contains the release binary, its separate debugging symbols, a `build-info.txt` file, and an `obj/` directory with build object files. Linux and macOS archives use the `.tar.xz` format. Windows archives use the `.zip` format. Archives exclude `obj/`. Binary-only archives contain only the release binary. Developer archives include the release binary, debugging symbols, and build information.

ELF releases use `.debug` files and GNU debug links. macOS releases use a `.dSYM` bundle. Windows releases use both `.debug` files and PDB files.


Release verification
--------------------

After every static Linux build, the deployment process runs test stages 1 and 2 against the final stripped binary. QEMU runs i686 and ARM64 binaries, and also runs the x86_64 binary for a uniform release path.

Stages 3 and 4 are intentionally excluded. Stage 3 downloads external test data and requires Bun or Node.js. Stage 4 is randomized and expensive under emulation. Run `make test` in a development environment for the complete suite.

Windows and macOS releases are built and inspected in the Docker image, but are not executed there. Their runtime tests require their respective operating systems.
