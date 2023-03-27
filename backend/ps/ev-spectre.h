/*
 * Ghostscript widget for GTK/GNOME
 *
 * Copyright 1998 - 2005 The Free Software Foundation
 *
 * Authors: Jaka Mocnik, Federico Mena (Quartic), Szekeres Istvan (Pista)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#include <glib-object.h>

#include "ev-macros.h"
#include "ev-document.h"

G_BEGIN_DECLS

#define PS_TYPE_DOCUMENT           (ps_document_get_type())
#define PS_DOCUMENT(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), PS_TYPE_DOCUMENT, PSDocument))
#define PS_DOCUMENT_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST ((klass), PS_TYPE_DOCUMENT, PSDocumentClass))
#define PS_IS_DOCUMENT(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PS_TYPE_DOCUMENT))
#define PS_DOCUMENT_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), PS_TYPE_DOCUMENT, PSDocumentClass))

typedef struct _PSDocument      PSDocument;
typedef struct _PSDocumentClass PSDocumentClass;

GType                 ps_document_get_type    (void) G_GNUC_CONST;

G_END_DECLS
