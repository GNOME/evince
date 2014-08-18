#!/bin/env python

import os
from gi.repository import Gio

# Reset GSettings
for schema in ['org.gnome.Evince']:
    os.system("gsettings reset-recursively %s" % schema)
