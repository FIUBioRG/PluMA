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
- **Behavior change:** a plugin failure now stops the pipeline (exit status 1)
  on every platform. Previously PluMA continued to later plugins unless the
  failing plugin had already written its output file, and Python exceptions
  were printed but otherwise ignored, so downstream plugins could silently
  run against missing output. Set `PLUMA_CONTINUE_ON_ERROR=1` for the previous
  behavior (a failed step that left no output is logged and the pipeline goes
  on), except that PluMA then exits with status 1, so scripts and CI see that
  steps failed. An unmet `dependency()` still ends the run, as it always did. Failures now include: Python exceptions, Java
  exceptions, an unknown/misspelled plugin name or one missing from the
  plugin path, a plugin whose language support is not compiled into this
  build, an unmet `dependency()`, a pipeline file that cannot be opened, a
  `Plugin` line missing its input or output file (previously run with the
  bare `Prefix` directory as that path), and a `Prefix`, `Pipeline` or
  `Kitty` line without its argument (previously the filesystem root, the
  previous pipeline, or the previous kitty's directory). A statement may
  still span several lines, but one cut short no longer takes the next
  statement's keyword (`Plugin`, `Prefix`, ...) as its missing argument.
  So a file name that is a statement keyword or starts with `#` must be on
  the same line as the word before it (`inputfile #raw.txt`): at the start
  of a continuation line it ends the statement, which is then incomplete.
  With parallel kitties (`LitterLaunch`), running kitties finish their current
  plugin and no new plugins start. The output file of a plugin that ran and
  failed is removed (as before); a step that failed before its plugin started
  (unknown name, language not enabled, runtime missing) keeps an existing
  output file.
- A failure outside any step, such as running out of memory reading a kitty's
  pipeline, stops the run in order (status 1) instead of aborting. The end of
  a failed run no longer waits for a stream another thread holds (a plugin
  blocked writing to a stalled pipe).
- `PluginManager::prefix()` is now per thread, so each parallel kitty sees
  its own prefix, and a nested `Pipeline` no longer changes the prefix seen by
  the plugins after it. After `LitterGather`, plugins again run under the
  pipeline's own `Prefix` (previously the last `Kitty`'s directory leaked into
  the steps after the parallel block). Threads a plugin starts itself see the most recently
  set prefix, which inside parallel kitties may be a sibling's: such plugins
  should read `prefix()` on their own thread and pass it to their workers.
  `PluginManager::myPrefix` is deprecated but still defined. A pointer
  `prefix()` returned stays valid for the whole run; a plugin that writes
  into it (e.g. `strtok`) sees its change on its own thread until the prefix
  is set again, as with the old buffer -- other threads, and later steps,
  get the real prefix. (Threads a plugin starts itself share one buffer per
  prefix: a write there reaches the other such threads holding the pointer
  until their next `prefix()` call, which returns an intact copy.)
- Without a `Prefix` line, a `Kitty` directory is now relative (`k1/`) instead
  of `/k1/` at the filesystem root, and kitty directories no longer accumulate
  (`/k1//k2/`). With a `Prefix`, kitty paths no longer contain a doubled
  separator (`out/k1/` instead of `out//k1/`).
- An unmet `dependency()` in a plugin's `input()` counts as "not started" (the
  existing output file is kept) in every language; later in the plugin it is
  an ordinary failure. A plugin that catches the error and carries on still
  fails its step (see below), as `dependency()` ended the run in earlier
  versions. In every "not started" case the existing output is kept
  only if the step left it untouched (same size, modification and change
  times; where the change time cannot tell -- unavailable, whole-second
  resolution, or changed within the last 2 seconds -- also the same content,
  for files up to 16 MB, so a same-size rewrite is caught even on file
  systems with coarse timestamps); one the plugin created, truncated or wrote
  before failing is removed. Only regular files are ever checked or removed:
  an output that is a device (`/dev/null`, `NUL`), FIFO or directory is left
  alone and never read.
- Restarting a run (`pluma <pipeline> <plugin>`) now also works when the
  restart point is in a nested `Pipeline` or a kitty: everything before it
  is skipped, including the earlier steps of nested pipelines (which used to
  run in full), and everything after it runs. Kitties run side by side, so
  in the `LitterLaunch` block that contains the point, every kitty holding
  it runs from the point (a fan-out of one pipeline resumes alike in each)
  and every other sibling kitty runs in full (a run that
  stopped on a failure may have cut them off); the kitties of blocks before
  the point are skipped, and the steps after `LitterGather` run. When the
  block's own steps hold the point too, kitties launched after those reach
  it run in full, as any step after the point. Which kitty
  holds the point is read from the pipeline files (again if one was
  rewritten during the run), so the outcome never depends on timing. Previously a point inside a
  kitty was never recognized, so every step after the parallel block was
  skipped.
- A pipeline file saved with a UTF-8 byte order mark (as some Windows
  editors do) is read correctly; the mark used to corrupt its first line.
- `PluginManager::dependency()` is no longer inline in `PluginManager.h`
  (plugins built without exception support compile again). An unmet
  dependency raised where the exception cannot propagate -- a `noexcept`
  function or destructor, a callback from C code -- ends the run in order
  (status 1, incomplete outputs removed) instead of aborting the process.
- A step's output is checked and, after a failure, removed by its absolute
  path as of the step's start, so a plugin that changes the current
  directory before failing cannot make PluMA remove a different file.
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
- PluMA no longer calls into the loaded OpenMP runtimes to detect parallel
  regions (that could initialize an unused second runtime, which Intel/LLVM
  libomp aborts on): an unmet `dependency()` inside an OpenMP region, which
  cannot propagate, ends the run through PluMA's terminate handler instead.
  Any other uncaught exception still goes to the runtime's own handler, so
  GCC/Clang still print its type and `what()` before aborting.
- A plugin that catches its own unmet `dependency()` (`catch (...)`,
  `except Exception`, `tryCatch`) and goes on still fails its step, as when
  `dependency()` ended the run with `exit(1)`.
- `pluma <pipeline> <plugin>` with a restart point that matches no step
  (e.g. misspelled) now reports it and exits with status 1, instead of
  skipping every step and exiting 0.
- `exit()` on a thread a plugin started itself, while steps are running on
  kitties, moves every output those steps changed to `<output>.incomplete`
  (or `.incomplete.1`, ... -- an earlier one is never replaced): which step
  the caller belongs to cannot be told -- the others were cut off, its own
  may be finished, and even a single running step may be another plugin's,
  the thread being left over from a step that already ended. An incomplete output that cannot be removed because a step
  still has it open (Windows) is emptied instead of left partial.
- An unmet `dependency()` outside a pipeline step (e.g. in a plugin's static
  initializer, before the log file is open) is also reported on stderr.
- Linux/macOS: a working directory that cannot be read (removed, no
  permission) no longer makes PluMA retry `getcwd` with ever larger buffers.
- `Plugin` lines with the keywords exactly swapped
  (`... outputfile a inputfile b`) stop the run with an error, instead of
  running the plugin with its files exchanged -- where a failure would then
  remove what is really its input. Other words in place of `Plugin`,
  `inputfile` and `outputfile` still run, as before, now with a warning.
- A pipeline file that is a directory (`pluma pipelines/`, or a `Pipeline`
  line naming one) is an error, as on Windows -- on Linux/macOS it read as
  an empty pipeline and the run ended successfully having done nothing. A
  read error partway through a pipeline file also fails the run. Pipelines
  read from a FIFO or `/dev/stdin` (`pluma <(make_pipeline)`) work, also with
  a UTF-8 byte order mark and when restarting from a step. A FIFO used more
  than once is opened at each use; only a restart, which has to scan the
  pipeline first, keeps what it read for the run.
- PluMA runs from a working directory whose name contains the path list
  separator (`;` on Windows, `:` on Linux/macOS); plugins there were not
  found. Java cannot use such a directory on its class path (logged).
- POSIX `scons -c` keeps `RPluMA.R`, which only swig can regenerate.
- `exit()` from a thread a plugin started ends the run as `exit()` while
  kitties run does (status 1, the running step's output moved to
  `<output>.incomplete`), instead of leaving a half-written output under its
  real name with status 0. `exit()` on the main thread is unchanged.
- `Prefix out/` no longer gives plugins `out//` paths.
- Docker: the image builds again on Alpine 3.23 (`libexecinfo` is no longer
  an Alpine package; `pythonds` is installed past PEP 668's guard on the
  system Python), and follows the 3.23 series for its security fixes.
- Windows: C++/CUDA plugins written on POSIX that include `<sys/time.h>`
  (`gettimeofday`, `struct timeval`) build: PluMA's plugin build provides it.
  A plugin that also uses winsock includes `<winsock2.h>` before
  `<sys/time.h>` (winsock defines `struct timeval` again otherwise).
- CUDA plugins that call `cudaThreadSynchronize()`, removed in CUDA 13,
  build: it is mapped to `cudaDeviceSynchronize()`, the same call.
- Windows: a pipeline file name with a drive but no root (`D:in.txt`, relative
  to drive D's current directory) is passed to the plugin as given; the
  `Prefix` is no longer put in front of it (which made an invalid path).
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
- The `PyPluMA`/`PerlPluMA`/`RPluMA` wrappers turn any C++ exception into a
  language error (not only `std::exception`), so none unwinds through the
  interpreter.
- POSIX: a `fork()`ed child (e.g. a `multiprocessing` worker) no longer
  inherits PluMA's prefix, log or dependency locks held by another thread
  at that moment.
- Plugins count as installed for `dependency()` after each plugin directory
  is loaded, as before: a plugin library that checks a dependency while it
  is loaded finds plugins of the directories searched before its own.
- Windows: Perl plugins load the Perl that PluMA was built against before
  any other `perl5XY.dll` of the same name on `PATH` (another Perl
  distribution may be built with incompatible options).
- The log's lock is never destroyed, so a thread a plugin left running can
  still log while the process exits (on POSIX the `PluginManager` object
  itself is destroyed at exit).
- Windows build: Perl's embedding flags are split respecting quotes, so a
  Strawberry Perl installed under a path with spaces builds.
- The prefix a plugin's own thread falls back to is kept in storage that is
  never destroyed, so such a thread still running at process exit reads a
  valid string.
- `PluginManager::log()` and `setLogFile()` are defined in pluma, like
  `dependency()`: plugins no longer compile the logger's internals in.
- The libpython version is read from the last include directory that
  `python3-config --includes` reports (e.g. `3.14t`), not from a
  `python3.14` in the install prefix.
- macOS: plugins are linked with `-undefined dynamic_lookup`, as they use
  `PluginManager` symbols defined in the `pluma` executable, and C++/CUDA
  plugins are built as `lib<Name>Plugin.dylib`, the name PluMA looks for
  there (they were built as `.so` and never found).
- A plugin that calls `exit()` itself while parallel kitties are running now
  ends PluMA at once with status 1 (C/C++ standard output flushed; the
  outputs that the steps it cuts off on other kitties had already changed
  are removed as incomplete -- one such a step had not touched yet is
  kept; the output of the step that called `exit()`, if it changed, is
  moved to `<output>.incomplete`, as it may be finished or not and the run
  fails; when `exit()` is called from a thread the plugin started itself,
  which step it belongs to is unknown, so the changed outputs are moved to
  `<output>.incomplete` instead, see above -- on Windows, an output its step
  still has open is copied there, then emptied; once `.incomplete` to
  `.incomplete.999` are all taken, a name with the process id is used; no further step starts once
  this has begun, and what the other kitties' steps changed while the
  languages' buffered output was being written is removed too), instead of running
  static destructors -- which tore down the shared `PluginManager` -- under
  the running kitties. Without running kitties, `exit(n)` behaves as before.
  Plugins should throw or return an error instead of calling `exit()`. The
  same applies to an unmet `dependency()` on a thread the plugin started
  itself (or inside an OpenMP parallel region), which ends PluMA with status
  1 as before, now also removing those unfinished outputs (its own included).
- PluMA creates the `logs` directory if it is missing (previously the log was
  then silently not written), and warns if the log file cannot be opened. Two
  runs started in the same second -- even at the same moment -- no longer
  overwrite each other's log (the name is claimed by creating the file
  exclusively; the next run gets a `-1` suffix, and once 999 are taken, a
  name with its process id -- never another run's log).
- `PLUMA_PLUGIN_PATH` entries are separated by `;` on Windows (`:` elsewhere),
  and empty entries are skipped instead of ending the search. Relative
  entries are resolved once at startup (on Windows including drive-relative
  ones such as `D:plugins`).
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
- Rust and Julia plugins fail their step like plugins of other languages:
  one that is not found, not compiled or fails to load counts as not
  started (an existing output file is kept); a Julia exception in a phase
  is a failure (an unmet `dependency()` in `input()` counts as not
  started).
- Julia plugins run on PluMA's main thread only: a Julia step on a parallel
  kitty fails with a clear message (the embedded Julia runtime may only be
  called from the thread that started it), instead of crashing.
