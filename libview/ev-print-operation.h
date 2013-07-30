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

#if !defined (__EV_EVINCE_VIEW_H_INSIDE__) && !defined (EVINCE_COMPILATION)
#error "Only <evince-view.h> can be included directly."
#endif

#ifndef __EV_PRINT_OPERATION_H__
#define __EV_PRINT_OPERATION_H__

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

GType             ev_print_operation_get_type               (void) G_GNUC_CONST;

gboolean          ev_print_operation_exists_for_document    (EvDocument       *document);
EvPrintOperation *ev_print_operation_new                    (EvDocument       *document);
void              ev_print_operation_set_current_page       (EvPrintOperation *op,
							     gint              current_page);
void              ev_print_operation_set_print_settings     (EvPrintOperation *op,
							     GtkPrintSettings *print_settings);
GtkPrintSettings *ev_print_operation_get_print_settings     (EvPrintOperation *op);
void              ev_print_operation_set_default_page_setup (EvPrintOperation *op,
							     GtkPageSetup     *page_setup);
GtkPageSetup     *ev_print_operation_get_default_page_setup (EvPrintOperation *op);
void              ev_print_operation_set_job_name           (EvPrintOperation *op,
							     const gchar      *job_name);
const gchar      *ev_print_operation_get_job_name           (EvPrintOperation *op);
void              ev_print_operation_run                    (EvPrintOperation *op,
							     GtkWindow        *parent);
void              ev_print_operation_cancel                 (EvPrintOperation *op);
void              ev_print_operation_get_error              (EvPrintOperation *op,
							     GError          **error);
void              ev_print_operation_set_embed_page_setup   (EvPrintOperation *op,
							     gboolean          embed);
gboolean          ev_print_operation_get_embed_page_setup   (EvPrintOperation *op);
const gchar      *ev_print_operation_get_status             (EvPrintOperation *op);
gdouble           ev_print_operation_get_progress           (EvPrintOperation *op);

G_END_DECLS
	
#endif /* __EV_PRINT_OPERATION_H__ */
