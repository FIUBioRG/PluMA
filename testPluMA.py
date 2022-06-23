############################################################################
# EXAMPLE OF TEXT FORMATTING OUTPUT IN PYTHON
import sys
import glob
import subprocess

BLACK, RED, GREEN, YELLOW, BLUE, MAGENTA, CYAN, WHITE = range(8)
EPS=1e-8
global numpass
global numfail
global numwarn
global numlocal
global numoff
global numincompat 

#following from Python cookbook, #475186
def has_colours(stream):
    if not hasattr(stream, "isatty"):
        return False
    if not stream.isatty():
        return False # auto color only on TTYs
    try:
        import curses
        curses.setupterm()
        return curses.tigetnum("colors") > 2
    except:
        # guess false in case of error
        return False
has_colours = has_colours(sys.stdout)


def is_number(s):
   try:
      float(s)
      return True
   except ValueError:
      return False

def check(file1, file2, filetype):
   noa1 = open(file1, 'r')
   noa2 = open(file2, 'r')

   lines1 = []
   lines2 = []
   for line in noa1:
      lines1.append(line)

   for line in noa2:
      lines2.append(line)

   lines1.sort()
   lines2.sort()

   if (len(lines1) != len(lines2)):
      return False
   else:
      for i in range(0, len(lines1)):
         if (filetype == "CSV"):
            data1 = lines1[i].split(',')
            data2 = lines2[i].split(',')
         else:
            data1 = lines1[i].split()
            data2 = lines2[i].split()
         if (len(data1) != len(data2)):
            return False
         else:
            for j in range(0, len(data1)):
               if (not is_number(data1[j])):
                  if data1[j] != data2[j]:
                     return False
               else:
                  if (abs(float(data2[j])-float(data1[j])) > EPS): 
                     return False
            return True



def checkAccuracy(file1, file2):
   if (file1.endswith("csv")):
      return check(file1, file2, "CSV")
   elif (file1.endswith("noa")):
      return check(file1, file2, "NOA")

def normalprintout(text, colour=WHITE):
        if has_colours:
                seq = "\x1b[1;%dm" % (30+colour) + text + "\x1b[0m"
                #sys.stdout.write(seq)
                sys.stdout.write('{}'.format(seq))
        else:
                print("FALSE")
                sys.stdout.write('{}'.format(text))

def printout(text, colour=WHITE):
        if has_colours:
                seq = "\x1b[1;%dm" % (30+colour) + text + "\x1b[0m"
                #sys.stdout.write(seq)
                sys.stdout.write('{:>50}'.format(seq))
        else:
                sys.stdout.write('{:>50}'.format(text))

##printout("[debug]   ", GREEN)
##print("in green")
#printout("[warning] ", YELLOW)
#print("in yellow")
#printout("[error]   ", RED)
#print("in red")
#############################################################################

import os
import filecmp

def warn(msg):
   global numwarn
   numwarn += 1
   printout("[WARN] ", YELLOW)
   normalprintout("\n\t"+msg+"\n", YELLOW)

def err(msg):
   #global numfail
   printout("[FAIL] ", RED)
   normalprintout("\n\t"+msg+"\n", RED)

def passed():
   global numpass
   printout("[PASS] ", GREEN)
   numpass += 1
   print("")

def disabled(msg):
   global numoff
   numoff += 1
   printout("[OFF] ", BLUE)
   normalprintout("\n\t"+msg+"\n", BLUE)

def incompat(msg):
   global numincompat
   numincompat += 1
   printout("[INCOMPAT] ", CYAN)
   normalprintout("\n\t"+msg+"\n", CYAN)

def localplugin(msg):
   global numlocal
   numlocal += 1
   printout("[LOCAL] ", MAGENTA)
   normalprintout("\n\t"+msg+"\n", MAGENTA)

#turnedoff = []
#turnedoff = {}
turnedoff = {#'SparCC':'',
             #'Ensemble':'',
             'MetaPhlAn3':'Shotgun sequences are too large, also requires database',
             'Deblur':'Qiime2 binary files not unique per input file set',
             'FASTQ2QZA':'Qiime2 binary files not unique per input file set',
             'Qiime2Viz':'Qiime2 binary files not unique per input file set',
             'Qiime2DADA2':'Qiime2 binary files not unique per input file set',
             'Qiime2Filter':'Qiime2 binary files not unique per input file set',
             'QualityFilter':'Qiime2 binary files not unique per input file set',
             'FASTA2QZA':'Qiime2 binary files not unique per input file set',
             'VSearch':'Qiime2 binary files not unique per input file set',
             'FeatureClassify':'Qiime2 binary files not unique per input file set',
             'PlotAlphaDiversity':'Random number inconsistency',
             'PlotBetaDiversity3D':'Random number inconsistency',
             'GetKraken':'Will change with every database update',
             'CNN':'Random number inconsistency',
             'Mothur':'Random number inconsistency',
             'SCIMM':'Random number inconsistency',
             #'HMMER':'Random number inconsistency',
             'MetaCluster':'Random number inconsistency',
             'MetaBAT':'Long execution time',
             #'AbundanceBin':'Outputs elapsed time, which will change every run',
             #'HIVE':'Generates PDF file that varies slightly everytime',
             #'UClust':'Generates a random temporary file, different everytime',
             #'DeNovoOTU':'Long execution time',
             'OTUHeatmap':'Output is a PDF file, which changes slightly everytime',
             'Redbiom':'Uses a public database that is constantly changing',
             'Reactome':'Uses a public database that is constantly changing',
             #'OTUSummary':'Long execution time',
             #'OTUPlot':'Long execution time',
             #'Infomap':'Random number inconsistency, would require modifying Infomap source',
             'Canu':'Requires Oxford Nanopore data, needs to be large for adequate coverage',
             'Flye':'Requires Oxford Nanopore data, needs to be large for adequate coverage',
             'IDBA':'Random number inconsistency',
             'MegaHit':'Random number inconsistency',
             'WikiPathway':'Changes with every database update',
             'STRING':'Changes with every database update',
             'Caffe':'Requires BVLC database installation, and agreement to license',
             'SIMLR':'Requires machine-dependent compilation of projsplx_R.so' 
           }
#turnedoff = ['SparCC', 'Ensemble','SCIMM','MaxBin','HMMER', 'MetaCluster', 'AbundanceBin', 'HIVE', 'UClust', 'MEGAN', 'DeNovoOTU', 'OTUHeatmap', 'OTUSummary', 'Infomap']#, 'CumulativeClassifier', 'CumulativeTime', 'GeneUnify', 'NeuralNet']
local = {'CSV2PathwayTools':'Requires PathwayTools installation/license',
         'FilterPathway':'Requires PathwayTools installation/license',
         'PathwayFilter':'Requries PathwayTools installation/license',
         'CSVPathways':'Requires PathwayTools installation/license',
         'PathwayTools':'Requires PathwayTools installation/license',
         'PathwayTools2HMDB':'Requires PathwayTools installation/license',
         'EM':'Requires EM package, which is not open source',
         'MEGAN':'Requires ultimate version of MEGAN, which is not open source',
         #'PhiLR':'',
         'CubicSpline':'For BioRG Viral pipeline, test data not public',
         'PLSDA-Multiple':'For BioRG viral pipeline, test data not public',
         'NeuralNet':'For BioRG Viral pipeline, test data not public',
         'RandomForest':'For BioRG Viral pipeline, test data not public',
         'CumulativeTime':'For BioRG Viral pipeline, test data not public',
         'CumulativeClassifier':'For BioRG Viral pipeline, test data not public',
         'TimeWarp':'For BioRG Viral pipeline, test data not public',
         'FeatureSelection':'For BioRG Viral pipeline, test data not public',
         'GeneUnify':'For BioRG Viral pipeline, test data not public',
         'RAxML':'Long Execution Time',
         #'DeMUX':'Test case uses private data currently'
        }

inc = {'PCMCI':'Uses rpy2, Python-embedded R not yet supported'}
        
#local = ["CSV2PathwayTools","EM","FilterPathway","PathwayFilter","PhiLR","CubicSpline","PLSDA-Multiple","NeuralNet","RandomForest","CumulativeTime","CumulativeClassifier","TimeWarp","FeatureSelection","GeneUnify","ValidateMapping","DeMUX"]
# Get installed plugins
if (len(sys.argv) > 1):
   plugins = [sys.argv[1]]
else:
   plugins = os.listdir("plugins")

numpass = 0
numfail = 0
numwarn = 0
numoff = 0
numlocal = 0
numincompat = 0

for plugin in plugins:
 if (os.path.isdir("plugins/"+plugin)):
   # We are going to assume everything to be tested is in plugins/
   files = os.listdir("plugins/"+plugin)
   print ('{:<50}'.format("Testing "+plugin+"..."), end='') 
   sys.stdout.flush()
   if (plugin in turnedoff):
       disabled(turnedoff[plugin])
   elif (plugin in local):
       localplugin(local[plugin])
   elif (plugin in inc):
       incompat(inc[plugin])
   else:
    if (os.path.exists("plugins/"+plugin+"/example/pretest.txt")):
     pretest = open("plugins/"+plugin+"/example/pretest.txt", "r")
     for line in pretest:
        os.system(line.strip())
    if ("example" not in files):
       # Test case not present, issue a warning
       warn("No example directory installed")
    elif (not os.path.exists("plugins/"+plugin+"/example/config.txt")):
       # Example exists, but no config file
       warn("No config.txt Present")
    else:
       # Test case found, open the config file
       filestuff = open("plugins/"+plugin+"/example/config.txt", 'r')
       # Find the output file in the config file
       outputfile = ""
       for line in filestuff:
          contents = line.split(" ")
          # We have found the plugin to test
          if ("Plugin" in contents and contents[1] == plugin):
              if (contents[4] == "outputfile"):
                 outputfile = contents[5].strip()
              else:
                 err("Config File Not Formatted Right")
       if (outputfile == ""):
           warn("Config File Does Not Test Plugin")
       else:
           # File output
           if (os.path.exists("plugins/"+plugin+"/example/interactive")):
              incompat("User-interactive plugins not supported by tests")
           elif (outputfile != "none"):
              #oldoutputfile = outputfile
              outputfile = "plugins/"+plugin+"/example/" + outputfile
              #expect = glob.glob(outputfile+"*.expected")
              expect = glob.glob("plugins/"+plugin+"/example/*.expected")
              #expected = outputfile+".expected"  # Get expected output
              #if (not os.path.exists(expected)):
              if (len(expect) == 0):
                 warn("No expected output present")
              else:
               anyfail = False
               for expected in expect:
                  outputfile = expected[0:len(expected)-9]
                  if (os.path.exists(outputfile)):
                     os.system("rm "+outputfile)  # In case it was there already
               os.system("./pluma plugins/"+plugin+"/example/config.txt > plugins/"+plugin+"/example/pluma_output.txt 2>/dev/null") # Run PluMA
               for expected in expect:
                 outputfile = expected[0:len(expected)-9]
                 if (not os.path.exists(outputfile)):
                    err("Output file "+outputfile+" did not generate, see example/pluma_output.txt")
                    anyfail = True
                 else:
                     result = filecmp.cmp(outputfile, expected) # Compare expected and actual output
                     if (not result):
                        if (os.path.exists("plugins/"+plugin+"/example/test.py")): # Own test
                           print("RUNNING OWN TEST")
                           exec(open("plugins/"+plugin+"/example/test.py").read())
                           result = test(outputfile, expected)
                           if (not result):
                              err("Output "+outputfile+" does not match expected, see example/diff_output.txt")
                              anyfail = True
                        else:
                           #err("Output "+outputfile+" does not match expected, see example/diff_output.txt")
                           #print "diff <(sort "+outputfile+") <(sort "+expected+") > plugins/"+plugin+"/example/diff_output.txt"
                           #os.system("diff <(sort "+outputfile+") <(sort "+expected+") > plugins/"+plugin+"/example/diff_output.txt")  # Run diff
                           #print outputfile, expected
                           if (expected.endswith(".gz.expected")):
                              subprocess.call(["bash", "-c", "zdiff "+outputfile+" "+expected+" > plugins/"+plugin+"/example/diff_output.txt"])
                           else:
                              subprocess.call(["bash", "-c", "diff <(sort "+outputfile+") <(sort "+expected+") > plugins/"+plugin+"/example/diff_output.txt"])
                           diffsize = os.path.getsize("plugins/"+plugin+"/example/diff_output.txt")
                           if (diffsize > 0):
                              if (not checkAccuracy(outputfile, expected)):
                                 err("Output "+outputfile+" does not match expected, see example/diff_output.txt")
                                 anyfail = True
                           else:
                              os.system("rm plugins/"+plugin+"/example/diff_output.txt")
                     #else:
                     #   passed()
               if (not anyfail):
                    passed()
               else:
                    numfail += 1

           else:
              anyfail = False
              expected = "plugins/"+plugin+"/example/screen.expected"
              if (not os.path.exists(expected)):
                 warn("No expected output present")
              else:
                     outputfile = "plugins/"+plugin+"/example/pluma_output.txt"
                     os.system("./pluma plugins/"+plugin+"/example/config.txt > plugins/"+plugin+"/example/pluma_output.txt 2>/dev/null") # Run PluMA
                     result = filecmp.cmp(outputfile, expected) # Compare expected and actual output
                     if (not result):
                      if (os.path.exists("plugins/"+plugin+"/example/test.py")): # Own test
                           print("RUNNING OWN TEST")
                           exec(open("plugins/"+plugin+"/example/test.py").read())
                           result = test(outputfile, expected)
                           if (not result):
                              err("Output "+outputfile+" does not match expected, see example/diff_output.txt")
                              anyfail = True
                      else:
                        err("Output does not match expected, see example/diff_output.txt")
                        #print outputfile, expected
                        if (expected.endswith(".gz.expected")):
                          os.system("zdiff "+outputfile+" "+expected+" > plugins/"+plugin+"/example/diff_output.txt")  # Run diff
                        else:
                          os.system("diff <(sort "+outputfile+") <(sort "+expected+") > plugins/"+plugin+"/example/diff_output.txt")  # Run diff
                        anyfail = True

              if (not anyfail):
                    passed()
              else:
                    numfail += 1


if (numpass + numfail != 0):
   passrate = numpass / float(numpass+numfail) * 100
else:
   passrate = 0

normalprintout("\n\n************************************\n", WHITE)
normalprintout("Tests Passed: "+str(numpass)+"\n", GREEN)
normalprintout("Tests Failed: "+str(numfail)+"\n", RED)
normalprintout("Warnings: "+str(numwarn)+"\n", YELLOW)
normalprintout("Off: "+str(numoff)+"\n", BLUE)
normalprintout("Local: "+str(numlocal)+"\n", MAGENTA)
normalprintout("Incompatible: "+str(numincompat)+"\n", CYAN)
normalprintout("--------------------\n", WHITE)
normalprintout("Passing Rate: "+str(passrate)+"%\n", WHITE)
normalprintout("************************************\n", WHITE)
