# Note this requires an internet connection
import os
import subprocess
import sys
import threading

from pool_utils import discover_websites, scrape_pool

pool = set()
local = set()
print("")
websites = discover_websites()

pool_lock = threading.Lock()

def process_website(website, entries):
  for href, name in entries:
    with pool_lock:
       pool.add(name)
    if (os.path.exists(name)):
       print("Plugin "+name+" already installed.")
    else:
       repo = href[1:len(href)-1] # Remove quotes
       result = subprocess.run(["git", "clone", repo])
       if result.returncode != 0:
          print("Failed to clone "+repo)

scrape_pool(websites, process_website)
