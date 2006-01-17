/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef __EGG_RECENT_VIEW_UIMANAGER_H__
#define __EGG_RECENT_VIEW_UIMANAGER_H__


#include <gtk/gtk.h>
#include "egg-recent-item.h"

G_BEGIN_DECLS

#define EGG_RECENT_VIEW_UIMANAGER(obj)		G_TYPE_CHECK_INSTANCE_CAST (obj, egg_recent_view_uimanager_get_type (), EggRecentViewUIManager)
#define EGG_RECENT_VIEW_UIMANAGER_CLASS(klass) 	G_TYPE_CHECK_CLASS_CAST (klass, egg_recent_view_uimanager_get_type (), EggRecentViewUIManagerClass)
#define EGG_IS_RECENT_VIEW_UIMANAGER(obj)	G_TYPE_CHECK_INSTANCE_TYPE (obj, egg_recent_view_uimanager_get_type ())

typedef char* (*EggUIManagerTooltipFunc) (EggRecentItem *item,
					  gpointer       user_data);

typedef struct _EggRecentViewUIManager      EggRecentViewUIManager;
typedef struct _EggRecentViewUIManagerClass EggRecentViewUIManagerClass;

struct _EggRecentViewUIManagerClass {
	GObjectClass parent_class;
	void (*activate) (EggRecentViewUIManager *view, EggRecentItem *item);
};

GType                   egg_recent_view_uimanager_get_type         (void);
EggRecentViewUIManager *egg_recent_view_uimanager_new              (GtkUIManager             *uimanager,
								    const gchar              *path,
								    GCallback                 callback,
								    gpointer                  user_data);
void                    egg_recent_view_uimanager_set_uimanager    (EggRecentViewUIManager   *view,
								    GtkUIManager             *uimanager);
GtkUIManager*           egg_recent_view_uimanager_get_uimanager    (EggRecentViewUIManager *view);
void                    egg_recent_view_uimanager_set_path         (EggRecentViewUIManager   *view,
								    const gchar              *path);
G_CONST_RETURN gchar   *egg_recent_view_uimanager_get_path         (EggRecentViewUIManager   *view);
void                    egg_recent_view_uimanager_set_action_func  (EggRecentViewUIManager   *view,
								    GCallback                 callback,
								    gpointer                  user_data);
void                    egg_recent_view_uimanager_set_leading_sep  (EggRecentViewUIManager   *view,
								    gboolean                  val);
void                    egg_recent_view_uimanager_set_trailing_sep (EggRecentViewUIManager   *view,
								    gboolean                  val);
void                    egg_recent_view_uimanager_show_icons       (EggRecentViewUIManager   *view,
								    gboolean                  show);
void                    egg_recent_view_uimanager_show_numbers     (EggRecentViewUIManager   *view,
								    gboolean                  show);
void                    egg_recent_view_uimanager_set_tooltip_func (EggRecentViewUIManager   *view,
								    EggUIManagerTooltipFunc   func,
								    gpointer                  user_data);
void                    egg_recent_view_uimanager_set_icon_size    (EggRecentViewUIManager   *view,
								    GtkIconSize               icon_size);
GtkIconSize             egg_recent_view_uimanager_get_icon_size    (EggRecentViewUIManager   *view);
EggRecentItem          *egg_recent_view_uimanager_get_item         (EggRecentViewUIManager   *view,
								    GtkAction                *action);
void                    egg_recent_view_uimanager_set_label_width  (EggRecentViewUIManager   *view,
								    gint                      chars);
gint                    egg_recent_view_uimanager_get_label_width  (EggRecentViewUIManager   *view);

G_END_DECLS


#endif /* __EGG_RECENT_VIEW_UIMANAGER_H__ */
