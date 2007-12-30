/* EggFileFormatChooser
 * Copyright (C) 2007 Mathias Hasselmann
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifndef __EGG_FILE_FORMAT_CHOOSER_H__
#define __EGG_FILE_FORMAT_CHOOSER_H__

#include <gtk/gtkexpander.h>

G_BEGIN_DECLS

#define EGG_TYPE_FILE_FORMAT_CHOOSER           (egg_file_format_chooser_get_type())
#define EGG_FILE_FORMAT_CHOOSER(obj)           (G_TYPE_CHECK_INSTANCE_CAST(obj, EGG_TYPE_FILE_FORMAT_CHOOSER, EggFileFormatChooser))
#define EGG_FILE_FORMAT_CHOOSER_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST(klass, EGG_TYPE_FILE_FORMAT_CHOOSER, EggFileFormatChooserClass))
#define EGG_IS_FILE_FORMAT_CHOOSER(obj)        (G_TYPE_CHECK_INSTANCE_TYPE(obj, EGG_TYPE_FILE_FORMAT_CHOOSER))
#define EGG_IS_FILE_FORMAT_CHOOSER_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE(obj, EGG_TYPE_FILE_FORMAT_CHOOSER))
#define EGG_FILE_FORMAT_CHOOSER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), EGG_TYPE_FILE_FORMAT_CHOOSER, EggFileFormatChooserClass))

typedef struct _EggFileFormatChooser        EggFileFormatChooser;
typedef struct _EggFileFormatChooserClass   EggFileFormatChooserClass;
typedef struct _EggFileFormatChooserPrivate EggFileFormatChooserPrivate;

struct _EggFileFormatChooser
{
  GtkExpander parent;
  EggFileFormatChooserPrivate *priv;
};

struct _EggFileFormatChooserClass
{
  GtkExpanderClass parent;

  void (*selection_changed)(EggFileFormatChooser *self);
};

GType           egg_file_format_chooser_get_type           (void) G_GNUC_CONST;
GtkWidget*      egg_file_format_chooser_new                (void);

guint           egg_file_format_chooser_add_format         (EggFileFormatChooser *self,
                                                            guint                 parent,
                                                            const gchar          *name,
                                                            const gchar          *icon,
                                                            ...) G_GNUC_NULL_TERMINATED;
void            egg_file_format_chooser_add_pixbuf_formats (EggFileFormatChooser *self,
                                                            guint                 parent,
                                                            guint               **formats);
void            egg_file_format_chooser_remove_format      (EggFileFormatChooser *self,
                                                            guint                 format);

void            egg_file_format_chooser_set_format         (EggFileFormatChooser *self,
                                                            guint                 format);
guint           egg_file_format_chooser_get_format         (EggFileFormatChooser *self,
                                                            const gchar          *filename);

void            egg_file_format_chooser_set_format_data    (EggFileFormatChooser *self,
                                                            guint                 format,
                                                            gpointer              data,
                                                            GDestroyNotify        destroy);
gpointer        egg_file_format_chooser_get_format_data    (EggFileFormatChooser *self,
                                                            guint                 format);

gchar*          egg_file_format_chooser_append_extension   (EggFileFormatChooser *self,
                                                            const gchar          *filename,
                                                            guint                 format);

G_END_DECLS

#endif /* __EGG_FILE_FORMAT_CHOOSER_H__ */

/* vim: set sw=2 sta et: */
