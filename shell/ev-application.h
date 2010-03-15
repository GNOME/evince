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

#ifndef EV_APPLICATION_H
#define EV_APPLICATION_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>
#include <gtk/gtk.h>

#include "ev-window.h"

G_BEGIN_DECLS

typedef struct _EvApplication EvApplication;
typedef struct _EvApplicationClass EvApplicationClass;

#define EV_TYPE_APPLICATION			(ev_application_get_type ())
#define EV_APPLICATION(object)			(G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_APPLICATION, EvApplication))
#define EV_APPLICATION_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), EV_TYPE_APPLICATION, EvApplicationClass))
#define EV_IS_APPLICATION(object)		(G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_APPLICATION))
#define EV_IS_APPLICATION_CLASS(klass)		(G_TYPE_CHECK_CLASS_TYPE((klass), EV_TYPE_APPLICATION))
#define EV_APPLICATION_GET_CLASS(object)	(G_TYPE_INSTANCE_GET_CLASS((object), EV_TYPE_APPLICATION, EvApplicationClass))

#define EV_APP					(ev_application_get_instance ())

GType	          ev_application_get_type	     (void) G_GNUC_CONST;
EvApplication    *ev_application_get_instance        (void);

void              ev_application_shutdown            (EvApplication   *application);
gboolean          ev_application_load_session        (EvApplication   *application);
void              ev_application_open_window         (EvApplication   *application,
						      GdkScreen       *screen,
						      guint32          timestamp);
void              ev_application_open_uri_at_dest    (EvApplication   *application,
						      const char      *uri,
						      GdkScreen       *screen,
						      EvLinkDest      *dest,
						      EvWindowRunMode  mode,
						      const gchar     *search_string,
						      guint32          timestamp);
void	          ev_application_open_uri_list       (EvApplication   *application,
		  			              GSList          *uri_list,
						      GdkScreen       *screen,
    						      guint32          timestamp);
gboolean	  ev_application_has_window	     (EvApplication   *application);
const gchar *     ev_application_get_uri             (EvApplication   *application);
GObject		 *ev_application_get_media_keys	     (EvApplication   *application);

void 		  ev_application_set_filechooser_uri (EvApplication   *application,
						      GtkFileChooserAction action,
						      const gchar     *uri);
const gchar	 *ev_application_get_filechooser_uri (EvApplication   *application,
						      GtkFileChooserAction action);
void		  ev_application_screensaver_enable  (EvApplication   *application);
void		  ev_application_screensaver_disable (EvApplication   *application);
const gchar      *ev_application_get_dot_dir         (EvApplication   *application,
                                                      gboolean         create);
const gchar      *ev_application_get_data_dir        (EvApplication   *application);

G_END_DECLS

#endif /* !EV_APPLICATION_H */

