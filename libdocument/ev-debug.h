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

#if !defined (EVINCE_COMPILATION)
#error "This is a private header."
#endif

#ifndef __EV_DEBUG_H__
#define __EV_DEBUG_H__

#include <glib-object.h>

#define EV_GET_TYPE_NAME(instance) g_type_name_from_instance ((gpointer)instance)

#ifndef EV_ENABLE_DEBUG

#define _ev_debug_init()
#define _ev_debug_shutdown()
#if defined(G_HAVE_GNUC_VARARGS)
#define ev_debug_message(section, format, args...) G_STMT_START { } G_STMT_END
#define ev_profiler_start(format, args...) G_STMT_START { } G_STMT_END
#define ev_profiler_stop(format, args...) G_STMT_START { } G_STMT_END
#elif defined(G_HAVE_ISO_VARARGS)
#define ev_debug_message(...) G_STMT_START { } G_STMT_END
#define ev_profiler_start(...) G_STMT_START { } G_STMT_END
#define ev_profiler_stop(...) G_STMT_START { } G_STMT_END
#else /* no varargs macros */
static void ev_debug_message(EvDebugSection section, const gchar *file, gint line, const gchar *function, const gchar *format, ...) {}
static void ev_profiler_start(EvProfileSection section,	const gchar *format, ...) {}
static void ev_profiler_stop(EvProfileSection section, const gchar *format, ...) {}
#endif

#else /* ENABLE_DEBUG */

G_BEGIN_DECLS

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
        EV_DEBUG_BORDER_SELECTIONS = 1 << 5,
        EV_DEBUG_BORDER_ALL        = (1 << 6) - 1
} EvDebugBorders;

#define DEBUG_JOBS      EV_DEBUG_JOBS,    __FILE__, __LINE__, G_STRFUNC

/*
 * Set an environmental var of the same name to turn on
 * profiling. Setting EV_PROFILE will turn on all
 * sections.
 */
typedef enum {
	EV_NO_PROFILE   = 0,
	EV_PROFILE_JOBS = 1 << 0
} EvProfileSection;

void _ev_debug_init     (void);
void _ev_debug_shutdown (void);

void ev_debug_message  (EvDebugSection   section,
			const gchar     *file,
			gint             line,
			const gchar     *function,
			const gchar     *format, ...) G_GNUC_PRINTF(5, 6);
void ev_profiler_start (EvProfileSection section,
			const gchar     *format, ...) G_GNUC_PRINTF(2, 3);
void ev_profiler_stop  (EvProfileSection section,
			const gchar     *format, ...) G_GNUC_PRINTF(2, 3);

EvDebugBorders ev_debug_get_debug_borders (void);

G_END_DECLS

#endif /* EV_ENABLE_DEBUG */
#endif /* __EV_DEBUG_H__ */
