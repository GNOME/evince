/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2009 Carlos Garcia Campos
 *  Copyright (C) 2005 Red Hat, Inc
 *
 * Evince is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Evince is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "config.h"

#include "ev-document-model.h"
#include "ev-view-type-builtins.h"
#include "ev-view-marshal.h"

struct _EvDocumentModel
{
	GObject base;

	EvDocument *document;
	gint n_pages;

	gint page;
	gint rotation;
	gdouble scale;
	EvSizingMode sizing_mode;
	EvPageLayout page_layout;
	guint continuous : 1;
	guint dual_page_odd_left : 1;
	guint rtl : 1;
	guint inverted_colors : 1;

	gdouble max_scale;
	gdouble min_scale;
};

enum {
	PROP_0,
	PROP_DOCUMENT,
	PROP_PAGE,
	PROP_ROTATION,
	PROP_INVERTED_COLORS,
	PROP_SCALE,
	PROP_SIZING_MODE,
	PROP_CONTINUOUS,
	PROP_DUAL_PAGE_ODD_LEFT,
	PROP_RTL,
	PROP_MIN_SCALE,
	PROP_MAX_SCALE,
	PROP_PAGE_LAYOUT
};

enum
{
	PAGE_CHANGED,
	N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0 };

G_DEFINE_TYPE (EvDocumentModel, ev_document_model, G_TYPE_OBJECT)

#define DEFAULT_MIN_SCALE 0.25
#define DEFAULT_MAX_SCALE 5.0

static void
ev_document_model_finalize (GObject *object)
{
	EvDocumentModel *model = EV_DOCUMENT_MODEL (object);

	g_clear_object (&model->document);

	G_OBJECT_CLASS (ev_document_model_parent_class)->finalize (object);
}

static void
ev_document_model_set_property (GObject      *object,
				guint         prop_id,
				const GValue *value,
				GParamSpec   *pspec)
{
	EvDocumentModel *model = EV_DOCUMENT_MODEL (object);

	switch (prop_id) {
	case PROP_DOCUMENT:
		ev_document_model_set_document (model, (EvDocument *)g_value_get_object (value));
		break;
	case PROP_PAGE:
		ev_document_model_set_page (model, g_value_get_int (value));
		break;
	case PROP_ROTATION:
		ev_document_model_set_rotation (model, g_value_get_int (value));
		break;
	case PROP_INVERTED_COLORS:
		ev_document_model_set_inverted_colors (model, g_value_get_boolean (value));
		break;
	case PROP_SCALE:
		ev_document_model_set_scale (model, g_value_get_double (value));
		break;
	case PROP_MIN_SCALE:
		ev_document_model_set_min_scale (model, g_value_get_double (value));
		break;
	case PROP_MAX_SCALE:
		ev_document_model_set_max_scale (model, g_value_get_double (value));
		break;
	case PROP_SIZING_MODE:
		ev_document_model_set_sizing_mode (model, g_value_get_enum (value));
		break;
	case PROP_CONTINUOUS:
		ev_document_model_set_continuous (model, g_value_get_boolean (value));
		break;
	case PROP_PAGE_LAYOUT:
		ev_document_model_set_page_layout (model, g_value_get_enum (value));
		break;
	case PROP_DUAL_PAGE_ODD_LEFT:
		ev_document_model_set_dual_page_odd_pages_left (model, g_value_get_boolean (value));
		break;
	case PROP_RTL:
		ev_document_model_set_rtl (model, g_value_get_boolean (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
ev_document_model_get_property (GObject    *object,
				guint       prop_id,
				GValue     *value,
				GParamSpec *pspec)
{
	EvDocumentModel *model = EV_DOCUMENT_MODEL (object);

	switch (prop_id) {
	case PROP_DOCUMENT:
		g_value_set_object (value, model->document);
		break;
	case PROP_PAGE:
		g_value_set_int (value, model->page);
		break;
	case PROP_ROTATION:
		g_value_set_int (value, model->rotation);
		break;
	case PROP_INVERTED_COLORS:
		g_value_set_boolean (value, model->inverted_colors);
		break;
	case PROP_SCALE:
		g_value_set_double (value, model->scale);
		break;
	case PROP_MIN_SCALE:
		g_value_set_double (value, model->min_scale);
		break;
	case PROP_MAX_SCALE:
		g_value_set_double (value, model->max_scale);
		break;
	case PROP_SIZING_MODE:
		g_value_set_enum (value, model->sizing_mode);
		break;
	case PROP_CONTINUOUS:
		g_value_set_boolean (value, ev_document_model_get_continuous (model));
		break;
	case PROP_PAGE_LAYOUT:
		g_value_set_enum (value, model->page_layout);
		break;
	case PROP_DUAL_PAGE_ODD_LEFT:
		g_value_set_boolean (value, ev_document_model_get_dual_page_odd_pages_left (model));
		break;
	case PROP_RTL:
		g_value_set_boolean (value, ev_document_model_get_rtl (model));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
ev_document_model_class_init (EvDocumentModelClass *klass)
{
	GObjectClass *g_object_class = G_OBJECT_CLASS (klass);

	g_object_class->get_property = ev_document_model_get_property;
	g_object_class->set_property = ev_document_model_set_property;
	g_object_class->finalize = ev_document_model_finalize;

	/* Properties */
	g_object_class_install_property (g_object_class,
					 PROP_DOCUMENT,
					 g_param_spec_object ("document",
							      "Document",
							      "The current document",
							      EV_TYPE_DOCUMENT,
							      G_PARAM_READWRITE |
                                                              G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (g_object_class,
					 PROP_PAGE,
					 g_param_spec_int ("page",
							   "Page",
							   "Current page",
							   -1, G_MAXINT, -1,
							   G_PARAM_READWRITE |
                                                           G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (g_object_class,
					 PROP_ROTATION,
					 g_param_spec_int ("rotation",
							   "Rotation",
							   "Current rotation angle",
							   0, 360, 0,
							   G_PARAM_READWRITE |
                                                           G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (g_object_class,
					 PROP_INVERTED_COLORS,
					 g_param_spec_boolean ("inverted-colors",
							       "Inverted Colors",
							       "Whether document is displayed with inverted colors",
							       FALSE,
							       G_PARAM_READWRITE |
                                                               G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (g_object_class,
					 PROP_SCALE,
					 g_param_spec_double ("scale",
							      "Scale",
							      "Current scale factor",
							      0., G_MAXDOUBLE, 1.,
							      G_PARAM_READWRITE |
                                                              G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (g_object_class,
					 PROP_MIN_SCALE,
					 g_param_spec_double ("min-scale",
							      "Minimum Scale",
							      "Minimum scale factor",
							      0., G_MAXDOUBLE, DEFAULT_MIN_SCALE,
							      G_PARAM_READWRITE |
                                                              G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (g_object_class,
					 PROP_MAX_SCALE,
					 g_param_spec_double ("max-scale",
							      "Maximum Scale",
							      "Maximum scale factor",
							      0., G_MAXDOUBLE, DEFAULT_MAX_SCALE,
							      G_PARAM_READWRITE |
                                                              G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (g_object_class,
					 PROP_SIZING_MODE,
					 g_param_spec_enum ("sizing-mode",
							    "Sizing Mode",
							    "Current sizing mode",
							    EV_TYPE_SIZING_MODE,
							    EV_SIZING_FIT_WIDTH,
							    G_PARAM_READWRITE |
                                                            G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (g_object_class,
					 PROP_PAGE_LAYOUT,
					 g_param_spec_enum ("page-layout",
							    "Page Layout",
							    "Current page layout",
							    EV_TYPE_PAGE_LAYOUT,
							    EV_PAGE_LAYOUT_SINGLE,
							    G_PARAM_READWRITE |
							    G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (g_object_class,
					 PROP_CONTINUOUS,
					 g_param_spec_boolean ("continuous",
							       "Continuous",
							       "Whether document is displayed in continuous mode",
							       TRUE,
							       G_PARAM_READWRITE |
                                                               G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (g_object_class,
					 PROP_DUAL_PAGE_ODD_LEFT,
					 g_param_spec_boolean ("dual-odd-left",
							       "Odd Pages Left",
							       "Whether odd pages are displayed on left side in dual mode",
							       FALSE,
							       G_PARAM_READWRITE |
                                                               G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (g_object_class,
					 PROP_RTL,
					 g_param_spec_boolean ("rtl",
							       "Right to Left",
							       "Whether the document is written from right to left",
							       FALSE,
							       G_PARAM_READWRITE |
                                                               G_PARAM_STATIC_STRINGS));

	/* Signals */
	signals [PAGE_CHANGED] =
		g_signal_new ("page-changed",
			      EV_TYPE_DOCUMENT_MODEL,
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      ev_view_marshal_VOID__INT_INT,
			      G_TYPE_NONE, 2,
			      G_TYPE_INT, G_TYPE_INT);
}

static void
ev_document_model_init (EvDocumentModel *model)
{
	model->page = -1;
	model->scale = 1.;
	model->sizing_mode = EV_SIZING_FIT_WIDTH;
	model->continuous = TRUE;
	model->inverted_colors = FALSE;
	model->min_scale = DEFAULT_MIN_SCALE;
	model->max_scale = DEFAULT_MAX_SCALE;
}

EvDocumentModel *
ev_document_model_new (void)
{
	return g_object_new (EV_TYPE_DOCUMENT_MODEL, NULL);
}

EvDocumentModel *
ev_document_model_new_with_document (EvDocument *document)
{
	g_return_val_if_fail (EV_IS_DOCUMENT (document), NULL);

	return g_object_new (EV_TYPE_DOCUMENT_MODEL, "document", document, NULL);
}

void
ev_document_model_set_document (EvDocumentModel *model,
				EvDocument      *document)
{
	g_return_if_fail (EV_IS_DOCUMENT_MODEL (model));
	g_return_if_fail (EV_IS_DOCUMENT (document));

	if (document == model->document)
		return;

	if (model->document)
		g_object_unref (model->document);
	model->document = g_object_ref (document);

	model->n_pages = ev_document_get_n_pages (document);
	ev_document_model_set_page (model, CLAMP (model->page, 0,
						  model->n_pages - 1));

	g_object_notify (G_OBJECT (model), "document");
}

/**
 * ev_document_model_get_document:
 * @model: a #EvDocumentModel
 *
 * Returns the #EvDocument referenced by the model.
 *
 * Returns: (transfer none): a #EvDocument
 */
EvDocument *
ev_document_model_get_document (EvDocumentModel *model)
{
	g_return_val_if_fail (EV_IS_DOCUMENT_MODEL (model), NULL);

	return model->document;
}

void
ev_document_model_set_page (EvDocumentModel *model,
			    gint             page)
{
	gint old_page;

	g_return_if_fail (EV_IS_DOCUMENT_MODEL (model));

	if (model->page == page)
		return;
	if (page < 0 || (model->document && page >= model->n_pages))
		return;

	old_page = model->page;
	model->page = page;
	g_signal_emit (model, signals[PAGE_CHANGED], 0, old_page, page);

	g_object_notify (G_OBJECT (model), "page");
}

void
ev_document_model_set_page_by_label (EvDocumentModel *model,
				     const gchar     *page_label)
{
	gint page;

	g_return_if_fail (EV_IS_DOCUMENT_MODEL (model));
	g_return_if_fail (model->document != NULL);

	if (ev_document_find_page_by_label (model->document, page_label, &page))
		ev_document_model_set_page (model, page);
}

gint
ev_document_model_get_page (EvDocumentModel *model)
{
	g_return_val_if_fail (EV_IS_DOCUMENT_MODEL (model), -1);

	return model->page;
}

void
ev_document_model_set_scale (EvDocumentModel *model,
			     gdouble          scale)
{
	g_return_if_fail (EV_IS_DOCUMENT_MODEL (model));

	scale = CLAMP (scale,
		       model->sizing_mode == EV_SIZING_FREE ?
		       model->min_scale : 0, model->max_scale);

	if (scale == model->scale)
		return;

	model->scale = scale;

	g_object_notify (G_OBJECT (model), "scale");
}

gdouble
ev_document_model_get_scale (EvDocumentModel *model)
{
	g_return_val_if_fail (EV_IS_DOCUMENT_MODEL (model), 1.0);

	return model->scale;
}

void
ev_document_model_set_max_scale (EvDocumentModel *model,
				 gdouble          max_scale)
{
	g_return_if_fail (EV_IS_DOCUMENT_MODEL (model));

	if (max_scale == model->max_scale)
		return;

	model->max_scale = max_scale;

	if (model->scale > max_scale)
		ev_document_model_set_scale (model, max_scale);

	g_object_notify (G_OBJECT (model), "max-scale");
}

gdouble
ev_document_model_get_max_scale (EvDocumentModel *model)
{
	g_return_val_if_fail (EV_IS_DOCUMENT_MODEL (model), 1.0);

	return model->max_scale;
}

void
ev_document_model_set_min_scale (EvDocumentModel *model,
				 gdouble          min_scale)
{
	g_return_if_fail (EV_IS_DOCUMENT_MODEL (model));

	if (min_scale == model->min_scale)
		return;

	model->min_scale = min_scale;

	if (model->scale < min_scale)
		ev_document_model_set_scale (model, min_scale);

	g_object_notify (G_OBJECT (model), "min-scale");
}

gdouble
ev_document_model_get_min_scale (EvDocumentModel *model)
{
	g_return_val_if_fail (EV_IS_DOCUMENT_MODEL (model), 0.);

	return model->min_scale;
}

void
ev_document_model_set_sizing_mode (EvDocumentModel *model,
				   EvSizingMode     mode)
{
	g_return_if_fail (EV_IS_DOCUMENT_MODEL (model));

	if (mode == model->sizing_mode)
		return;

	model->sizing_mode = mode;

	g_object_notify (G_OBJECT (model), "sizing-mode");
}

EvSizingMode
ev_document_model_get_sizing_mode (EvDocumentModel *model)
{
	g_return_val_if_fail (EV_IS_DOCUMENT_MODEL (model), EV_SIZING_FIT_WIDTH);

	return model->sizing_mode;
}

/**
 * ev_document_model_set_page_layout:
 * @model: a #EvDocumentModel
 * @layout: a #EvPageLayout
 *
 * Sets the document model's page layout to @layout.
 *
 * Since: 3.8
 */
void
ev_document_model_set_page_layout (EvDocumentModel *model,
				   EvPageLayout	    layout)
{
	g_return_if_fail (EV_IS_DOCUMENT_MODEL (model));

	if (layout == model->page_layout)
		return;

	model->page_layout = layout;

	g_object_notify (G_OBJECT (model), "page-layout");
}

/**
 * ev_document_model_get_page_layout:
 * @model: a #EvDocumentModel
 *
 * Returns: the document model's page layout
 *
 * Since: 3.8
 */
EvPageLayout
ev_document_model_get_page_layout (EvDocumentModel *model)
{
	g_return_val_if_fail (EV_IS_DOCUMENT_MODEL (model), EV_PAGE_LAYOUT_SINGLE);

	return model->page_layout;
}

void
ev_document_model_set_rotation (EvDocumentModel *model,
				gint             rotation)
{
	g_return_if_fail (EV_IS_DOCUMENT_MODEL (model));

	if (rotation >= 360)
		rotation -= 360;
	else if (rotation < 0)
		rotation += 360;

	if (rotation == model->rotation)
		return;

	model->rotation = rotation;

	g_object_notify (G_OBJECT (model), "rotation");
}

gint
ev_document_model_get_rotation (EvDocumentModel *model)
{
	g_return_val_if_fail (EV_IS_DOCUMENT_MODEL (model), 0);

	return model->rotation;
}

void
ev_document_model_set_inverted_colors (EvDocumentModel *model,
				       gboolean         inverted_colors)
{
	g_return_if_fail (EV_IS_DOCUMENT_MODEL (model));

	if (inverted_colors == model->inverted_colors)
		return;

	model->inverted_colors = inverted_colors;

	g_object_notify (G_OBJECT (model), "inverted-colors");
}

gboolean
ev_document_model_get_inverted_colors (EvDocumentModel *model)
{
	g_return_val_if_fail (EV_IS_DOCUMENT_MODEL (model), FALSE);

	return model->inverted_colors;
}

void
ev_document_model_set_continuous (EvDocumentModel *model,
				  gboolean         continuous)
{
	g_return_if_fail (EV_IS_DOCUMENT_MODEL (model));

	continuous = continuous != FALSE;

	if (continuous == model->continuous)
		return;

	model->continuous = continuous;

	g_object_notify (G_OBJECT (model), "continuous");
}

gboolean
ev_document_model_get_continuous (EvDocumentModel *model)
{
	g_return_val_if_fail (EV_IS_DOCUMENT_MODEL (model), TRUE);

	return model->continuous;
}

void
ev_document_model_set_dual_page_odd_pages_left (EvDocumentModel *model,
						gboolean         odd_left)
{
	g_return_if_fail (EV_IS_DOCUMENT_MODEL (model));

	odd_left = odd_left != FALSE;

	if (odd_left == model->dual_page_odd_left)
		return;

	model->dual_page_odd_left = odd_left;

	g_object_notify (G_OBJECT (model), "dual-odd-left");
}

gboolean
ev_document_model_get_dual_page_odd_pages_left (EvDocumentModel *model)
{
	g_return_val_if_fail (EV_IS_DOCUMENT_MODEL (model), FALSE);

	return model->dual_page_odd_left;
}

void
ev_document_model_set_rtl (EvDocumentModel *model,
                           gboolean         rtl)
{
	g_return_if_fail (EV_IS_DOCUMENT_MODEL (model));

	rtl = rtl != FALSE;

	if (rtl == model->rtl)
		return;

	model->rtl = rtl;

	g_object_notify (G_OBJECT (model), "rtl");
}

gboolean
ev_document_model_get_rtl (EvDocumentModel *model)
{
	g_return_val_if_fail (EV_IS_DOCUMENT_MODEL (model), FALSE);

	return model->rtl;
}
