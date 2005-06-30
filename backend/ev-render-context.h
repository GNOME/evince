/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2005 Jonathan Blandford <jrb@gnome.org>
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

#ifndef EV_RENDER_CONTEXT_H
#define EV_RENDER_CONTEXT_H

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _EvRenderContext EvRenderContext;
typedef struct _EvRenderContextClass EvRenderContextClass;

#define EV_TYPE_RENDER_CONTEXT		(ev_render_context_get_type())
#define EV_RENDER_CONTEXT(context)	((EvRenderContext *) (context))
#define EV_RENDER_CONTEXT_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST((klass), EV_TYPE_RENDER_CONTEXT, EvRenderContext))
#define EV_IS_RENDER_CONTEXT(object)	(G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_RENDER_CONTEXT))

typedef enum
{
	EV_ORIENTATION_PORTRAIT,
	EV_ORIENTATION_LANDSCAPE,
	EV_ORIENTATION_UPSIDEDOWN,
	EV_ORIENTATION_SEASCAPE
} EvOrientation;


struct _EvRenderContextClass
{
	GObjectClass klass;
};

struct _EvRenderContext
{
	GObject parent;
	EvOrientation orientation;
	gint page;
	gdouble scale;

	gpointer data;
	GDestroyNotify destroy;
};


GType            ev_render_context_get_type        (void) G_GNUC_CONST;
EvRenderContext *ev_render_context_new             (EvOrientation    orientation,
						    gint             page,
						    gdouble          scale);
void             ev_render_context_set_page        (EvRenderContext *rc,
						    gint             page);
void             ev_render_context_set_orientation (EvRenderContext *rc,
						    EvOrientation    orientation);
void             ev_render_context_set_scale       (EvRenderContext *rc,
						    gdouble          scale);


G_END_DECLS

#endif /* !EV_RENDER_CONTEXT */
