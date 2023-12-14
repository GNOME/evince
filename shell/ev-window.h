/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-indent-level: 8 -*- */
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

#include <glib.h>
#include <gtk/gtk.h>
#include <adwaita.h>

#include "ev-link.h"
#include "ev-history.h"
#include "ev-document-model.h"
#include "ev-metadata.h"

G_BEGIN_DECLS

typedef enum {
	EV_WINDOW_MODE_NORMAL,
	EV_WINDOW_MODE_FULLSCREEN,
	EV_WINDOW_MODE_PRESENTATION
} EvWindowRunMode;

typedef struct {
	gint start;
	gint end;
} EvPrintRange;

typedef enum {
	EV_PRINT_PAGE_SET_ALL,
	EV_PRINT_PAGE_SET_EVEN,
	EV_PRINT_PAGE_SET_ODD
} EvPrintPageSet;

typedef struct _EvWindow EvWindow;
typedef struct _EvWindowClass EvWindowClass;

#define EV_TYPE_WINDOW			(ev_window_get_type())
#define EV_WINDOW(object)		(G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_WINDOW, EvWindow))
#define EV_WINDOW_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), EV_TYPE_WINDOW, EvWindowClass))
#define EV_IS_WINDOW(object)		(G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_WINDOW))
#define EV_IS_WINDOW_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), EV_TYPE_WINDOW))
#define EV_WINDOW_GET_CLASS(object)	(G_TYPE_INSTANCE_GET_CLASS((object), EV_TYPE_WINDOW, EvWindowClass))


struct _EvWindow {
	AdwApplicationWindow base_instance;
};

struct _EvWindowClass {
	AdwApplicationWindowClass base_class;
};

GType		ev_window_get_type	                 (void) G_GNUC_CONST;
EvWindow       *ev_window_new                            (void);
const char     *ev_window_get_uri                        (EvWindow       *ev_window);
void		ev_window_open_uri	                 (EvWindow       *ev_window,
                                                          const char     *uri,
                                                          EvLinkDest     *dest,
                                                          EvWindowRunMode mode,
                                                          const gchar    *search_string);
void		ev_window_open_document                  (EvWindow       *ev_window,
                                                          EvDocument     *document,
                                                          EvLinkDest     *dest,
                                                          EvWindowRunMode mode,
                                                          const gchar    *search_string);
void            ev_window_open_recent_view               (EvWindow       *ev_window);
gboolean	ev_window_is_empty	                 (EvWindow       *ev_window);
void		ev_window_print_range                    (EvWindow       *ev_window,
                                                          int             first_page,
                                                          int		 last_page);
const gchar    *ev_window_get_dbus_object_path           (EvWindow       *ev_window);
void            ev_window_focus_view                     (EvWindow       *ev_window);
AdwHeaderBar   *ev_window_get_toolbar                    (EvWindow       *ev_window);
void            ev_window_handle_annot_popup             (EvWindow       *ev_window,
                                                          EvAnnotation   *annot);
EvMetadata     *ev_window_get_metadata			 (EvWindow	 *ev_window);
gint            ev_window_get_metadata_sidebar_size      (EvWindow       *ev_window);
void            ev_window_set_divider_position		 (EvWindow	 *ev_window,
							  gint		  sidebar_width);
void            ev_window_start_page_selector_search     (EvWindow       *window);

G_END_DECLS
