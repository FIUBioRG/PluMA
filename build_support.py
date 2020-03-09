#!python

import os
from os import path
import subprocess
from subprocess import Popen, PIPE

###################################################################
# HELPER FUNCTIONS
###################################################################


def SourcePath(*args):
    res = []
    for p in args:
        res.append(path.abspath("./src/" + p))
    return res


def ObjectPath(*args):
    res = []
    for p in args:
        res.append(path.abspath("./obj/" + p))
    return res


def LibPath(*args):
    res = []
    for p in args:
        res.append(path.abspath("./lib/" + p))
    return res


def OutPath(*args):
    res = []
    for p in args:
        res.append(path.abspath("./src/" + p))
    return res


def cmdline(command):
    process = Popen(args=command, stdout=PIPE, shell=True)
    return process.communicate()[0].decode("utf8")


def CheckPerl(ctx):
    ctx.Message("Checking Perl configuration... ")

    source = """
    use strict;
    use Config;
    use File::Spec;

    sub search {
        my $paths = shift;
        my $file = shift;
        foreach(@{$paths}) {
            if (-f "$_/$file") {
                return "$_/$file";
                last
            }
        }
        return;
    }

    my $coredir = File::Spec->catfile($Config{installarchlib}, "CORE");

    open(F, ">", ".perlconfig.txt");
    print F "perl=$Config{perlpath}\\n";
    print F "typemap=" . search(\\@INC, "ExtUtils/typemap") . "\\n";
    print F "xsubpp=" . search(\\@INC, "ExtUtils/xsubpp" || search([File::Spec->path()], "xsubpp")) . "\\n";
    print F "coredir=$coredir\\n";
    close F;
    """

    f = open(".perltest.pl", "w")
    f.write(source)
    f.close()

    retcode = subprocess.call("perl .perltest.pl", shell=True)

    ctx.Result(retcode == 0)

    os.unlink(".perltest.pl")

    return retcode == 0
