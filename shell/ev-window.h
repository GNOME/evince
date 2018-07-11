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

#ifndef EV_WINDOW_H
#define EV_WINDOW_H

#include <glib.h>
#include <gtk/gtk.h>

#include "ev-link.h"
#include "ev-history.h"
#include "ev-document-model.h"

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
typedef struct _EvWindowPrivate EvWindowPrivate;

#define EV_TYPE_WINDOW			(ev_window_get_type())
#define EV_WINDOW(object)		(G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_WINDOW, EvWindow))
#define EV_WINDOW_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), EV_TYPE_WINDOW, EvWindowClass))
#define EV_IS_WINDOW(object)		(G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_WINDOW))
#define EV_IS_WINDOW_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), EV_TYPE_WINDOW))
#define EV_WINDOW_GET_CLASS(object)	(G_TYPE_INSTANCE_GET_CLASS((object), EV_TYPE_WINDOW, EvWindowClass))


struct _EvWindow {
	GtkApplicationWindow base_instance;
	EvWindowPrivate     *priv;
};

struct _EvWindowClass {
	GtkApplicationWindowClass base_class;
};

GType		ev_window_get_type	                 (void) G_GNUC_CONST;
GtkWidget      *ev_window_new                            (void);
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
gboolean	ev_window_is_empty	                 (const EvWindow *ev_window);
void		ev_window_print_range                    (EvWindow       *ev_window,
                                                          int             first_page,
                                                          int		 last_page);
const gchar    *ev_window_get_dbus_object_path           (EvWindow       *ev_window);
GMenuModel     *ev_window_get_bookmarks_menu             (EvWindow       *ev_window);
EvHistory      *ev_window_get_history                    (EvWindow       *ev_window);
EvDocumentModel *ev_window_get_document_model            (EvWindow       *ev_window);
void            ev_window_focus_view                     (EvWindow       *ev_window);
GtkWidget      *ev_window_get_toolbar			 (EvWindow	 *ev_window);

G_END_DECLS

#endif /* !EV_WINDOW_H */
