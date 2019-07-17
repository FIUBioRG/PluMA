# Note this requires an internet connection
import os

import urllib2
response = urllib2.urlopen("http://biorg.cis.fiu.edu/pluma/plugins")
page_source = response.read()

# Plugin Table
plugin_table = page_source[page_source.find("<table "):page_source.find("</table>")]

# Individual Plugins
plugins = plugin_table.split("<tr>")
for plugin in plugins:
   content = plugin[plugin.find("<a href="):plugin.find("</a>")]
   content = content.replace('<a href=', '')
   data = content.split('>')
   if (len(data) == 2):
      if (os.path.exists(data[1])):
         print "Plugin "+data[1]+" already installed."
      else:
         repo = data[0][1:len(data[0])-1] # Remove quotes
         os.system("git clone "+repo)
