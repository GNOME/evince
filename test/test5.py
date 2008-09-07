#!/usr/bin/python

# This test tries document reload action.

import os
os.environ['LANG']='C'
srcdir = os.environ['srcdir']

from dogtail.procedural import *

run('evince', arguments=' '+srcdir+'/test-page-labels.pdf')

focus.widget('page-label-entry')
focus.widget.text = "iii"
activate()

if focus.widget.text != "III":
	click('File', roleName='menu')
	click('Close', roleName='menu item')
	exit (1)

# Close evince
click('File', roleName='menu')
click('Close', roleName='menu item')
