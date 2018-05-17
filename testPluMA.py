############################################################################
# EXAMPLE OF TEXT FORMATTING OUTPUT IN PYTHON
import sys

BLACK, RED, GREEN, YELLOW, BLUE, MAGENTA, CYAN, WHITE = range(8)

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
   printout("[WARNING] ", YELLOW)
   print(msg)

def err(msg):
   printout("[FAILED] ", RED)
   print(msg)

def passed():
   printout("[PASSED] ", GREEN)
   print("")

# Get installed plugins
plugins = os.listdir("plugins")


#print '{:<10}'.format('test'),
#print '{:>10}'.format('good')
#print '{:<10}'.format('test'),
#print '{:>10}'.format('good')
#print '{:<10}'.format('test'),
#print '{:>10}'.format('good')
#print '{:<10}'.format('test'),
#print '{:>10}'.format('good')

for plugin in plugins:
 if (os.path.isdir("plugins/"+plugin)):
   # We are going to assume everything to be tested is in plugins/
   files = os.listdir("plugins/"+plugin)
   print '{:<50}'.format("Testing "+plugin+"..."), 
   sys.stdout.flush()
   if ("example" not in files):
       # Test case not present, issue a warning
       warn("No example directory Installed")
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
           if (outputfile != "none"):
              outputfile = "plugins/"+plugin+"/example/" + outputfile
              expected = outputfile+".expected"  # Get expected output
              if (not os.path.exists(expected)):
                 warn("No expected output present")
              else:
                 os.system("./pluma plugins/"+plugin+"/example/config.txt > plugins/"+plugin+"/example/pluma_output.txt 2 >& 1") # Run PluMA
                 if (not os.path.exists(outputfile)):
                    err("Output file did not generate, see example/pluma_output.txt")
                 else:
                     result = filecmp.cmp(outputfile, expected) # Compare expected and actual output
                     if (not result):
                        err("Output does not match expected")
                     else:
                        passed()
           else:
              warn("Screen output test not implemented yet")
