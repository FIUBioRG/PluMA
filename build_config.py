#!python

import os
import sys
from os.path import abspath, relpath

platform = sys.platform

lib_search_path = ["/lib", "/usr/lib", "/usr/local/lib"]
include_search_path = ["/usr/include", "/usr/local/include", abspath("./src")]

source_base_dir = relpath("./src")
object_base_dir = relpath("./obj")
build_base_dir = relpath("./out")

prefix = abspath("/usr/local")
