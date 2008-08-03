/* imposter (OO.org Impress viewer)
** Copyright (C) 2003-2005 Gurer Ozen
** This code is free software; you can redistribute it and/or
** modify it under the terms of GNU General Public License.
*/

#include "imposter.h"

#ifndef OO_RENDER_H
#define OO_RENDER_H

#include <gtk/gtk.h>

#define OO_TYPE_RENDER (oo_render_get_type())
#define OO_RENDER(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), OO_TYPE_RENDER , OORender))
#define OO_RENDER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), OO_TYPE_RENDER , OORenderClass))

typedef struct _OORender OORender;
typedef struct _OORenderClass OORenderClass;

struct _OORender {
	GtkDrawingArea area;
	ImpRenderCtx *ctx;
	ImpPage *page;
	int step;
	int step_mode;
};

struct _OORenderClass {
	GtkDrawingAreaClass parent_class;
	void (*page_changed)(OORender *obj);
};

GType oo_render_get_type(void);
GtkWidget *oo_render_new(void);
void oo_render_set_page(OORender *obj, ImpPage *page);
ImpPage *oo_render_get_page(OORender *obj);
void oo_render_step(OORender *obj);
void oo_render_step_mode(OORender *obj, int mode);


#endif	/* OO_RENDER_H */
