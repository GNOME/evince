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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef EV_PRINT_JOB_H
#define EV_PRINT_JOB_H

#include <glib-object.h>
#include <gtk/gtk.h>
#include <libgnomeprintui/gnome-print-dialog.h>

#include "ev-document.h"

G_BEGIN_DECLS

typedef struct _EvPrintJob EvPrintJob;
typedef struct _EvPrintJobClass EvPrintJobClass;

#define EV_TYPE_PRINT_JOB		(ev_print_job_get_type())
#define EV_PRINT_JOB(object)		(G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_PRINT_JOB, EvPrintJob))
#define EV_IS_PRINT_JOB(object)		(G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_PRINT_JOB))

GType		ev_print_job_get_type			(void);
void		ev_print_job_set_gnome_print_job	(EvPrintJob *job, GnomePrintJob *gpj);
void		ev_print_job_set_document		(EvPrintJob *job, EvDocument *document);
void		ev_print_job_use_print_dialog_settings	(EvPrintJob *job, GnomePrintDialog *dialog);
void		ev_print_job_print			(EvPrintJob *job, GtkWindow *parent);

G_END_DECLS

#endif /* !EV_PRINT_JOB_H */
