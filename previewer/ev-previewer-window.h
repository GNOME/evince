/* ev-previewer-window.h:
 *  this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2009 Carlos Garcia Campos <carlosgc@gnome.org>
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

#include <gtk/gtk.h>

#include <evince-document.h>
#include <evince-view.h>

G_BEGIN_DECLS

#define EV_TYPE_PREVIEWER_WINDOW                  (ev_previewer_window_get_type())
#define EV_PREVIEWER_WINDOW(object)               (G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_PREVIEWER_WINDOW, EvPreviewerWindow))
#define EV_PREVIEWER_WINDOW_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST((klass), EV_TYPE_PREVIEWER_WINDOW, EvPreviewerWindowClass))
#define EV_IS_PREVIEWER_WINDOW(object)            (G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_PREVIEWER_WINDOW))
#define EV_IS_PREVIEWER_WINDOW_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE((klass), EV_TYPE_PREVIEWER_WINDOW))
#define EV_PREVIEWER_WINDOW_GET_CLASS(object)     (G_TYPE_INSTANCE_GET_CLASS((object), EV_TYPE_PREVIEWER_WINDOW, EvPreviewerWindowClass))

typedef struct _EvPreviewerWindow      EvPreviewerWindow;
typedef struct _EvPreviewerWindowClass EvPreviewerWindowClass;

GType              ev_previewer_window_get_type       (void) G_GNUC_CONST;

EvPreviewerWindow *ev_previewer_window_new            (void);

EvDocumentModel   *ev_previewer_window_get_document_model (EvPreviewerWindow *window);

void       ev_previewer_window_set_job                (EvPreviewerWindow *window,
                                                       EvJob             *job);
gboolean   ev_previewer_window_set_print_settings     (EvPreviewerWindow *window,
                                                       const gchar       *print_settings,
                                                       GError           **error);
gboolean   ev_previewer_window_set_print_settings_fd  (EvPreviewerWindow *window,
                                                       int                fd,
                                                       GError           **error);
void       ev_previewer_window_set_source_file        (EvPreviewerWindow *window,
                                                       const gchar       *source_file);
gboolean   ev_previewer_window_set_source_fd          (EvPreviewerWindow *window,
                                                       int                fd,
						       GError           **error);

G_END_DECLS
