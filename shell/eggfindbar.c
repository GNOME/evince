/* Copyright (C) 2004 Red Hat, Inc.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with the Gnome Library; see the file COPYING.LIB.  If not,
write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.
*/

#include <config.h>

#include "eggfindbar.h"

#include <glib/gi18n.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkcheckbutton.h>
#include <gtk/gtkvseparator.h>
#include <gtk/gtkstock.h>
#include <gtk/gtklabel.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkbindings.h>

#include <string.h>

struct _EggFindBarPrivate
{
  gchar *search_string;
  GtkWidget *hbox;
  GtkWidget *find_entry;
  GtkWidget *next_button;
  GtkWidget *previous_button;
  GtkWidget *case_button;
  GtkWidget *status_separator;
  GtkWidget *status_label;
  gulong set_focus_handler;
  guint case_sensitive : 1;
};

#define EGG_FIND_BAR_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), EGG_TYPE_FIND_BAR, EggFindBarPrivate))


enum
  {
    PROP_0,
    PROP_SEARCH_STRING,
    PROP_CASE_SENSITIVE
  };

static void egg_find_bar_finalize      (GObject        *object);
static void egg_find_bar_get_property  (GObject        *object,
                                        guint           prop_id,
                                        GValue         *value,
                                        GParamSpec     *pspec);
static void egg_find_bar_set_property  (GObject        *object,
                                        guint           prop_id,
                                        const GValue   *value,
                                        GParamSpec     *pspec);
static void egg_find_bar_size_request  (GtkWidget      *widget,
                                        GtkRequisition *requisition);
static void egg_find_bar_size_allocate (GtkWidget      *widget,
                                        GtkAllocation  *allocation);
static void egg_find_bar_show          (GtkWidget *widget);
static void egg_find_bar_hide          (GtkWidget *widget);
static void egg_find_bar_grab_focus    (GtkWidget *widget);

G_DEFINE_TYPE (EggFindBar, egg_find_bar, GTK_TYPE_BIN);

enum
  {
    NEXT,
    PREVIOUS,
    CLOSE,
    SCROLL,
    LAST_SIGNAL
  };

static guint find_bar_signals[LAST_SIGNAL] = { 0 };

static void
egg_find_bar_class_init (EggFindBarClass *klass)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkBinClass *bin_class;
  GtkBindingSet *binding_set;
        
  egg_find_bar_parent_class = g_type_class_peek_parent (klass);

  object_class = (GObjectClass *)klass;
  widget_class = (GtkWidgetClass *)klass;
  bin_class = (GtkBinClass *)klass;

  object_class->set_property = egg_find_bar_set_property;
  object_class->get_property = egg_find_bar_get_property;

  object_class->finalize = egg_find_bar_finalize;

  widget_class->size_request = egg_find_bar_size_request;
  widget_class->size_allocate = egg_find_bar_size_allocate;
  widget_class->show = egg_find_bar_show;
  widget_class->hide = egg_find_bar_hide;
  widget_class->grab_focus = egg_find_bar_grab_focus;

  find_bar_signals[NEXT] =
    g_signal_new ("next",
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (EggFindBarClass, next),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  find_bar_signals[PREVIOUS] =
    g_signal_new ("previous",
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (EggFindBarClass, previous),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  find_bar_signals[CLOSE] =
    g_signal_new ("close",
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (EggFindBarClass, close),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  find_bar_signals[SCROLL] =
    g_signal_new ("scroll",
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (EggFindBarClass, scroll),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__ENUM,
		  G_TYPE_NONE, 1,
		  GTK_TYPE_SCROLL_TYPE);

  /**
   * EggFindBar:search_string:
   *
   * The current string to search for. NULL or empty string
   * both mean no current string.
   *
   */
  g_object_class_install_property (object_class,
				   PROP_SEARCH_STRING,
				   g_param_spec_string ("search_string",
							_("Search string"),
							_("The name of the string to be found"),
							NULL,
							G_PARAM_READWRITE));

  /**
   * EggFindBar:case_sensitive:
   *
   * TRUE for a case sensitive search.
   *
   */
  g_object_class_install_property (object_class,
				   PROP_CASE_SENSITIVE,
				   g_param_spec_boolean ("case_sensitive",
                                                         _("Case sensitive"),
                                                         _("TRUE for a case sensitive search"),
                                                         FALSE,
                                                         G_PARAM_READWRITE));

  /* Style properties */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_boxed ("all_matches_color",
                                                               _("Highlight color"),
                                                               _("Color of highlight for all matches"),
                                                               GDK_TYPE_COLOR,
                                                               G_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_boxed ("current_match_color",
                                                               _("Current color"),
                                                               _("Color of highlight for the current match"),
                                                               GDK_TYPE_COLOR,
                                                               G_PARAM_READABLE));

  g_type_class_add_private (object_class, sizeof (EggFindBarPrivate));

  binding_set = gtk_binding_set_by_class (klass);

  gtk_binding_entry_add_signal (binding_set, GDK_Escape, 0,
				"close", 0);

  gtk_binding_entry_add_signal (binding_set, GDK_Up, 0,
                                "scroll", 1,
                                GTK_TYPE_SCROLL_TYPE, GTK_SCROLL_STEP_BACKWARD);

  gtk_binding_entry_add_signal (binding_set, GDK_Down, 0,
                                "scroll", 1,
                                GTK_TYPE_SCROLL_TYPE, GTK_SCROLL_STEP_FORWARD);
}

static void
egg_find_bar_emit_next (EggFindBar *find_bar)
{
  g_signal_emit (find_bar, find_bar_signals[NEXT], 0);
}

static void
egg_find_bar_emit_previous (EggFindBar *find_bar)
{
  g_signal_emit (find_bar, find_bar_signals[PREVIOUS], 0);
}

static void
next_clicked_callback (GtkButton *button,
                       void      *data)
{
  EggFindBar *find_bar = EGG_FIND_BAR (data);

  egg_find_bar_emit_next (find_bar);
}

static void
previous_clicked_callback (GtkButton *button,
                           void      *data)
{
  EggFindBar *find_bar = EGG_FIND_BAR (data);

  egg_find_bar_emit_previous (find_bar);
}

static void
case_sensitive_toggled_callback (GtkCheckButton *button,
                                 void           *data)
{
  EggFindBar *find_bar = EGG_FIND_BAR (data);

  egg_find_bar_set_case_sensitive (find_bar,
                                   gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)));
}

static void
entry_activate_callback (GtkEntry *entry,
                          void     *data)
{
  EggFindBar *find_bar = EGG_FIND_BAR (data);
  EggFindBarPrivate *priv = (EggFindBarPrivate *)find_bar->priv;

  /* We activate the "next" button here so we'll get a nice
     animation */
  if (find_bar->priv->search_string != NULL)
	  gtk_widget_activate (priv->next_button);
}

static void
entry_changed_callback (GtkEntry *entry,
                        void     *data)
{
  EggFindBar *find_bar = EGG_FIND_BAR (data);
  char *text;

  /* paranoid strdup because set_search_string() sets
   * the entry text
   */
  text = g_strdup (gtk_entry_get_text (entry));

  egg_find_bar_set_search_string (find_bar, text);
  
  g_free (text);
}

static void
set_focus_cb (GtkWidget *window,
	      GtkWidget *widget,
	      EggFindBar *bar)
{
  GtkWidget *wbar = GTK_WIDGET (bar);

  while (widget != NULL && widget != wbar)
    {
      widget = widget->parent;
    }

  /* if widget == bar, the new focus widget is in the bar, so we
   * don't deactivate.
   */
  if (widget != wbar)
    {
      g_signal_emit (bar, find_bar_signals[CLOSE], 0);
    }
}

static void
egg_find_bar_init (EggFindBar *find_bar)
{
  EggFindBarPrivate *priv;
  GtkWidget *label;
  GtkWidget *separator;
  GtkWidget *image_back;
  GtkWidget *image_forward;

  /* Data */
  priv = EGG_FIND_BAR_GET_PRIVATE (find_bar);
  find_bar->priv = priv;

  priv->search_string = NULL;

  /* Widgets */
  gtk_widget_push_composite_child ();
  priv->hbox = gtk_hbox_new (FALSE, 6);
  gtk_container_set_border_width (GTK_CONTAINER (priv->hbox), 3);

  label = gtk_label_new_with_mnemonic (_("F_ind:"));
  separator = gtk_vseparator_new ();

  priv->find_entry = gtk_entry_new ();
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), priv->find_entry);

  priv->previous_button = gtk_button_new_with_mnemonic (_("_Previous"));
  gtk_button_set_focus_on_click (GTK_BUTTON (priv->previous_button), FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET (priv->previous_button), FALSE);

  priv->next_button = gtk_button_new_with_mnemonic (_("_Next"));
  gtk_button_set_focus_on_click (GTK_BUTTON (priv->next_button), FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET (priv->next_button), FALSE);

  image_back = gtk_image_new_from_stock (GTK_STOCK_GO_BACK,
                                         GTK_ICON_SIZE_BUTTON);
  image_forward = gtk_image_new_from_stock (GTK_STOCK_GO_FORWARD,
                                            GTK_ICON_SIZE_BUTTON);

  gtk_button_set_image (GTK_BUTTON (priv->previous_button),
                        image_back);
  gtk_button_set_image (GTK_BUTTON (priv->next_button),
                        image_forward);
  
  priv->case_button = gtk_check_button_new_with_mnemonic (_("C_ase Sensitive"));

  priv->status_separator = gtk_vseparator_new ();

  priv->status_label = gtk_label_new (NULL);
  gtk_label_set_ellipsize (GTK_LABEL (priv->status_label),
                           PANGO_ELLIPSIZE_END);
  gtk_misc_set_alignment (GTK_MISC (priv->status_label), 0.0, 0.5);
  
#if 0
 {
   GtkWidget *button_label;
   /* This hack doesn't work because GtkCheckButton doesn't pass the
    * larger size allocation to the label, it always gives the label
    * its exact request. If you un-ifdef this, set the box back
    * on case_button to TRUE, TRUE below
    */
   button_label = gtk_bin_get_child (GTK_BIN (priv->case_button));
   gtk_label_set_ellipsize (GTK_LABEL (button_label),
                            PANGO_ELLIPSIZE_END);
 }
#endif

  gtk_box_pack_start (GTK_BOX (priv->hbox),
                      label, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (priv->hbox),
                      priv->find_entry, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (priv->hbox),
                      priv->previous_button, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (priv->hbox),
                      priv->next_button, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (priv->hbox),
                      separator, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (priv->hbox),
                      priv->case_button, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (priv->hbox),
                      priv->status_separator, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (priv->hbox),
                      priv->status_label, TRUE, TRUE, 0);
  
  gtk_container_add (GTK_CONTAINER (find_bar), priv->hbox);

  gtk_widget_show (priv->hbox);
  gtk_widget_show (priv->find_entry);
  gtk_widget_show (priv->previous_button);
  gtk_widget_show (priv->next_button);
  gtk_widget_show (separator);
  gtk_widget_show (label);
  gtk_widget_show (image_back);
  gtk_widget_show (image_forward);
  /* don't show status separator/label until they are set */

  gtk_widget_pop_composite_child ();

  g_signal_connect (priv->find_entry, "changed",
                    G_CALLBACK (entry_changed_callback),
                    find_bar);
  g_signal_connect (priv->find_entry, "activate",
                    G_CALLBACK (entry_activate_callback),
                    find_bar);
  g_signal_connect (priv->next_button, "clicked",
                    G_CALLBACK (next_clicked_callback),
                    find_bar);
  g_signal_connect (priv->previous_button, "clicked",
                    G_CALLBACK (previous_clicked_callback),
                    find_bar);
  g_signal_connect (priv->case_button, "toggled",
                    G_CALLBACK (case_sensitive_toggled_callback),
                    find_bar);
}

static void
egg_find_bar_finalize (GObject *object)
{
  EggFindBar *find_bar = EGG_FIND_BAR (object);
  EggFindBarPrivate *priv = (EggFindBarPrivate *)find_bar->priv;

  g_free (priv->search_string);

  G_OBJECT_CLASS (egg_find_bar_parent_class)->finalize (object);
}

static void
egg_find_bar_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  EggFindBar *find_bar = EGG_FIND_BAR (object);

  switch (prop_id)
    {
    case PROP_SEARCH_STRING:
      egg_find_bar_set_search_string (find_bar, g_value_get_string (value));
      break;
    case PROP_CASE_SENSITIVE:
      egg_find_bar_set_case_sensitive (find_bar, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
egg_find_bar_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  EggFindBar *find_bar = EGG_FIND_BAR (object);
  EggFindBarPrivate *priv = (EggFindBarPrivate *)find_bar->priv;

  switch (prop_id)
    {
    case PROP_SEARCH_STRING:
      g_value_set_string (value, priv->search_string);
      break;
    case PROP_CASE_SENSITIVE:
      g_value_set_boolean (value, priv->case_sensitive);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
egg_find_bar_size_request (GtkWidget      *widget,
                           GtkRequisition *requisition)
{
  GtkBin *bin = GTK_BIN (widget);
  GtkRequisition child_requisition;
  if (bin->child && GTK_WIDGET_VISIBLE (bin->child))
    {
      gtk_widget_size_request (bin->child, &child_requisition);

      *requisition = child_requisition;
    }
  else
    {
      requisition->width = 0;
      requisition->height = 0;
    }
}

static void
egg_find_bar_size_allocate (GtkWidget     *widget,
                            GtkAllocation *allocation)
{
  GtkBin *bin = GTK_BIN (widget);

  widget->allocation = *allocation;

  if (bin->child && GTK_WIDGET_VISIBLE (bin->child))
    gtk_widget_size_allocate (bin->child, allocation);
}

static void
egg_find_bar_show (GtkWidget *widget)
{
  EggFindBar *bar = EGG_FIND_BAR (widget);
  EggFindBarPrivate *priv = bar->priv;

  GTK_WIDGET_CLASS (egg_find_bar_parent_class)->show (widget);

  if (priv->set_focus_handler == 0)
    {
      GtkWidget *toplevel;

      toplevel = gtk_widget_get_toplevel (widget);

      priv->set_focus_handler =
	g_signal_connect (toplevel, "set-focus",
			  G_CALLBACK (set_focus_cb), bar);
    }
}

static void
egg_find_bar_hide (GtkWidget *widget)
{
  EggFindBar *bar = EGG_FIND_BAR (widget);
  EggFindBarPrivate *priv = bar->priv;

  if (priv->set_focus_handler != 0)
    {
      GtkWidget *toplevel;

      toplevel = gtk_widget_get_toplevel (widget);

      g_signal_handlers_disconnect_by_func
	(toplevel, (void (*)) G_CALLBACK (set_focus_cb), bar);
      priv->set_focus_handler = 0;
    }

  GTK_WIDGET_CLASS (egg_find_bar_parent_class)->hide (widget);
}

static void
egg_find_bar_grab_focus (GtkWidget *widget)
{
  EggFindBar *find_bar = EGG_FIND_BAR (widget);
  EggFindBarPrivate *priv = find_bar->priv;

  gtk_widget_grab_focus (priv->find_entry);
}

/**
 * egg_find_bar_new:
 *
 * Creates a new #EggFindBar.
 *
 * Returns: a newly created #EggFindBar
 *
 * Since: 2.6
 */
GtkWidget *
egg_find_bar_new (void)
{
  EggFindBar *find_bar;

  find_bar = g_object_new (EGG_TYPE_FIND_BAR, NULL);

  return GTK_WIDGET (find_bar);
}

/**
 * egg_find_bar_set_search_string:
 *
 * Sets the string that should be found/highlighted in the document.
 * Empty string is converted to NULL.
 *
 * Since: 2.6
 */
void
egg_find_bar_set_search_string  (EggFindBar *find_bar,
                                 const char *search_string)
{
  EggFindBarPrivate *priv;

  g_return_if_fail (EGG_IS_FIND_BAR (find_bar));

  priv = (EggFindBarPrivate *)find_bar->priv;

  g_object_freeze_notify (G_OBJECT (find_bar));
  
  if (priv->search_string != search_string)
    {
      char *old;
      
      old = priv->search_string;

      if (search_string && *search_string == '\0')
        search_string = NULL;

      /* Only update if the string has changed; setting the entry
       * will emit changed on the entry which will re-enter
       * this function, but we'll handle that fine with this
       * short-circuit.
       */
      if ((old && search_string == NULL) ||
          (old == NULL && search_string) ||
          (old && search_string &&
           strcmp (old, search_string) != 0))
        {
	  gboolean not_empty;
	  
          priv->search_string = g_strdup (search_string);
          g_free (old);
          
          gtk_entry_set_text (GTK_ENTRY (priv->find_entry),
                              priv->search_string ?
                              priv->search_string :
                              "");
	   
          not_empty = (search_string == NULL) ? FALSE : TRUE;		 

          gtk_widget_set_sensitive (GTK_WIDGET (find_bar->priv->next_button), not_empty);
          gtk_widget_set_sensitive (GTK_WIDGET (find_bar->priv->previous_button), not_empty);

          g_object_notify (G_OBJECT (find_bar),
                           "search_string");
        }
    }

  g_object_thaw_notify (G_OBJECT (find_bar));
}


/**
 * egg_find_bar_get_search_string:
 *
 * Gets the string that should be found/highlighted in the document.
 *
 * Returns: the string
 *
 * Since: 2.6
 */
const char*
egg_find_bar_get_search_string  (EggFindBar *find_bar)
{
  EggFindBarPrivate *priv;

  g_return_val_if_fail (EGG_IS_FIND_BAR (find_bar), NULL);

  priv = find_bar->priv;

  return priv->search_string ? priv->search_string : "";
}

/**
 * egg_find_bar_set_case_sensitive:
 *
 * Sets whether the search is case sensitive
 *
 * Since: 2.6
 */
void
egg_find_bar_set_case_sensitive (EggFindBar *find_bar,
                                 gboolean    case_sensitive)
{
  EggFindBarPrivate *priv;

  g_return_if_fail (EGG_IS_FIND_BAR (find_bar));

  priv = (EggFindBarPrivate *)find_bar->priv;

  g_object_freeze_notify (G_OBJECT (find_bar));

  case_sensitive = case_sensitive != FALSE;

  if (priv->case_sensitive != case_sensitive)
    {
      priv->case_sensitive = case_sensitive;

      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->case_button),
                                    priv->case_sensitive);

      g_object_notify (G_OBJECT (find_bar),
                       "case_sensitive");
    }

  g_object_thaw_notify (G_OBJECT (find_bar));
}

/**
 * egg_find_bar_get_case_sensitive:
 *
 * Gets whether the search is case sensitive
 *
 * Returns: TRUE if it's case sensitive
 *
 * Since: 2.6
 */
gboolean
egg_find_bar_get_case_sensitive (EggFindBar *find_bar)
{
  EggFindBarPrivate *priv;

  g_return_val_if_fail (EGG_IS_FIND_BAR (find_bar), FALSE);

  priv = (EggFindBarPrivate *)find_bar->priv;

  return priv->case_sensitive;
}

static void
get_style_color (EggFindBar *find_bar,
                 const char *style_prop_name,
                 GdkColor   *color)
{
  GdkColor *style_color;

  gtk_widget_ensure_style (GTK_WIDGET (find_bar));
  gtk_widget_style_get (GTK_WIDGET (find_bar),
                        "color", &style_color, NULL);
  if (style_color)
    {
      *color = *style_color;
      gdk_color_free (style_color);
    }
}

/**
 * egg_find_bar_get_all_matches_color:
 *
 * Gets the color to use to highlight all the
 * known matches.
 *
 * Since: 2.6
 */
void
egg_find_bar_get_all_matches_color (EggFindBar *find_bar,
                                    GdkColor   *color)
{
  GdkColor found_color = { 0, 0, 0, 0x0f0f };

  get_style_color (find_bar, "all_matches_color", &found_color);

  *color = found_color;
}

/**
 * egg_find_bar_get_current_match_color:
 *
 * Gets the color to use to highlight the match
 * we're currently on.
 *
 * Since: 2.6
 */
void
egg_find_bar_get_current_match_color (EggFindBar *find_bar,
                                      GdkColor   *color)
{
  GdkColor found_color = { 0, 0, 0, 0xffff };

  get_style_color (find_bar, "current_match_color", &found_color);

  *color = found_color;
}

/**
 * egg_find_bar_set_status_text:
 *
 * Sets some text to display if there's space; typical text would
 * be something like "5 results on this page" or "No results"
 *
 * @text: the text to display
 *
 * Since: 2.6
 */
void
egg_find_bar_set_status_text (EggFindBar *find_bar,
                              const char *text)
{
  EggFindBarPrivate *priv;

  g_return_if_fail (EGG_IS_FIND_BAR (find_bar));

  priv = (EggFindBarPrivate *)find_bar->priv;
  
  if (text == NULL || *text == '\0')
    {
      gtk_widget_hide (priv->status_label);
      gtk_widget_hide (priv->status_separator);
      gtk_label_set_text (GTK_LABEL (priv->status_label), NULL);
    }
  else
    {
      gtk_label_set_text (GTK_LABEL (priv->status_label), text);
      gtk_widget_show (priv->status_label);
      gtk_widget_show (priv->status_separator);
    }
}
