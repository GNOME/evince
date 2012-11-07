/* ev-sidebar-find-results.h
* this file is part of evince, a gnome document viewer
*
* Copyright (C) 2008 Sergey Pushkin < pushkinsv@gmail.com >
*
* Evince is free software; you can redistribute it and/or modify it
* under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* Evince is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
* General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 - 1307, USA.
*/

#ifndef __EV_SIDEBAR_FIND_RESULTS_H__
#define __EV_SIDEBAR_FIND_RESULTS_H__

#include <gtk/gtk.h>

#include "ev-jobs.h"

G_BEGIN_DECLS

typedef struct _EvSidebarFindResults EvSidebarFindResults;
typedef struct _EvSidebarFindResultsClass EvSidebarFindResultsClass;
typedef struct _EvSidebarFindResultsPrivate EvSidebarFindResultsPrivate;

#define EV_TYPE_SIDEBAR_FIND_RESULTS (ev_sidebar_find_results_get_type ())
#define EV_SIDEBAR_FIND_RESULTS(object)	(G_TYPE_CHECK_INSTANCE_CAST ((object), EV_TYPE_SIDEBAR_FIND_RESULTS, EvSidebarFindResults))
#define EV_SIDEBAR_FIND_RESULTS_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), EV_TYPE_SIDEBAR_FIND_RESULTS, EvSidebarFindResultsClass))
#define EV_IS_SIDEBAR_FIND_RESULTS(object)	(G_TYPE_CHECK_INSTANCE_TYPE ((object), EV_TYPE_SIDEBAR_FIND_RESULTS))
#define EV_IS_SIDEBAR_FIND_RESULTS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EV_TYPE_SIDEBAR_FIND_RESULTS))
#define EV_SIDEBAR_FIND_RESULTS_GET_CLASS(object) (G_TYPE_INSTANCE_GET_CLASS ((object), EV_TYPE_SIDEBAR_FIND_RESULTS, EvSidebarFindResultsClass))

enum {
	EV_DOCUMENT_FIND_RESULTS_COLUMN_TEXT,
	EV_DOCUMENT_FIND_RESULTS_COLUMN_PAGENO,
	EV_DOCUMENT_FIND_RESULTS_COLUMN_RESULTNO
};

struct _EvSidebarFindResults {
	GtkVBox base_instance;

	EvSidebarFindResultsPrivate *priv;
};

struct _EvSidebarFindResultsClass {
	GtkVBoxClass base_class;

	void (* find_result_activated) (EvSidebarFindResults *sidebar_find_results);
};

GType ev_sidebar_find_results_get_type (void);
GtkWidget *ev_sidebar_find_results_new (void);

void ev_sidebar_find_results_update (EvSidebarFindResults *sidebar_find_results,
				EvJobFind *job_find);
void
find_result_activate_result (EvSidebarFindResults *sidebar_find_results,
			gpointer results,
			gint pageno,
			gint resultno);

G_END_DECLS

#endif /* __EV_SIDEBAR_FIND_RESULTS_H__ */

