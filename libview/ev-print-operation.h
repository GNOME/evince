/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2008 Carlos Garcia Campos <carlosgc@gnome.org>
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

#if !defined (__EV_EVINCE_VIEW_H_INSIDE__) && !defined (EVINCE_COMPILATION)
#error "Only <evince-view.h> can be included directly."
#endif

#include <gtk/gtk.h>
#include <glib-object.h>

#include <evince-document.h>

G_BEGIN_DECLS

typedef struct _EvPrintOperation      EvPrintOperation;
typedef struct _EvPrintOperationClass EvPrintOperationClass;

#define EV_TYPE_PRINT_OPERATION            (ev_print_operation_get_type())
#define EV_PRINT_OPERATION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EV_TYPE_PRINT_OPERATION, EvPrintOperation))
#define EV_IS_PRINT_OPERATION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EV_TYPE_PRINT_OPERATION))
#define EV_PRINT_OPERATION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EV_TYPE_PRINT_OPERATION, EvPrintOperationClass))
#define EV_IS_PRINT_OPERATION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EV_TYPE_PRINT_OPERATION))
#define EV_PRINT_OPERATION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), EV_TYPE_PRINT_OPERATION, EvPrintOperationClass))

EV_PUBLIC
GType             ev_print_operation_get_type               (void) G_GNUC_CONST;

EV_PUBLIC
gboolean          ev_print_operation_exists_for_document    (EvDocument       *document);
EV_PUBLIC
EvPrintOperation *ev_print_operation_new                    (EvDocument       *document);
EV_PUBLIC
void              ev_print_operation_set_current_page       (EvPrintOperation *op,
							     gint              current_page);
EV_PUBLIC
void              ev_print_operation_set_print_settings     (EvPrintOperation *op,
							     GtkPrintSettings *print_settings);
EV_PUBLIC
GtkPrintSettings *ev_print_operation_get_print_settings     (EvPrintOperation *op);
EV_PUBLIC
void              ev_print_operation_set_default_page_setup (EvPrintOperation *op,
							     GtkPageSetup     *page_setup);
EV_PUBLIC
GtkPageSetup     *ev_print_operation_get_default_page_setup (EvPrintOperation *op);
EV_PUBLIC
void              ev_print_operation_set_job_name           (EvPrintOperation *op,
							     const gchar      *job_name);
EV_PUBLIC
const gchar      *ev_print_operation_get_job_name           (EvPrintOperation *op);
EV_PUBLIC
void              ev_print_operation_run                    (EvPrintOperation *op,
							     GtkWindow        *parent);
EV_PUBLIC
void              ev_print_operation_cancel                 (EvPrintOperation *op);
EV_PUBLIC
void              ev_print_operation_get_error              (EvPrintOperation *op,
							     GError          **error);
EV_PUBLIC
void              ev_print_operation_set_embed_page_setup   (EvPrintOperation *op,
							     gboolean          embed);
EV_PUBLIC
gboolean          ev_print_operation_get_embed_page_setup   (EvPrintOperation *op);
EV_PUBLIC
const gchar      *ev_print_operation_get_status             (EvPrintOperation *op);
EV_PUBLIC
gdouble           ev_print_operation_get_progress           (EvPrintOperation *op);

G_END_DECLS
