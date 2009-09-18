/*
 * gtkinfobar.h
 * This file is part of GTK+
 *
 * Copyright (C) 2005 - Paolo Maggi
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the gedit Team, 2005. See the gedit AUTHORS file for a
 * list of people on the gedit Team.
 * See the gedit ChangeLog files for a list of changes.
 *
 * Modified by the GTK+ Team, 2008-2009.
 */

#ifndef __EV_INFO_BAR_H__
#define __EV_INFO_BAR_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

/*
 * Type checking and casting macros
 */
#define EV_TYPE_INFO_BAR              (ev_info_bar_get_type())
#define EV_INFO_BAR(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), EV_TYPE_INFO_BAR, EvInfoBar))
#define EV_INFO_BAR_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), EV_TYPE_INFO_BAR, EvInfoBarClass))
#define EV_IS_INFO_BAR(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), EV_TYPE_INFO_BAR))
#define EV_IS_INFO_BAR_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), EV_TYPE_INFO_BAR))
#define EV_INFO_BAR_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), EV_TYPE_INFO_BAR, EvInfoBarClass))


typedef struct _EvInfoBarPrivate EvInfoBarPrivate;
typedef struct _EvInfoBarClass EvInfoBarClass;
typedef struct _EvInfoBar EvInfoBar;


struct _EvInfoBar
{
  GtkHBox parent;

  /*< private > */
  EvInfoBarPrivate *priv;
};


struct _EvInfoBarClass
{
  GtkHBoxClass parent_class;

  /* Signals */
  void (* response) (EvInfoBar *info_bar, gint response_id);

  /* Keybinding signals */
  void (* close)    (EvInfoBar *info_bar);

  /* Padding for future expansion */
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
  void (*_gtk_reserved5) (void);
  void (*_gtk_reserved6) (void);
};

GType          ev_info_bar_get_type               (void) G_GNUC_CONST;
GtkWidget     *ev_info_bar_new                    (void);

GtkWidget     *ev_info_bar_new_with_buttons       (const gchar    *first_button_text,
                                                    ...);

GtkWidget     *ev_info_bar_get_action_area        (EvInfoBar     *info_bar);
GtkWidget     *ev_info_bar_get_content_area       (EvInfoBar     *info_bar);
void           ev_info_bar_add_action_widget      (EvInfoBar     *info_bar,
						   GtkWidget      *child,
						   gint            response_id);
GtkWidget     *ev_info_bar_add_button             (EvInfoBar     *info_bar,
						   const gchar    *button_text,
						   gint            response_id);
void           ev_info_bar_add_buttons            (EvInfoBar     *info_bar,
						   const gchar    *first_button_text,
						   ...);
void           ev_info_bar_set_response_sensitive (EvInfoBar     *info_bar,
						   gint            response_id,
						   gboolean        setting);
void           ev_info_bar_set_default_response   (EvInfoBar     *info_bar,
						   gint            response_id);

/* Emit response signal */
void           ev_info_bar_response               (EvInfoBar     *info_bar,
						   gint            response_id);

void           ev_info_bar_set_message_type       (EvInfoBar     *info_bar,
						   GtkMessageType  message_type);
GtkMessageType ev_info_bar_get_message_type       (EvInfoBar     *info_bar);

G_END_DECLS

#endif  /* __EV_INFO_BAR_H__  */
