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
#include "eggfileformatchooser.h"
#include "egg-macros.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>
#include <ctype.h>

typedef struct _EggFileFormatFilterInfo EggFileFormatFilterInfo;
typedef struct _EggFileFormatSearch EggFileFormatSearch;

enum 
{
  MODEL_COLUMN_ID,
  MODEL_COLUMN_NAME,
  MODEL_COLUMN_ICON,
  MODEL_COLUMN_EXTENSIONS,
  MODEL_COLUMN_FILTER,
  MODEL_COLUMN_DATA,
  MODEL_COLUMN_DESTROY
};

enum 
{
  SIGNAL_SELECTION_CHANGED,
  SIGNAL_LAST
};

struct _EggFileFormatChooserPrivate
{
  GtkTreeStore *model;
  GtkTreeSelection *selection;
  guint idle_hack;
  guint last_id;

  GtkFileChooser *chooser;
  GtkFileFilter *all_files;
  GtkFileFilter *supported_files;
};

struct _EggFileFormatFilterInfo
{
  GHashTable *extension_set;
  GSList *extension_list;
  gboolean show_extensions;
  gchar *name;
};

struct _EggFileFormatSearch
{
  gboolean success;
  GtkTreeIter iter;

  guint format;
  const gchar *extension;
};

static guint signals[SIGNAL_LAST];

G_DEFINE_TYPE (EggFileFormatChooser, 
	       egg_file_format_chooser,
               GTK_TYPE_EXPANDER);
EGG_DEFINE_QUARK (EggFileFormatFilterInfo,
                  egg_file_format_filter_info);

static EggFileFormatFilterInfo*
egg_file_format_filter_info_new (const gchar *name,
                                 gboolean     show_extensions)
{
  EggFileFormatFilterInfo *self;

  self = g_new0 (EggFileFormatFilterInfo, 1);
  self->extension_set = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  self->show_extensions = show_extensions;
  self->name = g_strdup (name);

  return self;
}

static void
egg_file_format_filter_info_free (gpointer boxed)
{
  EggFileFormatFilterInfo *self;

  if (boxed)
    {
      self = boxed;

      g_hash_table_unref (self->extension_set);
      g_slist_foreach (self->extension_list, (GFunc) g_free, NULL);
      g_slist_free (self->extension_list);
      g_free (self->name);
      g_free (self);
    }
}

static gboolean
egg_file_format_filter_find (gpointer key,
                             gpointer value G_GNUC_UNUSED,
                             gpointer data)
{
  const GtkFileFilterInfo *info = data;
  const gchar *pattern = key;

  return g_str_has_suffix (info->filename, pattern + 1);
}

static gboolean
egg_file_format_filter_filter (const GtkFileFilterInfo *info,
                               gpointer                 data)
{
  EggFileFormatFilterInfo *self = data;

  return NULL != g_hash_table_find (self->extension_set,
                                    egg_file_format_filter_find,
                                    (gpointer) info);
}

static GtkFileFilter*
egg_file_format_filter_new (const gchar *name,
                            gboolean     show_extensions)
{
  GtkFileFilter *filter;
  EggFileFormatFilterInfo *info;

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, name);

  info = egg_file_format_filter_info_new (name, show_extensions);

  gtk_file_filter_add_custom (filter, GTK_FILE_FILTER_FILENAME,
                              egg_file_format_filter_filter,
                              info, NULL);
  g_object_set_qdata_full (G_OBJECT (filter),
                           egg_file_format_filter_info_quark (),
                           info, egg_file_format_filter_info_free);

  return filter;
}

static void
egg_file_format_filter_add_extensions (GtkFileFilter *filter,
                                       const gchar   *extensions)
{
  EggFileFormatFilterInfo *info;
  GString *filter_name;
  const gchar *extptr;
  gchar *pattern;
  gsize length;

  g_assert (NULL != extensions);

  info = g_object_get_qdata (G_OBJECT (filter),
                             egg_file_format_filter_info_quark ());

  info->extension_list = g_slist_prepend (info->extension_list,
                                          g_strdup (extensions));

  if (info->show_extensions)
    {
      filter_name = g_string_new (info->name);
      g_string_append (filter_name, " (");
    }
  else
    filter_name = NULL;

  extptr = extensions;
  while (*extptr)
    {
      length = strcspn (extptr, ",");
      pattern = g_new (gchar, length + 3);

      memcpy (pattern, "*.", 2);
      memcpy (pattern + 2, extptr, length);
      pattern[length + 2] = '\0';

      if (filter_name)
        {
          if (extptr != extensions)
            g_string_append (filter_name, ", ");

          g_string_append (filter_name, pattern);
        }

      extptr += length;

      if (*extptr)
        extptr += 2;

      g_hash_table_replace (info->extension_set, pattern, pattern);
    }

  if (filter_name)
    {
      g_string_append (filter_name, ")");
      gtk_file_filter_set_name (filter, filter_name->str);
      g_string_free (filter_name, TRUE);
    }
}

static void
selection_changed_cb (GtkTreeSelection     *selection,
                      EggFileFormatChooser *self)
{
  gchar *label;
  gchar *name;

  GtkFileFilter *filter;
  GtkTreeModel *model;
  GtkTreeIter parent;
  GtkTreeIter iter;

  if (gtk_tree_selection_get_selected (selection, &model, &iter)) 
    {
      gtk_tree_model_get (model, &iter, MODEL_COLUMN_NAME, &name, -1);

      label = g_strdup_printf (_("File _Format: %s"), name);
      gtk_expander_set_use_underline (GTK_EXPANDER (self), TRUE);
      gtk_expander_set_label (GTK_EXPANDER (self), label);

      g_free (name);
      g_free (label);

      if (self->priv->chooser)
        {
          while (gtk_tree_model_iter_parent (model, &parent, &iter))
            iter = parent;

          gtk_tree_model_get (model, &iter, MODEL_COLUMN_FILTER, &filter, -1);
          gtk_file_chooser_set_filter (self->priv->chooser, filter);
          g_object_unref (filter);
        }

      g_signal_emit (self, signals[SIGNAL_SELECTION_CHANGED], 0);
    }
}

/* XXX This hack is needed, as gtk_expander_set_label seems 
 * not to work from egg_file_format_chooser_init */
static gboolean
select_default_file_format (gpointer data)
{
  EggFileFormatChooser *self = EGG_FILE_FORMAT_CHOOSER (data);
  egg_file_format_chooser_set_format (self, 0);
  self->priv->idle_hack = 0;
  return FALSE;
}

static gboolean
find_by_format (GtkTreeModel *model,
                GtkTreePath  *path G_GNUC_UNUSED,
                GtkTreeIter  *iter,
                gpointer      data)
{
  EggFileFormatSearch *search = data;
  guint id;

  gtk_tree_model_get (model, iter, MODEL_COLUMN_ID, &id, -1);

  if (id == search->format)
    {
      search->success = TRUE;
      search->iter = *iter;
    }

  return search->success;
}

static gboolean
find_in_list (gchar       *list,
              const gchar *needle)
{
  gchar *saveptr;
  gchar *token;

  for (token = strtok_r (list, ",", &saveptr); NULL != token;
       token = strtok_r (NULL, ",", &saveptr))
    {
      token = g_strstrip (token);

      if (g_str_equal (needle, token))
        return TRUE;
    }

  return FALSE;
}

static gboolean
accept_filename (gchar       *extensions,
                 const gchar *filename)
{
  const gchar *extptr;
  gchar *saveptr;
  gchar *token;
  gsize length;

  length = strlen (filename);

  for (token = strtok_r (extensions, ",", &saveptr); NULL != token;
       token = strtok_r (NULL, ",", &saveptr))
    {
      token = g_strstrip (token);
      extptr = filename + length - strlen (token) - 1;

      if (extptr > filename && '.' == *extptr &&
          !strcmp (extptr + 1, token))
          return TRUE;
    }

  return FALSE;
}

static gboolean
find_by_extension (GtkTreeModel *model,
                   GtkTreePath  *path G_GNUC_UNUSED,
                   GtkTreeIter  *iter,
                   gpointer      data)
{
  EggFileFormatSearch *search = data;

  gchar *extensions = NULL;
  guint format = 0;

  gtk_tree_model_get (model, iter,
                      MODEL_COLUMN_EXTENSIONS, &extensions,
                      MODEL_COLUMN_ID, &format,
                      -1);

  if (extensions && find_in_list (extensions, search->extension))
    {
      search->format = format;
      search->success = TRUE;
      search->iter = *iter;
    }

  g_free (extensions);
  return search->success;
}

static void
egg_file_format_chooser_init (EggFileFormatChooser *self)
{
  GtkWidget *scroller;
  GtkWidget *view;

  GtkTreeViewColumn *column;
  GtkCellRenderer *cell;
  GtkTreeIter iter;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, EGG_TYPE_FILE_FORMAT_CHOOSER, 
                                            EggFileFormatChooserPrivate);

/* file filters */

  self->priv->all_files = g_object_ref_sink (gtk_file_filter_new ());
  gtk_file_filter_set_name (self->priv->all_files, _("All Files"));
  self->priv->supported_files = egg_file_format_filter_new (_("All Supported Files"), FALSE);

/* tree model */

  self->priv->model = gtk_tree_store_new (7, G_TYPE_UINT, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
                                             GTK_TYPE_FILE_FILTER, G_TYPE_POINTER, G_TYPE_POINTER);

  gtk_tree_store_append (self->priv->model, &iter, NULL);
  gtk_tree_store_set (self->priv->model, &iter,
                      MODEL_COLUMN_NAME, _("By Extension"),
                      MODEL_COLUMN_FILTER, self->priv->supported_files,
                      MODEL_COLUMN_ID, 0,
                      -1);

/* tree view */

  view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (self->priv->model));
  self->priv->selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
  gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (view), TRUE);

/* file format column */

  column = gtk_tree_view_column_new ();
  gtk_tree_view_column_set_expand (column, TRUE);
  gtk_tree_view_column_set_title (column, _("File Format"));
  gtk_tree_view_append_column (GTK_TREE_VIEW (view), column);

  cell = gtk_cell_renderer_pixbuf_new ();
  gtk_tree_view_column_pack_start (column, cell, FALSE);
  gtk_tree_view_column_set_attributes (column, cell,
                                       "icon-name", MODEL_COLUMN_ICON,
                                       NULL);

  cell = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (column, cell, TRUE);
  gtk_tree_view_column_set_attributes (column, cell,
                                       "text", MODEL_COLUMN_NAME,
                                       NULL);

/* extensions column */

  column = gtk_tree_view_column_new_with_attributes (
    _("Extension(s)"), gtk_cell_renderer_text_new (),
    "text", MODEL_COLUMN_EXTENSIONS, NULL);
  gtk_tree_view_column_set_expand (column, FALSE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (view), column);

/* selection */

  gtk_tree_selection_set_mode (self->priv->selection, GTK_SELECTION_BROWSE);
  g_signal_connect (self->priv->selection, "changed",
                    G_CALLBACK (selection_changed_cb), self);
  self->priv->idle_hack = g_idle_add (select_default_file_format, self);

/* scroller */

  scroller = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroller),
                                  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroller),
                                       GTK_SHADOW_IN);
  gtk_widget_set_size_request (scroller, -1, 150);
  gtk_container_add (GTK_CONTAINER (scroller), view);
  gtk_widget_show_all (scroller);

  gtk_container_add (GTK_CONTAINER (self), scroller);
}

static void
reset_model (EggFileFormatChooser *self)
{
  GtkTreeModel *model = GTK_TREE_MODEL (self->priv->model);
  GtkTreeIter iter;

  if (gtk_tree_model_get_iter_first (model, &iter))
    {
      do
        {
          GDestroyNotify destroy = NULL;
          gpointer data = NULL;

          gtk_tree_model_get (model, &iter,
                              MODEL_COLUMN_DESTROY, &destroy,
                              MODEL_COLUMN_DATA, &data,
                              -1);

          if (destroy)
            destroy (data);
        }
      while (gtk_tree_model_iter_next (model, &iter));
    }

  gtk_tree_store_clear (self->priv->model);
}

static void
egg_file_format_chooser_dispose (GObject *obj)
{
  EggFileFormatChooser *self = EGG_FILE_FORMAT_CHOOSER (obj);

  if (NULL != self)
    {
      if (self->priv->idle_hack)
        {
          g_source_remove (self->priv->idle_hack);
          self->priv->idle_hack = 0;
        }
    }

  G_OBJECT_CLASS (egg_file_format_chooser_parent_class)->dispose (obj);
}

static void
egg_file_format_chooser_finalize (GObject *obj)
{
  EggFileFormatChooser *self = EGG_FILE_FORMAT_CHOOSER (obj);

  if (NULL != self)
    {
      if (self->priv->model)
        {
          reset_model (self);

          g_object_unref (self->priv->model);
          self->priv->model = NULL;

          g_object_unref (self->priv->all_files);
          self->priv->all_files = NULL;
        }
    }

  G_OBJECT_CLASS (egg_file_format_chooser_parent_class)->finalize (obj);
}

static void
filter_changed_cb (GObject    *object,
                   GParamSpec *spec,
                   gpointer    data)
{
  EggFileFormatChooser *self;

  GtkFileFilter *current_filter;
  GtkFileFilter *format_filter;

  GtkTreeModel *model;
  GtkTreeIter iter;
  GtkTreeIter parent;

  self = EGG_FILE_FORMAT_CHOOSER (data);

  format_filter = NULL;
  current_filter = gtk_file_chooser_get_filter (GTK_FILE_CHOOSER (object));
  model = GTK_TREE_MODEL (self->priv->model);

  if (gtk_tree_selection_get_selected (self->priv->selection, &model, &iter)) 
    {
      while (gtk_tree_model_iter_parent (model, &parent, &iter))
        iter = parent;

      gtk_tree_model_get (model, &iter,
                          MODEL_COLUMN_FILTER,
                          &format_filter, -1);
      g_object_unref (format_filter);
    }

  if (current_filter && current_filter != format_filter &&
      gtk_tree_model_get_iter_first (model, &iter))
    {
      if (current_filter == self->priv->all_files)
        format_filter = current_filter;
      else
        {
          format_filter = NULL;

          do
            {
              gtk_tree_model_get (model, &iter,
                                  MODEL_COLUMN_FILTER,
                                  &format_filter, -1);
              g_object_unref (format_filter);

              if (format_filter == current_filter)
                break;
            }
          while (gtk_tree_model_iter_next (model, &iter));
        }

      if (format_filter)
        gtk_tree_selection_select_iter (self->priv->selection, &iter);
    }
}

/* Shows an error dialog set as transient for the specified window */
static void
error_message_with_parent (GtkWindow  *parent,
			   const char *msg,
			   const char *detail)
{
  gboolean first_call = TRUE;
  GtkWidget *dialog;

  if (first_call)
    {
      g_warning ("%s: Merge with the code in Gtk{File,Recent}ChooserDefault.", G_STRLOC);
      first_call = FALSE;
    }

  dialog = gtk_message_dialog_new (parent,
				   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				   GTK_MESSAGE_ERROR,
				   GTK_BUTTONS_OK,
				   "%s",
				   msg);
  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
					    "%s", detail);

  if (parent->group)
    gtk_window_group_add_window (parent->group, GTK_WINDOW (dialog));

  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
}

/* Returns a toplevel GtkWindow, or NULL if none */
static GtkWindow *
get_toplevel (GtkWidget *widget)
{
  GtkWidget *toplevel;

  toplevel = gtk_widget_get_toplevel (widget);
  if (!GTK_WIDGET_TOPLEVEL (toplevel))
    return NULL;
  else
    return GTK_WINDOW (toplevel);
}

/* Shows an error dialog for the file chooser */
static void
error_message (EggFileFormatChooser *impl,
	       const char           *msg,
	       const char           *detail)
{
  error_message_with_parent (get_toplevel (GTK_WIDGET (impl)), msg, detail);
}

static void
chooser_response_cb (GtkDialog *dialog,
                     gint       response_id,
                     gpointer   data)
{
  EggFileFormatChooser *self;
  gchar *filename, *basename;
  gchar *message;
  guint format;

  self = EGG_FILE_FORMAT_CHOOSER (data);

  if (EGG_IS_POSITIVE_RESPONSE (response_id))
    {
      filename = gtk_file_chooser_get_filename (self->priv->chooser);
      basename = g_filename_display_basename (filename);
      g_free (filename);

      format = egg_file_format_chooser_get_format (self, basename);
      g_print ("%s: %s - %d\n", G_STRFUNC, basename, format);

      if (0 == format)
        {

          message = g_strdup_printf (
            _("The program was not able to find out the file format "
              "you want to use for `%s'. Please make sure to use a "
              "known extension for that file or manually choose a "
              "file format from the list below."),
              basename);

          error_message (self,
		         _("File format not recognized"),
                        message);

          g_free (message);

          g_signal_stop_emission_by_name (dialog, "response");
        }
      else
        {
          filename = egg_file_format_chooser_append_extension (self, basename, format);

          if (strcmp (filename, basename))
            {
              gtk_file_chooser_set_current_name (self->priv->chooser, filename);
              g_signal_stop_emission_by_name (dialog, "response");
            }

          g_free (filename);
        }

      g_free (basename); 
    }

}

static void
egg_file_format_chooser_realize (GtkWidget *widget)
{
  EggFileFormatChooser *self;
  GtkWidget *parent;

  GtkFileFilter *filter;
  GtkTreeModel *model;
  GtkTreeIter iter;

  GTK_WIDGET_CLASS (egg_file_format_chooser_parent_class)->realize (widget);

  self = EGG_FILE_FORMAT_CHOOSER (widget);

  g_return_if_fail (NULL == self->priv->chooser);

  parent = gtk_widget_get_toplevel (widget);

  if (!GTK_IS_FILE_CHOOSER (parent))
    parent = gtk_widget_get_parent (widget);

  while (parent && !GTK_IS_FILE_CHOOSER (parent))
    parent = gtk_widget_get_parent (parent);

  self->priv->chooser = GTK_FILE_CHOOSER (parent);

  g_return_if_fail (GTK_IS_FILE_CHOOSER (self->priv->chooser));
  g_return_if_fail (gtk_file_chooser_get_action (self->priv->chooser) ==
                    GTK_FILE_CHOOSER_ACTION_SAVE);

  g_object_ref (self->priv->chooser);

  g_signal_connect (self->priv->chooser, "notify::filter", 
                    G_CALLBACK (filter_changed_cb), self);
  gtk_file_chooser_add_filter (self->priv->chooser, self->priv->all_files);

  model = GTK_TREE_MODEL (self->priv->model);

  if (gtk_tree_model_get_iter_first (model, &iter))
    {
      do
        {
          gtk_tree_model_get (model, &iter, MODEL_COLUMN_FILTER, &filter, -1);
          gtk_file_chooser_add_filter (self->priv->chooser, filter);
          g_object_unref (filter);
        }
      while (gtk_tree_model_iter_next (model, &iter));
    }

  gtk_file_chooser_set_filter (self->priv->chooser,
                               self->priv->supported_files);

  if (GTK_IS_DIALOG (self->priv->chooser))
    g_signal_connect (self->priv->chooser, "response",
                      G_CALLBACK (chooser_response_cb), self);
}

static void
egg_file_format_chooser_unrealize (GtkWidget *widget)
{
  EggFileFormatChooser *self;

  GtkFileFilter *filter;
  GtkTreeModel *model;
  GtkTreeIter iter;

  GTK_WIDGET_CLASS (egg_file_format_chooser_parent_class)->unrealize (widget);

  self = EGG_FILE_FORMAT_CHOOSER (widget);
  model = GTK_TREE_MODEL (self->priv->model);

  g_signal_handlers_disconnect_by_func (self->priv->chooser,
                                        filter_changed_cb, self);
  g_signal_handlers_disconnect_by_func (self->priv->chooser,
                                        chooser_response_cb, self);

  if (gtk_tree_model_get_iter_first (model, &iter))
    {
      do
        {
          gtk_tree_model_get (model, &iter, MODEL_COLUMN_FILTER, &filter, -1);
          gtk_file_chooser_remove_filter (self->priv->chooser, filter);
          g_object_unref (filter);
        }
      while (gtk_tree_model_iter_next (model, &iter));
    }

  gtk_file_chooser_remove_filter (self->priv->chooser, self->priv->all_files);
  g_object_unref (self->priv->chooser);
}

static void
egg_file_format_chooser_class_init (EggFileFormatChooserClass *cls)
{
  GObjectClass *object_class = G_OBJECT_CLASS (cls);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (cls);

  g_type_class_add_private (cls, sizeof (EggFileFormatChooserPrivate));

  object_class->dispose = egg_file_format_chooser_dispose;
  object_class->finalize = egg_file_format_chooser_finalize;

  widget_class->realize = egg_file_format_chooser_realize;
  widget_class->unrealize = egg_file_format_chooser_unrealize;

  signals[SIGNAL_SELECTION_CHANGED] = g_signal_new (
    "selection-changed", EGG_TYPE_FILE_FORMAT_CHOOSER, G_SIGNAL_RUN_FIRST,
    G_STRUCT_OFFSET (EggFileFormatChooserClass, selection_changed),
    NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
}

GtkWidget*
egg_file_format_chooser_new (void)
{
  return g_object_new (EGG_TYPE_FILE_FORMAT_CHOOSER, NULL);
}

static guint
egg_file_format_chooser_add_format_impl (EggFileFormatChooser *self,
                                         guint                 parent,
                                         const gchar          *name,
                                         const gchar          *icon,
                                         const gchar          *extensions)
{
  EggFileFormatSearch search;
  GtkFileFilter *filter;
  GtkTreeIter iter;

  search.success = FALSE;
  search.format = parent;
  filter = NULL;

  if (parent > 0)
    {
      gtk_tree_model_foreach (GTK_TREE_MODEL (self->priv->model),
                              find_by_format, &search);
      g_return_val_if_fail (search.success, -1);
    }
  else
    filter = egg_file_format_filter_new (name, TRUE);

  gtk_tree_store_append (self->priv->model, &iter, 
                         parent > 0 ? &search.iter : NULL);

  gtk_tree_store_set (self->priv->model, &iter,
                      MODEL_COLUMN_ID, ++self->priv->last_id,
                      MODEL_COLUMN_EXTENSIONS, extensions,
                      MODEL_COLUMN_FILTER, filter,
                      MODEL_COLUMN_NAME, name,
                      MODEL_COLUMN_ICON, icon,
                      -1);

  if (extensions)
    {
      if (parent > 0)
        gtk_tree_model_get (GTK_TREE_MODEL (self->priv->model), &search.iter,
                            MODEL_COLUMN_FILTER, &filter, -1);

      egg_file_format_filter_add_extensions (self->priv->supported_files, extensions);
      egg_file_format_filter_add_extensions (filter, extensions);

      if (parent > 0)
        g_object_unref (filter);
    }

  return self->priv->last_id;
}

guint
egg_file_format_chooser_add_format (EggFileFormatChooser *self,
                                    guint                 parent,
                                    const gchar          *name,
                                    const gchar          *icon,
                                    ...)
{
  GString *buffer = NULL;
  const gchar* extptr;
  va_list extensions;
  guint id;

  g_return_val_if_fail (EGG_IS_FILE_FORMAT_CHOOSER (self), 0);
  g_return_val_if_fail (NULL != name, 0);

  va_start (extensions, icon);

  while (NULL != (extptr = va_arg (extensions, const gchar*)))
    {
      if (NULL == buffer)
        buffer = g_string_new (NULL);
      else
        g_string_append (buffer, ", ");

      g_string_append (buffer, extptr);
    }

  va_end (extensions);

  id = egg_file_format_chooser_add_format_impl (self, parent, name, icon,
                                                buffer ? buffer->str : NULL);

  if (buffer)
    g_string_free (buffer, TRUE);

  return id;
}

static gchar*
get_icon_name (const gchar *mime_type)
{
  static gboolean first_call = TRUE;
  gchar *name = NULL;
  gchar *s;

  if (first_call)
    {
      g_warning ("%s: Replace by g_content_type_get_icon "
                 "when GVFS is merged into GLib.", G_STRLOC);
      first_call = FALSE;
    }

  if (mime_type)
    {
      name = g_strconcat ("gnome-mime-", mime_type, NULL);

      for(s = name; *s; ++s)
        {
          if (!isalpha (*s) || !isascii (*s))
            *s = '-';
        }
    }

  if (!name ||
      !gtk_icon_theme_has_icon (gtk_icon_theme_get_default (), name))
    {
      g_free (name);
      name = g_strdup ("gnome-mime-image");
    }

  return name;
}

void           
egg_file_format_chooser_add_pixbuf_formats (EggFileFormatChooser *self,
                                            guint                 parent G_GNUC_UNUSED,
                                            guint               **formats)
{
  GSList *pixbuf_formats = NULL;
  GSList *iter;
  gint i;

  g_return_if_fail (EGG_IS_FILE_FORMAT_CHOOSER (self));

  pixbuf_formats = gdk_pixbuf_get_formats ();

  if (formats)
    *formats = g_new0 (guint, g_slist_length (pixbuf_formats) + 1);

  for(iter = pixbuf_formats, i = 0; iter; iter = iter->next, ++i)
    {
      GdkPixbufFormat *format = iter->data;

      gchar *description, *name, *extensions, *icon;
      gchar **mime_types, **extension_list;
      guint id;

      if (gdk_pixbuf_format_is_disabled (format) ||
         !gdk_pixbuf_format_is_writable (format))
        continue;

      mime_types = gdk_pixbuf_format_get_mime_types (format);
      icon = get_icon_name (mime_types[0]);
      g_strfreev (mime_types);

      extension_list = gdk_pixbuf_format_get_extensions (format);
      extensions = g_strjoinv (", ", extension_list);
      g_strfreev (extension_list);

      description = gdk_pixbuf_format_get_description (format);
      name = gdk_pixbuf_format_get_name (format);

      id = egg_file_format_chooser_add_format_impl (self, parent, description, 
                                                    icon, extensions);

      g_free (description);
      g_free (extensions);
      g_free (icon);

      egg_file_format_chooser_set_format_data (self, id, name, g_free);

      if (formats)
        *formats[i] = id;
    }

  g_slist_free (pixbuf_formats);
}

void
egg_file_format_chooser_remove_format (EggFileFormatChooser *self,
                                       guint                 format)
{
  GDestroyNotify destroy = NULL;
  gpointer data = NULL;

  EggFileFormatSearch search;
  GtkFileFilter *filter;
  GtkTreeModel *model;

  g_return_if_fail (EGG_IS_FILE_FORMAT_CHOOSER (self));

  search.success = FALSE;
  search.format = format;

  model = GTK_TREE_MODEL (self->priv->model);
  gtk_tree_model_foreach (model, find_by_format, &search);

  g_return_if_fail (search.success);

  gtk_tree_model_get (model, &search.iter,
                      MODEL_COLUMN_FILTER, &filter,
                      MODEL_COLUMN_DESTROY, &destroy,
                      MODEL_COLUMN_DATA, &data,
                      -1);

  if (destroy)
    destroy (data);

  if (filter)
    {
      if (self->priv->chooser)
        gtk_file_chooser_remove_filter (self->priv->chooser, filter);

      g_object_unref (filter);
    }
  else
    g_warning ("TODO: Remove extensions from parent filter");

  gtk_tree_store_remove (self->priv->model, &search.iter);
}

void            
egg_file_format_chooser_set_format (EggFileFormatChooser *self,
                                    guint                 format)
{
  EggFileFormatSearch search;

  GtkTreeModel *model;
  GtkTreePath *path;
  GtkTreeView *view;

  g_return_if_fail (EGG_IS_FILE_FORMAT_CHOOSER (self));

  search.success = FALSE;
  search.format = format;

  model = GTK_TREE_MODEL (self->priv->model);
  gtk_tree_model_foreach (model, find_by_format, &search);

  g_return_if_fail (search.success);

  path = gtk_tree_model_get_path (model, &search.iter);
  view = gtk_tree_selection_get_tree_view (self->priv->selection);

  gtk_tree_view_expand_to_path (view, path);
  gtk_tree_selection_unselect_all (self->priv->selection);
  gtk_tree_selection_select_path (self->priv->selection, path);

  gtk_tree_path_free (path);

  if (self->priv->idle_hack > 0)
    {
      g_source_remove (self->priv->idle_hack);
      self->priv->idle_hack = 0;
    }
}

guint
egg_file_format_chooser_get_format (EggFileFormatChooser *self,
                                    const gchar          *filename)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  guint format = 0;

  g_return_val_if_fail (EGG_IS_FILE_FORMAT_CHOOSER (self), -1);

  if (gtk_tree_selection_get_selected (self->priv->selection, &model, &iter))
    gtk_tree_model_get (model, &iter, MODEL_COLUMN_ID, &format, -1);

  if (0 == format && NULL != filename)
    {
      EggFileFormatSearch search;

      search.extension = strrchr(filename, '.');
      search.success = FALSE;

      if (search.extension++)
        gtk_tree_model_foreach (model, find_by_extension, &search);
      if (search.success)
        format = search.format;
    }

  return format;
}

void            
egg_file_format_chooser_set_format_data (EggFileFormatChooser *self,
                                         guint                 format,
                                         gpointer              data,
                                         GDestroyNotify        destroy)
{
  EggFileFormatSearch search;

  g_return_if_fail (EGG_IS_FILE_FORMAT_CHOOSER (self));

  search.success = FALSE;
  search.format = format;

  gtk_tree_model_foreach (GTK_TREE_MODEL (self->priv->model),
                          find_by_format, &search);

  g_return_if_fail (search.success);

  gtk_tree_store_set (self->priv->model, &search.iter,
                      MODEL_COLUMN_DESTROY, destroy,
                      MODEL_COLUMN_DATA, data,
                      -1);
}

gpointer
egg_file_format_chooser_get_format_data (EggFileFormatChooser *self,
                                         guint                 format)
{
  EggFileFormatSearch search;
  gpointer data = NULL;
  GtkTreeModel *model;

  g_return_val_if_fail (EGG_IS_FILE_FORMAT_CHOOSER (self), NULL);

  search.success = FALSE;
  search.format = format;

  model = GTK_TREE_MODEL (self->priv->model);
  gtk_tree_model_foreach (model, find_by_format, &search);

  g_return_val_if_fail (search.success, NULL);

  gtk_tree_model_get (model, &search.iter,
                      MODEL_COLUMN_DATA, &data,
                      -1);
  return data;
}

gchar*
egg_file_format_chooser_append_extension (EggFileFormatChooser *self,
                                          const gchar          *filename,
                                          guint                 format)
{
  EggFileFormatSearch search;
  GtkTreeModel *model;
  GtkTreeIter child;

  gchar *extensions;
  gchar *result;

  g_return_val_if_fail (EGG_IS_FILE_FORMAT_CHOOSER (self), NULL);
  g_return_val_if_fail (NULL != filename, NULL);

  if (0 == format)
    format = egg_file_format_chooser_get_format (self, NULL);

  if (0 == format)
    {
      g_warning ("%s: No file format selected. Cannot append extension.", __FUNCTION__);
      return NULL;
    }

  search.success = FALSE;
  search.format = format;

  model = GTK_TREE_MODEL (self->priv->model);
  gtk_tree_model_foreach (model, find_by_format, &search);

  g_return_val_if_fail (search.success, NULL);

  gtk_tree_model_get (model, &search.iter, 
                      MODEL_COLUMN_EXTENSIONS, &extensions,
                      -1);

  if (NULL == extensions && 
      gtk_tree_model_iter_nth_child (model, &child, &search.iter, 0))
    {
      gtk_tree_model_get (model, &child, 
                          MODEL_COLUMN_EXTENSIONS, &extensions,
                          -1);
    }

  if (NULL == extensions)
    {
      g_warning ("%s: File format %d doesn't provide file extensions. "
                 "Cannot append extension.", __FUNCTION__, format);
      return NULL;
    }

  if (accept_filename (extensions, filename))
    result = g_strdup (filename);
  else
    result = g_strconcat (filename, ".", extensions, NULL);

  g_assert (NULL == strchr(extensions, ','));
  g_free (extensions);
  return result;
}

/* vim: set sw=2 sta et: */
