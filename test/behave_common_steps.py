# -*- coding: UTF-8 -*-
from dogtail.utils import isA11yEnabled, enableA11y
if isA11yEnabled() is False:
    enableA11y(True)

from time import time, sleep
from functools import wraps
from os import strerror, system
import errno
from signal import signal, alarm, SIGALRM
from subprocess import Popen, PIPE
from behave import step,then
from gi.repository import GLib, Gio
import fcntl, os
from dogtail.rawinput import keyCombo, click, typeText, absoluteMotion, pressKey
from dogtail.tree import root, SearchError
from configparser import ConfigParser
import traceback
from unittest import TestCase
import logging


# Create a dummy unittest class to have nice assertions
class dummy(TestCase):
    def runTest(self):  # pylint: disable=R0201
        assert True


def wait_until(my_lambda, element, timeout=30, period=0.25):
    """
    This function keeps running lambda with specified params until the result is True
    or timeout is reached
    Sample usages:
     * wait_until(lambda x: x.name != 'Loading...', context.app)
       Pause until window title is not 'Loading...'.
       Return False if window title is still 'Loading...'
       Throw an exception if window doesn't exist after default timeout

     * wait_until(lambda element, expected: x.text == expected, element, ('Expected text'))
       Wait until element text becomes the expected (passed to the lambda)

    """
    exception_thrown = None
    mustend = int(time()) + timeout
    while int(time()) < mustend:
        try:
            if my_lambda(element):
                return True
        except Exception as e:
            # If lambda has thrown the exception we'll re-raise it later
            # and forget about if lambda passes
            exception_thrown = e
        sleep(period)
    if exception_thrown:
        raise exception_thrown
    else:
        return False


class TimeoutError(Exception):
    """
    Timeout exception class for limit_execution_time_to function
    """
    pass


def limit_execution_time_to(
        seconds=10, error_message=strerror(errno.ETIME)):
    """
    Decorator to limit function execution to specified limit
    """
    def decorator(func):
        def _handle_timeout(signum, frame):
            raise TimeoutError(error_message)

        def wrapper(*args, **kwargs):
            signal(SIGALRM, _handle_timeout)
            alarm(seconds)
            try:
                result = func(*args, **kwargs)
            finally:
                alarm(0)
            return result

        return wraps(func)(wrapper)

    return decorator


class App(object):
    """
    This class does all basic events with the app
    """
    def __init__(
        self, appName, desktopFileName = None, shortcut='<Control><Q>', a11yAppName=None,
            forceKill=True, parameters='', recordVideo=False):
        """
        Initialize object App
        appName     command to run the app
        shortcut    default quit shortcut
        a11yAppName app's a11y name is different than binary
        forceKill   is the app supposed to be kill before/after test?
        parameters  has the app any params needed to start? (only for startViaCommand)
        recordVideo start gnome-shell recording while running the app
        """
        self.appCommand = appName
        self.shortcut = shortcut
        self.forceKill = forceKill
        self.parameters = parameters
        self.internCommand = self.appCommand.lower()
        self.a11yAppName = a11yAppName
        self.recordVideo = recordVideo
        self.pid = None
        if desktopFileName is None:
            desktopFileName = self.appCommand
        self.desktopFileName = desktopFileName
        # a way of overcoming overview autospawn when mouse in 1,1 from start
        pressKey('Esc')
        absoluteMotion(100, 100, 2)

        # attempt to make a recording of the test
        if self.recordVideo:
            keyCombo('<Control><Alt><Shift>R')

    def isRunning(self):
        """
        Is the app running?
        """
        if self.a11yAppName is None:
            self.a11yAppName = self.internCommand

        # Trap weird bus errors
        for attempt in xrange(0, 30):
            sleep(1)
            try:
                return self.a11yAppName in [x.name for x in root.applications()]
            except GLib.GError:
                continue
        raise Exception("10 at-spi errors, seems that bus is blocked")

    def getDashIconPosition(name):
        """Get a position of miniature on Overview"""
        over = root.application('gnome-shell').child(name='Overview')
        button = over[2].child(name=name)
        (x, y) = button.position
        (a, b) = button.size
        return (x + a / 2, y + b / 2)

    def parseDesktopFile(self):
        """
        Getting all necessary data from *.dektop file of the app
        """
        cmd = "rpm -qlf $(which %s)" % self.appCommand
        cmd += '| grep "^/usr/share/applications/.*%s.desktop$"' % self.desktopFileName
        proc = Popen(cmd, shell=True, stdout=PIPE)
        # !HAVE TO check if the command and its desktop file exist
        if proc.wait() != 0:
            raise Exception("*.desktop file of the app not found")
        output = proc.communicate()[0].rstrip()
        self.desktopConfig = ConfigParser()
        self.desktopConfig.read(output)

    def kill(self):
        """
        Kill the app via 'killall'
        """
        if self.recordVideo:
            keyCombo('<Control><Alt><Shift>R')

        try:
            self.process.kill()
        except:
            # Fall back to killall
            Popen("killall " + self.appCommand, shell=True).wait()

    def getName(self):
        return self.desktopConfig.get('Desktop Entry', 'name')

    def startViaCommand(self):
        """
        Start the app via command
        """
        if self.forceKill and self.isRunning():
            self.kill()
            sleep(2)
            assert not self.isRunning(), "Application cannot be stopped"

        self.process = Popen(self.appCommand.split() + self.parameters.split(),
                             stdout=PIPE, stderr=PIPE, bufsize=0)
        self.pid = self.process.pid

        assert self.isRunning(), "Application failed to start"
        return root.application(self.a11yAppName)

    def startViaMenu(self, throughCategories = False):
        self.parseDesktopFile()
        if self.forceKill and self.isRunning():
            self.kill()
            sleep(2)
            assert not self.isRunning(), "Application cannot be stopped"
        try:
            gnomeShell = root.application('gnome-shell')
            pressKey('Super_L')
            sleep(6)
            if throughCategories:
                # menu Applications
                x, y = getDashIconPosition('Show Applications')
                absoluteMotion(x, y)
                time.sleep(1)
                click(x, y)
                time.sleep(4) # time for all the oversized app icons to appear

                # submenu that contains the app
                submenu = gnomeShell.child(
                    name=self.getMenuGroups(), roleName='list item')
                submenu.click()
                time.sleep(4)

                # the app itself
                app = gnomeShell.child(
                    name=self.getName(), roleName='label')
                app.click()
            else:
                typeText(self.getName())
                sleep(2)
                pressKey('Enter')

            assert self.isRunning(), "Application failed to start"
        except SearchError:
            print("!!! Lookup error while passing the path")

        return root.application(self.a11yAppName)



    def closeViaShortcut(self):
        """
        Close the app via shortcut
        """
        if not self.isRunning():
            raise Exception("App is not running")

        keyCombo(self.shortcut)
        assert not self.isRunning(), "Application cannot be stopped"


@step(u'Press "{sequence}"')
def press_button_sequence(context, sequence):
    keyCombo(sequence)
    sleep(0.5)


def wait_for_app_to_appear(context, app):
    # Waiting for a window to appear
    for attempt in xrange(0, 10):
        try:
            context.app.instance = root.application(app.lower())
            context.app.instance.child(roleName='frame')
            break
        except (GLib.GError, SearchError):
            sleep(1)
            continue
    context.execute_steps("Then %s should start" % app)


@step(u'Start {app} via {type:w}')
def start_app_via_command(context, app, type):
    for attempt in xrange(0, 10):
        try:
            if type == 'command':
                context.app.startViaCommand()
            if type == 'menu':
                context.app.startViaMenu()
            break
        except GLib.GError:
            sleep(1)
            if attempt == 6:
                # Killall the app processes if app didn't show up after 5 seconds
                os.system("pkill -f %s 2&> /dev/null" % app.lower())
                os.system("python cleanup.py")
                context.execute_steps("* Start %s via command" % app)
            continue

@step(u'Close app via gnome panel')
def close_app_via_gnome_panel(context):
    context.app.closeViaGnomePanel()

@step(u'Click "Quit" in GApplication menu')
def close_app_via_shortcut(context) :
    context.app.closeViaShortcut

@step(u'Make sure that {app} is running')
def ensure_app_running(context, app):
    start_app_via_command(context, app, 'command')
    wait_for_app_to_appear(context, app)
    logging.debug("app = %s", root.application(app.lower()))

@then(u'{app} should start')
def test_app_started(context, app):
    # Dogtail seems to cache applications list
    # So we should wait for exception here
    try:
        root.application(app.lower()).child(roleName='frame')
    except SearchError:
        raise RuntimeError("App '%s' is not running" % app)

@then(u"{app} shouldn't be running anymore")
def then_app_is_dead(context, app):
    try:
        root.application(app.lower()).child(roleName='frame')
        raise RuntimeError("App '%s' is running" % app)
    except SearchError:
        pass

def non_block_read(output):
    fd = output.fileno()
    fl = fcntl.fcntl(fd, fcntl.F_GETFL)
    fcntl.fcntl(fd, fcntl.F_SETFL, fl | os.O_NONBLOCK)
    try:
        return output.read()
    except:
        return ""
