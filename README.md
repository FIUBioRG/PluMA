# PluMA
A lightweight and flexible analysis pipeline

------------------------------------------------------------------------------

## Documentation

The PluMA userguide is available at:
http://biorg.cis.fiu.edu/pluma/userguide.pdf

------------------------------------------------------------------------------

## Citation Information

All professional work making use of PluMA or its features should cite:

T. Cickovski, V. Aguiar-Pulido, W. Huang, S. Mahmoud, and G. Narasimhan.  Lightweight
Microbiome Analysis Pipelines.  In Proceedings of International Work Conference on
Bioinformatics and Biomedical Engineering (IWBBIO16), Granada, Spain, April 2016.

------------------------------------------------------------------------------

## Support

This work was partially supported by grants from the Department of
Defense Contract W911NF-16-1-0494, NIH grant 1R15AI128714-01,
and NIJ grant 2017-NE-BX-0001.

Additional support provided by the Florida Department of Health (FDOH 09KW-10),
the Alpha-One Foundation, NVIDIA, The College of Engineering and Computer Science
at Florida International University and the Natural Sciences Collegium at
Eckerd College.

------------------------------------------------------------------------------

## Plugin Languages

PluMA can execute plugins implemented in C/C++, Python, Perl, R, and Java.
Java support requires a JDK with JNI headers and `libjvm`; the build scans `JAVA_HOME`
or the location of `javac` to configure the JNI include path and library.

A failing plugin stops the pipeline (exit status 1). To get the behavior of
earlier versions, where a failed step that left no output file was only logged
and the pipeline went on, set `PLUMA_CONTINUE_ON_ERROR=1` (PluMA still exits
with status 1 at the end if any step failed, and an unmet `dependency()` still
stops the run).

Notes for C/C++ plugin authors:

* As in earlier versions, the plugin object is not deleted after its step
  (its destructor does not run): close or flush output files in `output()`.
* `PluginManager::prefix()` / `addPrefix()` are per pipeline thread: each
  parallel kitty sees its own directory. A thread the plugin starts itself
  (`std::thread`, OpenMP workers) cannot be traced back to its kitty and sees
  the most recently set prefix, which may be a sibling kitty's: call
  `prefix()`/`addPrefix()` in the plugin's own `input()`/`run()`/`output()`
  and hand the result to its worker threads.
* On PluMA's main thread, a C/C++ plugin whose library uses the Python C API
  (it has undefined `Py*` symbols on Linux and macOS, or links a library whose
  name contains "python" -- `pythonXY.dll`, also delay-loaded, `libpython3.x`,
  Boost.Python -- itself or through a helper library in its own folder that
  it links) runs holding Python's GIL whenever the embedded Python is
  running, as in earlier versions, so it may call the C API directly. If it
  waits for its own threads that use Python, it must release the GIL around
  that wait (`Py_BEGIN_ALLOW_THREADS`), as before. Other C/C++ plugins, and R
  and Perl steps, run without the GIL, so Python steps on parallel kitties keep
  running meanwhile; R or Perl code that calls into Python (reticulate,
  Inline::Python) takes the GIL itself. On kitty threads, take it with
  `PyGILState_Ensure()`/`PyGILState_Release()`. A plugin that
  starts Python itself (`Py_Initialize()`) must release the GIL before
  returning (`PyEval_SaveThread()`) whenever Python plugins may run on parallel
  kitties, on whichever thread it runs; otherwise their Python steps wait for
  it forever.

------------------------------------------------------------------------------

## Building on Windows (MSVC)

Requirements: Visual Studio 2022 (or the Build Tools) with the *Desktop development
with C++* workload, and a 64-bit Python 3 from python.org. Optional, for the other
plugin languages: a JDK (Java), R for Windows (R), Strawberry Perl (Perl), and the
NVIDIA CUDA Toolkit (CUDA, with `--with-cuda`). All plugin languages are supported
on Windows, as on Linux.

```powershell
python -m pip install -r requirements.txt   # installs scons and packaging
python -m SCons                             # builds pluma.exe, PluGen\plugen.exe and plugins
.\pluma.exe pipeline.txt
```

`python -m SCons` works even when pip's `Scripts` folder is not on `PATH` (the
default for a per-user install). SCons locates MSVC on its own, so a Developer
prompt is not required. Notes:

* C++ plugins are built as `plugins\<Name>\<Name>Plugin.dll` and link against
  `lib\pluma.lib`, the import library for the `PluginManager` symbols exported by
  `pluma.exe`. A plugin DLL built outside SCons must use the same compiler
  and the shared release C runtime (`/MD`) as `pluma.exe`: `std::string`
  arguments cross the DLL boundary.
* Python plugins run in an embedded interpreter, using the Python that ran SCons.
  `python3XX.dll` is delay-loaded and the interpreter starts only when a Python
  plugin first runs. The DLL is looked up in this order: the directories on
  `PATH`, the PEP 514 registry entries written by the python.org installer, then
  the Python directory used at build time. The `PyPluMA` helper module is built
  only when `swig` is found: on `PATH`, or in the Scripts folder of the Python
  running SCons (where `pip install swig` puts it, also with `--user`).
* Java plugins work when a JDK is found. `jvm.dll` is delay-loaded and looked up
  in this order: `JAVA_HOME`, `jvm.dll` on `PATH`, the JDK of `java.exe` on
  `PATH`, the JavaSoft registry keys, then the JDK used at build time.
* Relative `PATH` entries and the current directory are never searched for these
  DLLs.
* R plugins work when R for Windows is found, in this order: `R_HOME`, the R
  installer's registry entry, then the R used at build time. When it starts R
  (R needs it), PluMA sets `R_HOME` to that R. R for Windows is built with
  GCC, so the RInside/Rcpp layer used on Linux is not available to MSVC;
  PluMA embeds R through R's C API in `pluma_r.dll`, which `pluma.exe` loads
  -- after `R.dll`, by absolute path -- on the first R step or
  `languageLoad("R")`. The
  `RPluMA` module is `RPluMA.dll` (`dyn.load("<PluMA>/RPluMA.dll")`, then
  `source("<PluMA>/RPluMA.R")`), built from the swig wrapper, or from the
  committed `RPluMA_wrap.cxx` without swig.
* C++/CUDA plugins that include `<sys/time.h>` get PluMA's Windows version of
  it (`struct timeval`, `gettimeofday`). A plugin that also uses winsock
  includes `<winsock2.h>` before `<sys/time.h>`: winsock defines
  `struct timeval` again otherwise.
* Perl plugins work with Strawberry Perl (found on `PATH`, else `C:\Strawberry`).
  Its headers require GCC, so the Perl runner is built with Strawberry's own
  `g++` into `pluma_perl.dll`, loaded the first time a Perl plugin runs, and
  `PerlPluMA` likewise (`auto\PerlPluMA\PerlPluMA.xs.dll` + `PerlPluMA.pm`; use
  `use lib '<PluMA>'; use PerlPluMA;`). Both talk to `pluma.exe` through plain C
  only. At run time `perl5XY.dll` is loaded from the Strawberry Perl used at
  build time, then from `PATH` (another Perl distribution's DLL of the same
  name may be built with incompatible options).
* CUDA plugins (`plugins\<Name>\<Name>Plugin.cu` -> `<Name>Plugin.dll`) are
  built with `python -m SCons --with-cuda` by `nvcc` (CUDA Toolkit from
  `CUDA_PATH` or its default install directory), linked against
  `lib\pluma.lib` like C++ plugins. Without `--gpu-architecture`, they target
  the GPU in the build machine (`-arch=native`), or every major architecture
  when there is none.
* A plugin directory with its own `SConscript` builds the plugin itself (it
  gets `env`, the C++ plugin environment, and `envPluginCuda`), on Windows and
  POSIX alike.
* `PLUMA_PLUGIN_PATH` entries are separated with `;` on Windows (`:` elsewhere).
