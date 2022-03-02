from behave import step
from dogtail.tree import *
from dogtail.rawinput import *
from dogtail.utils import *
from behave_common_steps import *

import os
os.environ['LANG']='C'
srcdir = os.getcwd()

@step(u'Open the New file Dialog')
def open_new_file_dia(context):
    frame = context.app.instance.child(roleName='frame')
    filler = frame.child(roleName='filler')
    toolbar = filler.child(roleName='tool bar')
    open_button = toolbar[7][0]
    assert open_button.click(),"Open Dialogue is not clickable"

@then(u'Open document window should be open')
def check_file_chooser_open(context):
    file_chooser = context.app.instance.child(roleName='file chooser')
    assert file_chooser

@step(u'Open Document Viewer Settings')
def open_settings(context):
    frame = context.app.instance.child(roleName='frame')
    filler = frame.child(roleName='filler')
    toolbar = filler.child(roleName='tool bar')
    setting_button = toolbar[6][0]
    assert setting_button.click(), "Settings button is not clickable"

@step(u'Copy test document to ~/Documents')
def open_new_file(context):
    global srcdir
    print(srcdir+'/test-page-labels.pdf')
    os.rename(srcdir+'/test-page-labels.pdf',os.environ["HOME"]+'/Documents/test-page-labels.pdf')

@step(u'Remove test document from ~/Documents')
def remove_test_doc(context):
    global srcdir
    os.rename(os.environ["HOME"]+'/Documents/test-page-labels.pdf', srcdir+'/test-page-labels.pdf')

@then(u'Print dialogue should appear')
def check_print_dialog(context):
    print_dialog = context.app.instance.child(roleName='dialog')
    assert print_dialog
@step(u'Open About Document Viewer')
def open_doc_view(context):
    frame = context.app.instance.child(roleName='frame')
    filler = frame.child(roleName='filler')
    toolbar = filler.child(roleName='tool bar')
    open_button = toolbar[7][0]
    open_button.click()
    open_button.pressKey('enter')
