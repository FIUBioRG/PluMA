# Note this requires an internet connection
import os
import sys
import urllib.request


#import urllib2
response = urllib.request.urlopen("http://biorg.cis.fiu.edu/pluma/plugins.html")
page_source = str(response.read())
#print(page_source)


pool = set()
local = set()
print("")
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
      if (len(data) == 2):
         pool.add(data[1])
         if (os.path.exists(data[1])):
            print("Plugin "+data[1]+" already installed.")
         else:
            repo = data[0][1:len(data[0])-1] # Remove quotes
            os.system("git clone "+repo)
      plugin = plugin[plugin.find("</a>")+1:]

      #normalprintout(str(count)+" ", GREEN)
      #print(count,end=" ")

    page_source = page_source[page_source.find("</table>")+1:]
