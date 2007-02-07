#!/usr/bin/python

# This test opens a password encrypted file and tries to unlock it.

import os
os.environ['LANG']='C'
srcdir = os.environ['srcdir']

from dogtail.procedural import *

run('evince', arguments=' '+srcdir+'/test-mime.bin')

# Close evince
click('File', roleName='menu')
click('Close', roleName='menu item')
