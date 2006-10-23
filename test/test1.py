#!/usr/bin/python

from dogtail.procedural import *
from dogtail.utils import screenshot

import os

os.environ['LANG']='en_US.UTF-8'
os.system ('rm -rf ~/.gnome2/evince')

run('evince')

focus.application('evince')

click('File', roleName='menu')
click('Open...', roleName='menu item')
focus.dialog('Open Document')
click('Cancel', roleName='push button')
click('File', roleName='menu')
click('Toolbar', roleName='menu item')
focus.dialog('Toolbar Editor')
click('Close', roleName='push button')
click('About', roleName='menu item')
focus.dialog('About Evince')
click('Close', roleName='push button')
click('Close', roleName='menu item')

