/*
 * ev-debug.h
 * This file is part of Evince
 *
 * Copyright (C) 1998, 1999 Alex Roberts, Evan Lawrence
 * Copyright (C) 2000, 2001 Chema Celorio, Paolo Maggi
 * Copyright (C) 2002 - 2005 Paolo Maggi
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

/*
 * Modified by the gedit Team, 1998-2005. See the AUTHORS file for a
 * list of people on the gedit Team.
 * See the ChangeLog files for a list of changes.
 *
 * $Id: gedit-debug.h 4809 2006-04-08 14:46:31Z pborelli $
 */

/* Modified by Evince Team */

#pragma once

#if !defined (EVINCE_COMPILATION)
#error "This is a private header."
#endif

#include <glib-object.h>

#include "ev-macros.h"

G_BEGIN_DECLS

#define EV_GET_TYPE_NAME(instance) g_type_name_from_instance ((gpointer)instance)

#ifndef EV_ENABLE_DEBUG

#define _ev_debug_init()
#define _ev_debug_shutdown()
#define ev_debug_message(...) G_STMT_START { } G_STMT_END
#define ev_profiler_start(...) G_STMT_START { } G_STMT_END
#define ev_profiler_stop(...) G_STMT_START { } G_STMT_END

#else /* ENABLE_DEBUG */

/*
 * Set an environmental var of the same name to turn on
 * debugging output. Setting EV_DEBUG will turn on all
 * sections.
 */
typedef enum {
	EV_NO_DEBUG           = 0,
	EV_DEBUG_JOBS         = 1 << 0,
        EV_DEBUG_SHOW_BORDERS = 1 << 1
} EvDebugSection;

typedef enum {
        EV_DEBUG_BORDER_NONE       = 0,
        EV_DEBUG_BORDER_CHARS      = 1 << 0,
        EV_DEBUG_BORDER_LINKS      = 1 << 1,
        EV_DEBUG_BORDER_FORMS      = 1 << 2,
        EV_DEBUG_BORDER_ANNOTS     = 1 << 3,
        EV_DEBUG_BORDER_IMAGES     = 1 << 4,
        EV_DEBUG_BORDER_MEDIA      = 1 << 5,
        EV_DEBUG_BORDER_SELECTIONS = 1 << 6,
        EV_DEBUG_BORDER_ALL        = (1 << 7) - 1
} EvDebugBorders;

#define DEBUG_JOBS      EV_DEBUG_JOBS,    __FILE__, __LINE__, G_STRFUNC

void _ev_debug_init     (void);
void _ev_debug_shutdown (void);

EV_PRIVATE
void ev_debug_message  (EvDebugSection   section,
			const gchar     *file,
			gint             line,
			const gchar     *function,
			const gchar     *format, ...) G_GNUC_PRINTF(5, 6);
EV_PRIVATE
void ev_profiler_start (const gchar     *format, ...) G_GNUC_PRINTF(1, 2);
EV_PRIVATE
void ev_profiler_stop  (const gchar     *format, ...) G_GNUC_PRINTF(1, 2);

EV_PRIVATE
EvDebugBorders ev_debug_get_debug_borders (void);

#endif /* EV_ENABLE_DEBUG */

G_END_DECLS
