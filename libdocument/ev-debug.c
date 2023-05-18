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
static EvDebugBorders ev_debug_borders = EV_DEBUG_BORDER_NONE;

void
_ev_debug_init (void)
{
        const GDebugKey keys[] = {
                { "jobs",    EV_DEBUG_JOBS         },
                { "borders", EV_DEBUG_SHOW_BORDERS }
        };
        const GDebugKey border_keys[] = {
                { "chars",      EV_DEBUG_BORDER_CHARS      },
                { "links",      EV_DEBUG_BORDER_LINKS      },
                { "forms",      EV_DEBUG_BORDER_FORMS      },
                { "annots",     EV_DEBUG_BORDER_ANNOTS     },
                { "images",     EV_DEBUG_BORDER_IMAGES     },
                { "media",      EV_DEBUG_BORDER_MEDIA      },
                { "selections", EV_DEBUG_BORDER_SELECTIONS }
        };

        ev_debug = g_parse_debug_string (g_getenv ("EV_DEBUG"), keys, G_N_ELEMENTS (keys));
        if (ev_debug & EV_DEBUG_SHOW_BORDERS)
                ev_debug_borders = g_parse_debug_string (g_getenv ("EV_DEBUG_SHOW_BORDERS"),
                                                         border_keys, G_N_ELEMENTS (border_keys));
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

EvDebugBorders
ev_debug_get_debug_borders (void)
{
        return ev_debug_borders;
}

#endif /* EV_ENABLE_DEBUG */
