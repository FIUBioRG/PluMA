# Note this requires an internet connection
import os
import sys
import urllib.request

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


#import urllib2
response = urllib.request.urlopen("http://biorg.cis.fiu.edu/pluma/plugins")
page_source = str(response.read())

pool = set()
local = set()
# Plugin Table
while (page_source.find("</table>") != -1):
 plugin_table = page_source[page_source.find("<table "):page_source.find("</table>")]

 # Individual Plugins
 plugins = plugin_table.split("<tr>")
 for plugin in plugins:
  while(plugin.find("</a>") != -1):
   content = plugin[plugin.find("<a href="):plugin.find("</a>")]
   content = content.replace('<a href=', '')
   data = content.split('>')
   if (len(data) == 2):
      pool.add(data[1])
      #if (os.path.exists(data[1])):
      #   print("Plugin "+data[1]+" already installed.")
      #else:
      #   repo = data[0][1:len(data[0])-1] # Remove quotes
         #os.system("git clone "+repo)
   plugin = plugin[plugin.find("</a>")+1:]

 
 page_source = page_source[page_source.find("</table>")+1:]

if (len(sys.argv) > 1):
   plugins = [sys.argv[1]]
else:
   plugins = os.listdir(".")

for plugin in plugins:
   local.add(plugin)

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
   if (plugin[0] != "." and plugin != "README" and not plugin.endswith(".py")):
      normalprintout(plugin, MAGENTA)
      print("")
normalprintout("************************************", MAGENTA)
print("")
normalprintout("************************************", YELLOW)
print("")
normalprintout("PLUGINS THAT DIFFER FROM REPOSITORY:", YELLOW)
print("")
for plugin in local.intersection(pool):
   x = os.popen('cd '+plugin+'; git diff; cd ..').read()
   if (len(x) != 0):
      normalprintout(plugin, YELLOW)
      print("")
   os.system("cd ..")
normalprintout("************************************", YELLOW)
print("")
   
