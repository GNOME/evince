/* ev-statusbar.h
 *  this file is part of evince, a gnome document viewer
 * 
 * Copyright (C) 2004 Red Hat, Inc.
 *
 * Author:
 *   Jonathan Blandford <jrb@alum.mit.edu>
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

#ifndef __EV_STATUSBAR_H__
#define __EV_STATUSBAR_H__

#include <gtk/gtkvbox.h>
#include "ev-document.h"

G_BEGIN_DECLS

typedef struct _EvStatusbar EvStatusbar;
typedef struct _EvStatusbarClass EvStatusbarClass;
typedef struct _EvStatusbarPrivate EvStatusbarPrivate;

#define EV_TYPE_STATUSBAR		     (ev_statusbar_get_type())
#define EV_STATUSBAR(object)	     (G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_STATUSBAR, EvStatusbar))
#define EV_STATUSBAR_CLASS(klass)	     (G_TYPE_CHACK_CLASS_CAST((klass), EV_TYPE_STATUSBAR, EvStatusbarClass))
#define EV_IS_STATUSBAR(object)	     (G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_STATUSBAR))
#define EV_IS_STATUSBAR_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass), EV_TYPE_STATUSBAR))
#define EV_STATUSBAR_GET_CLASS(object) (G_TYPE_INSTANCE_GET_CLASS((object), EV_TYPE_STATUSBAR, EvStatusbarClass))

struct _EvStatusbar {
	GtkHBox base_instance;

	EvStatusbarPrivate *priv;
};

struct _EvStatusbarClass {
	GtkHBoxClass base_class;
};

GType      ev_statusbar_get_type  	   (void);
GtkWidget *ev_statusbar_new         	   (void);

typedef enum {
	EV_CONTEXT_HELP,
	EV_CONTEXT_VIEW,
	EV_CONTEXT_PROGRESS,
} EvStatusbarContext;

void 	   ev_statusbar_push          (EvStatusbar *ev_statusbar, 
			               EvStatusbarContext context, 
				       const gchar *message);
void 	   ev_statusbar_pop           (EvStatusbar *ev_statusbar, 
			               EvStatusbarContext context);
void       ev_statusbar_set_maximized (EvStatusbar *ev_statusbar, 
				       gboolean maximized);
void       ev_statusbar_set_progress  (EvStatusbar *ev_statusbar, 
				       gboolean active);
				       
G_END_DECLS

#endif /* __EV_STATUSBAR_H__ */


