#!/usr/bin/python

# This test tries document reload action.

import os
os.environ['LANG']='C'
srcdir = os.environ['srcdir']

from dogtail.procedural import *

run('evince', arguments=' '+srcdir+'/test-links.pdf')

# Close evince
click('View', roleName='menu')
click('Reload', roleName='menu item')

# Close evince
click('File', roleName='menu')
click('Close', roleName='menu item')
