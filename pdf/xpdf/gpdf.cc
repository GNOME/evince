/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * test-container.c
 *
 * A simple program to act as a test container for embeddable
 * components.
 *
 * Authors:
 *    Nat Friedman (nat@gnome-support.com)
 *    Miguel de Icaza (miguel@gnu.org)
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
extern "C" {
#define GString G_String
#include <gnome.h>
#include <libgnorba/gnorba.h>
#include <gdk/gdkprivate.h>
#include <gdk/gdkx.h>
#include <bonobo/gnome-bonobo.h>
#undef  GString 
}
#include <sys/stat.h>
#include <unistd.h>
#include "gtypes.h"
#include "GString.h"
#include "parseargs.h"
#include "gfile.h"
#include "gmem.h"
#include "Object.h"
#include "Stream.h"
#include "Array.h"
#include "Dict.h"
#include "XRef.h"
#include "Catalog.h"
#include "Page.h"
#include "Link.h"
#include "PDFDoc.h"
#include "GOutputDev.h"
#include "PSOutputDev.h"
#include "TextOutputDev.h"
#include "Params.h"
#include "Error.h"
#include "config.h"

CORBA_Environment ev;
CORBA_ORB orb;
poptContext ctx;
gint  gpdf_debug=1;

const struct poptOption gpdf_popt_options [] = {
  { "debug", '\0', POPT_ARG_INT, &gpdf_debug, 0,
    N_("Enables some debugging functions"), N_("LEVEL") },
  { NULL, '\0', 0, NULL, 0 }
};

/*
 * A handle to some Embeddables and their ClientSites so we can add
 * views to existing components.
 */
GnomeObjectClient *text_obj;
GnomeClientSite *text_client_site;

GnomeObjectClient *image_png_obj;
GnomeClientSite   *image_client_site;

/*
 * The currently active view.  We keep track of this
 * so we can deactivate it when a new view is activated.
 */
GnomeViewFrame *active_view_frame;

char *server_goadid = "gnome_xpdf_viewer";

typedef struct {
	GtkWidget *app;
	GnomeContainer *container;
	GtkWidget *box;
	GnomeUIHandler *uih;
	gboolean contains_pdf;
} Application;

/* List of applications */
GList *apps = NULL;

static Application * application_new (void);

static void
application_destroy (Application *app)
{
	apps = g_list_remove (apps, app);
	gtk_widget_destroy (app->app);
	g_free (app);
	if (!apps)
		gtk_main_quit ();
}

static void
applications_destroy ()
{
	while (apps)
		application_destroy ((Application *)apps->data);
}

static GnomeObjectClient *
launch_server (GnomeClientSite *client_site, GnomeContainer *container, char *goadid)
{
	GnomeObjectClient *object_server;
	
	gnome_container_add (container, GNOME_OBJECT (client_site));

	printf ("Launching...\n");
	object_server = gnome_object_activate_with_goad_id (NULL, goadid, GOAD_ACTIVATE_SHLIB, NULL);
	printf ("Return: %p\n", object_server);
	if (!object_server){
		g_warning (_("Can not activate object_server\n"));
		return NULL;
	}

	if (!gnome_client_site_bind_embeddable (client_site, object_server)){
		g_warning (_("Can not bind object server to client_site\n"));
		return NULL;
	}

	return object_server;
}

static GnomeObjectClient *
launch_server_moniker (GnomeClientSite *client_site, GnomeContainer *container, char *moniker)
{
	GnomeObjectClient *object_server;
	
	gnome_container_add (container, GNOME_OBJECT (client_site));

	printf ("Launching moniker %s...\n", moniker);
	object_server = gnome_object_activate (moniker, GOAD_ACTIVATE_SHLIB);
	printf ("Return: %p\n", object_server);
	if (!object_server){
		g_warning (_("Can not activate object_server\n"));
		return NULL;
	}

	if (!gnome_client_site_bind_embeddable (client_site, object_server)){
		g_warning (_("Can not bind object server to client_site\n"));
		return NULL;
	}

	return object_server;
}

/*
 * This function is called when the user double clicks on a View in
 * order to activate it.
 */
static gint
user_activation_request_cb (GnomeViewFrame *view_frame)
{
	/*
	 * If there is already an active View, deactivate it.
	 */
        if (active_view_frame != NULL) {
		/*
		 * This just sends a notice to the embedded View that
		 * it is being deactivated.  We will also forcibly
		 * cover it so that it does not receive any Gtk
		 * events.
		 */
                gnome_view_frame_view_deactivate (active_view_frame);

		/*
		 * Here we manually cover it if it hasn't acquiesced.
		 * If it has consented to be deactivated, then it will
		 * already have notified us that it is inactive, and
		 * we will have covered it and set active_view_frame
		 * to NULL.  Which is why this check is here.
		 */
		if (active_view_frame != NULL)
			gnome_view_frame_set_covered (active_view_frame, TRUE);
									     
		active_view_frame = NULL;
	}

        /*
	 * Activate the View which the user clicked on.  This just
	 * sends a request to the embedded View to activate itself.
	 * When it agrees to be activated, it will notify its
	 * ViewFrame, and our view_activated_cb callback will be
	 * called.
	 *
	 * We do not uncover the View here, because it may not wish to
	 * be activated, and so we wait until it notifies us that it
	 * has been activated to uncover it.
	 */
        gnome_view_frame_view_activate (view_frame);

        return FALSE;
}                                                                               

/*
 * Gets called when the View notifies the ViewFrame that it would like
 * to be activated or deactivated.
 */
static gint
view_activated_cb (GnomeViewFrame *view_frame, gboolean activated)
{

        if (activated) {
		/*
		 * If the View is requesting to be activated, then we
		 * check whether or not there is already an active
		 * View.
		 */
		if (active_view_frame != NULL) {
			g_warning ("View requested to be activated but there is already "
				   "an active View!\n");
			return FALSE;
		}

		/*
		 * Otherwise, uncover it so that it can receive
		 * events, and set it as the active View.
		 */
		gnome_view_frame_set_covered (view_frame, FALSE);
                active_view_frame = view_frame;
        } else {
		/*
		 * If the View is asking to be deactivated, always
		 * oblige.  We may have already deactivated it (see
		 * user_activation_request_cb), but there's no harm in
		 * doing it again.  There is always the possibility
		 * that a View will ask to be deactivated when we have
		 * not told it to deactivate itself, and that is
		 * why we cover the view here.
		 */
		gnome_view_frame_set_covered (view_frame, TRUE);

		if (view_frame == active_view_frame)
			active_view_frame = NULL;
        }                                                                       

        return FALSE;
}                                                                               

static GnomeViewFrame *
add_view (Application *app,
	  GnomeClientSite *client_site, GnomeObjectClient *server) 
{
	GnomeViewFrame *view_frame;
	GtkWidget *view_widget;
	
	view_frame = gnome_client_site_embeddable_new_view (client_site);

	gtk_signal_connect (GTK_OBJECT (view_frame), "user_activate",
			    GTK_SIGNAL_FUNC (user_activation_request_cb), NULL);
	gtk_signal_connect (GTK_OBJECT (view_frame), "view_activated",
			    GTK_SIGNAL_FUNC (view_activated_cb), NULL);

	gnome_view_frame_set_ui_handler (view_frame, app->uih);

	view_widget = gnome_view_frame_get_wrapper (view_frame);

	gtk_box_pack_start (GTK_BOX (app->box), view_widget, TRUE, TRUE, 0);
	gtk_widget_show_all (app->box);

	return view_frame;
} /* add_view */

static GnomeObjectClient *
add_cmd (Application *app, char *server_goadid,
	 GnomeClientSite **client_site)
{
	GnomeObjectClient *server;
	
	*client_site = gnome_client_site_new (app->container);

	server = launch_server (*client_site, app->container, server_goadid);
	if (server == NULL)
		return NULL;

	add_view (app, *client_site, server);
	return server;
}

static void
open_pdf (Application *app, const char *name)
{
	GnomeObjectClient *object;
	GnomeStream *stream;
	GNOME_PersistStream persist;

	object = add_cmd (app, "bonobo-object:image-x-pdf", &image_client_site);
	if (object == NULL) {
		gnome_error_dialog (_("Could not launch bonobo object."));
		return;
	}

	image_png_obj = object;

	persist = GNOME_Unknown_query_interface (
		gnome_object_corba_objref (GNOME_OBJECT (object)),
		"IDL:GNOME/PersistStream:1.0", &ev);

        if (ev._major != CORBA_NO_EXCEPTION ||
	    persist == CORBA_OBJECT_NIL) {
		gnome_error_dialog ("Panic: component is well broken.");
                return;
	}
	
	stream = gnome_stream_fs_open (name, GNOME_Storage_READ);

	if (stream == NULL) {
		char *err = g_strconcat (_("Could not open "), name, NULL);
		gnome_error_dialog_parented (err, GTK_WINDOW(app->app));
		g_free (err);
		return;
	}
	
	GNOME_PersistStream_load (persist,
	     (GNOME_Stream) gnome_object_corba_objref (GNOME_OBJECT (stream)), &ev);

	GNOME_Unknown_unref (persist, &ev);
	CORBA_Object_release (persist, &ev);
	app->contains_pdf = TRUE;
}

static void
set_ok (GtkWidget *widget, gboolean *dialog_result)
{
	*dialog_result = TRUE;
	gtk_main_quit ();
}

static guint
file_dialog_delete_event (GtkWidget *widget, GdkEventAny *event)
{
	gtk_main_quit ();
	return TRUE;
}

static void
file_open_cmd (GtkWidget *widget, Application *app)
{
	GtkFileSelection *fsel;
	gboolean accepted = FALSE;

	fsel = GTK_FILE_SELECTION (gtk_file_selection_new (_("Load file")));
	gtk_window_set_modal (GTK_WINDOW (fsel), TRUE);

	gtk_window_set_transient_for (GTK_WINDOW (fsel),
				      GTK_WINDOW (app->app));

	/* Connect the signals for Ok and Cancel */
	gtk_signal_connect (GTK_OBJECT (fsel->ok_button), "clicked",
			    GTK_SIGNAL_FUNC (set_ok), &accepted);
	gtk_signal_connect (GTK_OBJECT (fsel->cancel_button), "clicked",
			    GTK_SIGNAL_FUNC (gtk_main_quit), NULL);
	gtk_window_set_position (GTK_WINDOW (fsel), GTK_WIN_POS_MOUSE);

	/*
	 * Make sure that we quit the main loop if the window is destroyed 
	 */
	gtk_signal_connect (GTK_OBJECT (fsel), "delete_event",
			    GTK_SIGNAL_FUNC (file_dialog_delete_event), NULL);

	/* Run the dialog */
	gtk_widget_show (GTK_WIDGET (fsel));
	gtk_grab_add (GTK_WIDGET (fsel));
	gtk_main ();

	if (accepted) {
		char *name = gtk_file_selection_get_filename (fsel);

		if (name [strlen (name)-1] != '/') {
			if (app->contains_pdf)
				app = application_new ();
			char *fname = g_strdup (name);
			open_pdf (app, fname);
			g_free (fname);
		} else {
			GtkWidget *dialog;
			dialog = gnome_message_box_new ("Can't open a directory",
							GNOME_MESSAGE_BOX_ERROR,
							GNOME_STOCK_BUTTON_OK, NULL);
			gnome_dialog_set_parent (GNOME_DIALOG (dialog),
						 GTK_WINDOW (app->app));
			gnome_dialog_run (GNOME_DIALOG (dialog));
		}
	}

	gtk_widget_destroy (GTK_WIDGET (fsel));
}

static void
close_cmd (GtkWidget *widget, Application *app)
{
	application_destroy (app);
}

static void
exit_cmd (void)
{
	applications_destroy ();
}

static GnomeUIInfo container_file_menu [] = {
	GNOMEUIINFO_MENU_OPEN_ITEM (file_open_cmd, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_MENU_CLOSE_ITEM(close_cmd, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_MENU_EXIT_ITEM (exit_cmd, NULL),
	GNOMEUIINFO_END
};

static GnomeUIInfo container_main_menu [] = {
	GNOMEUIINFO_MENU_FILE_TREE (container_file_menu),
	GNOMEUIINFO_END
};

static Application *
application_new (void)
{
	Application *app;
	GnomeUIHandlerMenuItem *menu_list;

	app = g_new0 (Application, 1);
	app->app = gnome_app_new ("gpdf",
				  "GNOME PDF viewer");
	app->container = GNOME_CONTAINER (gnome_container_new ());
	app->contains_pdf = FALSE;

	app->box = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (app->box);
	gnome_app_set_contents (GNOME_APP (app->app), app->box);

	/*
	 * Create the menus.
	 */
	app->uih = gnome_ui_handler_new ();

	gnome_ui_handler_set_app (app->uih, GNOME_APP (app->app));
	gnome_ui_handler_create_menubar (app->uih);

	menu_list = gnome_ui_handler_menu_parse_uiinfo_list_with_data (container_main_menu, app);
	gnome_ui_handler_menu_add_list (app->uih, "/", menu_list);
	gnome_ui_handler_menu_free_list (menu_list);

/*	gnome_ui_handler_create_toolbar (app->uih, "Common");
	gnome_ui_handler_toolbar_new_item (app->uih,
					   "/Common/item 1",
					   "Container-added Item 1", "I am the container.  Hear me roar.",
					   0, GNOME_UI_HANDLER_PIXMAP_NONE, NULL, 0, 0,
					   NULL, NULL);*/

	gtk_widget_show (app->app);

	apps = g_list_append (apps, app);
	return app;
}

int
main (int argc, char *argv [])
{
	Application *app;
	char **view_files = NULL;

	if (argc != 1){
		server_goadid = argv [1];
	}
	
	CORBA_exception_init (&ev);
	
	gnome_CORBA_init_with_popt_table ("PDFViewer", "0.0.1",
					  &argc, argv,
					  gpdf_popt_options, 0, &ctx,
					  GNORBA_INIT_SERVER_FUNC, &ev);
	orb = gnome_CORBA_ORB ();
	
	if (bonobo_init (orb, NULL, NULL) == FALSE)
		g_error (_("Can not bonobo_init\n"));

	app = application_new ();

	view_files = poptGetArgs (ctx);

	/* Load files */
	if (view_files) {
		int i;
		for (i = 0; view_files[i]; i++) {
			if (app->contains_pdf)
				app = application_new ();
			open_pdf (app, view_files[i]);
		}
	}

	poptFreeContext (ctx);

	gtk_main ();

	CORBA_exception_free (&ev);
	
	return 0;
}
