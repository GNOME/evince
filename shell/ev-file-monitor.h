/* ev-file-monitor.h
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

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _EvFileMonitor        EvFileMonitor;
typedef struct _EvFileMonitorClass   EvFileMonitorClass;

#define EV_TYPE_FILE_MONITOR              (ev_file_monitor_get_type())
#define EV_FILE_MONITOR(object)           (G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_FILE_MONITOR, EvFileMonitor))
#define EV_FILE_MONITOR_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), EV_TYPE_FILE_MONITOR, EvFileMonitorClass))
#define EV_IS_FILE_MONITOR(object)        (G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_FILE_MONITOR))
#define EV_IS_FILE_MONITOR_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass), EV_TYPE_FILE_MONITOR))
#define EV_FILE_MONITOR_GET_CLASS(object) (G_TYPE_INSTANCE_GET_CLASS((object), EV_TYPE_FILE_MONITOR, EvFileMonitorClass))

struct _EvFileMonitor {
	GObject base_instance;
};

struct _EvFileMonitorClass {
	GObjectClass base_class;

	/* Signals */
	void (*changed) (EvFileMonitor *ev_monitor);
};

GType          ev_file_monitor_get_type (void) G_GNUC_CONST;
EvFileMonitor *ev_file_monitor_new      (const gchar *uri);

G_END_DECLS
