/*
 * ev-debug.h
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
 * $Id: gedit-debug.h 4809 2006-04-08 14:46:31Z pborelli $
 */

/* Modified by Evince Team */

#ifndef __EV_DEBUG_H__
#define __EV_DEBUG_H__

#include <glib.h>

#ifndef EV_ENABLE_DEBUG
#define ev_debug_init()
#define ev_debug_message(...)
#else

G_BEGIN_DECLS

/*
 * Set an environmental var of the same name to turn on
 * debugging output. Setting EV_DEBUG will turn on all
 * sections.
 */
typedef enum {
	EV_NO_DEBUG          = 0,
	EV_DEBUG_JOBS        = 1 << 0
} EvDebugSection;


#define	DEBUG_JOBS	EV_DEBUG_JOBS,    __FILE__, __LINE__, G_STRFUNC

void ev_debug_init (void);

void ev_debug_message (EvDebugSection  section,
		       const gchar    *file,
		       gint            line,
		       const gchar    *function,
		       const gchar    *format, ...) G_GNUC_PRINTF(5, 6);


G_END_DECLS

#endif /* EV_ENABLE_DEBUG */
#endif /* __EV_DEBUG_H__ */
