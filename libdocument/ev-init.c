/* this file is part of evince, a gnome document viewer
 *
 * Copyright © 2009 Christian Persch
 *
 * Evince is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * Evince is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <config.h>

#include <glib.h>
#include <glib/gi18n-lib.h>
#ifdef G_OS_WIN32
#include <windows.h>
#endif

#include <exempi/xmp.h>

#include "ev-init.h"
#include "ev-document-factory.h"
#include "ev-debug.h"
#include "ev-file-helpers.h"

static int ev_init_count;

#ifdef G_OS_WIN32

static HMODULE evdocument_dll = NULL;
static gchar *locale_dir = NULL;

#ifdef DLL_EXPORT
BOOL WINAPI DllMain (HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved);
BOOL WINAPI
DllMain (HINSTANCE hinstDLL,
	 DWORD     fdwReason,
	 LPVOID    lpvReserved)
{
	if (fdwReason == DLL_PROCESS_ATTACH)
		evdocument_dll = hinstDLL;

	return TRUE;
}
#endif

static const gchar *
_ev_win32_get_locale_dir (HMODULE module)
{
	if (locale_dir)
		return locale_dir;

	gchar *install_dir = NULL, *utf8_locale_dir;

	if (evdocument_dll != NULL)
		install_dir =
		g_win32_get_package_installation_directory_of_module (module);

	if (install_dir) {
		utf8_locale_dir = g_build_filename (install_dir,
			"share", "locale", NULL);

		locale_dir = g_win32_locale_filename_from_utf8 (utf8_locale_dir);

		g_free (install_dir);
		g_free (utf8_locale_dir);
	}

	if (!locale_dir)
		locale_dir = g_strdup ("");

	return locale_dir;
}

#endif

const gchar *
ev_get_locale_dir (void)
{
#ifdef G_OS_WIN32
	return _ev_win32_get_locale_dir (evdocument_dll);
#else
	return EV_LOCALEDIR;
#endif
}

/**
 * ev_init:
 *
 * Initializes the evince document library, and binds the evince
 * gettext domain.
 *
 * You must call this before calling any other function in the evince
 * document library.
 *
 * Returns: %TRUE if any backends were found; %FALSE otherwise
 */
gboolean
ev_init (void)
{
        static gboolean have_backends;

        if (ev_init_count++ > 0)
                return have_backends;

	/* set up translation catalog */
	bindtextdomain (GETTEXT_PACKAGE, ev_get_locale_dir ());
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

	xmp_init ();
        gdk_pixbuf_init_modules (EXTRA_GDK_PIXBUF_LOADERS_DIR, NULL);
        _ev_debug_init ();
        _ev_file_helpers_init ();
        have_backends = _ev_document_factory_init ();

        return have_backends;
}

/**
 * ev_shutdown:
 *
 * Shuts the evince document library down.
 */
void
ev_shutdown (void)
{
        g_assert (_ev_is_initialized ());

        if (--ev_init_count > 0)
                return;

#ifdef G_OS_WIN32
	if (locale_dir != NULL)
		g_free(locale_dir);
#endif

	xmp_terminate ();
        _ev_document_factory_shutdown ();
        _ev_file_helpers_shutdown ();
}

/*
 * _ev_is_initialized:
 *
 * Returns: %TRUE if the evince document library has been initialized
 */
gboolean
_ev_is_initialized (void)
{
        return ev_init_count > 0;
}
