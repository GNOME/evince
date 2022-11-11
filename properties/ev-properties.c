/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-indent-level: 8 -*- */
/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2005 Red Hat, Inc
 *  Copyright (C) 2022 Qiu Wenbo <qiuwenbo@kylinos.com.cn>
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

#include "config.h"

#ifdef HAVE__NL_MEASUREMENT_MEASUREMENT
#include <langinfo.h>
#endif

#include "ev-properties.h"

static GtkUnit
get_default_user_units (void)
{
	/* Translate to the default units to use for presenting
	 * lengths to the user. Translate to default:inch if you
	 * want inches, otherwise translate to default:mm.
	 * Do *not* translate it to "predefinito:mm", if it
	 * it isn't default:mm or default:inch it will not work
	 */
	gchar *e = _("default:mm");

#ifdef HAVE__NL_MEASUREMENT_MEASUREMENT
	gchar *imperial = NULL;

	imperial = nl_langinfo (_NL_MEASUREMENT_MEASUREMENT);
	if (imperial && imperial[0] == 2)
		return GTK_UNIT_INCH;  /* imperial */
	if (imperial && imperial[0] == 1)
		return GTK_UNIT_MM;  /* metric */
#endif

	if (strcmp (e, "default:mm") == 0)
		return GTK_UNIT_MM;
	if (strcmp (e, "default:inch") == 0)
		return GTK_UNIT_INCH;

	g_warning ("Whoever translated default:mm did so wrongly.\n");

	return GTK_UNIT_MM;
}

static gdouble
get_tolerance (gdouble size)
{
	if (size < 150.0f)
		return 1.5f;
	else if (size >= 150.0f && size <= 600.0f)
		return 2.0f;
	else
		return 3.0f;
}

char *
ev_regular_paper_size (const EvDocumentInfo *info)
{
	GList *paper_sizes, *l;
	gchar *exact_size;
	gchar *str = NULL;
	GtkUnit units;

	units = get_default_user_units ();

	if (units == GTK_UNIT_MM) {
		exact_size = g_strdup_printf(_("%.0f × %.0f mm"),
					     info->paper_width,
					     info->paper_height);
	} else {
		exact_size = g_strdup_printf (_("%.2f × %.2f inch"),
					      info->paper_width  / 25.4f,
					      info->paper_height / 25.4f);
	}

	paper_sizes = gtk_paper_size_get_paper_sizes (FALSE);

	for (l = paper_sizes; l && l->data; l = g_list_next (l)) {
		GtkPaperSize *size = (GtkPaperSize *) l->data;
		gdouble paper_width;
		gdouble paper_height;
		gdouble width_tolerance;
		gdouble height_tolerance;

		paper_width = gtk_paper_size_get_width (size, GTK_UNIT_MM);
		paper_height = gtk_paper_size_get_height (size, GTK_UNIT_MM);

		width_tolerance = get_tolerance (paper_width);
		height_tolerance = get_tolerance (paper_height);

		if (ABS (info->paper_height - paper_height) <= height_tolerance &&
		    ABS (info->paper_width  - paper_width) <= width_tolerance) {
			/* Note to translators: first placeholder is the paper name (eg.
			 * A4), second placeholder is the paper size (eg. 297x210 mm) */
			str = g_strdup_printf (_("%s, Portrait (%s)"),
					       gtk_paper_size_get_display_name (size),
					       exact_size);
		} else if (ABS (info->paper_width  - paper_height) <= height_tolerance &&
			   ABS (info->paper_height - paper_width) <= width_tolerance) {
			/* Note to translators: first placeholder is the paper name (eg.
			 * A4), second placeholder is the paper size (eg. 297x210 mm) */
			str = g_strdup_printf ( _("%s, Landscape (%s)"),
						gtk_paper_size_get_display_name (size),
						exact_size);
		}
	}

	g_list_free_full (paper_sizes, (GDestroyNotify)gtk_paper_size_free);

	if (str != NULL) {
		g_free (exact_size);
		return str;
	}

	return exact_size;
}
