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

#ifndef EV_PS_EXPORTER_H
#define EV_PS_EXPORTER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define EV_TYPE_PS_EXPORTER	       (ev_ps_exporter_get_type ())
#define EV_PS_EXPORTER(o)	       (G_TYPE_CHECK_INSTANCE_CAST ((o), EV_TYPE_PS_EXPORTER, EvPSExporter))
#define EV_PS_EXPORTER_IFACE(k)	       (G_TYPE_CHECK_CLASS_CAST((k), EV_TYPE_PS_EXPORTER, EvPSExporterIface))
#define EV_IS_PS_EXPORTER(o)	       (G_TYPE_CHECK_INSTANCE_TYPE ((o), EV_TYPE_PS_EXPORTER))
#define EV_IS_PS_EXPORTER_IFACE(k)     (G_TYPE_CHECK_CLASS_TYPE ((k), EV_TYPE_PS_EXPORTER))
#define EV_PS_EXPORTER_GET_IFACE(inst) (G_TYPE_INSTANCE_GET_INTERFACE ((inst), EV_TYPE_PS_EXPORTER, EvPSExporterIface))

typedef struct _EvPSExporter EvPSExporter;
typedef struct _EvPSExporterIface EvPSExporterIface;

struct _EvPSExporterIface {
	GTypeInterface base_iface;

	/* Methods  */
	void		(* begin)	(EvPSExporter *exporter,
					 const char   *filename);
	void		(* do_page)	(EvPSExporter *exporter,
					 int	       page);
	void		(* end)		(EvPSExporter *exporter);
};

GType	ev_ps_exporter_get_type (void);
void	ev_ps_exporter_begin	(EvPSExporter *exporter, const char *filename);
void	ev_ps_exporter_do_page	(EvPSExporter *exporter, int page);
void	ev_ps_exporter_end	(EvPSExporter *exporter);

G_END_DECLS

#endif
