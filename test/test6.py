#!/usr/bin/python

# Test printing

import os
os.environ['LANG']='C'
srcdir = os.environ['srcdir']
homedir = os.environ["HOME"] + "/";

from dogtail.procedural import *

if os.path.exists(homedir + "output.ps"):
    os.unlink(homedir + "output.ps")

run('evince', arguments=' '+srcdir+'/test-page-labels.pdf')

click('File', roleName='menu')
click('Print...', roleName='menu item')

focus.dialog('Print')
click('Print to File', roleName='table cell', raw=True)
click('Print', roleName='push button')

statinfo = os.stat (homedir + "output.ps")
if statinfo.st_size > 100000:
    exit(1)
os.unlink (homedir + "output.ps")

# Close evince
click('File', roleName='menu')
click('Close', roleName='menu item')
