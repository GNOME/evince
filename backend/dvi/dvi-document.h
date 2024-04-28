/* dvi-document.h: Implementation of EvDocument for dvi documents
 * Copyright (C) 2005, Nickolay V. Shmyrev <nshmyrev@yandex.ru>
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

#pragma once

#include "ev-macros.h"
#include "ev-document.h"

G_BEGIN_DECLS

#define DVI_TYPE_DOCUMENT             (dvi_document_get_type ())
#define DVI_DOCUMENT(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), DVI_TYPE_DOCUMENT, DviDocument))
#define DVI_IS_DOCUMENT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), DVI_TYPE_DOCUMENT))

typedef struct _DviDocument DviDocument;

GType                 dvi_document_get_type   (void) G_GNUC_CONST;

G_END_DECLS
