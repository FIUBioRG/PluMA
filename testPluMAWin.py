import sys
import glob
import subprocess
import os
import filecmp

############################################################################
# TEXT FORMATTING FOR OUTPUT IN PYTHON

os.system("color")

COLOR = {
    "BLUE": "\033[94m",
    "GREEN": "\033[92m",
    "RED": "\033[91m",
	"YELLOW": "\033[93m",
	"CYAN": "\033[96m",
	"MAGENTA": "\033[95m",
    "ENDC": "\033[0m",
}

def warn(msg):
   print(COLOR["YELLOW"], '{:>50}'.format("[WARNING] "), COLOR["ENDC"])
   print(msg)

def err(msg):
   print("")
   print(COLOR["RED"], '{:>50}'.format("[FAILED] "), COLOR["ENDC"])
   print(msg)

def passed():
   print("")
   print(COLOR["GREEN"], '{:>50}'.format("[PASSED] "), COLOR["ENDC"])
   print("")

def disabled():
   print(COLOR["BLUE"], '{:>50}'.format("[DISABLED] "), COLOR["ENDC"])
   print("")

def incompat(msg):
   print(COLOR["CYAN"], '{:>50}'.format("[INCOMPATIBLE] "), COLOR["ENDC"])
   print(msg)

def localplugin():
   print("")
   print(COLOR["MAGENTA"], '{:>50}'.format("[LOCAL] "), COLOR["ENDC"])
   print("")

# Test Colors   
# warn("")
# err("")
# passed()
# disabled()
# incompat("")
# localplugin()
#############################################################################

turnedoff = []
local = ["CSV2PathwayTools","EM","FilterPathway","PathwayFilter","PhiLR"]
# Get installed plugins
if (len(sys.argv) == 2):
   plugins = [sys.argv[1]]
elif (len(sys.argv) == 1):
   plugins = os.listdir("plugins")
else:
	print(COLOR["CYAN"], "Usage: python testPluMAWin.py <plugin-name>  \t<-- test one plugin\n        python testPluMAWin.py \t\t\t<-- test all plugins in the plugin folder", COLOR["ENDC"])
	exit()

for plugin in plugins:
   if (os.path.isdir("plugins\\"+plugin)):
      # We are going to assume everything to be tested is in plugins/
      files = os.listdir("plugins\\"+plugin)
      print ('{:<10}'.format("\nTesting "+plugin+"..."), end='') 
      sys.stdout.flush()
      if (plugin in turnedoff):
         disabled()
      elif (plugin in local):
         localplugin()
      elif ("example" not in files):
         # Test case not present, issue a warning
         warn("No example directory installed")
      elif (not os.path.exists("plugins\\"+plugin+"\\example\\config.txt")):
         # Example exists, but no config file
         warn("No config.txt Present")
      else:
         # Test case found, open the config file
         filestuff = open("plugins\\"+plugin+"\\example\\config.txt", 'r')
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
            if (os.path.exists("plugins\\"+plugin+"\\example\\interactive")):
               incompat("User-interactive plugins not supported by tests")
            elif (outputfile != "none"):
               outputfile = "plugins\\"+plugin+"\\example\\" + outputfile
               expect = glob.glob(outputfile+"*.expected")
               if (len(expect) == 0):
                  warn("No expected output present")
               else:
                  anyfail = False
                  for expected in expect:
                     outputfile = expected[0:len(expected)-9]
                     if (os.path.exists(outputfile)):
                        os.system("del "+outputfile)  # In case it was there already
                  subprocess.run("pluma plugins\\"+plugin+"\\example\\config.txt > plugins\\"+plugin+"\\example\\pluma_output.txt 2>plugins\\"+plugin+"\\example\\pluma_plugin_error.txt", shell = True) # Run PluMA
                  if(os.path.exists("plugins\\"+plugin+"\\example\\pluma_plugin_error.txt") and os.path.getsize("plugins\\"+plugin+"\\example\\pluma_plugin_error.txt") == 0):
                    os.system("del "+"plugins\\"+plugin+"\\example\\pluma_plugin_error.txt")
                  for expected in expect:
                     outputfile = expected[0:len(expected)-9]
                     if (not os.path.exists(outputfile)):
                        err("Output file "+outputfile+" did not generate, see example\\pluma_output.txt")
                        anyfail = True
                     else:
                        result = filecmp.cmp(outputfile, expected) # Compare expected and actual output
                        if (not result):
                           diffsize = subprocess.run("FC "+outputfile+" "+expected+" > plugins\\"+plugin+"\\example\\diff_output.txt", shell = True)
                           
                           if (diffsize.returncode != 0):
                              err("Output "+outputfile+" does not match expected, see example\\diff_output.txt")
                              anyfail = True
                           else:
                              os.system("del plugins\\"+plugin+"\\example\\diff_output.txt")
                  if (not anyfail):
                    passed()
            else:
               expected = "plugins\\"+plugin+"\\example\\screen.expected"
               if (not os.path.exists(expected)):
                  warn("No expected output present")
               else:
                  outputfile = "plugins/"+plugin+"/example/pluma_output.txt"
                  subprocess.run("pluma plugins\\"+plugin+"\\example\\config.txt > plugins\\"+plugin+"\\example\\pluma_output.txt 2>plugins\\"+plugin+"\\example\\pluma_plugin_error.txt", shell = True) # Run PluMA
                  result = filecmp.cmp(outputfile, expected) # Compare expected and actual output
                  if (not result):
                    err("Output does not match expected, see example/diff_output.txt")
                    subprocess.run("FC "+outputfile+" "+expected+" > plugins\\"+plugin+"\\example\\diff_output.txt", shell = True)
                  else:
                    passed()
   else:
      warn(plugin+" does not exist.")