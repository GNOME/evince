/*
 *  Copyright (C) 2003 Marco Pesenti Gritti
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  $Id$
 */

#include "config.h"

#include "ev-debug.h"

#ifndef DISABLE_PROFILING

#include <glib/gbacktrace.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_EXECINFO_H
#include <execinfo.h>
#endif

static GHashTable *ev_profilers_hash = NULL;
static const char *ev_profile_modules = NULL;
static const char *ev_debug_break = NULL;

#endif

#ifndef DISABLE_LOGGING

static const char *ev_log_modules;

static void
log_module (const gchar *log_domain,
	    GLogLevelFlags log_level,
	    const gchar *message,
	    gpointer user_data)
{
	gboolean should_log = FALSE;

	if (!ev_log_modules) return;

	if (strcmp (ev_log_modules, "all") != 0)
	{
		char **modules;
		int i;

		modules = g_strsplit (ev_log_modules, ":", 100);

		for (i = 0; modules[i] != NULL; i++)
		{
			if (strstr (message, modules [i]) != NULL)
			{
				should_log = TRUE;
				break;
			}
		}

		g_strfreev (modules);
	}
	else
	{
		should_log = TRUE;
	}

	if (should_log)
	{
		g_print ("%s\n", message);
	}
}

#define MAX_DEPTH 200

static void 
trap_handler (const char *log_domain,
	      GLogLevelFlags log_level,
	      const char *message,
	      gpointer user_data)
{
	g_log_default_handler (log_domain, log_level, message, user_data);

	if (ev_debug_break != NULL &&
	    (log_level & (G_LOG_LEVEL_WARNING |
			  G_LOG_LEVEL_ERROR |
			  G_LOG_LEVEL_CRITICAL |
			  G_LOG_FLAG_FATAL)))
	{
		if (strcmp (ev_debug_break, "stack") == 0)
		{
#ifdef HAVE_EXECINFO_H
			void *array[MAX_DEPTH];
			size_t size;
			
			size = backtrace (array, MAX_DEPTH);
			backtrace_symbols_fd (array, size, 2);
#else
			g_on_error_stack_trace (g_get_prgname ());
#endif
		}
		else if (strcmp (ev_debug_break, "trap") == 0)
		{
			G_BREAKPOINT ();
		}
		else if (strcmp (ev_debug_break, "suspend") == 0)
		{
			g_print ("Suspending program; attach with the debugger.\n");

			raise (SIGSTOP);
		}
	}
}

#endif

void
ev_debug_init (void)
{
#ifndef DISABLE_LOGGING
	ev_log_modules = g_getenv ("EV_LOG_MODULES");
	ev_debug_break = g_getenv ("EV_DEBUG_BREAK");

	g_log_set_default_handler (trap_handler, NULL);

	g_log_set_handler (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, log_module, NULL);

#endif
#ifndef DISABLE_PROFILING
	ev_profile_modules = g_getenv ("EV_PROFILE_MODULES");
#endif
}

#ifndef DISABLE_PROFILING

static EvProfiler *
ev_profiler_new (const char *name, const char *module)
{
	EvProfiler *profiler;

	profiler = g_new0 (EvProfiler, 1);
	profiler->timer = g_timer_new ();
	profiler->name  = g_strdup (name);
	profiler->module  = g_strdup (module);

	g_timer_start (profiler->timer);

	return profiler;
}

static gboolean
ev_should_profile (const char *module)
{
	char **modules;
	int i;
	gboolean res = FALSE;

	if (!ev_profile_modules) return FALSE;
	if (strcmp (ev_profile_modules, "all") == 0) return TRUE;

	modules = g_strsplit (ev_profile_modules, ":", 100);

	for (i = 0; modules[i] != NULL; i++)
	{
		if (strcmp (module, modules [i]) == 0)
		{
			res = TRUE;
			break;
		}
	}

	g_strfreev (modules);

	return res;
}

static void
ev_profiler_dump (EvProfiler *profiler)
{
	double seconds;

	g_return_if_fail (profiler != NULL);

	seconds = g_timer_elapsed (profiler->timer, NULL);

	g_print ("[ %s ] %s %f s elapsed\n",
		 profiler->module, profiler->name,
		 seconds);
}

static void
ev_profiler_free (EvProfiler *profiler)
{
	g_return_if_fail (profiler != NULL);

	g_timer_destroy (profiler->timer);
	g_free (profiler->name);
	g_free (profiler->module);
	g_free (profiler);
}

void
ev_profiler_start (const char *name, const char *module)
{
	EvProfiler *profiler;

	if (ev_profilers_hash == NULL)
	{
		ev_profilers_hash =
			g_hash_table_new_full (g_str_hash, g_str_equal,
					       g_free, NULL);
	}

	if (!ev_should_profile (module)) return;

	profiler = ev_profiler_new (name, module);

	g_hash_table_insert (ev_profilers_hash, g_strdup (name), profiler);
}

void
ev_profiler_stop (const char *name)
{
	EvProfiler *profiler;

	profiler = g_hash_table_lookup (ev_profilers_hash, name);
	if (profiler == NULL) return;
	g_hash_table_remove (ev_profilers_hash, name);

	ev_profiler_dump (profiler);
	ev_profiler_free (profiler);
}

#endif
