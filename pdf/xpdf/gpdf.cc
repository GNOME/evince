/*
 * PDF viewer Bonobo container.
 *
 * Author:
 *   Michael Meeks <michael@imaginator.com>
 *
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

#define UNTESTED 0

poptContext ctx;
gint  gpdf_debug=0;

const struct poptOption gpdf_popt_options [] = {
  { "debug", '\0', POPT_ARG_INT, &gpdf_debug, 0,
    N_("Enables some debugging functions"), N_("LEVEL") },
  { NULL, '\0', 0, NULL, 0 }
};

typedef struct _Component Component;
typedef struct _Container Container;
/* NB. there is a 1 to 1 Container -> Component mapping, this
   is due to how much MDI sucks; unutterably */
struct _Container {
  GnomeContainer  *container;
  GnomeUIHandler  *uih;
  
  GnomeViewFrame  *active_view_frame;
  
  GtkWidget	*app;
  GtkWidget	*view_widget;
  Component     *component;
  gdouble zoom;
};

struct  _Component {
	Container	  *container;

	GnomeClientSite   *client_site;
	GnomeViewFrame	  *view_frame;
	GnomeObjectClient *server;
};

GList *containers = NULL;
/*
 * Static prototypes.
 */
extern "C" {
  static Container *container_new       (const char *fname);
  static void       container_destroy   (Container *cont);
  static void       container_open_cmd  (GtkWidget *widget, Container *container);
  static void       container_close_cmd (GtkWidget *widget, Container *container);
  static void       container_exit_cmd  (void);
  static Component *container_activate_component (Container *container, char *component_goad_id);
  static void       zoom_in_cmd         (GtkWidget *widget, Container *container);
  static void       zoom_out_cmd        (GtkWidget *widget, Container *container);
  static void       zoom_set            (Container *container);
}

/*
 * The menus.
 */
static GnomeUIInfo container_file_menu [] = {
	GNOMEUIINFO_MENU_OPEN_ITEM (container_open_cmd, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_MENU_CLOSE_ITEM(container_close_cmd, NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_MENU_EXIT_ITEM (container_exit_cmd, NULL),
	GNOMEUIINFO_END
};

static GnomeUIInfo container_menu_zoom [] = {
	{ GNOME_APP_UI_ITEM, N_("_Zoom in"),
	  N_("Increase the size of objects in the PDF"),
	  NULL, zoom_in_cmd },
	{ GNOME_APP_UI_ITEM, N_("_Zoom out"),
	  N_("Decrease the size of objects in the PDF"),
	  zoom_out_cmd },
	GNOMEUIINFO_END
};

static GnomeUIInfo container_main_menu [] = {
	GNOMEUIINFO_MENU_FILE_TREE (container_file_menu),
	{ GNOME_APP_UI_SUBTREE, N_("_Zoom"), NULL, container_menu_zoom },
	GNOMEUIINFO_END
};

static GnomeUIInfo container_toolbar [] = {
	GNOMEUIINFO_ITEM_STOCK (
		N_("Open"), N_("Opens an existing workbook"),
		container_open_cmd, GNOME_STOCK_PIXMAP_OPEN),

	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_END
};

extern "C" {
  static gboolean
  open_pdf (Container *container, const char *name)
  {
    GnomeObjectClient *object;
    GnomeStream *stream;
    GNOME_PersistStream persist;
    Component *comp;
    CORBA_Environment ev;

    g_return_val_if_fail (container != NULL, FALSE);
    g_return_val_if_fail (container->view_widget == NULL, FALSE);

    comp = container_activate_component (container, "bonobo-object:image-x-pdf");
    if (!comp || !(object = comp->server)) {
      gnome_error_dialog (_("Could not launch bonobo object."));
      return FALSE;
    }
    
    CORBA_exception_init (&ev);
    persist = GNOME_Unknown_query_interface (
      gnome_object_corba_objref (GNOME_OBJECT (object)),
      "IDL:GNOME/PersistStream:1.0", &ev);
    
    if (ev._major != CORBA_NO_EXCEPTION ||
	persist == CORBA_OBJECT_NIL) {
      gnome_error_dialog ("Panic: component is well broken.");
      return FALSE;
    }
    
    stream = gnome_stream_fs_open (name, GNOME_Storage_READ);
    
    if (stream == NULL) {
      char *err = g_strconcat (_("Could not open "), name, NULL);
      gnome_error_dialog_parented (err, GTK_WINDOW(container->app));
      g_free (err);
      return FALSE;
    }
    
    GNOME_PersistStream_load (persist,
			      (GNOME_Stream) gnome_object_corba_objref (GNOME_OBJECT (stream)), &ev);

    
    
    GNOME_Unknown_unref (persist, &ev);
    CORBA_Object_release (persist, &ev);
    CORBA_exception_free (&ev);
    return TRUE;
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
  container_open_cmd (GtkWidget *widget, Container *container)
  {
    GtkFileSelection *fsel;
    gboolean accepted = FALSE;
    
    fsel = GTK_FILE_SELECTION (gtk_file_selection_new (_("Load file")));
    gtk_window_set_modal (GTK_WINDOW (fsel), TRUE);
    
    gtk_window_set_transient_for (GTK_WINDOW (fsel),
				  GTK_WINDOW (container->app));
    
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
	char *fname = g_strdup (name);
	if (container->view_widget) /* any sort of MDI sucks :-] */
	  container = container_new (fname);
	else
	  open_pdf (container, fname);
	g_free (fname);
      } else {
	GtkWidget *dialog;
	dialog = gnome_message_box_new ("Can't open a directory",
					GNOME_MESSAGE_BOX_ERROR,
					GNOME_STOCK_BUTTON_OK, NULL);
	gnome_dialog_set_parent (GNOME_DIALOG (dialog),
				 GTK_WINDOW (container->app));
	gnome_dialog_run (GNOME_DIALOG (dialog));
      }
    }
    
    gtk_widget_destroy (GTK_WIDGET (fsel));
  }

  static void
  container_destroy (Container *cont)
  {
    containers = g_list_remove (containers, cont);
    gtk_widget_destroy (cont->app);
    g_free (cont);
    if (!containers)
      gtk_main_quit ();
  }
  
  static void
  container_close_cmd (GtkWidget *widget, Container *cont)
  {
    container_destroy (cont);
  }
  
  static void
  container_exit_cmd (void)
  {
    while (containers)
      container_destroy ((Container *)containers->data);
  }

  /*
   * Enforces the containers zoom factor.
   */
  static void
  zoom_set (Container *container)
  {
    g_return_if_fail (container != NULL);
    g_return_if_fail (container->component != NULL);

    gnome_view_frame_set_zoom_factor (container->component->view_frame,
				      container->zoom);
  }

  static void
  zoom_in_cmd (GtkWidget *widget, Container *container)
  {
    g_return_if_fail (container != NULL);
    container->zoom *= 1.4;
    zoom_set (container);
  }

  static void
  zoom_out_cmd (GtkWidget *widget, Container *container)
  {
    g_return_if_fail (container != NULL);
    container->zoom /= 1.4;
    zoom_set (container);
  }

  static void
  component_user_activate_request_cb (GnomeViewFrame *view_frame, gpointer data)
  {
    Component *component = (Component *) data;
    Container *container = component->container;
    
    /*
     * If there is a
     * If there is already an active View, deactivate it.
     */
    if (container->active_view_frame != NULL) {
      /*
       * This just sends a notice to the embedded View that
       * it is being deactivated.  We will also forcibly
       * cover it so that it does not receive any Gtk
       * events.
       */
      gnome_view_frame_view_deactivate (container->active_view_frame);
      
      /*
       * Here we manually cover it if it hasn't acquiesced.
       * If it has consented to be deactivated, then it will
       * already have notified us that it is inactive, and
       * we will have covered it and set active_view_frame
       * to NULL.  Which is why this check is here.
       */
      if (container->active_view_frame != NULL)
	gnome_view_frame_set_covered (container->active_view_frame, TRUE);
      
      container->active_view_frame = NULL;
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
  }
  
  static void
  component_view_activated_cb (GnomeViewFrame *view_frame, gboolean activated, gpointer data)
  {
    Component *component = (Component *) data;
    Container *container = component->container;
    
    if (activated) {
      /*
       * If the View is requesting to be activated, then we
       * check whether or not there is already an active
       * View.
       */
      if (container->active_view_frame != NULL) {
	g_warning ("View requested to be activated but there is already "
		   "an active View!\n");
	return;
      }
      
      /*
       * Otherwise, uncover it so that it can receive
       * events, and set it as the active View.
       */
      gnome_view_frame_set_covered (view_frame, FALSE);
      container->active_view_frame = view_frame;
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
      
      if (view_frame == container->active_view_frame)
	container->active_view_frame = NULL;
    }                                                                       
  }
  
  static void
  component_user_context_cb (GnomeViewFrame *view_frame, gpointer data)
  {
    Component *component = (Component *) data;
    char *executed_verb;
    GList *l;
    
    /*
     * See if the remote GnomeEmbeddable supports any verbs at
     * all.
     */
    l = gnome_client_site_get_verbs (component->client_site);
    if (l == NULL)
      return;
    gnome_client_site_free_verbs (l);
    
    /*
     * Popup the verb popup and execute the chosen verb.  This
     * function saves us the work of creating the menu, connecting
     * the callback, and executing the verb on the remove
     * GnomeView.  We could implement all this functionality
     * ourselves if we wanted.
     */
    executed_verb = gnome_view_frame_popup_verbs (view_frame);
    
    g_free (executed_verb);
  }
}

static void
container_set_view (Container *container, Component *component)
{
	GnomeViewFrame *view_frame;
	GtkWidget *view_widget;

	/*
	 * Create the remote view and the local ViewFrame.
	 */
	view_frame = gnome_client_site_new_view (component->client_site);
	component->view_frame = view_frame;

	/*
	 * Set the GnomeUIHandler for this ViewFrame.  That way, the
	 * embedded component can get access to our UIHandler server
	 * so that it can merge menu and toolbar items when it gets
	 * activated.
	 */
	gnome_view_frame_set_ui_handler (view_frame, container->uih);

	/*
	 * Embed the view frame into the application.
	 */
	view_widget = gnome_view_frame_get_wrapper (view_frame);
	container->view_widget = view_widget;
	container->component   = component;
/*	gtk_box_pack_start (GTK_BOX (container->app), view_widget,
	FALSE, FALSE, 5);*/
	gnome_app_set_contents (GNOME_APP (container->app), view_widget);
	/*
	 * The "user_activate" signal will be emitted when the user
	 * double clicks on the "cover".  The cover is a transparent
	 * window which sits on top of the component and keeps any
	 * events (mouse, keyboard) from reaching it.  When the user
	 * double clicks on the cover, the container (that's us)
	 * can choose to activate the component.
	 */
	gtk_signal_connect (GTK_OBJECT (view_frame), "user_activate",
			    GTK_SIGNAL_FUNC (component_user_activate_request_cb), component);

	/*
	 * In-place activation of a component is a two-step process.
	 * After the user double clicks on the component, our signal
	 * callback (compoennt_user_activate_request_cb()) asks the
	 * component to activate itself (see
	 * gnome_view_frame_view_activate()).  The component can then
	 * choose to either accept or refuse activation.  When an
	 * embedded component notifies us of its decision to change
	 * its activation state, the "view_activated" signal is
	 * emitted from the view frame.  It is at that point that we
	 * actually remove the cover so that events can get through.
	 */
	gtk_signal_connect (GTK_OBJECT (view_frame), "view_activated",
			    GTK_SIGNAL_FUNC (component_view_activated_cb), component);

	/*
	 * The "user_context" signal is emitted when the user right
	 * clicks on the wrapper.  We use it to pop up a verb menu.
	 */
	gtk_signal_connect (GTK_OBJECT (view_frame), "user_context",
			    GTK_SIGNAL_FUNC (component_user_context_cb), component);

	/*
	 * Show the component.
	 */
	gtk_widget_show_all (view_widget);
}

static GnomeObjectClient *
container_launch_component (GnomeClientSite *client_site,
			    GnomeContainer *container,
			    char *component_goad_id)
{
	GnomeObjectClient *object_server;

	/*
	 * Launch the component.
	 */
	object_server = gnome_object_activate_with_goad_id (
		NULL, component_goad_id, 0, NULL);

	if (object_server == NULL)
		return NULL;

	/*
	 * Bind it to the local ClientSite.  Every embedded component
	 * has a local GnomeClientSite object which serves as a
	 * container-side point of contact for the embeddable.  The
	 * container talks to the embeddable through its ClientSite
	 */
	if (!gnome_client_site_bind_embeddable (client_site, object_server)) {
		gnome_object_unref (GNOME_OBJECT (object_server));
		return NULL;
	}

	/*
	 * The GnomeContainer object maintains a list of the
	 * ClientSites which it manages.  Here we add the new
	 * ClientSite to that list.
	 */
	gnome_container_add (container, GNOME_OBJECT (client_site));

	return object_server;
}

/*
 * Use query_interface to see if `obj' has `interface'.
 */
static gboolean
gnome_object_has_interface (GnomeObject *obj, char *interface)
{
	CORBA_Environment ev;
	CORBA_Object requested_interface;

	CORBA_exception_init (&ev);

	requested_interface = GNOME_Unknown_query_interface (
		gnome_object_corba_objref (obj), interface, &ev);

	CORBA_exception_free (&ev);

	if (!CORBA_Object_is_nil(requested_interface, &ev) &&
	    ev._major == CORBA_NO_EXCEPTION)
	{
		/* Get rid of the interface we've been passed */
		CORBA_Object_release (requested_interface, &ev);
		return TRUE;
	}

	return FALSE;
}

extern "C" {
  static Component *
  container_activate_component (Container *container, char *component_goad_id)
  {
    Component *component;
    GnomeClientSite *client_site;
    GnomeObjectClient *server;
    
    /*
     * The ClientSite is the container-side point of contact for
     * the Embeddable.  So there is a one-to-one correspondence
     * between GnomeClientSites and GnomeEmbeddables.  */
    client_site = gnome_client_site_new (container->container);
    
    /*
     * A GnomeObjectClient is a simple wrapper for a remote
     * GnomeObject (a server supporting GNOME::Unknown).
     */
    server = container_launch_component (client_site, container->container,
					 component_goad_id);
    if (server == NULL) {
      char *error_msg;
      
      error_msg = g_strdup_printf (_("Could not launch Embeddable %s!"),
				   component_goad_id);
      gnome_warning_dialog (error_msg);
      g_free (error_msg);
      
      return NULL;
    }
    
    /*
     * Create the internal data structure which we will use to
     * keep track of this component.
     */
    component = g_new0 (Component, 1);
    component->container = container;
    component->client_site = client_site;
    component->server = server;
    
    container_set_view (container, component);

    return component;
  }
}

static void
container_create_menus (Container *container)
{
	GnomeUIHandlerMenuItem *menu_list;

	gnome_ui_handler_create_menubar (container->uih);

	/*
	 * Create the basic menus out of UIInfo structures.
	 */
	menu_list = gnome_ui_handler_menu_parse_uiinfo_list_with_data (container_main_menu, container);
	gnome_ui_handler_menu_add_list (container->uih, "/", menu_list);
	gnome_ui_handler_menu_free_list (menu_list);
}

static void
container_create_toolbar (Container *container)
{
	GnomeUIHandlerMenuItem *toolbar;

#if UNTESTED > 0
	gnome_ui_handler_create_menubar (container->uih);

	/*
	 * Create the basic menus out of UIInfo structures.
	 */
	toolbar = gnome_ui_handler_toolbar_parse_uiinfo_list_with_data (container_toolbar, container);
	gnome_ui_handler_toolbar_add_list (container->uih, "/", menu_list);
	gnome_ui_handler_toolbar_free_list (menu_list);
#endif
}

static Container *
container_new (const char *fname)
{
	Container *container;

	container = g_new0 (Container, 1);

	container->app  = gnome_app_new ("pdf-viewer",
					 "GNOME PDF viewer");
	container->zoom = 43.0;

	gtk_window_set_default_size (GTK_WINDOW (container->app), 400, 400);
	gtk_window_set_policy (GTK_WINDOW (container->app), TRUE, TRUE, FALSE);

	container->container   = gnome_container_new ();
	container->view_widget = NULL;

	/*
	 * Create the GnomeUIHandler object which will be used to
	 * create the container's menus and toolbars.  The UIHandler
	 * also creates a CORBA server which embedded components use
	 * to do menu/toolbar merging.
	 */
	container->uih = gnome_ui_handler_new ();
	gnome_ui_handler_set_app (container->uih, GNOME_APP (container->app));

	container_create_menus   (container);
	container_create_toolbar (container);

	gtk_widget_show_all (container->app);

	if (fname)
	  if (!open_pdf (container, fname)) {
	    container_destroy (container);
	    return NULL;
	  }

	containers = g_list_append (containers, container);

	gtk_widget_show_all (container->app);

	return container;
}

int
main (int argc, char **argv)
{
  CORBA_Environment ev;
  CORBA_ORB orb;
  char **view_files = NULL;
  int    i;
  
  CORBA_exception_init (&ev);
  
  gnome_CORBA_init_with_popt_table ("PDFViewer", "0.0.1",
				    &argc, argv,
				    gpdf_popt_options, 0, &ctx,
				    GNORBA_INIT_SERVER_FUNC, &ev);

  CORBA_exception_free (&ev);

  orb = gnome_CORBA_ORB ();

  if (bonobo_init (orb, NULL, NULL) == FALSE)
    g_error (_("Could not initialize Bonobo!\n"));
  bonobo_activate ();

  view_files = poptGetArgs (ctx);

  /* Load files */
  i = 0;
  if (view_files) {
    for (i = 0; view_files[i]; i++)
      container_new (view_files[i]);
  }
  if (i == 0)
    container_new (NULL);
  
  poptFreeContext (ctx);

  gtk_main ();
	
  return 0;
}

