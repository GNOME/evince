#include "ev-render-context.h"

static void ev_render_context_init       (EvRenderContext      *rc);
static void ev_render_context_class_init (EvRenderContextClass *class);


G_DEFINE_TYPE (EvRenderContext, ev_render_context, G_TYPE_OBJECT);

static void ev_render_context_init (EvRenderContext *rc) { /* Do Nothing */ }

static void
ev_render_context_dispose (GObject *object)
{
	EvRenderContext *rc;

	rc = (EvRenderContext *) object;

	if (rc->destroy) {
		(*rc->destroy) (rc->data);
		rc->destroy = NULL;
	}

	(* G_OBJECT_CLASS (ev_render_context_parent_class)->dispose) (object);
}

static void
ev_render_context_class_init (EvRenderContextClass *class)
{
	GObjectClass *oclass;

	oclass = G_OBJECT_CLASS (class);

	oclass->dispose = ev_render_context_dispose;
}


EvRenderContext *
ev_render_context_new (int           rotation,
		       gint          page,
		       gdouble       scale)
{
	EvRenderContext *rc;

	rc = (EvRenderContext *) g_object_new (EV_TYPE_RENDER_CONTEXT, NULL);

	rc->rotation = rotation;
	rc->page = page;
	rc->scale = scale;

	return rc;
}

void
ev_render_context_set_page (EvRenderContext *rc,
			    gint             page)
{
	g_return_if_fail (rc != NULL);

	rc->page = page;
}

void
ev_render_context_set_rotation (EvRenderContext *rc,
				int              rotation)
{
	g_return_if_fail (rc != NULL);

	rc->rotation = rotation;
}

void
ev_render_context_set_scale (EvRenderContext *rc,
			     gdouble          scale)
{
	g_return_if_fail (rc != NULL);

	rc->scale = scale;
}

