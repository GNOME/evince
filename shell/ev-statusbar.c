/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2004 Red Hat, Inc.
 *
 *  Author:
 *    Jonathan Blandford <jrb@alum.mit.edu>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <gtk/gtk.h>

#include "ev-statusbar.h"

struct _EvStatusbarPrivate {
	GtkWidget *bar;
	GtkWidget *progress;

	guint help_message_cid;
	guint view_message_cid;
	guint progress_message_cid;
	
	guint pulse_timeout_id;
	guint progress_timeout_id;
};

G_DEFINE_TYPE (EvStatusbar, ev_statusbar, GTK_TYPE_HBOX)

#define EV_STATUSBAR_GET_PRIVATE(object) \
		(G_TYPE_INSTANCE_GET_PRIVATE ((object), EV_TYPE_STATUSBAR, EvStatusbarPrivate))

static void
ev_statusbar_destroy (GtkObject *object)
{
	EvStatusbar *ev_statusbar = EV_STATUSBAR (object);
	
	ev_statusbar_set_progress (ev_statusbar, FALSE);
	
	(* GTK_OBJECT_CLASS (ev_statusbar_parent_class)->destroy) (object);
}

static void
ev_statusbar_class_init (EvStatusbarClass *ev_statusbar_class)
{
	GObjectClass *g_object_class;
	GtkWidgetClass *widget_class;
	GtkObjectClass *gtk_object_klass;
 
	g_object_class = G_OBJECT_CLASS (ev_statusbar_class);
	widget_class = GTK_WIDGET_CLASS (ev_statusbar_class);
	gtk_object_klass = GTK_OBJECT_CLASS (ev_statusbar_class);
	   
	g_type_class_add_private (g_object_class, sizeof (EvStatusbarPrivate));
	   
	gtk_object_klass->destroy = ev_statusbar_destroy;
}

static void
ev_statusbar_init (EvStatusbar *ev_statusbar)
{
	ev_statusbar->priv = EV_STATUSBAR_GET_PRIVATE (ev_statusbar);

	ev_statusbar->priv->progress = gtk_progress_bar_new ();
	gtk_box_pack_start (GTK_BOX (ev_statusbar), ev_statusbar->priv->progress, FALSE, FALSE, 3);
	ev_statusbar->priv->bar = gtk_statusbar_new ();
	gtk_box_pack_start (GTK_BOX (ev_statusbar), ev_statusbar->priv->bar, TRUE, TRUE, 0);
	    
	ev_statusbar->priv->help_message_cid = gtk_statusbar_get_context_id
		(GTK_STATUSBAR (ev_statusbar->priv->bar), "help_message");
	ev_statusbar->priv->view_message_cid = gtk_statusbar_get_context_id
		(GTK_STATUSBAR (ev_statusbar->priv->bar), "view_message");
	ev_statusbar->priv->progress_message_cid = gtk_statusbar_get_context_id
		(GTK_STATUSBAR (ev_statusbar->priv->bar), "progress_message");
    
        gtk_widget_show (GTK_WIDGET (ev_statusbar->priv->bar));
        gtk_widget_show (GTK_WIDGET (ev_statusbar));
	
	ev_statusbar->priv->progress_timeout_id = 0;
	ev_statusbar->priv->pulse_timeout_id = 0;
}

/* Public functions */

GtkWidget *
ev_statusbar_new (void)
{
	GtkWidget *ev_statusbar;

	ev_statusbar = g_object_new (EV_TYPE_STATUSBAR, NULL);

	return ev_statusbar;
}

static guint 
ev_statusbar_get_context_id (EvStatusbar *statusbar, EvStatusbarContext context)
{
    switch (context) {
	case EV_CONTEXT_HELP:
		return statusbar->priv->help_message_cid;
	case EV_CONTEXT_VIEW:
		return statusbar->priv->view_message_cid;
	case EV_CONTEXT_PROGRESS:
		return statusbar->priv->progress_message_cid;
    }
    return -1;
}

void
ev_statusbar_push    (EvStatusbar *ev_statusbar, 
	              EvStatusbarContext context, 
		      const gchar *message)
{
	gtk_statusbar_push (GTK_STATUSBAR (ev_statusbar->priv->bar),
			    ev_statusbar_get_context_id (ev_statusbar, context), 
			    message);
	return;
}

void
ev_statusbar_pop     (EvStatusbar *ev_statusbar, 
 	              EvStatusbarContext context)
{
	gtk_statusbar_pop (GTK_STATUSBAR (ev_statusbar->priv->bar),
			   ev_statusbar_get_context_id (ev_statusbar, context));
	return;
}

void 
ev_statusbar_set_maximized (EvStatusbar *ev_statusbar, 
		            gboolean maximized)
{
       gtk_statusbar_set_has_resize_grip (GTK_STATUSBAR (ev_statusbar->priv->bar),
    				          maximized);
       return;
}

static gboolean
ev_statusbar_pulse (gpointer data) 
{
    EvStatusbar *ev_statusbar = EV_STATUSBAR (data);
    
    gtk_progress_bar_pulse (GTK_PROGRESS_BAR (ev_statusbar->priv->progress));
    
    return TRUE;
}

static gboolean
ev_statusbar_show_progress (gpointer data)
{
    EvStatusbar *ev_statusbar = EV_STATUSBAR (data);

    gtk_widget_show (ev_statusbar->priv->progress);
    ev_statusbar->priv->pulse_timeout_id = g_timeout_add (300, ev_statusbar_pulse, ev_statusbar);
    ev_statusbar->priv->progress_timeout_id = 0;

    return FALSE;
}

void
ev_statusbar_set_progress  (EvStatusbar *ev_statusbar, 
		            gboolean active)
{
    if (active){
	    if (ev_statusbar->priv->progress_timeout_id == 0 
			&& ev_statusbar->priv->pulse_timeout_id == 0)
		    ev_statusbar->priv->progress_timeout_id = 
			    g_timeout_add (500, ev_statusbar_show_progress, ev_statusbar);
    } else {
	    if (ev_statusbar->priv->pulse_timeout_id) {
		    g_source_remove (ev_statusbar->priv->pulse_timeout_id);
	    	    gtk_widget_hide (ev_statusbar->priv->progress);
	    }

	    if (ev_statusbar->priv->progress_timeout_id)
		    g_source_remove (ev_statusbar->priv->progress_timeout_id);

	    ev_statusbar->priv->progress_timeout_id = 0;
	    ev_statusbar->priv->pulse_timeout_id = 0;
	    
    }
}



