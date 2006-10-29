#!/usr/bin/python

# This test opens a password encrypted file and tries to unlock it.

from dogtail.procedural import *

run('evince', arguments=' ./test-encrypt.pdf',)

# Try an incorrect password first
focus.dialog('Enter password')
focus.widget('Password Entry', roleName='password text')
type('wrong password')
click('OK', roleName='push button')
click('Cancel', roleName='push button')

# Try again with the correct password
click('Unlock Document', roleName='push button')
focus.widget('Password Entry', roleName='password text')
type('Foo')
click('OK', roleName='push button')

# Close evince
click('File', roleName='menu')
click('Close', roleName='menu item')
