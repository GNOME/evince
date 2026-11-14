/*
 * Copyright (C) 2026 Germán Poo-Caamaño <gpoo@gnome.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <config.h>

#include <string.h>
#include <sys/wait.h>
#include <glib/gstdio.h>
#include <gio/gdesktopappinfo.h>

#include "ev-utils.h"

#define ARGS_SCRIPT "#!/bin/sh\nfor a; do printf '%s\\0' \"$a\"; done > \"${0%/*}/out\"\n"

typedef struct {
	gchar *dir;
	gchar *script;
	gchar *path;
	gchar *uri;
} Fixture;

static void
fixture_setup (Fixture       *fixture,
	       gconstpointer  data)
{
	GError *error = NULL;
	fixture->dir = g_dir_make_tmp ("test-ev-spawn-XXXXXX", &error);
	g_assert_no_error (error);

	fixture->script = g_build_filename (fixture->dir, "args.sh", NULL);
	g_file_set_contents (fixture->script, ARGS_SCRIPT, -1, &error);
	g_assert_no_error (error);
	g_assert_cmpint (g_chmod (fixture->script, 0700), ==, 0);

	fixture->path = g_build_filename (fixture->dir, "x --foo.pdf", NULL);
	fixture->uri = g_filename_to_uri (fixture->path, NULL, &error);
	g_assert_no_error (error);
}

static void
fixture_teardown (Fixture       *fixture,
		  gconstpointer  data)
{
	gchar *out;

	out = g_build_filename (fixture->dir, "out", NULL);
	g_unlink (out);
	g_free (out);
	g_unlink (fixture->script);
	g_rmdir (fixture->dir);

	g_free (fixture->dir);
	g_free (fixture->script);
	g_free (fixture->path);
	g_free (fixture->uri);
}

static void
child_pid_cb (GDesktopAppInfo *appinfo,
	      GPid             pid,
	      gpointer         user_data)
{
	*(GPid *) user_data = pid;
}

/* Launches the script the way ev_spawn() launches evince, and returns the
 * arguments it received. GLib passes the local file URI as a path.
 */
static gchar **
launch_with_find (Fixture     *fixture,
		  const gchar *value)
{
	GError    *error = NULL;
	GAppInfo  *app;
	GList      uris = { fixture->uri, NULL, NULL };
	GPid       pid = 0;
	gchar     *script, *quoted, *cmdline, *out, *contents;
	gsize      length;
	GPtrArray *args;
	gsize      i;

	script = ev_spawn_quote_argument (fixture->script);
	quoted = ev_spawn_quote_argument (value);
	cmdline = g_strdup_printf ("%s --find=%s", script, quoted);
	g_free (script);
	g_free (quoted);

	app = g_app_info_create_from_commandline (cmdline, NULL,
						  G_APP_INFO_CREATE_SUPPORTS_URIS,
						  &error);
	g_assert_no_error (error);
	g_free (cmdline);

	g_desktop_app_info_launch_uris_as_manager (G_DESKTOP_APP_INFO (app),
						   &uris, NULL,
						   G_SPAWN_DO_NOT_REAP_CHILD,
						   NULL, NULL,
						   child_pid_cb, &pid,
						   &error);
	g_assert_no_error (error);
	g_assert_cmpint (pid, >, 0);
	g_assert_cmpint (waitpid (pid, NULL, 0), ==, pid);
	g_object_unref (app);

	out = g_build_filename (fixture->dir, "out", NULL);
	g_file_get_contents (out, &contents, &length, &error);
	g_assert_no_error (error);
	g_unlink (out);
	g_free (out);

	args = g_ptr_array_new ();
	for (i = 0; i < length; i += strlen (contents + i) + 1)
		g_ptr_array_add (args, g_strdup (contents + i));
	g_ptr_array_add (args, NULL);
	g_free (contents);

	return (gchar **) g_ptr_array_free (args, FALSE);
}

static const gchar *find_values[] = {
	"%f",
	"%u",
	"%F %U %i %c %k",
	"%%",
	"50% off",
	"%",
	"a b -- --page-label=1",
	"it's \"quoted\" $HOME `id`",
};

static void
test_spawn_find_string (Fixture       *fixture,
			gconstpointer  data)
{
	gsize i;

	for (i = 0; i < G_N_ELEMENTS (find_values); i++) {
		gchar **args;
		gchar  *expected;

		args = launch_with_find (fixture, find_values[i]);
		expected = g_strconcat ("--find=", find_values[i], NULL);

		g_assert_cmpuint (g_strv_length (args), ==, 2);
		g_assert_cmpstr (args[0], ==, expected);
		g_assert_cmpstr (args[1], ==, fixture->path);

		g_free (expected);
		g_strfreev (args);
	}
}

int
main (int argc, char *argv[])
{
	g_test_init (&argc, &argv, NULL);

	g_test_add ("/shell/spawn/find-string", Fixture, NULL,
		    fixture_setup, test_spawn_find_string, fixture_teardown);

	return g_test_run ();
}
