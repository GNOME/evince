/* pdfdocument.h: Implementation of EvDocument for pixbufs
 * Copyright (C) 2004, Anders Carlsson <andersca@gnome.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef __PIXBUF_DOCUMENT_H__
#define __PIXBUF_DOCUMENT_H__

#include "ev-document.h"

G_BEGIN_DECLS

#define PIXBUF_TYPE_DOCUMENT             (pixbuf_document_get_type ())
#define PIXBUF_DOCUMENT(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), PIXBUF_TYPE_DOCUMENT, PixbufDocument))
#define PIXBUF_IS_DOCUMENT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PIXBUF_TYPE_DOCUMENT))

typedef struct _PixbufDocument PixbufDocument;

GType                 pixbuf_document_get_type (void) G_GNUC_CONST;

G_MODULE_EXPORT GType register_evince_backend  (GTypeModule *module); 
     
G_END_DECLS

#endif /* __PIXBUF_DOCUMENT_H__ */
