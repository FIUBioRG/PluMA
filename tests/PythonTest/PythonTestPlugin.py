import sys
import os

class PythonTestPlugin:
    def input(self, inputfile):
        the_input = open(inputfile, 'r')
        self.input = the_input.readline().strip()

    def run(self):
        self.input += " Bar"

    def output(self, outputfile):
        outfile = open(outputfile, 'w')

        outfile.write(self.input)
