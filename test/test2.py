#!/usr/bin/python

from dogtail.procedural import *
from dogtail.utils import screenshot

import os

os.environ['LANG']='en_US.UTF-8'
os.system ('rm -rf ~/.gnome2/evince')

run('evince',arguments=' ./test-encrypt.pdf',)
focus.dialog('Enter password')
focus.widget('Password Entry', roleName='password text')
type("wrong password")
click('OK', roleName='push button')
click('Cancel', roleName='push button')
click('Unlock Document', roleName='push button')
focus.widget('Password Entry', roleName='password text')
type("Foo")
click('OK', roleName='push button')
click('Close', roleName='menu item')

