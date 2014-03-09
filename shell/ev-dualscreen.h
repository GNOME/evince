/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-indent-level: 8 -*- */
/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2007 Johannes Buchner
 *
 *  Author:
 *    Johannes Buchner <buchner.johannes@gmx.at>
 *    Lukas Bezdicka <255993@mail.muni.cz>
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

#ifndef EV_DUALSCREEN_H
#define EV_DUALSCREEN_H

#include "ev-window.h"
#include "ev-view-presentation.h"
#include "ev-metadata.h"

G_BEGIN_DECLS

typedef struct _EvDSCWindow EvDSCWindow;
typedef struct _EvDSCWindowClass EvDSCWindowClass;
typedef struct _EvDSCWindowPrivate EvDSCWindowPrivate;

#define EV_TYPE_DSCWINDOW	        (ev_dscwindow_get_type())
#define EV_DSCWINDOW(object)	        (G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_DSCWINDOW, EvDSCWindow))
#define EV_DSCWINDOW_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST((klass), EV_TYPE_DSCWINDOW, EvDSCWindowClass))
#define EV_IS_DSCWINDOW(object)		(G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_DSCWINDOW))
#define EV_IS_DSCWINDOW_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), EV_TYPE_DSCWINDOW))
#define EV_DSCWINDOW_GET_CLASS(object)  (G_TYPE_INSTANCE_GET_CLASS((object), EV_TYPE_DSCWINDOW, EvDSCWindowClass))

struct _EvDSCWindowClass {
	GtkWindowClass		base_class;
};

struct _EvDSCWindow {
	GtkWindow		 base_instance;
	EvDSCWindowPrivate      *priv;
};

GType		ev_dscwindow_get_type   (void);
GtkWidget      *ev_dscwindow_new (void);
void		ev_dscwindow_set_presentation   (EvDSCWindow		*ev_dscwindow,
						 EvWindow		*presentation_window,
						 EvDocument	        *document,
						 EvViewPresentation     *pview,
						 EvMetadata		*metadata);
EvDSCWindow*    ev_dscwindow_get_control (void);

G_END_DECLS

#endif /* EV_DUALSCREEN_H */
