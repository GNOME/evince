/*
 * PDF viewer Bonobo container.
 *
 * Author:
 *   Michael Meeks <michael@imaginator.com>
 */
#include <aconf.h>
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
extern "C" {
#define GString G_String
#include <gnome.h>

#include <liboaf/liboaf.h>

#include <gdk/gdkprivate.h>
#include <gdk/gdkx.h>
#include <bonobo.h>
#undef  GString 
}
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
	BonoboItemContainer *container;
	BonoboUIComponent   *ui_component;
  
	GtkWidget           *app;
	GtkWidget           *slot;
	GtkWidget           *view_widget;
	Component           *component;
};

struct  _Component {
	Container	  *container;

	BonoboClientSite   *client_site;
	BonoboViewFrame	   *view_frame;
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
  static void       container_dump_cmd  (GtkWidget *widget, Container *container);
  static Component *container_activate_component (Container *container, char *component_goad_id);
}

/*
 * The menus.
 */
BonoboUIVerb verbs [] = {
	BONOBO_UI_UNSAFE_VERB ("FileOpen",  container_open_cmd),
	BONOBO_UI_UNSAFE_VERB ("FileClose", container_close_cmd),
	BONOBO_UI_UNSAFE_VERB ("FileExit",  container_exit_cmd),

	BONOBO_UI_UNSAFE_VERB ("HelpAbout", container_about_cmd),

	BONOBO_UI_UNSAFE_VERB ("DebugDumpXml", container_dump_cmd),

	BONOBO_UI_VERB_END
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

    comp = container_activate_component (
	    container, "OAFIID:GNOME_XPDF_Embeddable");

    if (!comp || !(object = comp->server)) {
      gnome_error_dialog (_("Could not launch bonobo object."));
      return FALSE;
    }
    
    CORBA_exception_init (&ev);
    persist = Bonobo_Unknown_queryInterface (
      bonobo_object_corba_objref (BONOBO_OBJECT (object)),
      "IDL:Bonobo/PersistStream:1.0", &ev);
    
    if (ev._major != CORBA_NO_EXCEPTION ||
	persist == CORBA_OBJECT_NIL) {
      gnome_error_dialog ("Panic: component doesn't implement PersistStream.");
      return FALSE;
    }
    
    stream = bonobo_stream_open (BONOBO_IO_DRIVER_FS, name, Bonobo_Storage_READ, 0);
    
    if (stream == NULL) {
      char *err = g_strdup_printf (_("Could not open %s"), name);
      gnome_error_dialog_parented (err, GTK_WINDOW(container->app));
      g_free (err);
      return FALSE;
    }
    
    Bonobo_PersistStream_load (persist,
			      (Bonobo_Stream) bonobo_object_corba_objref (BONOBO_OBJECT (stream)),
			       "application/pdf",
			       &ev);

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
	dialog = gnome_message_box_new (_("Can't open a directory"),
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
container_dump_cmd (GtkWidget *widget, Container *container)
{
	bonobo_window_dump (BONOBO_WINDOW(container->app), "on demand");
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
	view_frame = bonobo_client_site_new_view (
		component->client_site,
		bonobo_ui_component_get_container (container->ui_component));

	component->view_frame = view_frame;

	/*
	 * Embed the view frame into the application.
	 */
	view_widget = bonobo_view_frame_get_wrapper (view_frame);
	bonobo_wrapper_set_visibility (BONOBO_WRAPPER (view_widget), FALSE);
	container->view_widget = view_widget;
	container->component   = component;

	gtk_container_add (GTK_CONTAINER (container->slot), view_widget);

	/*
	 * Activate it ( get it to merge menus etc. )
	 */
	bonobo_view_frame_view_activate (view_frame);
	bonobo_view_frame_set_covered   (view_frame, FALSE);

	gtk_widget_show_all (GTK_WIDGET (container->slot));
}

static BonoboObjectClient *
container_launch_component (BonoboClientSite    *client_site,
			    BonoboItemContainer *container,
			    char                *component_goad_id)
{
	BonoboObjectClient *object_server;

	/*
	 * Launch the component.
	 */
	object_server = bonobo_object_activate (component_goad_id, 0);

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
	BonoboUIContainer *ui_container;
	
	container = g_new0 (Container, 1);

	container->app = bonobo_window_new ("pdf-viewer",
					 _("GNOME PDF viewer"));

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

	container->container   = bonobo_item_container_new ();
	container->view_widget = NULL;
	container->slot = gtk_event_box_new ();
	gtk_widget_show (container->slot);

	bonobo_window_set_contents (BONOBO_WINDOW(container->app),
				 GTK_WIDGET (container->slot));
	gtk_widget_show_all (container->slot);

	gtk_object_set_data (GTK_OBJECT (container->app), "container_data", container);
	gtk_signal_connect  (GTK_OBJECT (container->app), "delete_event",
			     GTK_SIGNAL_FUNC (container_destroy_cb), container);

	ui_container = bonobo_ui_container_new ();
	bonobo_ui_container_set_win (ui_container, BONOBO_WINDOW(container->app));

	container->ui_component = bonobo_ui_component_new ("gpdf");
	bonobo_ui_component_set_container (
		container->ui_component,
		bonobo_object_corba_objref (BONOBO_OBJECT (ui_container)));

	bonobo_ui_component_add_verb_list_with_data (
		container->ui_component, verbs, container);

	bonobo_ui_util_set_ui (container->ui_component, DATADIR, "gpdf-ui.xml", "gpdf");

	gtk_widget_show (container->app);

	containers = g_list_append (containers, container);

	if (fname)
	  if (!open_pdf (container, fname)) {
	    container_destroy (container);
	    return NULL;
	  }

	gtk_widget_show (container->app);

	return container;
}

int
main (int argc, char **argv)
{
	const char      **view_files = NULL;
	gboolean          loaded;
	int               i;

	bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	textdomain (PACKAGE);
 	
	gnomelib_register_popt_table (oaf_popt_options, "OAF");
	gnome_init_with_popt_table("PDFViewer", "0." VERSION,
				   argc, argv,
				   gpdf_popt_options, 0, &ctx); 
	
	oaf_init (argc, argv);
	
	if (!bonobo_init (CORBA_OBJECT_NIL, 
			  CORBA_OBJECT_NIL, 
			  CORBA_OBJECT_NIL))
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
	
	gtk_widget_push_visual (gdk_rgb_get_visual ());
	gtk_widget_push_colormap (gdk_rgb_get_cmap ());

	gtk_main ();
	
	return 0;
}
