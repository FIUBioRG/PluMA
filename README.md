# PluMA
A lightweight and flexible analysis pipeline

## Requirements
PluMA requires the following applications and/or libraries to compile:
- python >= 3.0
- libc++
- libc (glibc, clang, musl, ..)
- A C++ compiler (g++, clang++, ..)
- perl >= 5.0

Optional plugins may require:
- perl
- python
- C++
- R
- CUDA

## Compilation
PluMA is compiled using the Scons build system.

After cloning this repository and adding any desired plugins to the ```plugins``` folder, PluMA may be compiled by exectuing:
```sh
$ cd pluma
$ pip install -r requirements.txt
$ scons
```

A previous compilation may be cleaned by executing:
```sh
$ scons -c
```

## Documentation

The PluMA userguide is available at:
http://biorg.cis.fiu.edu/pluma/userguide.pdf

------------------------------------------------------------------------------

## Citation Information

All professional work making use of PluMA or its features should cite:

> _T. Cickovski, V. Aguiar-Pulido, W. Huang, S. Mahmoud, and G. Narasimhan._
> Lightweight Microbiome Analysis Pipelines.  _In Proceedings of International
> Work Conference on Bioinformatics and Biomedical Engineering_ (IWBBIO16),
> Granada, Spain, April 2016.

------------------------------------------------------------------------------

## Support

This work was partially supported by grants from:
- The Department of Defense Contract W911NF-16-1-0494
- NIH grant 1R15AI128714-01
- NIJ grant 2017-NE-BX-0001.

Additional support provided by:
- The Florida Department of Health (FDOH 09KW-10)
- [The Alpha-One Foundation](https://www.alpha1.org/)
- [NVIDIA](https://www.nvidia.com/)
- [The College of Engineering and Computer Science at Florida International University](https://www.cis.fiu.edu/), and
- [The Natural Sciences Collegium at Eckerd College](https://www.eckerd.edu/)

------------------------------------------------------------------------------
