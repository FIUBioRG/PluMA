# Note this requires an internet connection
import os
import urllib.request
#import urllib2
response = urllib.request.urlopen("http://biorg.cis.fiu.edu/pluma/plugins.html")
page_source = str(response.read())

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
   dirPath = os.path.dirname(os.path.realpath(__file__))
   if (len(data) == 2):
      if (os.path.exists(dirPath + "/" + data[1])):
         print("Plugin "+data[1]+" already installed.")
      else:
         repo = data[0][1:len(data[0])-1] # Remove quotes
         os.system("git clone " + repo + " " + dirPath + "/" + data[1])
   plugin = plugin[plugin.find("</a>")+1:]

 page_source = page_source[page_source.find("</table>")+1:]
