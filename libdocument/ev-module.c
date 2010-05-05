/*
 * ev-module.c
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
 
/* This is a modified version of ephy-module.c from Epiphany source code.
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
 * $Id: gedit-module.c 5367 2006-12-17 14:29:49Z pborelli $
 */

/* Modified by evince team */

#include "config.h"

#include "ev-module.h"

#include <gmodule.h>

typedef struct _EvModuleClass EvModuleClass;

struct _EvModuleClass {
        GTypeModuleClass parent_class;
};

struct _EvModule {
        GTypeModule parent_instance;

        GModule *library;
	gboolean resident;

        gchar *path;
        GType type;
};

typedef GType (*EvModuleRegisterFunc) (GTypeModule *);

static void ev_module_init       (EvModule *action);
static void ev_module_class_init (EvModuleClass *class);

#define ev_module_get_type _ev_module_get_type
G_DEFINE_TYPE (EvModule, ev_module, G_TYPE_TYPE_MODULE)

static gboolean
ev_module_load (GTypeModule *gmodule)
{
        EvModule *module = EV_MODULE (gmodule);
        EvModuleRegisterFunc register_func;

        module->library = g_module_open (module->path, 0);

        if (!module->library) {
                g_warning ("%s", g_module_error ());

                return FALSE;
        }

        /* extract symbols from the lib */
        if (!g_module_symbol (module->library, "register_evince_backend",
                              (void *) &register_func)) {
                g_warning ("%s", g_module_error ());
                g_module_close (module->library);

                return FALSE;
        }

        /* symbol can still be NULL even though g_module_symbol
         * returned TRUE */
        if (!register_func) {
                g_warning ("Symbol 'register_evince_backend' should not be NULL");
                g_module_close (module->library);

                return FALSE;
        }

        module->type = register_func (gmodule);

        if (module->type == 0) {
                g_warning ("Invalid evince backend contained by module %s", module->path);
		
                return FALSE;
        }

	if (module->resident)
		g_module_make_resident (module->library);

        return TRUE;
}

static void
ev_module_unload (GTypeModule *gmodule)
{
        EvModule *module = EV_MODULE (gmodule);

        g_module_close (module->library);
        module->library = NULL;
}

const gchar *
_ev_module_get_path (EvModule *module)
{
        g_return_val_if_fail (EV_IS_MODULE (module), NULL);

        return module->path;
}

GObject *
_ev_module_new_object (EvModule *module)
{
	g_return_val_if_fail (EV_IS_MODULE (module), NULL);
	
        if (module->type == 0)
                return NULL;

        return g_object_new (module->type, NULL);
}

GType
_ev_module_get_object_type (EvModule *module)
{
	g_return_val_if_fail (EV_IS_MODULE (module), 0);

	return module->type;
}

static void
ev_module_init (EvModule *module)
{
}

static void
ev_module_finalize (GObject *object)
{
        EvModule *module = EV_MODULE (object);

        g_free (module->path);

        G_OBJECT_CLASS (ev_module_parent_class)->finalize (object);
}

static void
ev_module_class_init (EvModuleClass *class)
{
        GObjectClass *object_class = G_OBJECT_CLASS (class);
        GTypeModuleClass *module_class = G_TYPE_MODULE_CLASS (class);

        object_class->finalize = ev_module_finalize;

        module_class->load = ev_module_load;
        module_class->unload = ev_module_unload;
}

EvModule *
_ev_module_new (const gchar *path,
                gboolean     resident)
{
        EvModule *result;

	g_return_val_if_fail (path != NULL && path[0] != '\0', NULL);

        result = g_object_new (EV_TYPE_MODULE, NULL);

        g_type_module_set_name (G_TYPE_MODULE (result), path);
        result->path = g_strdup (path);
	result->resident = resident;

        return result;
}
