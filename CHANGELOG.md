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
  their plugins fail with the reason when the runtime is not installed. On
  Windows, the embedded
  Python's `sys.executable` is the installation's `python.exe` (not
  `pluma.exe`; `python3.Xt.exe` for a free-threaded build, where present),
  so `multiprocessing` and `subprocess` work from plugins.
  `PyPluMA` calls are available only in the PluMA process itself: in a
  `multiprocessing` worker on Windows they raise `RuntimeError`. (On POSIX,
  as before, only forked workers can import `PyPluMA`; spawned/forkserver
  workers -- the default on macOS and on Linux from Python 3.14 -- cannot.)
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
- A Java plugin calling `System.exit()` still ends the whole run with its
  status (a JVM cannot confine it), but PluMA now logs and prints that it did,
  so later steps are not skipped silently.
- A failure outside any step, such as running out of memory reading a kitty's
  pipeline, stops the run in order (status 1) instead of aborting. The end of
  a failed run no longer waits for a stream another thread holds (a plugin
  blocked writing to a stalled pipe).
- A Python plugin calling `sys.exit()` / `sys.exit(0)` now ends just that
  plugin successfully (its remaining phases are skipped) instead of ending
  PluMA; a non-zero `sys.exit(n)` is a plugin failure.
- Plugin precedence is unchanged (`./plugins` first, then `PLUMA_PLUGIN_PATH`;
  the last registration picks the language; a C/C++ plugin is the last library
  loaded; a script plugin's file is the first one in the search path), and is
  now reported: PluMA prints a note when a plugin found later replaces or is
  ignored in favor of another. New: a language compiled out of the build never
  replaces one that can run.
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
- After `languageUnload("Python")`, a later Python step starts the interpreter
  again, as before (logged, since some C extension modules such as numpy cannot
  be imported again in a re-initialized interpreter). Java (one JVM per
  process) and R (embedded R cannot be restarted) are not restarted; their
  later steps fail with a clear message.
- The embedded Python interpreter starts on first use, as before (not at all
  for pipelines without Python steps), installs no SIGINT handler (Ctrl-C
  keeps ending PluMA), and supports Python plugins on parallel kitties.
  C/C++ plugins on PluMA's main thread that use the Python C API still run
  holding the GIL while Python is running, as before, so they may call the
  C API directly -- also during a parallel block, where Python steps on the
  kitties then wait for them; other C/C++ plugins no longer hold it (see the
  GIL entry below for how use of the C API is detected). On kitty threads,
  C/C++ plugins take it with `PyGILState_Ensure()`/`PyGILState_Release()`.
- A Perl plugin calling `exit()` no longer terminates PluMA: `exit(0)` ends the
  plugin successfully, any other status is a plugin failure.
- `run()` and `output()` are optional for Python, Java and R plugins (as they
  are for C++ plugins via the `Plugin` base class); a missing one is logged and
  skipped. A missing `input()` is a failure that keeps existing output. An R
  plugin no longer inherits the previous R plugin's `input`/`run`/`output`
  functions from R's shared global environment.
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
- An R plugin calling `quit()`/`q()` now ends only that plugin, as Python's
  `sys.exit()` and Perl's `exit()` do: status 0 ends it successfully (its
  remaining phases are skipped), any other status is a failure. Previously
  it ended PluMA itself, skipping the rest of the pipeline. As with R's own
  `quit()`, a `try()` or `tryCatch(error = ...)` around it does not catch it.
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
- POSIX: once Python has started, SIGPIPE and SIGXFSZ are ignored, as
  earlier versions' `Py_Initialize()` did on the first Python step (PluMA
  now starts Python with `Py_InitializeEx(0)`, which installs no SIGINT
  handler, so Ctrl-C keeps ending PluMA): a Python plugin writing to a
  closed pipe gets `BrokenPipeError` instead of PluMA being killed.
- A pipeline file saved with a UTF-8 byte order mark (as some Windows
  editors do) is read correctly; the mark used to corrupt its first line.
- `PluginManager::dependency()` is no longer inline in `PluginManager.h`
  (plugins built without exception support compile again). An unmet
  dependency raised where the exception cannot propagate -- a `noexcept`
  function or destructor, a callback from C code -- ends the run in order
  (status 1, incomplete outputs removed) instead of aborting the process.
- Embedded R is started with PluMA on POSIX, as before (C/C++ plugins may
  use its RInside directly). On Windows it is started only when R plugins
  are installed, or on first use (`languageLoad("R")`, an R step), so runs
  without R plugins do not load R.
- A step's output is checked and, after a failure, removed by its absolute
  path as of the step's start, so a plugin that changes the current
  directory before failing cannot make PluMA remove a different file.
- Python plugins' `sys.stdout`/`sys.stderr` are flushed at the end of each
  Python step, so what a step printed is not lost if a later step ends the
  run abruptly.
- Python plugins on parallel kitties import concurrently: only two kitties
  importing the *same* plugin module wait for each other.
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
- A JVM started by `languageLoad("Java")` on a plugin's own thread (an
  OpenMP pool thread, a detached `std::thread`) no longer keeps PluMA from
  exiting: that thread is attached to the JVM as a daemon thread.
- A plugin that catches its own unmet `dependency()` (`catch (...)`,
  `except Exception`, `tryCatch`) and goes on still fails its step, as when
  `dependency()` ended the run with `exit(1)` -- also when it then ends
  itself cleanly (`sys.exit(0)`, `quit(status = 0)`, Perl's `exit 0`).
- An R plugin script that R cannot read (e.g. Latin-1 characters in a
  string, not valid UTF-8) fails to load with R's message instead of ending
  PluMA ("Execution halted" on Linux, a crash on Windows).
- Windows: when PluMA starts R it points `R_HOME` at that R (an `R_HOME`
  naming another or a missing R made R fail to start); with R plugins
  installed, at startup on the main thread, before any kitty runs. Pipelines
  without R leave the environment alone.
- Linux: an `R_HOME` that is not an R installation is ignored with a notice
  (R would end PluMA at startup); the R PluMA was built with is used, as on
  Windows. An exception from RInside's startup no longer aborts PluMA: R
  plugins then fail with the reason.
- `pluma <pipeline> <plugin>` with a restart point that matches no step
  (e.g. misspelled) now reports it and exits with status 1, instead of
  skipping every step and exiting 0.
- `languageUnload("Java")` from a parallel kitty (or a plugin's own thread)
  after the JVM was started on PluMA's main thread is ignored with a log
  message, instead of deadlocking; the JVM is shut down at the end of the run.
- `exit()` on a thread a plugin started itself, while steps are running on
  kitties, moves every output those steps changed to `<output>.incomplete`
  (or `.incomplete.1`, ... -- an earlier one is never replaced): which step
  the caller belongs to cannot be told -- the others were cut off, its own
  may be finished, and even a single running step may be another plugin's,
  the thread being left over from a step that already ended. An incomplete output that cannot be removed because a step
  still has it open (Windows) is emptied instead of left partial.
- An unmet `dependency()` outside a pipeline step (e.g. in a plugin's static
  initializer, before the log file is open) is also reported on stderr.
- A native plugin that starts Python itself on the main thread
  (`Py_Initialize()` leaves that thread holding the GIL) no longer makes a
  later Python plugin on a parallel kitty hang: the GIL is released between
  steps and taken back for each later C/C++ step on the main thread, which
  can still use the C API as before. Such a plugin calling
  `languageLoad("Python")` while a kitty is starting to use that
  interpreter no longer deadlocks: the GIL is given up while it waits.
- An abort while Python is being shut down no longer touches the
  interpreter under the shutdown, and incomplete outputs are cleaned up
  before Python's buffered output is flushed.
- A C/C++ plugin in a directory that itself starts with the library prefix
  (`plugins/libFoo/libFooPlugin.so`) is registered under the name its
  `PluginProxy` uses -- `Foo`, as in earlier versions, or `libFoo` (judged
  by what that library itself registers, not by another plugin's name).
- A Python plugin's `sys.path` again has `<root>/<plugin>/` for every
  plugin root, as in earlier versions, so a helper module can live in
  another root's copy of the plugin directory.
- A plugin library that starts Python while it is loaded (`Py_Initialize()`
  in a static initializer) no longer makes Python plugins on kitties hang.
- Linux/macOS: a working directory that cannot be read (removed, no
  permission) no longer makes PluMA retry `getcwd` with ever larger buffers.
- `Plugin` lines with the keywords exactly swapped
  (`... outputfile a inputfile b`) stop the run with an error, instead of
  running the plugin with its files exchanged -- where a failure would then
  remove what is really its input. Other words in place of `Plugin`,
  `inputfile` and `outputfile` still run, as before, now with a warning.
- A C/C++ step on the main thread that calls `languageLoad("R")` or
  `languageUnload("R")` while holding the GIL no longer deadlocks with an R
  step on a kitty that calls into Python (reticulate).
- On PluMA's main thread, only C/C++ plugins whose library uses the Python C
  API hold the GIL during their steps (as earlier versions' main thread always
  held it); other C/C++ steps, and R and Perl steps, run without it, so Python
  steps on parallel kitties are no longer held up by them. R or Perl code that
  calls into Python (reticulate, Inline::Python) takes the GIL itself.
  A library counts as using the C API when it imports the Python runtime
  or a library whose name contains "python" (e.g. Boost.Python; on Windows
  also as a delay-load import; on Linux -- 32- or 64-bit -- and macOS, also
  undefined `Py*` symbols), or links a helper library next to it that does; when its
  imports cannot be read, it holds the GIL. (A helper that imports nothing,
  such as a resource-only DLL, does not count.) When a library replaces an
  earlier one of the same plugin name, its own imports decide. A GIL that
  any other C/C++ step leaves held on the main thread (e.g. it started
  Python through `dlopen`/`GetProcAddress`) is still released after it, and
  from then on every C/C++ step on the main thread takes it back for its
  duration, as in earlier versions (that plugin's later steps may rely on
  holding it). The same for an R, Perl or Java step that leaves it held
  (reticulate or Inline::Python starting Python): a Python step on a kitty
  no longer waits for it forever. A plugin library is scanned for Python
  use when the plugin first runs, not for every plugin at startup.
- An R plugin whose script fails while loading is reported with R's own
  error message (e.g. that of a `stop()` at the top of the script).
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
- An R plugin's `.Last` that raises an error at the end of the run no longer
  crashes PluMA (Windows) or makes it exit with status 1 (Linux/macOS): the
  error is printed and the run's status stands.
- Java: file names with characters outside the Basic Multilingual Plane (or
  bytes that are not UTF-8) reach Java plugins intact (`NewStringUTF`, used
  before, expects modified UTF-8; such paths were mangled or failed).
- Linux/macOS: R plugin scripts saved with Windows line endings (CRLF) run;
  R's parser rejected the carriage returns ("unexpected input"). R scripts
  saved as UTF-8 with a byte order mark run too, on every platform.
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
- Linux/macOS: an R script with a syntax error, or one that ends inside an
  unfinished expression (an unclosed `{`), fails to load with "Parse error at
  line N: ...", as on Windows. Before, an unfinished script loaded, and its
  text was prepended to every later R evaluation of the run (a crash).
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
- At the end of a run PluMA finalizes Python (flushing its output and
  running its `atexit` handlers) -- but not while a non-daemon Python thread
  a plugin started is still running, which finalization would wait for
  forever; then it only flushes the output. The JVM is shut down the same
  way (shutdown hooks run, `System.out` is flushed) unless a non-daemon Java
  thread a plugin started is still alive; then only `System.out`/`System.err`
  are flushed and it is left, as before. So a plugin that leaves such a
  thread running no longer keeps PluMA from exiting. The languages are shut
  down in reverse order, Python last, so R's `.Last` and Perl's `END`
  blocks can still call into Python (reticulate, Inline::Python); a Python
  left unfinalized gives its GIL back first.
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
- An R plugin whose input path cannot be passed to R fails as "not started"
  (its existing output is kept), like any failure before `input()`.
- The prefix a plugin's own thread falls back to is kept in storage that is
  never destroyed, so such a thread still running at process exit reads a
  valid string.
- `PluginManager::log()` and `setLogFile()` are defined in pluma, like
  `dependency()`: plugins no longer compile the logger's internals in.
- A Python step that starts while another kitty's `languageUnload("Python")`
  is shutting the interpreter down waits for the shutdown and runs (Python
  is started again), instead of failing -- and stopping the run -- only
  because of when it started.
- When a run is aborted, every language's buffered output is flushed first
  (Python's `sys.stdout`, Java's `System.out`; each waits only briefly).
- When a run is aborted, buffered Python output is still flushed if another
  thread holds PluMA's Python state lock for a moment.
- The libpython version is read from the last include directory that
  `python3-config --includes` reports (e.g. `3.14t`), not from a
  `python3.14` in the install prefix.
- macOS: plugins are linked with `-undefined dynamic_lookup`, as they use
  `PluginManager` symbols defined in the `pluma` executable, and C++/CUDA
  plugins are built as `lib<Name>Plugin.dylib`, the name PluMA looks for
  there (they were built as `.so` and never found).
- Python plugins' single-file helper modules are per plugin: two plugins that
  each have a `utils.py` each import their own (before, whichever was imported
  first was used by both). The standard library and site-packages still win
  over a same-named helper (a same-named folder without `__init__.py`
  elsewhere on `sys.path`, e.g. a `data/` in the current directory, does not:
  a plain import prefers a module to it too). Helper packages (directories), and helpers a
  plugin finds only in another plugin's directory, are still shared through
  `sys.modules`. `importlib.import_module("utils")` from a plugin's code
  gets its own helper too (other importlib loaders do not).
  The first plugin to import a helper of a given name has it under that
  name, as before, so its functions and objects pickle (e.g. to
  `multiprocessing` workers) as before; another plugin's same-named helper
  pickles within the process and to forked workers, but not to spawned ones
  (Windows), which import modules by name. Kitties importing the same
  helper at once wait for it to finish loading, as with a plain import, and
  a helper importing its own plugin's module gets that module, not a second
  copy.
- A Python plugin is imported from the file found in the plugin path (under
  its usual module name), so a same-named `FooPlugin.py` in the working
  directory or another plugin directory no longer replaces it. Plugin
  directories are still appended to `sys.path`, so the standard library and
  installed packages win over same-named helper files.
- Known limitation (unchanged): all Java plugins share one JVM classpath (every
  plugin root and plugin directory), so helper classes with the same name in
  different plugin directories resolve to the first one. A stray
  `<Name>Plugin.class` that would shadow a plugin is now reported instead of
  run. Give helper classes plugin-specific names or packages.
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
- Windows: a C/C++ plugin linked against the Python C API loads even when
  Python is not on `PATH` -- PluMA loads `pythonXY.dll` (and, for plugins
  built against the stable ABI, `python3.dll`) from the Python installation,
  as for Python plugins. (Also for a DLL whose optional header holds fewer
  than the usual 16 data directories, which the PE format allows.)
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
- A Rust plugin's library is no longer also loaded and listed as a C/C++
  plugin (`pluma plugins` showed it twice).
