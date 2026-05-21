# Changelog

## v2.1.0

- Added experimental Rust language support for plugins using `pluma-plugin-trait` crate
- New `--with-rust` build flag to enable Rust plugin compilation
- Added `Rust.h` and `Rust.cxx` language implementation files
- Added `RustIO.rs` module providing FFI bindings and utilities for Rust plugins
- Added `Cargo.toml` for `pluma-io` crate integration
- Included ATria example plugin demonstrating Rust plugin development with `atria-rs`
- Rust plugins compile to shared libraries (cdylib) and integrate via FFI
- PluGen now supports Rust plugin generation with `--lang=rust` option
- Added `RustPluginGenerator` class for generating Rust plugin scaffolding
- Added `--build-rust-plugins` build option to compile all Rust plugins in plugins directory
- Added `--rust-release` option (default: True) for release/debug builds
- Added `--rust-features` option to enable cargo features (e.g., `--rust-features=gpu,cuda`)
- Added `scons -c rust-plugins` target to clean only Rust plugin build artifacts

## v1.0.0

- Initial public introduction of PluMA to the public.
- Subsequent untagged commits are retroactively part of the v1.0.0 line.

## v2.0.0

- Reworking of project build system
- Moving from using `cpp` suffix for C++ files for `cxx`.
- Consolidation of source code into `src` subdirectory.
- Adding Dockerfile and enabling generation of PluMA in a docker image.
