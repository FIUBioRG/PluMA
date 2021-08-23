#!/usr/bin/env python
'''Module for downloading a full Pipeline from a given configuration file'''
# Note this requires an internet connection
import os
from os import path
from urllib import request
import argparse

if __name__ == '__main__':

    pipelines = set()
    plugins = set()

    parser = argparse.ArgumentParser('Download plugins for a given pipeline pool.')

    parser.add_argument('pipelineDirectory', action='store',
        metavar='PIPELINE-DIRECTORY', help='The directory of the cloned pipeline.',
        default=path.join(path.realpath(path.dirname(__file__)), 'pipelines'))

    parser.add_argument('pluginDirectory', action='store',
        metavar='PLUGIN-DIRECTORY', help='The directory to clone plugin into.',
        default=path.join(path.realpath(path.dirname(__file__)), 'plugins'))

    args = parser.parse_args()

    pipelines.add(path.join(args.directory, "/config.txt"))

    while len(pipelines) != 0:
        with open(pipelines.pop(), mode='r', encoding='UTF-8') as configfile:
            for line in configfile:
                contents = line.strip().split(" ")
                if contents[0] == "Pipeline":
                    pipelines.add("../"+contents[1])
                elif contents[0] == "Plugin":
                    plugins.add(contents[1])
                else:
                    pass

    with request.urlopen("http://biorg.cis.fiu.edu/pluma/plugins.html") as response:
        page_source = str(response.read())

        # Plugin Table
        while page_source.find("</table>") != -1:
            plugin_table = page_source[page_source.find("<table "):page_source.find("</table>")]

        # Individual Plugins
        plugins = plugin_table.split("<tr>")
        for plugin in plugins:
            while plugin.find("</a>") != -1:
                content = plugin[plugin.find("<a href="):plugin.find("</a>")]
                content = content.replace('<a href=', '')
                data = content.split('>')
                if (len(data) == 2 and data[1] in plugins):
                    if path.exists(path.join(args.pluginDirectory, data[1])):
                        print("Plugin "+data[1]+" already installed.")
                    else:
                        repo = data[0][1:len(data[0])-1] # Remove quotes
                        os.system("git clone " + repo + " " + args.pluginDirectory + "/" + data[1])
            plugin = plugin[plugin.find("</a>")+1:]

            page_source = page_source[page_source.find("</table>")+1:]
