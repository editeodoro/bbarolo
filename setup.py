#!/usr/bin/env python
from __future__ import print_function
import os, sys, pip, subprocess
from distutils.core import setup
from distutils.dir_util import remove_tree
import multiprocessing as mpr
#from pyBBarolo import __version__ as version
version = "1.0"
logfile = "setup.log"
try: os.remove(logfile)
except: pass
finally: f = open("setup.log", "a")

def checkModule(module):
    print('Checking %s... '%module,end='')
    try:
        __import__(module)
        print ("OK.")
    except ImportError:
        print("Module '%s' is not present, I will try to install it."%module)
        pip.main(['install',module])
        
# First: check if dependencies are available
modules = ['numpy','astropy']
for m in modules: 
    checkModule(m)
    
# Second: compile BB library
if not os.path.isfile("Makefile"):
    print ("Running BBarolo configure... ",end="")
    sys.stdout.flush()
    ret = subprocess.call(["./configure"], shell=True, stdout=f)
    if ret!=0: sys.exit("\nConfiguration script failed. Check %s for errors.\n"%logfile)
    print ("OK.")

print ("Compiling BBarolo... ",end="")
sys.stdout.flush()
ret = subprocess.call("make -j%i pybb"%mpr.cpu_count(), shell=True, stdout=f)
if ret!=0: sys.exit("\nCompilation failed. Check %s for errors.\n"%logfile)
print ("OK.")

print ("Cleaning... ",end="")
sys.stdout.flush()
ret = subprocess.call("make cleanup", shell=True, stdout=f)
print ("OK.")

# Installing pyBBarolo package
setup(name='pyBBarolo',
      version=version,
      description='a Python wrapper to BBarolo code',
      author=['Enrico Di Teodoro'],
      author_email=['enrico.diteodoro@gmail.com'], 
      url='https://github.com/editeodoro/Bbarolo',
      download_url="https://",
      packages=['pyBBarolo'],
      package_dir={'pyBBarolo':'pyBBarolo'}, 
      package_data={'pyBBarolo': ['libBB*']},
      classifiers=[
                   "Development Status :: 3 - Alpha",
                   "Programming Language :: Python, C++",
                   "License :: OSI Approved :: GNU General Public License v3 (GPLv3)"
                  ],
)

remove_tree('build')
     