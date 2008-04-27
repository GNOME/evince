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
 * Foundation, Inc., 59 Temple Place, Suite 330, 
 * Boston, MA 02111-1307, USA.
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

#include "ev-debug.h"

#ifdef EV_ENABLE_DEBUG
static EvDebugSection ev_debug = EV_NO_DEBUG;

void
ev_debug_init ()
{
	if (g_getenv ("EV_DEBUG") != NULL) {
		/* enable all debugging */
		ev_debug = ~EV_NO_DEBUG;
		return;
	}

	if (g_getenv ("EV_DEBUG_JOBS") != NULL)
		ev_debug |= EV_DEBUG_JOBS;
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

#endif /* EV_ENABLE_DEBUG */
