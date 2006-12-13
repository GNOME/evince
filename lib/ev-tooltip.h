/* ev-sidebar.h
 *  this file is part of evince, a gnome document viewer
 * 
 * Copyright (C) 2005 Red Hat, Inc.
 *
 * Author:
 *   Marco Pesenti Gritti <mpg@redhat.com>
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

#ifndef __EV_TOOLTIP_H__
#define __EV_TOOLTIP_H__

#include <gtk/gtkwindow.h>

G_BEGIN_DECLS

typedef struct _EvTooltip EvTooltip;
typedef struct _EvTooltipClass EvTooltipClass;
typedef struct _EvTooltipPrivate EvTooltipPrivate;

#define EV_TYPE_TOOLTIP		     (ev_tooltip_get_type())
#define EV_TOOLTIP(object)	     (G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_TOOLTIP, EvTooltip))
#define EV_TOOLTIP_CLASS(klass)	     (G_TYPE_CHECK_CLASS_CAST((klass), EV_TYPE_TOOLTIP, EvTooltipClass))
#define EV_IS_TOOLTIP(object)	     (G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_TOOLTIP))
#define EV_IS_TOOLTIP_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass), EV_TYPE_TOOLTIP))
#define EV_TOOLTIP_GET_CLASS(object) (G_TYPE_INSTANCE_GET_CLASS((object), EV_TYPE_TOOLTIP, EvTooltipClass))

struct _EvTooltip {
	GtkWindow base_instance;

	GtkWidget *parent;

	EvTooltipPrivate *priv;
};

struct _EvTooltipClass {
	GtkWindowClass base_class;
};

GType      ev_tooltip_get_type     (void);
GtkWidget *ev_tooltip_new          (GtkWidget *parent);
void       ev_tooltip_set_text	   (EvTooltip  *tooltip,
				    const char *text);
void       ev_tooltip_set_position (EvTooltip  *tooltip,
				    int         x,
				    int         y);
void       ev_tooltip_activate     (EvTooltip  *tooltip);
void       ev_tooltip_deactivate   (EvTooltip  *tooltip);

G_END_DECLS

#endif /* __EV_TOOLTIP_H__ */


