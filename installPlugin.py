import sys, subprocess
from subprocess import Popen, DEVNULL
# Install git-clone
Popen(["pip", "install", "git-clone"], shell=True, stdout=DEVNULL)
from git_clone import git_clone

if len(sys.argv) == 2:
	# Clone plugin
	git_clone(sys.argv[1], "plugins/")
else:
	print("Invalid usage.")
	print("Usage: python installPlugin.py <git-repo-link>")
	sys.exit(-1)