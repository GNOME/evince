/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2004 Martin Kretzschmar
 *
 *  Author:
 *    Martin Kretzschmar <martink@gnome.org>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>
#include <gtk/gtk.h>

#include "ev-window.h"

G_BEGIN_DECLS

#define EV_TYPE_APPLICATION			(ev_application_get_type ())
G_DECLARE_FINAL_TYPE (EvApplication, ev_application, EV, APPLICATION, AdwApplication)

#define EV_APP					((EvApplication *) g_application_get_default ())

EvApplication    *ev_application_new                 (void);

void              ev_application_open_recent_view    (EvApplication   *application,
						      GdkDisplay      *display);
void              ev_application_open_uri_at_dest    (EvApplication   *application,
						      const char      *uri,
						      GdkDisplay      *display,
						      EvLinkDest      *dest,
						      EvWindowRunMode  mode,
						      const gchar     *search_string,
						      guint32          timestamp);
void	          ev_application_open_uri_list       (EvApplication   *application,
						      GListModel      *files,
						      GdkDisplay      *display,
						      guint32          timestamp);
gboolean	  ev_application_has_window	     (EvApplication   *application);
guint             ev_application_get_n_windows       (EvApplication   *application);
const gchar *     ev_application_get_uri             (EvApplication   *application);
void              ev_application_clear_uri           (EvApplication   *application);

const gchar      *ev_application_get_dot_dir         (EvApplication   *application,
                                                      gboolean         create);
void              ev_application_new_window          (EvApplication *application,
						      GdkDisplay      *display,
						      guint32        timestamp);

G_END_DECLS
