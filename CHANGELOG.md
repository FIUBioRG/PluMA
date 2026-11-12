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

## Unreleased

- Native Windows build with MSVC via SCons (`SConscript.windows`): `pluma.exe`,
  `plugen.exe`, and every plugin language Linux supports: C++ plugins as DLLs,
  CUDA plugins (`--with-cuda`, nvcc), and embedded Python, Java, R (through
  R's C API, in `pluma_r.dll`; RPluMA as `RPluMA.dll`) and Perl (Strawberry
  Perl, through a runner built with its own g++ in `pluma_perl.dll`;
  PerlPluMA likewise). R, Perl, Java and Python are optional at run time:
  their plugins are skipped, with the reason logged, when the runtime is not
  installed. On Windows, the embedded Python's `sys.executable` is the
  installation's `python.exe` (not `pluma.exe`), so `multiprocessing` and
  `subprocess` work from plugins.
- Building with swig no longer rewrites the git-tracked `RPluMA_wrap.cxx`: the
  wrapper is generated into `obj/`, and the tracked copy is used only when
  swig is missing. Without swig, the `PyPluMA` and `PerlPluMA` helper modules
  (which have no committed wrapper) are skipped with a notice instead of
  failing the build.
- POSIX build: `python3-config` is taken from `/usr/share/python3.14` where
  that exists (as before) and otherwise from `PATH`; `pluma` links Perl and
  R, and builds `RPluMA.so` and Perl's embedding flags, only when they are
  enabled, so `--without-perl` / `--without-r` builds no longer need those
  installed. A plugin directory with its own `SConscript` no longer stops the
  build with a NameError (`toExport`): it is given `env` and `envPluginCuda`
  (on Windows too, with `--with-cuda`), and is built by that script alone;
  PluMA's headers are found from it (the include path is `#src`, not a
  `src` relative to the plugin's folder). `--without-python` builds no
  longer need Python's headers or libpython. `pluma` records R's and
  RInside's library directories in its RPATH, so it runs as built (before,
  `libRInside.so` had to be put on `LD_LIBRARY_PATH`): every library
  directory it links `libR`/`libRInside` from, including where pkg-config
  reports R (e.g. an R under `/opt`) and the `R_LIB_DIR` / `RINSIDE_LIB_DIR`
  overrides. The `R_INCLUDE_DIR` override
  now reaches the compiler (it was set on a misspelled variable), and the
  language sources are no longer compiled twice.
- Linux/macOS: a working directory that cannot be read (removed, no
  permission) no longer makes PluMA retry `getcwd` with ever larger buffers.
- POSIX `scons -c` keeps `RPluMA.R`, which only swig can regenerate.
- Docker: the image builds again on Alpine 3.23 (`libexecinfo` is no longer
  an Alpine package; `pythonds` is installed past PEP 668's guard on the
  system Python), and follows the 3.23 series for its security fixes.
- Windows: C++/CUDA plugins written on POSIX that include `<sys/time.h>`
  (`gettimeofday`, `struct timeval`) build: PluMA's plugin build provides it.
  A plugin that also uses winsock includes `<winsock2.h>` before
  `<sys/time.h>` (winsock defines `struct timeval` again otherwise).
- CUDA plugins that call `cudaThreadSynchronize()`, removed in CUDA 13,
  build: it is mapped to `cudaDeviceSynchronize()`, the same call.
- A plugin's own `SConscript` gets copies of `env` and `envPluginCuda`:
  what it adds (defines, flags, libraries) no longer reaches the `pluma`
  executable or the plugins built after it. `envPluginCuda` already has
  PluMA's include directory and `-std=c++14` in `NVCCFLAGS` when it gets it.
- Windows: `scons -c` also removes the R and Perl bridges (`pluma_r.dll`,
  `RPluMA.dll`, `pluma_perl.dll`, `PerlPluMA`) and CUDA plugin DLLs.
- `plugen` creates directories as before: for a C++ plugin only the
  plugin's own directory (`mkdir`) -- run where `../plugins` does not exist,
  it reports that instead of creating it -- and for `--lang=java` any
  missing parents too (`mkdir -p`).
- Windows: an explicit `--gpu-architecture` (also `sm_35`) is used as given;
  only when it is not given do CUDA plugins target this machine's GPU (or
  every major architecture).
- Windows: Perl plugins load the Perl that PluMA was built against before
  any other `perl5XY.dll` of the same name on `PATH` (another Perl
  distribution may be built with incompatible options).
- Windows build: Perl's embedding flags are split respecting quotes, so a
  Strawberry Perl installed under a path with spaces builds.
- The libpython version is read from the last include directory that
  `python3-config --includes` reports (e.g. `3.14t`), not from a
  `python3.14` in the install prefix.
- macOS: plugins are linked with `-undefined dynamic_lookup`, as they use
  `PluginManager` symbols defined in the `pluma` executable, and C++/CUDA
  plugins are built as `lib<Name>Plugin.dylib`, the name PluMA looks for
  there (they were built as `.so` and never found).
- `PLUMA_PLUGIN_PATH` entries are separated by `;` on Windows (`:` elsewhere).
- SCons build (POSIX): C++ plugins are built as `lib<Name>Plugin.so` again
  (they had become `liblib<Name>Plugin.so`, which PluMA never found), and the
  `PyPluMA`/`PerlPluMA`/`RPluMA` modules under their own names
  (`_PyPluMA.so`, not `lib_PyPluMA.so`). `--with-cuda` no longer stops the
  build with a `NameError`, and `--with-rust` / `--build-rust-plugins`
  build the Rust plugins (that step could not be reached).
- `src/platform.h` holds PluMA's portability helpers (namespace `pluma`). Its
  platform macros (`PLUMA_PLATFORM_WINDOWS`, `PLUMA_SHARED_LIB_EXT`,
  `PLUMA_PATH_SEPARATOR`, ...) are kept; the `pluma::platform` functions and
  the `glob()` emulation are removed (PluMA no longer uses them). PluGen's
  Python, R, Julia and Rust generators build on Windows too; `--with-rust`
  and `--with-julia` are ignored there with a warning.
