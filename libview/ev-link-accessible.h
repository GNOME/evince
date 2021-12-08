/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-indent-level: 8 -*- */
/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2013 Carlos Garcia Campos <carlosgc@gnome.org>
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

#pragma once

#if !defined (EVINCE_COMPILATION)
#error "This is a private header."
#endif

#include "ev-page-accessible.h"
#include "ev-link.h"

#define EV_TYPE_LINK_ACCESSIBLE      (ev_link_accessible_get_type ())
#define EV_LINK_ACCESSIBLE(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), EV_TYPE_LINK_ACCESSIBLE, EvLinkAccessible))
#define EV_IS_LINK_ACCESSIBLE(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EV_TYPE_LINK_ACCESSIBLE))

typedef struct _EvLinkAccessible        EvLinkAccessible;
typedef struct _EvLinkAccessibleClass   EvLinkAccessibleClass;
typedef struct _EvLinkAccessiblePrivate EvLinkAccessiblePrivate;

struct _EvLinkAccessible {
        AtkObject parent;

        EvLinkAccessiblePrivate *priv;
};

struct _EvLinkAccessibleClass {
        AtkObjectClass parent_class;
};

GType             ev_link_accessible_get_type (void);
EvLinkAccessible *ev_link_accessible_new      (EvPageAccessible *page,
                                               EvLink           *link,
                                               EvRectangle      *area);
