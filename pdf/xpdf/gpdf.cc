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
#include <bonobo.h>
#undef  GString 
}
#include "config.h"
#include "bonobo-application-x-pdf.h"

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
  BonoboContainer    *container;
  BonoboUIHandler    *uih;
  
  GtkWidget	    *app;
  GtkScrolledWindow *scroll;
  GtkWidget	    *view_widget;
  Component         *component;
};

struct  _Component {
	Container	  *container;

	BonoboClientSite   *client_site;
	BonoboViewFrame	  *view_frame;
	BonoboObjectClient *server;
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
  static void       container_about_cmd (GtkWidget *widget, Container *container);
  static Component *container_activate_component (Container *container, char *component_goad_id);
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

static GnomeUIInfo container_help_menu [] = {
        GNOMEUIINFO_MENU_ABOUT_ITEM(container_about_cmd, NULL),
	GNOMEUIINFO_END
};

static GnomeUIInfo container_main_menu [] = {
	GNOMEUIINFO_MENU_FILE_TREE (container_file_menu),
	GNOMEUIINFO_MENU_HELP_TREE (container_help_menu),
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
    BonoboObjectClient *object;
    BonoboStream *stream;
    Bonobo_PersistStream persist;
    Component *comp;
    CORBA_Environment ev;

    g_return_val_if_fail (container != NULL, FALSE);
    g_return_val_if_fail (container->view_widget == NULL, FALSE);

    comp = container_activate_component (container, "bonobo-object:application-x-pdf");
    if (!comp || !(object = comp->server)) {
      gnome_error_dialog (_("Could not launch bonobo object."));
      return FALSE;
    }
    
    CORBA_exception_init (&ev);
    persist = Bonobo_Unknown_query_interface (
      bonobo_object_corba_objref (BONOBO_OBJECT (object)),
      "IDL:Bonobo/PersistStream:1.0", &ev);
    
    if (ev._major != CORBA_NO_EXCEPTION ||
	persist == CORBA_OBJECT_NIL) {
      gnome_error_dialog ("Panic: component doesn't implement PersistStream.");
      return FALSE;
    }
    
    stream = bonobo_stream_fs_open (name, Bonobo_Storage_READ);
    
    if (stream == NULL) {
      char *err = g_strconcat (_("Could not open "), name, NULL);
      gnome_error_dialog_parented (err, GTK_WINDOW(container->app));
      g_free (err);
      return FALSE;
    }
    
    Bonobo_PersistStream_load (persist,
			      (Bonobo_Stream) bonobo_object_corba_objref (BONOBO_OBJECT (stream)), &ev);

    Bonobo_Unknown_unref (persist, &ev);
    CORBA_Object_release (persist, &ev);
    CORBA_exception_free (&ev);

/*    bonobo_view_frame_view_do_verb (comp->view_frame, "ZoomFit"); */
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
	else {
	  if (!open_pdf (container, fname))
	    container_destroy (container);
	}
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
  component_destroy (Component *component)
  {
    CORBA_Environment ev;
    Container *container;
    g_return_if_fail (component != NULL);

    CORBA_exception_init (&ev);

    /* Kill merged menus et al. */
    bonobo_view_frame_view_deactivate (component->view_frame);

    container = component->container;
    gtk_widget_destroy (container->view_widget);
    container->view_widget = NULL;

    if (component->server)
      Bonobo_Unknown_unref (
	bonobo_object_corba_objref (BONOBO_OBJECT (component->server)), &ev);
    component->server = NULL;

    CORBA_exception_free (&ev);

    g_free (component);
  }

  static void
  container_destroy (Container *cont)
  {
    g_return_if_fail (g_list_find (containers, cont) != NULL);

    containers = g_list_remove (containers, cont);
    if (cont->app)
      gtk_widget_destroy (cont->app);
    cont->app = NULL;
    
    if (cont->component)
      component_destroy (cont->component);
    cont->component = NULL;
    
    g_free (cont);

    if (!containers)
      gtk_main_quit ();
  }

  static void
  container_close (Container *cont)
  {
    g_return_if_fail (g_list_find (containers, cont) != NULL);
    
    if (cont->component) {
      component_destroy (cont->component);
      cont->component = NULL;
    } else
      container_destroy (cont);
  }

  
  static void
  container_close_cmd (GtkWidget *widget, Container *cont)
  {
    container_close (cont);
  }
  
  static int
  container_destroy_cb (GtkWidget *widget, GdkEvent *event, Container *cont)
  {
    container_destroy (cont);
    return 1;
  }
  
  static void
  container_exit_cmd (void)
  {
    while (containers)
      container_destroy ((Container *)containers->data);
  }

static void
container_about_cmd (GtkWidget *widget, Container *container)
{
  GtkWidget *about;

  const gchar *authors[] = {
    N_("Derek B. Noonburg, main author"),
    N_("Michael Meeks, GNOME port maintainer."),
    N_("Miguel de Icaza."),
    N_("Nat Friedman."),
    NULL
  };
  
#ifdef ENABLE_NLS
  int i;

  for (i = 0; authors[i] != NULL; i++)
    authors [i] = _(authors [i]);
#endif
  
  about = gnome_about_new (_("GPDF"), xpdfVersion,
			   _("(C) 1996-1999 Derek B. Noonburg."),
			   authors, NULL, NULL);
  
  gnome_dialog_set_parent (GNOME_DIALOG (about), GTK_WINDOW (container->app));
  gnome_dialog_set_close (GNOME_DIALOG (about), TRUE);
  gtk_widget_show (about);
}
}

static void
container_set_view (Container *container, Component *component)
{
	BonoboViewFrame *view_frame;
	GtkWidget *view_widget;

	/*
	 * Create the remote view and the local ViewFrame.
	 */
	view_frame = bonobo_client_site_new_view (component->client_site,
						  bonobo_object_corba_objref (BONOBO_OBJECT (
							  container->uih)));
	component->view_frame = view_frame;

	/*
	 * Embed the view frame into the application.
	 */
	view_widget = bonobo_view_frame_get_wrapper (view_frame);
	bonobo_wrapper_set_visibility (BONOBO_WRAPPER (view_widget), FALSE);
	container->view_widget = view_widget;
	container->component   = component;

	/*
	 * Show the component.
	 */
	gtk_scrolled_window_add_with_viewport (container->scroll, view_widget);

	/*
	 * Activate it ( get it to merge menus etc. )
	 */
	bonobo_view_frame_view_activate (view_frame);
	bonobo_view_frame_set_covered   (view_frame, FALSE);

	gtk_widget_show_all (GTK_WIDGET (container->scroll));
}

static BonoboObjectClient *
container_launch_component (BonoboClientSite *client_site,
			    BonoboContainer *container,
			    char *component_goad_id)
{
	BonoboObjectClient *object_server;

	/*
	 * Launch the component.
	 */
	object_server = bonobo_object_activate_with_goad_id (
		NULL, component_goad_id, GOAD_ACTIVATE_SHLIB, NULL);

	if (object_server == NULL)
		return NULL;

	/*
	 * Bind it to the local ClientSite.  Every embedded component
	 * has a local BonoboClientSite object which serves as a
	 * container-side point of contact for the embeddable.  The
	 * container talks to the embeddable through its ClientSite
	 */
	if (!bonobo_client_site_bind_embeddable (client_site, object_server)) {
		bonobo_object_unref (BONOBO_OBJECT (object_server));
		return NULL;
	}

	/*
	 * The BonoboContainer object maintains a list of the
	 * ClientSites which it manages.  Here we add the new
	 * ClientSite to that list.
	 */
	bonobo_container_add (container, BONOBO_OBJECT (client_site));

	return object_server;
}

extern "C" {
  static Component *
  container_activate_component (Container *container, char *component_goad_id)
  {
    Component *component;
    BonoboClientSite *client_site;
    BonoboObjectClient *server;
    
    /*
     * The ClientSite is the container-side point of contact for
     * the Embeddable.  So there is a one-to-one correspondence
     * between BonoboClientSites and BonoboEmbeddables.  */
    client_site = bonobo_client_site_new (container->container);
    
    /*
     * A BonoboObjectClient is a simple wrapper for a remote
     * BonoboObject (a server supporting Bonobo::Unknown).
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
  
  static void
  filenames_dropped (GtkWidget * widget,
		     GdkDragContext   *context,
		     gint              x,
		     gint              y,
		     GtkSelectionData *selection_data,
		     guint             info,
		     guint             time,
		     Container        *container)
  {
    GList *names, *tmp_list;
    
    names = gnome_uri_list_extract_filenames ((char *)selection_data->data);
    tmp_list = names;
    
    while (tmp_list) {
      const char *fname = (const char *)tmp_list->data;

      if (fname) {
	if (container->view_widget)
	  container = container_new (fname);
	else
	  open_pdf (container, fname);
      }

      tmp_list = g_list_next (tmp_list);
    }
  }
  
  /*
   * GtkWidget key_press method override
   *
   * Scrolls the window on keypress
   */
  static gint
  key_press_event_cb (GtkWidget *widget, GdkEventKey *event)
  {
    Container *container = (Container *) gtk_object_get_data (GTK_OBJECT (widget), "container_data");
    Component *component;
    GtkScrolledWindow *win;
    float              delta;

    g_return_val_if_fail (container != NULL, FALSE);

    win       = container->scroll;
    component = container->component;
    if (component == NULL || win == NULL)
      return FALSE;

    /*
     * Scrolling the view.
     */
    if (event->keyval == GDK_Up) {
      GtkAdjustment *adj = gtk_scrolled_window_get_vadjustment (win);

      if (event->state & GDK_CONTROL_MASK)
	delta = adj->step_increment * 3;
      else
	delta = adj->step_increment;

      adj->value = CLAMP (adj->value - delta,
			  adj->lower, adj->upper - adj->page_size);

      gtk_adjustment_value_changed (adj);
      return TRUE;
    } else if (event->keyval == GDK_Down) {
      GtkAdjustment *adj = gtk_scrolled_window_get_vadjustment (win);

      if (event->state & GDK_CONTROL_MASK)
	delta = adj->step_increment * 3;
      else
	delta = adj->step_increment;

      adj->value = CLAMP (adj->value + delta,
			  adj->lower, adj->upper - adj->page_size);
      gtk_adjustment_value_changed (adj);
      return TRUE;
    } else if (event->keyval == GDK_Left) {
      GtkAdjustment *adj = gtk_scrolled_window_get_hadjustment (win);

      if (event->state & GDK_CONTROL_MASK)
	delta = adj->step_increment * 3;
      else
	delta = adj->step_increment;

      adj->value = CLAMP (adj->value - delta,
			  adj->lower, adj->upper - adj->page_size);
      gtk_adjustment_value_changed (adj);
      return TRUE;
    } else if (event->keyval == GDK_Right) {
      GtkAdjustment *adj = gtk_scrolled_window_get_hadjustment (win);

      if (event->state & GDK_CONTROL_MASK)
	delta = adj->step_increment * 3;
      else
	delta = adj->step_increment;

      adj->value = CLAMP (adj->value + delta,
			  adj->lower, adj->upper - adj->page_size);
      gtk_adjustment_value_changed (adj);
      return TRUE;

      /*
       * Various shortcuts mapped to verbs.
       */

    } else if (event->keyval == GDK_Home) {
      bonobo_view_frame_view_do_verb (component->view_frame, VERB_FIRST);
      return TRUE;
    } else if (event->keyval == GDK_End) {
      bonobo_view_frame_view_do_verb (component->view_frame, VERB_LAST);
      return TRUE;
    } else if (event->keyval == GDK_Page_Down ||
	       event->keyval == GDK_Next) {
      bonobo_view_frame_view_do_verb (component->view_frame, VERB_NEXT);
      return TRUE;
    } else if (event->keyval == GDK_Page_Up ||
	       event->keyval == GDK_Prior) {
      bonobo_view_frame_view_do_verb (component->view_frame, VERB_PREV);
      return TRUE;
    } else if (event->keyval == GDK_plus ||
	       event->keyval == GDK_equal) {
      bonobo_view_frame_view_do_verb (component->view_frame, VERB_Z_IN);
    } else if (event->keyval == GDK_underscore ||
	       event->keyval == GDK_minus) {
      bonobo_view_frame_view_do_verb (component->view_frame, VERB_Z_OUT);
    }    
    return FALSE;
  }
}

static void
container_create_menus (Container *container)
{
	BonoboUIHandlerMenuItem *menu_list;

	bonobo_ui_handler_create_menubar (container->uih);

	/*
	 * Create the basic menus out of UIInfo structures.
	 */
	menu_list = bonobo_ui_handler_menu_parse_uiinfo_list_with_data (container_main_menu, container);
	bonobo_ui_handler_menu_add_list (container->uih, "/", menu_list);
	bonobo_ui_handler_menu_free_list (menu_list);
}

static void
container_create_toolbar (Container *container)
{
	BonoboUIHandlerToolbarItem *toolbar;

	bonobo_ui_handler_create_toolbar (container->uih, "pdf");
	toolbar = bonobo_ui_handler_toolbar_parse_uiinfo_list_with_data (container_toolbar, container);
	bonobo_ui_handler_toolbar_add_list (container->uih, "/pdf/", toolbar);
	bonobo_ui_handler_toolbar_free_list (toolbar);
}

static Container *
container_new (const char *fname)
{
	Container *container;
	static GtkTargetEntry drag_types[] =
	{
	  { "text/uri-list", 0, 0 },
	};
	static gint n_drag_types = sizeof (drag_types) / sizeof (drag_types [0]);
	
	container = g_new0 (Container, 1);

	container->app = gnome_app_new ("pdf-viewer",
					"GNOME PDF viewer");

	gtk_drag_dest_set (container->app,
			   GTK_DEST_DEFAULT_ALL,
			   drag_types, n_drag_types,
			   GDK_ACTION_COPY);

	gtk_signal_connect (GTK_OBJECT(container->app),
			    "drag_data_received",
			    GTK_SIGNAL_FUNC(filenames_dropped),
			    (gpointer)container);

	gtk_window_set_default_size (GTK_WINDOW (container->app), 600, 600);
	gtk_window_set_policy (GTK_WINDOW (container->app), TRUE, TRUE, FALSE);

	container->container   = bonobo_container_new ();
	container->view_widget = NULL;
	container->scroll = GTK_SCROLLED_WINDOW (gtk_scrolled_window_new (NULL, NULL));
	gtk_scrolled_window_set_policy (container->scroll, GTK_POLICY_ALWAYS,
					GTK_POLICY_ALWAYS);
	gnome_app_set_contents (GNOME_APP (container->app), GTK_WIDGET (container->scroll));

	gtk_object_set_data (GTK_OBJECT (container->app), "container_data", container);
	gtk_signal_connect  (GTK_OBJECT (container->app), "key_press_event",
			     GTK_SIGNAL_FUNC (key_press_event_cb), NULL);
	gtk_signal_connect  (GTK_OBJECT (container->app), "delete_event",
			     GTK_SIGNAL_FUNC (container_destroy_cb), container);

	/*
	 * Create the BonoboUIHandler object which will be used to
	 * create the container's menus and toolbars.  The UIHandler
	 * also creates a CORBA server which embedded components use
	 * to do menu/toolbar merging.
	 */
	container->uih = bonobo_ui_handler_new ();
	bonobo_ui_handler_set_app (container->uih, GNOME_APP (container->app));

	container_create_menus   (container);
	container_create_toolbar (container);

	gtk_widget_show_all (container->app);

	containers = g_list_append (containers, container);

	if (fname)
	  if (!open_pdf (container, fname)) {
	    container_destroy (container);
	    return NULL;
	  }

	gtk_widget_show_all (container->app);

	return container;
}

int
main (int argc, char **argv)
{
  CORBA_Environment ev;
  CORBA_ORB         orb;
  const char      **view_files = NULL;
  gboolean          loaded;
  int               i;
  
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
  loaded = FALSE;
  if (view_files) {
    for (i = 0; view_files[i]; i++)
      if (container_new (view_files[i])) {
	loaded = TRUE;
	while (gtk_events_pending ())
	  gtk_main_iteration ();
      }
  }
  if ((i == 0) || !loaded)
    container_new (NULL);
  
  poptFreeContext (ctx);

  gtk_main ();
	
  return 0;
}
