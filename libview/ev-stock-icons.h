/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Stock icons for Evince
 *
 * Copyright (C) 2003 Martin Kretzschmar
 *
 * Author:
 *   Martin Kretzschmar <Martin.Kretzschmar@inf.tu-dresden.de>
 *
 * Evince is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Evince is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
 * License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#if !defined (__EV_EVINCE_VIEW_H_INSIDE__) && !defined (EVINCE_COMPILATION)
#error "Only <evince-view.h> can be included directly."
#endif

#ifndef __EV_STOCK_ICONS_H__
#define __EV_STOCK_ICONS_H__

#include <gdk/gdk.h>

G_BEGIN_DECLS

/* Evince stock icons */
#define EV_STOCK_ZOOM	                "zoom"
#define EV_STOCK_ZOOM_PAGE              "zoom-fit-height"
#define EV_STOCK_ZOOM_WIDTH             "zoom-fit-width"
#define EV_STOCK_VIEW_DUAL       	"view-page-facing"
#define EV_STOCK_VIEW_CONTINUOUS        "view-page-continuous"
#define EV_STOCK_ROTATE_LEFT            "object-rotate-left"
#define EV_STOCK_ROTATE_RIGHT           "object-rotate-right"
#define EV_STOCK_RUN_PRESENTATION       "x-office-presentation"
#define EV_STOCK_VISIBLE                "eye"
#define EV_STOCK_RESIZE_SE              "resize-se"
#define EV_STOCK_RESIZE_SW              "resize-sw"
#define EV_STOCK_CLOSE                  "close"
#define EV_STOCK_INVERTED_COLORS        "inverted"
#define EV_STOCK_ATTACHMENT             "mail-attachment"
#define EV_STOCK_SEND_TO                "document-send"

void ev_stock_icons_init       (void);
void ev_stock_icons_shutdown   (void);
void ev_stock_icons_set_screen (GdkScreen *screen);

G_END_DECLS

#endif /* __EV_STOCK_ICONS_H__ */
