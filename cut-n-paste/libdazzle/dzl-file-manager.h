/* dzl-file-manager.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef DZL_FILE_MANAGER_H
#define DZL_FILE_MANAGER_H

#include <gio/gio.h>

#include "dzl-version-macros.h"

G_BEGIN_DECLS

DZL_AVAILABLE_IN_ALL
gboolean dzl_file_manager_show (GFile   *file,
                                GError **error);

G_END_DECLS

#endif /* DZL_FILE_MANAGER_H */
