/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef __EGG_RECENT_VIEW_BONOBO_H__
#define __EGG_RECENT_VIEW_BONOBO_H__

#include <libbonoboui.h>

G_BEGIN_DECLS

#define EGG_RECENT_VIEW_BONOBO(obj)		G_TYPE_CHECK_INSTANCE_CAST (obj, egg_recent_view_bonobo_get_type (), EggRecentViewBonobo)
#define EGG_RECENT_VIEW_BONOBO_CLASS(klass) 	G_TYPE_CHECK_CLASS_CAST (klass, egg_recent_view_bonobo_get_type (), EggRecentViewBonoboClass)
#define EGG_IS_RECENT_VIEW_BONOBO(obj)		G_TYPE_CHECK_INSTANCE_TYPE (obj, egg_recent_view_bonobo_get_type ())

typedef char *(*EggRecentViewBonoboTooltipFunc) (EggRecentItem *item,
						 gpointer user_data);

typedef struct _EggRecentViewBonobo EggRecentViewBonobo;

typedef struct _EggRecentViewBonoboClass EggRecentViewBonoboClass;

struct _EggRecentViewBonoboClass {
	GObjectClass parent_class;
	
	void (*activate) (EggRecentViewBonobo *view, EggRecentItem *item);
};

GType        egg_recent_view_bonobo_get_type (void);

EggRecentViewBonobo * egg_recent_view_bonobo_new (BonoboUIComponent *uic,
						      const gchar *path);


void egg_recent_view_bonobo_set_ui_component (EggRecentViewBonobo *view,
						BonoboUIComponent *uic);

void egg_recent_view_bonobo_set_ui_path      (EggRecentViewBonobo *view,
						const gchar *path);

gchar * egg_recent_view_bonobo_get_ui_path   (EggRecentViewBonobo *view);
const BonoboUIComponent *egg_recent_view_bonobo_get_ui_component (EggRecentViewBonobo *view);

void egg_recent_view_bonobo_show_icons (EggRecentViewBonobo *view,
					gboolean show);

void egg_recent_view_bonobo_show_numbers (EggRecentViewBonobo *view,
					  gboolean show);

void egg_recent_view_bonobo_set_tooltip_func (EggRecentViewBonobo *view,
					EggRecentViewBonoboTooltipFunc func,
					gpointer user_data);

void egg_recent_view_bonobo_set_icon_size (EggRecentViewBonobo *view,
					   GtkIconSize icon_size);

GtkIconSize egg_recent_view_bonobo_get_icon_size (EggRecentViewBonobo *view);

G_END_DECLS

#endif /* __EGG_RECENT_VIEW_BONOBO_H__ */
