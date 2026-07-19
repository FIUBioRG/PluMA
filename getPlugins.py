# Note this requires an internet connection
import os
import subprocess
import sys

from pool_utils import discover_websites, scrape_pool

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
     
if (len(sys.argv) > 2):
   pluginpath = sys.argv[2]
else:
   pluginpath = "../plugins"

websites = discover_websites()

def process_website(website, entries):
  for href, name in entries:
    if (name in theplugins):
       if (os.path.exists(pluginpath+"/"+name)):
          print("Plugin "+name+" already installed.")
       else:
          repo = href[1:len(href)-1] # Remove quotes
          target_dir = pluginpath+"/"+name
          result = subprocess.run(["git", "clone", repo, target_dir])
          if result.returncode != 0:
             print("Failed to clone "+repo+" into "+target_dir)

scrape_pool(websites, process_website)
