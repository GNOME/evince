/*
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
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "ev-portal.h"

/* Copied from gtk+, under LGPL2.1+ */

/**
 * ev_should_use_portal:
 *
 * Checks whether evince should use the portal.
 *
 * Returns: whether evince should use the portal
 *
 * Since: 3.30
 */
gboolean
ev_should_use_portal (void)
{
        static const char *use_portal = NULL;

        if (G_UNLIKELY (use_portal == NULL))
                {
                        char *path;

                        path = g_build_filename (g_get_user_runtime_dir (), "flatpak-info", NULL);
                        if (g_file_test (path, G_FILE_TEST_EXISTS))
                                use_portal = "1";
                        else
                                {
                                        use_portal = g_getenv ("GTK_USE_PORTAL");
                                        if (!use_portal)
                                                use_portal = "";
                                }
                        g_free (path);
                }

        return use_portal[0] == '1';
}
