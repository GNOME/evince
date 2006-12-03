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

#ifndef EV_FILE_EXPORTER_H
#define EV_FILE_EXPORTER_H

#include <glib-object.h>

#include "ev-render-context.h"

G_BEGIN_DECLS

typedef enum {
        EV_FILE_FORMAT_PS,
        EV_FILE_FORMAT_PDF,
        EV_FILE_FORMAT_UNKNOWN
} EvFileExporterFormat;

#define EV_TYPE_FILE_EXPORTER            (ev_file_exporter_get_type ())
#define EV_FILE_EXPORTER(o)              (G_TYPE_CHECK_INSTANCE_CAST ((o), EV_TYPE_FILE_EXPORTER, EvFileExporter))
#define EV_FILE_EXPORTER_IFACE(k)        (G_TYPE_CHECK_CLASS_CAST((k), EV_TYPE_FILE_EXPORTER, EvFileExporterIface))
#define EV_IS_FILE_EXPORTER(o)           (G_TYPE_CHECK_INSTANCE_TYPE ((o), EV_TYPE_FILE_EXPORTER))
#define EV_IS_FILE_EXPORTER_IFACE(k)     (G_TYPE_CHECK_CLASS_TYPE ((k), EV_TYPE_FILE_EXPORTER))
#define EV_FILE_EXPORTER_GET_IFACE(inst) (G_TYPE_INSTANCE_GET_INTERFACE ((inst), EV_TYPE_FILE_EXPORTER, EvFileExporterIface))

typedef struct _EvFileExporter EvFileExporter;
typedef struct _EvFileExporterIface EvFileExporterIface;

struct _EvFileExporterIface {
        GTypeInterface base_iface;

        /* Methods  */
        gboolean   (* format_supported) (EvFileExporter      *exporter,
                                         EvFileExporterFormat format);
        void       (* begin)            (EvFileExporter      *exporter,
                                         EvFileExporterFormat format,
                                         const gchar         *filename,
                                         gint                 first_page,
                                         gint                 last_page,
                                         gdouble              paper_width,
                                         gdouble              paper_height,
                                         gboolean             duplex);
        void       (* do_page)          (EvFileExporter      *exporter,
                                         EvRenderContext     *rc);
        void       (* end)              (EvFileExporter      *exporter);
};

GType    ev_file_exporter_get_type         (void) G_GNUC_CONST;
gboolean ev_file_exporter_format_supported (EvFileExporter      *exporter,
                                            EvFileExporterFormat format);
void     ev_file_exporter_begin            (EvFileExporter      *exporter,
                                            EvFileExporterFormat format, 
                                            const gchar         *filename,
                                            gint                 first_page,
                                            gint                 last_page,
                                            gdouble              paper_width,
                                            gdouble              paper_height,
                                            gboolean             duplex);
void     ev_file_exporter_do_page          (EvFileExporter      *exporter,
                                            EvRenderContext     *rc);
void     ev_file_exporter_end              (EvFileExporter      *exporter);

G_END_DECLS

#endif
