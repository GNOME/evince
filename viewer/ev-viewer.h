/*
 *  Copyright (C) 2000-2003 Marco Pesenti Gritti
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  $Id$
 */

#ifndef EV_VIEWER_H
#define EV_VIEWER_H

#include <glib-object.h>
#include <glib.h>

G_BEGIN_DECLS

#define EV_TYPE_VIEWER		  (ev_viewer_get_type ())
#define EV_VIEWER(o)		  (G_TYPE_CHECK_INSTANCE_CAST ((o), EV_TYPE_VIEWER, EvViewer))
#define EV_VIEWER_IFACE(k)	  (G_TYPE_CHECK_CLASS_CAST((k), EV_TYPE_VIEWER, EvViewerIface))
#define EV_IS_VIEWER(o)		  (G_TYPE_CHECK_INSTANCE_TYPE ((o), EV_TYPE_VIEWER))
#define EV_IS_VIEWER_IFACE(k)	  (G_TYPE_CHECK_CLASS_TYPE ((k), EV_TYPE_VIEWER))
#define EV_VIEWER_GET_IFACE(inst) (G_TYPE_INSTANCE_GET_INTERFACE ((inst), EV_TYPE_VIEWER, EvViewerIface))

typedef struct _EvViewer	EvViewer;
typedef struct _EvViewerIface	EvViewerIface;

struct _EvViewerIface
{
	GTypeInterface base_iface;

	/* Methods  */
	void		   (* load)	(EvViewer *embed,
					 const char *uri);
};

GType		  ev_viewer_get_type	(void);

void		  ev_viewer_load	(EvViewer *embed,
					 const char *uri);

G_END_DECLS

#endif
