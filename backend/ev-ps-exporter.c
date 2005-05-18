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

#include "config.h"

#include "ev-ps-exporter.h"

GType
ev_ps_exporter_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		static const GTypeInfo our_info =
		{
			sizeof (EvPSExporterIface),
			NULL,
			NULL,
		};

		type = g_type_register_static (G_TYPE_INTERFACE,
					       "EvPSExporter",
					       &our_info, (GTypeFlags)0);
	}

	return type;
}

void
ev_ps_exporter_begin (EvPSExporter *exporter, const char *filename,
		      int first_page, int last_page)
{
	EvPSExporterIface *iface = EV_PS_EXPORTER_GET_IFACE (exporter);

	iface->begin (exporter, filename, first_page, last_page);
}

void
ev_ps_exporter_do_page	(EvPSExporter *exporter, int page)
{
	EvPSExporterIface *iface = EV_PS_EXPORTER_GET_IFACE (exporter);

	iface->do_page (exporter, page);
}

void
ev_ps_exporter_end	(EvPSExporter *exporter)
{
	EvPSExporterIface *iface = EV_PS_EXPORTER_GET_IFACE (exporter);

	iface->end (exporter);
}
