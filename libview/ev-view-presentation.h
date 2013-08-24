/* ev-view-presentation.h
 *  this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2010 Carlos Garcia Campos <carlosgc@gnome.org>
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

#if !defined (__EV_EVINCE_VIEW_H_INSIDE__) && !defined (EVINCE_COMPILATION)
#error "Only <evince-view.h> can be included directly."
#endif

#ifndef __EV_VIEW_PRESENTATION_H__
#define __EV_VIEW_PRESENTATION_H__

#include <gtk/gtk.h>

#include <evince-document.h>

G_BEGIN_DECLS

#define EV_TYPE_VIEW_PRESENTATION            (ev_view_presentation_get_type ())
#define EV_VIEW_PRESENTATION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EV_TYPE_VIEW_PRESENTATION, EvViewPresentation))
#define EV_IS_VIEW_PRESENTATION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EV_TYPE_VIEW_PRESENTATION))
#define EV_VIEW_PRESENTATION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EV_TYPE_VIEW_PRESENTATION, EvViewPresentationClass))
#define EV_IS_VIEW_PRESENTATION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EV_TYPE_VIEW_PRESENTATION))
#define EV_VIEW_PRESENTATION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), EV_TYPE_VIEW_PRESENTATION, EvViewPresentationClass))

typedef struct _EvViewPresentation       EvViewPresentation;
typedef struct _EvViewPresentationClass  EvViewPresentationClass;

GType		ev_view_presentation_get_type	      (void) G_GNUC_CONST;

GtkWidget      *ev_view_presentation_new	      (EvDocument         *document,
						       guint               current_page,
						       guint               rotation,
						       gboolean            inverted_colors);
guint           ev_view_presentation_get_current_page (EvViewPresentation *pview);
void            ev_view_presentation_next_page        (EvViewPresentation *pview);
void            ev_view_presentation_previous_page    (EvViewPresentation *pview);
void            ev_view_presentation_set_rotation     (EvViewPresentation *pview,
                                                       gint                rotation);
guint           ev_view_presentation_get_rotation     (EvViewPresentation *pview);

G_END_DECLS

#endif /* __EV_VIEW_PRESENTATION_H__ */
