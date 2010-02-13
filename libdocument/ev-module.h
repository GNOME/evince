/*
 * ev-module.h
 * This file is part of Evince
 *
 * Copyright (C) 2005 - Paolo Maggi 
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
 
/* This is a modified version of gedit-module.h from Epiphany source code.
 * Here the original copyright assignment:
 *
 *  Copyright (C) 2003 Marco Pesenti Gritti
 *  Copyright (C) 2003, 2004 Christian Persch
 *
 */

/*
 * Modified by the gedit Team, 2005. See the AUTHORS file for a 
 * list of people on the gedit Team.  
 * See the ChangeLog files for a list of changes. 
 *
 * $Id: gedit-module.h 5263 2006-10-08 14:26:02Z pborelli $
 */

/* Modified by Evince Team */
 
#if !defined (EVINCE_COMPILATION)
#error "This is a private header."
#endif

#ifndef EV_MODULE_H
#define EV_MODULE_H

#include <glib-object.h>

G_BEGIN_DECLS

#define EV_TYPE_MODULE            (_ev_module_get_type ())
#define EV_MODULE(obj)		  (G_TYPE_CHECK_INSTANCE_CAST ((obj), EV_TYPE_MODULE, EvModule))
#define EV_MODULE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EV_TYPE_MODULE, EvModuleClass))
#define EV_IS_MODULE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EV_TYPE_MODULE))
#define EV_IS_MODULE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), EV_TYPE_MODULE))
#define EV_MODULE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), EV_TYPE_MODULE, EvModuleClass))

typedef struct _EvModule EvModule;

GType        _ev_module_get_type        (void) G_GNUC_CONST;

EvModule    *_ev_module_new             (const gchar *path,
                                        gboolean     resident);

const gchar *_ev_module_get_path        (EvModule    *module);

GObject     *_ev_module_new_object      (EvModule    *module);

GType        _ev_module_get_object_type (EvModule    *module);

G_END_DECLS

#endif /* EV_MODULE_H */
