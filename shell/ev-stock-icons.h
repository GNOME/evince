/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Stock icons for Evince
 *
 * Copyright (C) 2003 Martin Kretzschmar
 *
 * Author:
 *   Martin Kretzschmar <Martin.Kretzschmar@inf.tu-dresden.de>
 *
 * GPdf is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * GPdf is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
 * License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __EV_STOCK_ICONS_H__
#define __EV_STOCK_ICONS_H__

#include <glib/gmacros.h>

G_BEGIN_DECLS

/* Evince stock icons */
#define EV_STOCK_ZOOM	                "zoom"
#define EV_STOCK_ZOOM_PAGE              "zoom-fit-page" 
#define EV_STOCK_ZOOM_WIDTH             "zoom-fit-width" 
#define EV_STOCK_VIEW_DUAL       	"view-page-facing"
#define EV_STOCK_VIEW_CONTINUOUS        "view-page-continuous"
#define EV_STOCK_ROTATE_LEFT            "object-rotate-left"
#define EV_STOCK_ROTATE_RIGHT           "object-rotate-right"
#define EV_STOCK_RUN_PRESENTATION       "x-office-presentation"

void ev_stock_icons_init (void);

G_END_DECLS

#endif /* __EV_STOCK_ICONS_H__ */
