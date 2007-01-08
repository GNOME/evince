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

#include "ev-file-exporter.h"

GType
ev_file_exporter_get_type (void)
{
        static GType type = 0;

        if (G_UNLIKELY (type == 0)) {
                const GTypeInfo our_info =
                {
                        sizeof (EvFileExporterIface),
                        NULL,
                        NULL,
                };

                type = g_type_register_static (G_TYPE_INTERFACE,
                                               "EvFileExporter",
                                               &our_info, (GTypeFlags)0);
        }

        return type;
}

gboolean
ev_file_exporter_format_supported (EvFileExporter      *exporter,
				   EvFileExporterFormat format)
{
	EvFileExporterIface *iface = EV_FILE_EXPORTER_GET_IFACE (exporter);

	if (format < EV_FILE_FORMAT_PS ||
	    format > EV_FILE_FORMAT_PDF)
		return FALSE;

	return iface->format_supported (exporter, format);
}

void
ev_file_exporter_begin (EvFileExporter      *exporter,
                        EvFileExporterFormat format,
                        const gchar         *filename,
                        gint                 first_page,
                        gint                 last_page,
                        gdouble              paper_width,
                        gdouble              paper_height,
                        gboolean             duplex)
{
        EvFileExporterIface *iface = EV_FILE_EXPORTER_GET_IFACE (exporter);

	g_return_if_fail (ev_file_exporter_format_supported (exporter, format));

        iface->begin (exporter, format, filename, first_page, last_page,
                      paper_width, paper_height, duplex);
}

void
ev_file_exporter_do_page (EvFileExporter *exporter, EvRenderContext *rc)
{
        EvFileExporterIface *iface = EV_FILE_EXPORTER_GET_IFACE (exporter);

        iface->do_page (exporter, rc);
}

void
ev_file_exporter_end (EvFileExporter *exporter)
{
        EvFileExporterIface *iface = EV_FILE_EXPORTER_GET_IFACE (exporter);

        iface->end (exporter);
}
