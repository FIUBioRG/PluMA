# Note this requires an internet connection
import os
import urllib.request
import sys

thepipelines = set()
theplugins = set()
thepipelines.add(sys.argv[1]+"/config.txt")

while (len(thepipelines) != 0):
   configfile = open(thepipelines.pop(), 'r')
   for line in configfile:
      contents = line.strip().split(" ")
      if (contents[0] == "Pipeline"):
         thepipelines.add("../"+contents[1])
      elif (contents[0] == "Plugin"):
         theplugins.add(contents[1])
     
#import urllib2
response = urllib.request.urlopen("http://biorg.cis.fiu.edu/pluma/plugins.html")
page_source = str(response.read())

if (len(sys.argv) > 2):
   pluginpath = sys.argv[2]
else:
   pluginpath = "../plugins"


websites =set()
# Plugin Table
while (page_source.find("</table>") != -1):
 plugin_table = page_source[page_source.find("<table "):page_source.find("</table>")]
 plugins = plugin_table.split("<tr>")
 count=0
 for plugin in plugins:
  while(plugin.find("</a>") != -1):
   content = plugin[plugin.find("<a href="):plugin.find("</a>")]
   content = content.replace('<a href=', '')
   data = content.split('>')
   websites.add(data[0][1:len(data[0])-1])
   plugin = plugin[plugin.find("</a>")+1:]
 page_source = page_source[page_source.find("</table>")+1:]

for website in websites:
  response = urllib.request.urlopen("http://biorg.cis.fiu.edu/pluma/"+website)
  page_source = str(response.read())
  while (page_source.find("</table>") != -1):
    plugin_table = page_source[page_source.find("<table "):page_source.find("</table>")]
    # Individual Plugins
    plugins = plugin_table.split("<tr>")
    for plugin in plugins:
     while(plugin.find("</a>") != -1):
      content = plugin[plugin.find("<a href="):plugin.find("</a>")]
      content = content.replace('<a href=', '')
      data = content.split('>')
      if (len(data) == 2 and data[1] in theplugins):
         if (os.path.exists(pluginpath+"/"+data[1])):
            print("Plugin "+data[1]+" already installed.")
         else:
            repo = data[0][1:len(data[0])-1] # Remove quotes
            os.system("git clone "+repo+" "+pluginpath+"/"+data[1])
      plugin = plugin[plugin.find("</a>")+1:]

      #normalprintout(str(count)+" ", GREEN)
      #print(count,end=" ")

    page_source = page_source[page_source.find("</table>")+1:]

# Plugin Table
#while (page_source.find("</table>") != -1):
# plugin_table = page_source[page_source.find("<table "):page_source.find("</table>")]
#
# # Individual Plugins
# plugins = plugin_table.split("<tr>")
# for plugin in plugins:
#  while(plugin.find("</a>") != -1):
#   content = plugin[plugin.find("<a href="):plugin.find("</a>")]
#   content = content.replace('<a href=', '')
#   data = content.split('>')
#   if (len(data) == 2 and data[1] in theplugins):
#      if (os.path.exists(pluginpath+"/"+data[1])):
#         print("Plugin "+data[1]+" already installed.")
#      else:
#         repo = data[0][1:len(data[0])-1] # Remove quotes
#         os.system("git clone "+repo+" "+pluginpath+"/"+data[1])
#   plugin = plugin[plugin.find("</a>")+1:]
#
# page_source = page_source[page_source.find("</table>")+1:]
