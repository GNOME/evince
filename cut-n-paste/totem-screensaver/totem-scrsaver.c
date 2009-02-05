/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-

   Copyright (C) 2004-2006 Bastien Nocera <hadess@hadess.net>

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: Bastien Nocera <hadess@hadess.net>
 */


#include "config.h"

#include <glib/gi18n.h>

#include <gdk/gdk.h>

#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#include <X11/keysym.h>

#ifdef HAVE_XTEST
#include <X11/extensions/XTest.h>
#endif /* HAVE_XTEST */
#endif /* GDK_WINDOWING_X11 */

#ifdef ENABLE_DBUS
#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>

#define GS_SERVICE   "org.gnome.ScreenSaver"
#define GS_PATH      "/org/gnome/ScreenSaver"
#define GS_INTERFACE "org.gnome.ScreenSaver"
#endif /* ENABLE_DBUS */

#include "totem-scrsaver.h"

#define XSCREENSAVER_MIN_TIMEOUT 60

static GObjectClass *parent_class = NULL;
static void totem_scrsaver_class_init (TotemScrsaverClass *class);
static void totem_scrsaver_init       (TotemScrsaver      *parser);
static void totem_scrsaver_finalize   (GObject *object);


struct TotemScrsaverPrivate {
	/* Whether the screensaver is disabled */
	gboolean disabled;

#ifdef ENABLE_DBUS
	DBusGConnection *connection;
	DBusGProxy *gs_proxy;
	guint32 cookie;
#endif /* ENABLE_DBUS */

	/* To save the screensaver info */
	int timeout;
	int interval;
	int prefer_blanking;
	int allow_exposures;

	/* For use with XTest */
	int keycode1, keycode2;
	int *keycode;
	gboolean have_xtest;
};

G_DEFINE_TYPE(TotemScrsaver, totem_scrsaver, G_TYPE_OBJECT)

static gboolean
screensaver_is_running_dbus (TotemScrsaver *scr)
{
#ifdef ENABLE_DBUS
	if (! scr->priv->connection)
		return FALSE;

	if (! scr->priv->gs_proxy)
		return FALSE;

	return TRUE;
#else
	return FALSE;
#endif /* ENABLE_DBUS */
}

static void
screensaver_inhibit_dbus (TotemScrsaver *scr,
			  gboolean	 inhibit)
{
#ifdef ENABLE_DBUS
	GError *error;
	gboolean res;

	g_return_if_fail (scr != NULL);
	g_return_if_fail (scr->priv->connection != NULL);
	g_return_if_fail (scr->priv->gs_proxy != NULL);

	error = NULL;
	if (inhibit) {
		char   *application;
		char   *reason;
		guint32 cookie;

		application = g_strdup ("Evince");
		reason = g_strdup (_("Running in presentation mode"));

		res = dbus_g_proxy_call (scr->priv->gs_proxy,
					 "Inhibit",
					 &error,
					 G_TYPE_STRING, application,
					 G_TYPE_STRING, reason,
					 G_TYPE_INVALID,
					 G_TYPE_UINT, &cookie,
					 G_TYPE_INVALID);

		if (res) {
			/* save the cookie */
			scr->priv->cookie = cookie;
		} else {
			/* try the old API */
			res = dbus_g_proxy_call (scr->priv->gs_proxy,
						 "InhibitActivation",
						 &error,
						 G_TYPE_STRING, reason,
						 G_TYPE_INVALID,
						 G_TYPE_INVALID);
		}

		g_free (reason);
		g_free (application);

	} else {
		res = dbus_g_proxy_call (scr->priv->gs_proxy,
					 "UnInhibit",
					 &error,
					 G_TYPE_UINT, scr->priv->cookie,
					 G_TYPE_INVALID,
					 G_TYPE_INVALID);
		if (res) {
			/* clear the cookie */
			scr->priv->cookie = 0;
		} else {
			/* try the old API */
			res = dbus_g_proxy_call (scr->priv->gs_proxy,
						 "AllowActivation",
						 &error,
						 G_TYPE_INVALID,
						 G_TYPE_INVALID);
		}
	}

	if (! res) {
		if (error) {
			g_warning ("Problem inhibiting the screensaver: %s", error->message);
			g_error_free (error);
		}
	}
#endif /* ENABLE_DBUS */
}

static void
screensaver_enable_dbus (TotemScrsaver *scr)
{
	screensaver_inhibit_dbus (scr, FALSE);
}

static void
screensaver_disable_dbus (TotemScrsaver *scr)
{
	screensaver_inhibit_dbus (scr, TRUE);
}

#ifdef ENABLE_DBUS
static void
gs_proxy_destroy_cb (GObject *proxy,
		     TotemScrsaver *scr)
{
	g_warning ("Detected that GNOME screensaver has left the bus");

	/* just invalidate for now */
	scr->priv->gs_proxy = NULL;
}
#endif

#ifdef ENABLE_DBUS
static void
screensaver_init_dbus (TotemScrsaver *scr, DBusGConnection *connection)
{
	GError *error = NULL;

	if (!connection)
		scr->priv->connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	else
		scr->priv->connection = connection;

	if (! scr->priv->connection) {
		if (error) {
			g_warning ("Failed to connect to the session bus: %s", error->message);
			g_error_free (error);
		}
		return;
	}

	scr->priv->gs_proxy = dbus_g_proxy_new_for_name_owner (scr->priv->connection,
							       GS_SERVICE,
							       GS_PATH,
							       GS_INTERFACE,
							       NULL);
	if (scr->priv->gs_proxy != NULL) {
		g_signal_connect_object (scr->priv->gs_proxy,
					 "destroy",
					 G_CALLBACK (gs_proxy_destroy_cb),
					 scr,
					 0);

	}

}
#endif /* ENABLE_DBUS */

static void
screensaver_finalize_dbus (TotemScrsaver *scr)
{
#ifdef ENABLE_DBUS
	if (scr->priv->gs_proxy) {
		g_object_unref (scr->priv->gs_proxy);
	}
#endif /* ENABLE_DBUS */
}

#ifdef GDK_WINDOWING_X11
static void
screensaver_enable_x11 (TotemScrsaver *scr)
{

#ifdef HAVE_XTEST
	if (scr->priv->have_xtest != FALSE)
	{
		g_source_remove_by_user_data (scr);
		return;
	}
#endif /* HAVE_XTEST */

	XLockDisplay (GDK_DISPLAY());
	XSetScreenSaver (GDK_DISPLAY(),
			scr->priv->timeout,
			scr->priv->interval,
			scr->priv->prefer_blanking,
			scr->priv->allow_exposures);
	XUnlockDisplay (GDK_DISPLAY());
}

#ifdef HAVE_XTEST
static gboolean
fake_event (TotemScrsaver *scr)
{
	if (scr->priv->disabled)
	{
		XLockDisplay (GDK_DISPLAY());
		XTestFakeKeyEvent (GDK_DISPLAY(), *scr->priv->keycode,
				True, CurrentTime);
		XTestFakeKeyEvent (GDK_DISPLAY(), *scr->priv->keycode,
				False, CurrentTime);
		XUnlockDisplay (GDK_DISPLAY());
		/* Swap the keycode */
		if (scr->priv->keycode == &scr->priv->keycode1)
			scr->priv->keycode = &scr->priv->keycode2;
		else
			scr->priv->keycode = &scr->priv->keycode1;
	}

	return TRUE;
}
#endif /* HAVE_XTEST */

static void
screensaver_disable_x11 (TotemScrsaver *scr)
{

#ifdef HAVE_XTEST
	if (scr->priv->have_xtest != FALSE)
	{
		XLockDisplay (GDK_DISPLAY());
		XGetScreenSaver(GDK_DISPLAY(), &scr->priv->timeout,
				&scr->priv->interval,
				&scr->priv->prefer_blanking,
				&scr->priv->allow_exposures);
		XUnlockDisplay (GDK_DISPLAY());

		if (scr->priv->timeout != 0)
		{
			g_timeout_add_seconds (scr->priv->timeout / 2,
					       (GSourceFunc) fake_event, scr);
		} else {
			g_timeout_add_seconds (XSCREENSAVER_MIN_TIMEOUT / 2,
					(GSourceFunc) fake_event, scr);
		}

		return;
	}
#endif /* HAVE_XTEST */

	XLockDisplay (GDK_DISPLAY());
	XGetScreenSaver(GDK_DISPLAY(), &scr->priv->timeout,
			&scr->priv->interval,
			&scr->priv->prefer_blanking,
			&scr->priv->allow_exposures);
	XSetScreenSaver(GDK_DISPLAY(), 0, 0,
			DontPreferBlanking, DontAllowExposures);
	XUnlockDisplay (GDK_DISPLAY());
}

static void
screensaver_init_x11 (TotemScrsaver *scr)
{
#ifdef HAVE_XTEST
	int a, b, c, d;

	XLockDisplay (GDK_DISPLAY());
	scr->priv->have_xtest = (XTestQueryExtension (GDK_DISPLAY(), &a, &b, &c, &d) == True);
	if (scr->priv->have_xtest != FALSE)
	{
		scr->priv->keycode1 = XKeysymToKeycode (GDK_DISPLAY(), XK_Alt_L);
		if (scr->priv->keycode1 == 0) {
			g_warning ("scr->priv->keycode1 not existant");
		}
		scr->priv->keycode2 = XKeysymToKeycode (GDK_DISPLAY(), XK_Alt_R);
		if (scr->priv->keycode2 == 0) {
			scr->priv->keycode2 = XKeysymToKeycode (GDK_DISPLAY(), XK_Alt_L);
			if (scr->priv->keycode2 == 0) {
				g_warning ("scr->priv->keycode2 not existant");
			}
		}
		scr->priv->keycode = &scr->priv->keycode1;
	}
	XUnlockDisplay (GDK_DISPLAY());
#endif /* HAVE_XTEST */
}

static void
screensaver_finalize_x11 (TotemScrsaver *scr)
{
	g_source_remove_by_user_data (scr);
}
#endif

static void
totem_scrsaver_class_init (TotemScrsaverClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = totem_scrsaver_finalize;
}

#ifdef ENABLE_DBUS
TotemScrsaver *
totem_scrsaver_new	(DBusGConnection *connection)
{
	TotemScrsaver * scr;
	scr = TOTEM_SCRSAVER (g_object_new (TOTEM_TYPE_SCRSAVER, NULL));

	screensaver_init_dbus (scr, connection);
#ifdef GDK_WINDOWING_X11
	screensaver_init_x11 (scr);
#else
#warning Unimplemented
#endif
	
	return scr;
}
#else
TotemScrsaver *
totem_scrsaver_new()
{
	TotemScrsaver * scr;
	scr = TOTEM_SCRSAVER (g_object_new (TOTEM_TYPE_SCRSAVER, NULL));

#ifdef GDK_WINDOWING_X11
	screensaver_init_x11 (scr);
#else
#warning Unimplemented
#endif
	
	return scr;
}
#endif

static void
totem_scrsaver_init (TotemScrsaver *scr)
{
	scr->priv = g_new0 (TotemScrsaverPrivate, 1);

	
}

void
totem_scrsaver_disable (TotemScrsaver *scr)
{
	g_return_if_fail (TOTEM_SCRSAVER (scr));

	if (scr->priv->disabled != FALSE)
		return;

	scr->priv->disabled = TRUE;

	if (screensaver_is_running_dbus (scr) != FALSE)
		screensaver_disable_dbus (scr);
	else 
#ifdef GDK_WINDOWING_X11
		screensaver_disable_x11 (scr);
#else
#warning Unimplemented
	{}
#endif
}

void
totem_scrsaver_enable (TotemScrsaver *scr)
{
	g_return_if_fail (TOTEM_SCRSAVER (scr));

	if (scr->priv->disabled == FALSE)
		return;

	scr->priv->disabled = FALSE;

	if (screensaver_is_running_dbus (scr) != FALSE)
		screensaver_enable_dbus (scr);
	else 
#ifdef GDK_WINDOWING_X11
		screensaver_enable_x11 (scr);
#else
#warning Unimplemented
	{}
#endif
}

static void
totem_scrsaver_finalize (GObject *object)
{
	TotemScrsaver *scr = TOTEM_SCRSAVER (object);

	screensaver_finalize_dbus (scr);
#ifdef GDK_WINDOWING_X11
	screensaver_finalize_x11 (scr);
#else
#warning Unimplemented
	{}
#endif

	g_free (scr->priv);

	if (G_OBJECT_CLASS (parent_class)->finalize != NULL) {
		(* G_OBJECT_CLASS (parent_class)->finalize) (object);
	}
}

