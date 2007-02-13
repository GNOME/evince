/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2007 Carlos Garcia Campos <carlosgc@gnome.org>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __PS_INTERPRETER_H__
#define __PS_INTERPRETER_H__

#include <glib-object.h>

#include "ps.h"

G_BEGIN_DECLS

typedef struct _PSInterpreter        PSInterpreter;
typedef struct _PSInterpreterClass   PSInterpreterClass;

#define PS_TYPE_INTERPRETER              (ps_interpreter_get_type())
#define PS_INTERPRETER(object)           (G_TYPE_CHECK_INSTANCE_CAST((object), PS_TYPE_INTERPRETER, PSInterpreter))
#define PS_INTERPRETER_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), PS_TYPE_INTERPRETER, PSInterpreterClass))
#define PS_IS_INTERPRETER(object)        (G_TYPE_CHECK_INSTANCE_TYPE((object), PS_TYPE_INTERPRETER))
#define PS_IS_INTERPRETER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass), PS_TYPE_INTERPRETER))
#define PS_INTERPRETER_GET_CLASS(object) (G_TYPE_INSTANCE_GET_CLASS((object), PS_TYPE_INTERPRETER, PSInterpreterClass))

GType          ps_interpreter_get_type    (void) G_GNUC_CONST;
PSInterpreter *ps_interpreter_new         (const gchar           *filename,
					   const struct document *doc);
void           ps_interpreter_render_page (PSInterpreter         *gs,
					   gint                   page,
					   gdouble                scale,
					   gint                   rotation);

G_END_DECLS

#endif /* __PS_INTERPRETER_H__ */
