/*
 * ev-debug.c
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
 * $Id: gedit-debug.c 4809 2006-04-08 14:46:31Z pborelli $
 */

/* Modified by Evince Team */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>

#include "ev-debug.h"

#ifdef EV_ENABLE_DEBUG
static EvDebugSection ev_debug = EV_NO_DEBUG;
static EvProfileSection ev_profile = EV_NO_PROFILE;
static EvDebugBorders ev_debug_borders = EV_DEBUG_BORDER_NONE;

static GHashTable *timers = NULL;

static gboolean
ev_debug_parse_show_borders (const gchar *show_debug_borders)
{
        gchar **items;
        guint   i;

        if (!show_debug_borders)
                return FALSE;

        items = g_strsplit (show_debug_borders, ",", -1);
        if (!items)
                return FALSE;

        for (i = 0; items[i]; i++) {
                if (g_strcmp0 (items[i], "all") == 0) {
                        ev_debug_borders = EV_DEBUG_BORDER_ALL;
                        break;
                }

                if (g_strcmp0 (items[i], "none") == 0) {
                        ev_debug_borders = EV_DEBUG_BORDER_NONE;
                        break;
                }

                if (strcmp (items[i], "chars") == 0)
                        ev_debug_borders |= EV_DEBUG_BORDER_CHARS;
                if (strcmp (items[i], "links") == 0)
                        ev_debug_borders |= EV_DEBUG_BORDER_LINKS;
                if (strcmp (items[i], "forms") == 0)
                        ev_debug_borders |= EV_DEBUG_BORDER_FORMS;
                if (strcmp (items[i], "annots") == 0)
                        ev_debug_borders |= EV_DEBUG_BORDER_ANNOTS;
                if (strcmp (items[i], "images") == 0)
                        ev_debug_borders |= EV_DEBUG_BORDER_IMAGES;
                if (strcmp (items[i], "selections") == 0)
                        ev_debug_borders |= EV_DEBUG_BORDER_SELECTIONS;
        }

        g_strfreev (items);

        return ev_debug_borders != EV_DEBUG_BORDER_NONE;
}

static void
debug_init (void)
{
	if (g_getenv ("EV_DEBUG") != NULL) {
		/* enable all debugging */
		ev_debug = ~EV_NO_DEBUG;
		return;
	}

	if (g_getenv ("EV_DEBUG_JOBS") != NULL)
		ev_debug |= EV_DEBUG_JOBS;

        if (ev_debug_parse_show_borders (g_getenv ("EV_DEBUG_SHOW_BORDERS")))
                ev_debug |= EV_DEBUG_SHOW_BORDERS;
}

static void
profile_init (void)
{
	if (g_getenv ("EV_PROFILE") != NULL) {
		/* enable all profiling */
		ev_profile = ~EV_NO_PROFILE;
	} else {
		if (g_getenv ("EV_PROFILE_JOBS") != NULL)
			ev_profile |= EV_PROFILE_JOBS;
	}

	if (ev_profile) {
		timers = g_hash_table_new_full (g_str_hash,
						g_str_equal,
						(GDestroyNotify) g_free,
						(GDestroyNotify) g_timer_destroy);
	}
}

void
_ev_debug_init (void)
{
	debug_init ();
	profile_init ();
}

void
_ev_debug_shutdown (void)
{
	if (timers) {
		g_hash_table_destroy (timers);
		timers = NULL;
	}
}

void
ev_debug_message (EvDebugSection  section,
		  const gchar    *file,
		  gint            line,
		  const gchar    *function,
		  const gchar    *format, ...)
{
	if (G_UNLIKELY (ev_debug & section)) {
		gchar *msg = NULL;

		if (format) {
			va_list args;
			
			va_start (args, format);
			msg = g_strdup_vprintf (format, args);
			va_end (args);
		}

		g_print ("%s:%d (%s) %s\n", file, line, function, msg ? msg : "");	

		fflush (stdout);

		g_free (msg);
	}
}

void
ev_profiler_start (EvProfileSection section,
		   const gchar     *format, ...)
{
	if (G_UNLIKELY (ev_profile & section)) {
		GTimer *timer;
		gchar  *name;
		va_list args;

		if (!format)
			return;

		va_start (args, format);
		name = g_strdup_vprintf (format, args);
		va_end (args);

		timer = g_hash_table_lookup (timers, name);
		if (!timer) {
			timer = g_timer_new ();
			g_hash_table_insert (timers, g_strdup (name), timer);
		} else {
			g_timer_start (timer);
		}
	}
}

void
ev_profiler_stop (EvProfileSection section,
		  const gchar     *format, ...)
{
	if (G_UNLIKELY (ev_profile & section)) {
		GTimer *timer;
		gchar  *name;
		va_list args;
		gdouble seconds;

		if (!format)
			return;

		va_start (args, format);
		name = g_strdup_vprintf (format, args);
		va_end (args);
		
		timer = g_hash_table_lookup (timers, name);
		if (!timer)
			return;
		
		g_timer_stop (timer);
		seconds = g_timer_elapsed (timer, NULL);
		g_print ("[ %s ] %f s elapsed\n", name, seconds);
		fflush (stdout);
	}
}

EvDebugBorders
ev_debug_get_debug_borders (void)
{
        return ev_debug_borders;
}

#endif /* EV_ENABLE_DEBUG */
