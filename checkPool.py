# Note this requires an internet connection
# Filename: checkPool.py
import os
import subprocess
import sys
import threading

from pool_utils import discover_websites, scrape_pool

BLACK, RED, GREEN, YELLOW, BLUE, MAGENTA, CYAN, WHITE = range(8)
EPS=1e-8

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

def normalprintout(text, colour=WHITE):
        if has_colours:
                seq = "\x1b[1;%dm" % (30+colour) + text + "\x1b[0m"
                #sys.stdout.write(seq)
                sys.stdout.write('{}'.format(seq))
        else:
                sys.stdout.write('{}'.format(text))

def printout(text, colour=WHITE):
        if has_colours:
                seq = "\x1b[1;%dm" % (30+colour) + text + "\x1b[0m"
                #sys.stdout.write(seq)
                sys.stdout.write('{:>50}'.format(seq))
        else:
                sys.stdout.write('{:>50}'.format(text))


pool = set()
local = set()
normalprintout("************************************", GREEN)
print("")
normalprintout("PLUGIN COUNTS:", GREEN)
print("")
websites = discover_websites()

count=0
count_lock = threading.Lock()

def process_website(website, entries):
  global count
  localcount = 0
  for href, name in entries:
    with count_lock:
       pool.add(name)
       count += 1
    localcount += 1
  normalprintout(website+"\t["+str(localcount)+"]\n", GREEN)

scrape_pool(websites, process_website)

if (len(sys.argv) > 1):
   plugins = [sys.argv[1]]
else:
   plugins = os.listdir("./plugins")

for plugin in plugins:
   local.add(plugin)

normalprintout("\nTOTAL\t["+str(len(pool))+"]\n", RED)
print("")
normalprintout("************************************", GREEN)
print("")
normalprintout("************************************", BLUE)
print("")
normalprintout("PLUGINS IN POOL, NOT LOCAL:", BLUE)
print("")
for plugin in pool-local:
   normalprintout(plugin, BLUE)
   print("")
normalprintout("************************************", BLUE)
print("")
normalprintout("************************************", MAGENTA)
print("")
normalprintout("PLUGINS LOCAL, NOT IN POOL:", MAGENTA)
print("")
for plugin in local-pool:
   if (plugin[0] != "." and plugin != "README" and not plugin.endswith(".py") and not plugin.endswith(".txt")):
      normalprintout(plugin, MAGENTA)
      print("")
normalprintout("************************************", MAGENTA)
print("")
normalprintout("************************************", YELLOW)
print("")
normalprintout("PLUGINS THAT DIFFER FROM REPOSITORY:", YELLOW)
print("")
for plugin in local.intersection(pool):
   result = subprocess.run(["git", "diff", "--quiet"], cwd="plugins/"+plugin, capture_output=True, text=True)
   if result.returncode == 1:
      normalprintout(plugin, YELLOW)
      print("")
   elif result.returncode not in (0, 1):
      print("Warning: could not check " + plugin + ": " + result.stderr.strip())
normalprintout("************************************", YELLOW)
print("")
   
