#!python

from os import path
from subprocess import PIPE, Popen

###################################################################
# HELPER FUNCTIONS
###################################################################


def SourcePath(p):
    return path.abspath("./src/" + p)


def OutPath(p):
    return path.abspath("./out/" + p)


def cmdline(command):
    process = Popen(args=command, stdout=PIPE, shell=True)
    return process.communicate()[0].decode("utf8")
