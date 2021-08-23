#!/usr/bin/env python
# Note this requires an internet connection
import os
from os import path
import argparse
import urllib.request

if __name__ == '__main__':
    response = urllib.request.urlopen("http://biorg.cis.fiu.edu/pluma/plugins.html")
    page_source = str(response.read())

    parser = argparse.ArgumentParser('Download the collection of public PluMA plugins.')

    parser.add_argument('--directory', dest='dirPath', action='store',
        default=path.join(path.realpath(path.dirname(__file__)), 'plugins'),
        help='The directory to download the plugins to.')

    args = parser.parse_args()

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
                    if (os.path.exists(args.dirPath + "/" + data[1])):
                        print("Plugin " + data[1] + " already installed.")
                    else:
                        repo = data[0][1:len(data[0])-1] # Remove quotes
                        os.system("git clone " + repo + " " + args.dirPath + "/" + data[1])
                plugin = plugin[plugin.find("</a>")+1:]

        page_source = page_source[page_source.find("</table>")+1:]
