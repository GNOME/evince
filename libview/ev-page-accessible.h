/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-indent-level: 8 -*- */
/* this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2014 Igalia S.L.
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
 *
 * Author: Alejandro Piñeiro Iglesias <apinheiro@igalia.com>
 */

#pragma once

#if !defined (EVINCE_COMPILATION)
#error "This is a private header."
#endif

#include "ev-view-accessible.h"
#include "ev-view.h"

#define EV_TYPE_PAGE_ACCESSIBLE      (ev_page_accessible_get_type ())
#define EV_PAGE_ACCESSIBLE(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), EV_TYPE_PAGE_ACCESSIBLE, EvPageAccessible))
#define EV_IS_PAGE_ACCESSIBLE(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EV_TYPE_PAGE_ACCESSIBLE))

typedef struct _EvPageAccessible        EvPageAccessible;
typedef struct _EvPageAccessibleClass   EvPageAccessibleClass;
typedef struct _EvPageAccessiblePrivate EvPageAccessiblePrivate;

struct _EvPageAccessible {
        AtkObject parent;

        EvPageAccessiblePrivate *priv;
};

struct _EvPageAccessibleClass {
        AtkObjectClass parent_class;
};

GType             ev_page_accessible_get_type            (void) G_GNUC_CONST;
EvPageAccessible *ev_page_accessible_new                 (EvViewAccessible *view_accessible,
							  gint              page);
gint              ev_page_accessible_get_page            (EvPageAccessible *page_accessible);
EvViewAccessible *ev_page_accessible_get_view_accessible (EvPageAccessible *page_accessible);
EvView           *ev_page_accessible_get_view            (EvPageAccessible *page_accessible);
AtkObject        *ev_page_accessible_get_accessible_for_mapping (EvPageAccessible *page_accessible,
								 EvMapping        *mapping);
void              ev_page_accessible_update_element_state (EvPageAccessible *page_accessible,
							   EvMapping        *mapping);
