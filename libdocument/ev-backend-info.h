/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2007 Carlos Garcia Campos <carlosgc@gnome.org>
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

#if !defined (EVINCE_COMPILATION)
#error "This is a private header."
#endif

#ifndef EV_BACKEND_INFO
#define EV_BACKEND_INFO

#include <glib.h>

G_BEGIN_DECLS

typedef struct _EvBackendInfo EvBackendInfo;

struct _EvBackendInfo {
        /* These two fields must be first for API/ABI compat with EvTypeInfo */
        gchar       *type_desc;
        gchar      **mime_types;

        volatile int ref_count;

	gchar       *module_name;
	gboolean     resident;
};

EvBackendInfo *_ev_backend_info_ref           (EvBackendInfo *info);

void           _ev_backend_info_unref         (EvBackendInfo *info);

EvBackendInfo *_ev_backend_info_new_from_file (const char *file,
                                               GError **error);

GList         *_ev_backend_info_load_from_dir (const char *path);

G_END_DECLS

#endif /* !EV_BACKEND_INFO */
