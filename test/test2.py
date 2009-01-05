#!/usr/bin/python

# This test opens a password encrypted file and tries to unlock it.

import os
os.environ['LANG']='C'
srcdir = os.environ['srcdir']

from dogtail.procedural import *

run('evince', arguments=' '+srcdir+'/test-encrypt.pdf')

# Try an incorrect password first
focus.dialog('Enter password')
focus.widget('Password Entry', roleName='password text')
type('wrong password')
click('Unlock Document', roleName='push button')
click('Cancel', roleName='push button')

# Try again with the correct password
click('Unlock Document', roleName='push button')
focus.widget('Password Entry', roleName='password text')
type('Foo')
click('Unlock Document', roleName='push button')

# Close evince
click('File', roleName='menu')
click('Close', roleName='menu item')
