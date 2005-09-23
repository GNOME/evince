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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef EV_APPLICATION_H
#define EV_APPLICATION_H

#include <glib/gerror.h>
#include <glib-object.h>

#include "ev-window.h"

#include "egg-toolbars-model.h"
#include "egg-recent-model.h"

G_BEGIN_DECLS

typedef struct _EvApplication EvApplication;
typedef struct _EvApplicationClass EvApplicationClass;
typedef struct _EvApplicationPrivate EvApplicationPrivate;

#define EV_TYPE_APPLICATION			(ev_application_get_type ())
#define EV_APPLICATION(object)			(G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_APPLICATION, EvApplication))
#define EV_APPLICATION_CLASS(klass)		(G_TYPE_CHACK_CLASS_CAST((klass), EV_TYPE_APPLICATION, EvApplicationClass))
#define EV_IS_APPLICATION(object)		(G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_APPLICATION))
#define EV_IS_APPLICATION_CLASS(klass)		(G_TYPE_CHECK_CLASS_TYPE((klass), EV_TYPE_APPLICATION))
#define EV_APPLICATION_GET_CLASS(object)	(G_TYPE_INSTANCE_GET_CLASS((object), EV_TYPE_APPLICATION, EvApplicationClass))

#define EV_APP					(ev_application_get_instance ())

struct _EvApplication {
	GObject base_instance;
	
	gchar *toolbars_file;
	
	EggToolbarsModel *toolbars_model;
	EggRecentModel  *recent_model;
};

struct _EvApplicationClass {
	GObjectClass base_class;
};

GType	          ev_application_get_type	     (void);
EvApplication    *ev_application_get_instance        (void);
gboolean          ev_application_register_service    (EvApplication   *application);
void	          ev_application_shutdown	     (EvApplication   *application);


gboolean          ev_application_open_window         (EvApplication   *application,
						      guint32         timestamp,
						      GError         **error);
gboolean          ev_application_open_uri            (EvApplication   *application,
				                      const char      *uri,
					              const char      *page_label,
						      guint32         timestamp,
						      GError         **error);
void	          ev_application_open_uri_list       (EvApplication   *application,
		  			              GSList          *uri_list,
    						      guint32          timestamp);
GList		 *ev_application_get_windows	     (EvApplication   *application);

EggToolbarsModel *ev_application_get_toolbars_model  (EvApplication   *application);
void              ev_application_save_toolbars_model (EvApplication   *application);
EggRecentModel   *ev_application_get_recent_model    (EvApplication   *application);

G_END_DECLS

#endif /* !EV_APPLICATION_H */

