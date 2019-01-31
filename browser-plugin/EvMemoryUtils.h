/*
 * Copyright (C) 2014 Igalia S.L.
 *
 * Evince is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Evince is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef EvMemoryUtils_h
#define EvMemoryUtils_h

#include <glib.h>
#include <memory>

template<typename T>
struct unique_gptr_deleter {
        void operator()(T* ptr) const { g_free(ptr); }
};

template<typename T>
using unique_gptr = std::unique_ptr<T, unique_gptr_deleter<T>>;

#endif // EvMemoryUtils_h
