#!/usr/bin/python

# Test printing

import os
os.environ['LANG']='C'
srcdir = os.environ['srcdir']

from dogtail.procedural import *

os.unlink("output.ps")

run('evince', arguments=' '+srcdir+'/test-page-labels.pdf')

click('File', roleName='menu')
click('Print...', roleName='menu item')

focus.dialog('Print')
click('Print to File', roleName='table cell', raw=True)
click('Print', roleName='push button')

statinfo = os.stat ("output.ps")
if statinfo.st_size > 100000:
    exit(1)
os.unlink ("output.ps")

# Close evince
click('File', roleName='menu')
click('Close', roleName='menu item')
