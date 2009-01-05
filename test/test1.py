#!/usr/bin/python

# This test opens the interface and just clicks around a bit.

import os
os.environ['LANG']='C'

from dogtail.procedural import *

run('evince')

# Test file->open
click('File', roleName='menu')
click('Open...', roleName='menu item')
focus.dialog('Open Document')
click('Cancel', roleName='push button')

# Toolbar editor
click('Edit', roleName='menu')
click('Toolbar', roleName='menu item')
focus.dialog('Toolbar Editor')
click('Close', roleName='push button')

# About dialog
click('Help', roleName='menu')
click('About', roleName='menu item')
focus.dialog('About Document Viewer')
click('Credits', roleName='push button')
focus.dialog('Credits')
click('Close', roleName='push button')
focus.dialog('About Document Viewer')
click('Close', roleName='push button')

# Close evince
click('File', roleName='menu')
click('Close', roleName='menu item')
