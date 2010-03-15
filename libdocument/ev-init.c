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

#include "ev-init.h"
#include "ev-backends-manager.h"
#include "ev-debug.h"
#include "ev-file-helpers.h"

static int ev_init_count;

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
	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

        _ev_debug_init ();
        _ev_file_helpers_init ();
        have_backends = _ev_backends_manager_init ();

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

        _ev_backends_manager_shutdown ();
        _ev_file_helpers_shutdown ();
        _ev_debug_shutdown ();
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
