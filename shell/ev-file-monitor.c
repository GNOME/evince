/* ev-file-monitor.c
 *  this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2008 Carlos Garcia Campos <carlosgc@gnome.org>
 *
 * Evince is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Evince is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "config.h"

#include <glib.h>
#include <gio/gio.h>

#include "ev-file-monitor.h"

enum {
	CHANGED,
	N_SIGNALS
};

struct _EvFileMonitorPrivate {
	GFileMonitor *monitor;

	guint         timeout_id;
};

static void ev_file_monitor_timeout_start (EvFileMonitor    *ev_monitor);
static void ev_file_monitor_timeout_stop  (EvFileMonitor    *ev_monitor);
static void ev_file_monitor_changed_cb    (GFileMonitor     *monitor,
					   GFile            *file,
					   GFile            *other_file,
					   GFileMonitorEvent event_type,
					   EvFileMonitor    *ev_monitor);
	
#define EV_FILE_MONITOR_GET_PRIVATE(object) \
                (G_TYPE_INSTANCE_GET_PRIVATE ((object), EV_TYPE_FILE_MONITOR, EvFileMonitorPrivate))

G_DEFINE_TYPE (EvFileMonitor, ev_file_monitor, G_TYPE_OBJECT)

static guint signals[N_SIGNALS];

static void
ev_file_monitor_init (EvFileMonitor *ev_monitor)
{
	ev_monitor->priv = EV_FILE_MONITOR_GET_PRIVATE (ev_monitor);
}

static void
ev_file_monitor_finalize (GObject *object)
{
	EvFileMonitor *ev_monitor = EV_FILE_MONITOR (object);

	ev_file_monitor_timeout_stop (ev_monitor);
	
	if (ev_monitor->priv->monitor) {
		g_signal_handlers_disconnect_by_func (ev_monitor->priv->monitor,
						      ev_file_monitor_changed_cb,
						      ev_monitor);
		g_object_unref (ev_monitor->priv->monitor);
		ev_monitor->priv->monitor = NULL;
	}

	G_OBJECT_CLASS (ev_file_monitor_parent_class)->finalize (object);
}

static void
ev_file_monitor_class_init (EvFileMonitorClass *klass)
{
	GObjectClass *g_object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (g_object_class, sizeof (EvFileMonitorPrivate));

	g_object_class->finalize = ev_file_monitor_finalize;

	/* Signals */
	signals[CHANGED] =
		g_signal_new ("changed",
			      EV_TYPE_FILE_MONITOR,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EvFileMonitorClass, changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
}

static gboolean
timeout_cb (EvFileMonitor *ev_monitor)
{
	g_signal_emit (ev_monitor, signals[CHANGED], 0);
	
	ev_monitor->priv->timeout_id = 0;
	return FALSE;
}

static void
ev_file_monitor_timeout_start (EvFileMonitor *ev_monitor)
{
	ev_file_monitor_timeout_stop (ev_monitor);
	
	ev_monitor->priv->timeout_id =
		g_timeout_add_seconds (5, (GSourceFunc)timeout_cb, ev_monitor);
}

static void
ev_file_monitor_timeout_stop (EvFileMonitor *ev_monitor)
{
	if (ev_monitor->priv->timeout_id > 0) {
		g_source_remove (ev_monitor->priv->timeout_id);
		ev_monitor->priv->timeout_id = 0;
	}
}

static void
ev_file_monitor_changed_cb (GFileMonitor     *monitor,
			    GFile            *file,
			    GFile            *other_file,
			    GFileMonitorEvent event_type,
			    EvFileMonitor    *ev_monitor)
{
	switch (event_type) {
	case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
		ev_file_monitor_timeout_stop (ev_monitor);
		g_signal_emit (ev_monitor, signals[CHANGED], 0);

		break;
	case G_FILE_MONITOR_EVENT_CHANGED:
		ev_file_monitor_timeout_start (ev_monitor);
		break;
	default:
		break;
	}
}

EvFileMonitor *
ev_file_monitor_new (const gchar *uri)
{
	EvFileMonitor *ev_monitor;
	GFile         *file;
	GError        *error = NULL;
	
	ev_monitor = EV_FILE_MONITOR (g_object_new (EV_TYPE_FILE_MONITOR, NULL));

	file = g_file_new_for_uri (uri);
	ev_monitor->priv->monitor = g_file_monitor_file (file, G_FILE_MONITOR_NONE, NULL, &error);
	if (ev_monitor->priv->monitor) {
		g_signal_connect (ev_monitor->priv->monitor, "changed",
				  G_CALLBACK (ev_file_monitor_changed_cb), ev_monitor);
	} else if (error) {
		g_warning ("%s", error->message);
		g_error_free (error);
	}

	g_object_unref (file);

	return ev_monitor;
}
