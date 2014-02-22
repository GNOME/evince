/* Copyright (C) 2004 Red Hat, Inc.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 * 
 * You should have received a copy of the GNU Library General Public
 * License along with the Gnome Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, 
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"

#include <string.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "eggfindbar.h"

struct _EggFindBarPrivate
{
  gchar *search_string;

  GtkWidget *next_button;
  GtkWidget *previous_button;
  GtkToolItem *status_item;

  GtkWidget *find_entry;
  GtkWidget *status_label;

  guint case_sensitive : 1;
  guint case_sensitive_enabled : 1;
  guint whole_words_only : 1;
  guint whole_words_only_enabled : 1;
};

#define EGG_FIND_BAR_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), EGG_TYPE_FIND_BAR, EggFindBarPrivate))

enum {
    PROP_0,
    PROP_SEARCH_STRING,
    PROP_CASE_SENSITIVE,
    PROP_WHOLE_WORDS_ONLY
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
static void egg_find_bar_grab_focus    (GtkWidget *widget);

G_DEFINE_TYPE (EggFindBar, egg_find_bar, GTK_TYPE_TOOLBAR);

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
  GtkBindingSet *binding_set;
        
  egg_find_bar_parent_class = g_type_class_peek_parent (klass);

  object_class = (GObjectClass *)klass;
  widget_class = (GtkWidgetClass *)klass;

  object_class->set_property = egg_find_bar_set_property;
  object_class->get_property = egg_find_bar_get_property;

  object_class->finalize = egg_find_bar_finalize;

  widget_class->grab_focus = egg_find_bar_grab_focus;

  find_bar_signals[NEXT] =
    g_signal_new ("next",
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (EggFindBarClass, next),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  find_bar_signals[PREVIOUS] =
    g_signal_new ("previous",
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
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
				   g_param_spec_string ("search-string",
							"Search string",
							"The name of the string to be found",
							NULL,
							G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));

  /**
   * EggFindBar:case_sensitive:
   *
   * TRUE for a case sensitive search.
   *
   */
  g_object_class_install_property (object_class,
				   PROP_CASE_SENSITIVE,
				   g_param_spec_boolean ("case-sensitive",
                                                         "Case sensitive",
                                                         "TRUE for a case sensitive search",
                                                         FALSE,
                                                         G_PARAM_READWRITE |
                                                         G_PARAM_STATIC_STRINGS));

  /**
   * EggFindBar:whole-words-only:
   *
   * Whether search whole words only
   */
  g_object_class_install_property (object_class,
                                   PROP_WHOLE_WORDS_ONLY,
                                   g_param_spec_boolean ("whole-words-only",
                                                         "Whole words only",
                                                         "Whether search whole words only",
                                                         FALSE,
                                                         G_PARAM_READWRITE |
                                                         G_PARAM_STATIC_STRINGS));

  g_type_class_add_private (object_class, sizeof (EggFindBarPrivate));

  binding_set = gtk_binding_set_by_class (klass);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Escape, 0,
				"close", 0);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Up, 0,
                                "scroll", 1,
                                GTK_TYPE_SCROLL_TYPE, GTK_SCROLL_STEP_BACKWARD);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Down, 0,
                                "scroll", 1,
                                GTK_TYPE_SCROLL_TYPE, GTK_SCROLL_STEP_FORWARD);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Page_Up, 0,
				"scroll", 1,
				GTK_TYPE_SCROLL_TYPE, GTK_SCROLL_PAGE_BACKWARD);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Page_Up, 0,
				"scroll", 1,
				GTK_TYPE_SCROLL_TYPE, GTK_SCROLL_PAGE_BACKWARD);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Page_Down, 0,
				"scroll", 1,
				GTK_TYPE_SCROLL_TYPE, GTK_SCROLL_PAGE_FORWARD);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Page_Down, 0,
				"scroll", 1,
				GTK_TYPE_SCROLL_TYPE, GTK_SCROLL_PAGE_FORWARD);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Return, GDK_SHIFT_MASK,
                                "previous", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Up, GDK_CONTROL_MASK,
                                "previous", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Down, GDK_CONTROL_MASK,
                                "next", 0);
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
close_button_clicked_callback (GtkButton *button,
                               EggFindBar *find_bar)
{
  g_signal_emit (find_bar, find_bar_signals[CLOSE], 0);
}

static void
case_sensitive_toggled_callback (GtkCheckMenuItem *menu_item,
                                 void             *data)
{
  EggFindBar *find_bar = EGG_FIND_BAR (data);

  egg_find_bar_set_case_sensitive (find_bar, gtk_check_menu_item_get_active (menu_item));
}

static void
whole_words_only_toggled_callback (GtkCheckMenuItem *menu_item,
                                   void             *data)
{
  EggFindBar *find_bar = EGG_FIND_BAR (data);

  egg_find_bar_set_whole_words_only (find_bar, gtk_check_menu_item_get_active (menu_item));
}

static void
entry_activate_callback (GtkEntry *entry,
                          void     *data)
{
  EggFindBar *find_bar = EGG_FIND_BAR (data);

  if (find_bar->priv->search_string != NULL)
    egg_find_bar_emit_next (find_bar);
}

static void
egg_find_bar_entry_populate_popup (EggFindBar *find_bar,
                                   GtkWidget  *menu)
{
  GtkWidget *menu_item;

  if (find_bar->priv->whole_words_only_enabled)
    {
      menu_item = gtk_check_menu_item_new_with_mnemonic (_("_Whole Words Only"));
      g_signal_connect (menu_item, "toggled",
                        G_CALLBACK (whole_words_only_toggled_callback),
                        find_bar);
      gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menu_item),
                                      find_bar->priv->whole_words_only);
      gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), menu_item);
      gtk_widget_show (menu_item);
    }

  if (find_bar->priv->case_sensitive_enabled)
    {
      menu_item = gtk_check_menu_item_new_with_mnemonic (_("C_ase Sensitive"));
      g_signal_connect (menu_item, "toggled",
                        G_CALLBACK (case_sensitive_toggled_callback),
                        find_bar);
      gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menu_item),
                                      find_bar->priv->case_sensitive);
      gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), menu_item);
      gtk_widget_show (menu_item);
    }
}

static void
entry_icon_release_callback (GtkEntry            *entry,
                             GtkEntryIconPosition icon_pos,
                             GdkEventButton      *event,
                             void                *data)
{
  EggFindBar *find_bar = EGG_FIND_BAR (data);
  GtkWidget  *menu;

  if (!find_bar->priv->case_sensitive_enabled &&
      !find_bar->priv->whole_words_only_enabled)
    return;

  menu = gtk_menu_new ();
  egg_find_bar_entry_populate_popup (find_bar, menu);
  gtk_widget_show (menu);

  gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL,
                  event->button, event->time);
}

static void
entry_populate_popup_callback (GtkEntry *entry,
                               GtkMenu  *menu,
                               void     *data)
{
  EggFindBar *find_bar = EGG_FIND_BAR (data);
  GtkWidget  *separator;

  if (!find_bar->priv->case_sensitive_enabled &&
      !find_bar->priv->whole_words_only_enabled)
    return;

  separator = gtk_separator_menu_item_new ();
  gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), separator);
  gtk_widget_show (separator);
  egg_find_bar_entry_populate_popup (find_bar, GTK_WIDGET (menu));
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
egg_find_bar_init (EggFindBar *find_bar)
{
  EggFindBarPrivate *priv;
  GtkWidget *box;
  GtkWidget *close_button;
  GtkToolItem *item;
  GtkStyleContext *style_context;

  /* Data */
  priv = EGG_FIND_BAR_GET_PRIVATE (find_bar);
  
  find_bar->priv = priv;  
  priv->search_string = NULL;

  gtk_toolbar_set_style (GTK_TOOLBAR (find_bar), GTK_TOOLBAR_BOTH_HORIZ);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  style_context = gtk_widget_get_style_context (box);
  gtk_style_context_add_class (style_context, GTK_STYLE_CLASS_LINKED);
  gtk_style_context_add_class (style_context, GTK_STYLE_CLASS_RAISED);

  /* Entry */
  priv->find_entry = gtk_entry_new ();
  gtk_entry_set_width_chars (GTK_ENTRY (priv->find_entry), 32);
  gtk_entry_set_max_length (GTK_ENTRY (priv->find_entry), 512);

  /* Find options */
  gtk_entry_set_icon_from_icon_name (GTK_ENTRY (priv->find_entry),
                                     GTK_ENTRY_ICON_PRIMARY,
                                     "edit-find-symbolic");
  gtk_entry_set_icon_activatable (GTK_ENTRY (priv->find_entry),
                                  GTK_ENTRY_ICON_PRIMARY,
                                  TRUE);
  gtk_entry_set_icon_tooltip_text (GTK_ENTRY (priv->find_entry),
                                   GTK_ENTRY_ICON_PRIMARY,
                                   _("Find options"));

  gtk_box_pack_start (GTK_BOX (box), priv->find_entry, TRUE, TRUE, 0);
  gtk_widget_show (priv->find_entry);

  /* Prev */
  priv->previous_button = gtk_button_new ();
  gtk_button_set_image (GTK_BUTTON (priv->previous_button),
                        gtk_image_new_from_icon_name ("go-up-symbolic", GTK_ICON_SIZE_MENU));
  gtk_widget_set_tooltip_text (priv->previous_button, _("Find previous occurrence of the search string"));
  gtk_widget_set_can_focus (priv->previous_button, FALSE);
  gtk_widget_set_sensitive (priv->previous_button, FALSE);
  gtk_container_add (GTK_CONTAINER (box), priv->previous_button);
  gtk_widget_show (priv->previous_button);

  /* Next */
  priv->next_button = gtk_button_new ();
  gtk_button_set_image (GTK_BUTTON (priv->next_button),
                        gtk_image_new_from_icon_name ("go-down-symbolic", GTK_ICON_SIZE_MENU));
  gtk_widget_set_tooltip_text (priv->next_button, _("Find next occurrence of the search string"));
  gtk_widget_set_can_focus (priv->next_button, FALSE);
  gtk_widget_set_sensitive (priv->next_button, FALSE);
  gtk_container_add (GTK_CONTAINER (box), priv->next_button);
  gtk_widget_show (priv->next_button);

  item = gtk_tool_item_new ();
  gtk_widget_set_margin_right (GTK_WIDGET (item), 12);
  gtk_container_add (GTK_CONTAINER (item), box);
  gtk_widget_show (box);

  gtk_container_add (GTK_CONTAINER (find_bar), GTK_WIDGET (item));
  gtk_widget_show (GTK_WIDGET (item));

  /* Status */
  priv->status_item = gtk_tool_item_new();
  gtk_tool_item_set_expand (priv->status_item, TRUE);
  priv->status_label = gtk_label_new (NULL);
  gtk_label_set_ellipsize (GTK_LABEL (priv->status_label),
                           PANGO_ELLIPSIZE_END);
  gtk_misc_set_alignment (GTK_MISC (priv->status_label), 0.0, 0.5);
  gtk_container_add (GTK_CONTAINER (priv->status_item), priv->status_label);
  gtk_widget_show (priv->status_label);
  gtk_container_add (GTK_CONTAINER (find_bar), GTK_WIDGET (priv->status_item));

  /* Separator */
  item = gtk_tool_item_new ();
  gtk_tool_item_set_expand (GTK_TOOL_ITEM (item), TRUE);
  gtk_container_add (GTK_CONTAINER (find_bar), GTK_WIDGET (item));
  gtk_widget_show (GTK_WIDGET (item));

  /* Close button */
  close_button = gtk_button_new ();
  style_context = gtk_widget_get_style_context (close_button);
  gtk_style_context_add_class (style_context, GTK_STYLE_CLASS_RAISED);
  gtk_button_set_relief (GTK_BUTTON (close_button), GTK_RELIEF_NONE);
  gtk_button_set_image (GTK_BUTTON (close_button),
                        gtk_image_new_from_icon_name ("window-close-symbolic", GTK_ICON_SIZE_MENU));
  gtk_widget_set_can_focus (close_button, FALSE);
  item = gtk_tool_item_new ();
  gtk_container_add (GTK_CONTAINER (item), close_button);
  gtk_widget_show (close_button);
  gtk_container_add (GTK_CONTAINER (find_bar), GTK_WIDGET (item));
  gtk_widget_show (GTK_WIDGET (item));

  g_signal_connect (priv->find_entry, "changed",
                    G_CALLBACK (entry_changed_callback),
                    find_bar);
  g_signal_connect (priv->find_entry, "activate",
                    G_CALLBACK (entry_activate_callback),
                    find_bar);
  g_signal_connect (priv->find_entry, "icon-release",
                    G_CALLBACK (entry_icon_release_callback),
                    find_bar);
  g_signal_connect (priv->find_entry, "populate-popup",
                    G_CALLBACK (entry_populate_popup_callback),
                    find_bar);
  g_signal_connect (priv->next_button, "clicked",
                    G_CALLBACK (next_clicked_callback),
                    find_bar);
  g_signal_connect (priv->previous_button, "clicked",
                    G_CALLBACK (previous_clicked_callback),
                    find_bar);
  g_signal_connect (close_button, "clicked",
                    G_CALLBACK (close_button_clicked_callback),
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
    case PROP_WHOLE_WORDS_ONLY:
      egg_find_bar_set_whole_words_only (find_bar, g_value_get_boolean (value));
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
    case PROP_WHOLE_WORDS_ONLY:
      g_value_set_boolean (value, priv->whole_words_only);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
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
      g_object_notify (G_OBJECT (find_bar), "case_sensitive");
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

  if (!priv->case_sensitive_enabled)
    return FALSE;

  return priv->case_sensitive;
}

/**
 * egg_find_bar_enable_case_sensitive:
 *
 * Enable or disable the case sensitive option
 */
void
egg_find_bar_enable_case_sensitive (EggFindBar *find_bar,
                                    gboolean    enable)
{
  EggFindBarPrivate *priv;

  g_return_if_fail (EGG_IS_FIND_BAR (find_bar));

  priv = (EggFindBarPrivate *)find_bar->priv;

  priv->case_sensitive_enabled = !!enable;
}

/**
 * egg_find_bar_set_whole_words_only:
 *
 * Sets whether search whole words only
 */
void
egg_find_bar_set_whole_words_only (EggFindBar *find_bar,
                                   gboolean    whole_words_only)
{
  EggFindBarPrivate *priv;

  g_return_if_fail (EGG_IS_FIND_BAR (find_bar));

  priv = (EggFindBarPrivate *)find_bar->priv;

  g_object_freeze_notify (G_OBJECT (find_bar));

  whole_words_only = whole_words_only != FALSE;

  if (priv->whole_words_only != whole_words_only)
    {
      priv->whole_words_only = whole_words_only;
      g_object_notify (G_OBJECT (find_bar), "whole-words-only");
    }

  g_object_thaw_notify (G_OBJECT (find_bar));
}

/**
 * egg_find_bar_get_whole_words_only:
 *
 * Gets whether search whole words only
 *
 * Returns: %TRUE if only whole words are searched
 */
gboolean
egg_find_bar_get_whole_words_only (EggFindBar *find_bar)
{
  EggFindBarPrivate *priv;

  g_return_val_if_fail (EGG_IS_FIND_BAR (find_bar), FALSE);

  priv = (EggFindBarPrivate *)find_bar->priv;

  if (!priv->whole_words_only_enabled)
    return FALSE;

  return priv->whole_words_only;
}

/**
 * egg_find_bar_enable_whole_words_only:
 *
 * Enable or disable the whole words only option
 */
void
egg_find_bar_enable_whole_words_only (EggFindBar *find_bar,
                                      gboolean    enable)
{
  EggFindBarPrivate *priv;

  g_return_if_fail (EGG_IS_FIND_BAR (find_bar));

  priv = (EggFindBarPrivate *)find_bar->priv;

  priv->whole_words_only_enabled = !!enable;
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
  const gchar *current_text;

  g_return_if_fail (EGG_IS_FIND_BAR (find_bar));

  priv = (EggFindBarPrivate *)find_bar->priv;
  
  current_text = gtk_label_get_text (GTK_LABEL (priv->status_label));

  if (g_strcmp0 (current_text, text) != 0)
	  gtk_label_set_text (GTK_LABEL (priv->status_label), text);

  g_object_set (priv->status_item, "visible", text != NULL && *text !='\0', NULL);
}
