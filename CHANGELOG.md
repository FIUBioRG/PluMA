# Changelog

## v2.1.0

- Adding support for installing dependencies in `PLUMA_PLUGIN_PATH` directories.

  Support for installing plugins comes in four 'flavors':

  - Python (which assumes `pip` is installed) as `requirements.txt`
  - Linux (A shell script) as `requirements.sh`
  - macOS (A shell script) as `requirements-macos.sh`
  - Windows (A Windows Batch script) as `requirements.bat`

  Each plugin creator/maintainer will need to implement
  their desired dependency files for their plugin
  repository.

## v2.0.0

- Reworking of project build system
- Moving from using `cpp` suffix for C++ files for `cxx`.
- Consolidation of source code into `src` subdirectory.
- Adding Dockerfile and enabling generation of PluMA in a docker image.

## v1.0.0

- Initial public introduction of PluMA to the public.
- Subsequent untagged commits are retroactively part of the v1.0.0 line.
