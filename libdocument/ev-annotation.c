/* ev-annotation.c
 *  this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2009 Carlos Garcia Campos <carlosgc@gnome.org>
 * Copyright (C) 2007 Iñigo Martinez <inigomartinez@gmail.com>
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

#include "ev-annotation.h"


static void ev_annotation_markup_iface_base_init       (EvAnnotationMarkupIface *iface);
static void ev_annotation_text_markup_iface_init       (EvAnnotationMarkupIface *iface);
static void ev_annotation_attachment_markup_iface_init (EvAnnotationMarkupIface *iface);

enum {
	PROP_0,
	PROP_LABEL,
	PROP_OPACITY,
	PROP_HAS_POPUP,
	PROP_RECTANGLE,
	PROP_IS_OPEN
};

G_DEFINE_ABSTRACT_TYPE (EvAnnotation, ev_annotation, G_TYPE_OBJECT)
GType
ev_annotation_markup_get_type (void)
{
	static volatile gsize g_define_type_id__volatile = 0;

	if (g_once_init_enter (&g_define_type_id__volatile)) {
		GType g_define_type_id;
		const GTypeInfo our_info = {
			sizeof (EvAnnotationMarkupIface),
			(GBaseInitFunc) ev_annotation_markup_iface_base_init,
			NULL,
		};

		g_define_type_id = g_type_register_static (G_TYPE_INTERFACE,
							   "EvAnnotationMarkup",
							   &our_info, (GTypeFlags)0);
		g_type_interface_add_prerequisite (g_define_type_id, EV_TYPE_ANNOTATION);

		g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);
	}

	return g_define_type_id__volatile;
}

G_DEFINE_TYPE_WITH_CODE (EvAnnotationText, ev_annotation_text, EV_TYPE_ANNOTATION,
	 {
		 G_IMPLEMENT_INTERFACE (EV_TYPE_ANNOTATION_MARKUP,
					ev_annotation_text_markup_iface_init);
	 });
G_DEFINE_TYPE_WITH_CODE (EvAnnotationAttachment, ev_annotation_attachment, EV_TYPE_ANNOTATION,
	 {
		 G_IMPLEMENT_INTERFACE (EV_TYPE_ANNOTATION_MARKUP,
					ev_annotation_attachment_markup_iface_init);
	 });

/* EvAnnotation */
static void
ev_annotation_finalize (GObject *object)
{
        EvAnnotation *annot = EV_ANNOTATION (object);

	if (annot->page) {
		g_object_unref (annot->page);
		annot->page = NULL;
	}

        if (annot->contents) {
                g_free (annot->contents);
                annot->contents = NULL;
        }

        if (annot->name) {
                g_free (annot->name);
                annot->name = NULL;
        }

        if (annot->modified) {
                g_free (annot->modified);
                annot->modified = NULL;
        }

        G_OBJECT_CLASS (ev_annotation_parent_class)->finalize (object);
}

static void
ev_annotation_init (EvAnnotation *annot)
{
}

static void
ev_annotation_class_init (EvAnnotationClass *klass)
{
	GObjectClass *g_object_class = G_OBJECT_CLASS (klass);

	g_object_class->finalize = ev_annotation_finalize;
}

/* EvAnnotationMarkup */
typedef struct {
	gchar   *label;
	gdouble  opacity;
	gboolean has_popup;
	gboolean is_open;
	EvRectangle *rectangle;
} EvAnnotationMarkupProps;

static void
ev_annotation_markup_iface_base_init (EvAnnotationMarkupIface *iface)
{
	static gboolean initialized = FALSE;

	if (!initialized) {
		g_object_interface_install_property (iface,
						     g_param_spec_string ("label",
									  "Label",
									  "Label of the markup annotation",
									  NULL,
									  G_PARAM_READWRITE));
		g_object_interface_install_property (iface,
						     g_param_spec_double ("opacity",
									  "Opacity",
									  "Opacity of the markup annotation",
									  0,
									  G_MAXDOUBLE,
									  0,
									  G_PARAM_READWRITE));
		g_object_interface_install_property (iface,
						     g_param_spec_boolean ("has_popup",
									   "Has popup",
									   "Whether the markup annotation has "
									   "a popup window associated",
									   TRUE,
									   G_PARAM_READWRITE));
		g_object_interface_install_property (iface,
						     g_param_spec_boxed ("rectangle",
									 "Rectangle",
									 "The Rectangle of the popup associated "
									 "to the markup annotation",
									 EV_TYPE_RECTANGLE,
									 G_PARAM_READWRITE));
		g_object_interface_install_property (iface,
						     g_param_spec_boolean ("is_open",
									   "Is open",
									   "Whether the popup associated to "
									   "the markup annotation is open",
									   FALSE,
									   G_PARAM_READWRITE));
		initialized = TRUE;
	}
}

static void
ev_annotation_markup_props_free (EvAnnotationMarkupProps *props)
{
	g_free (props->label);
	ev_rectangle_free (props->rectangle);
	g_slice_free (EvAnnotationMarkupProps, props);
}

static EvAnnotationMarkupProps *
ev_annotation_markup_get_properties (EvAnnotationMarkup *markup)
{
	EvAnnotationMarkupProps *props;
	static GQuark props_key = 0;

	if (!props_key)
		props_key = g_quark_from_static_string ("ev-annotation-markup-props");

	props = g_object_get_qdata (G_OBJECT (markup), props_key);
	if (!props) {
		props = g_slice_new0 (EvAnnotationMarkupProps);
		g_object_set_qdata_full (G_OBJECT (markup),
					 props_key, props,
					 (GDestroyNotify) ev_annotation_markup_props_free);
	}

	return props;
}

static void
ev_annotation_markup_set_property (GObject      *object,
				   guint         prop_id,
				   const GValue *value,
				   GParamSpec   *pspec)
{
	EvAnnotationMarkupProps *props;

	props = ev_annotation_markup_get_properties (EV_ANNOTATION_MARKUP (object));

	switch (prop_id) {
	case PROP_LABEL:
		g_free (props->label);
		props->label = g_value_dup_string (value);
		break;
	case PROP_OPACITY:
		props->opacity = g_value_get_double (value);
		break;
	case PROP_HAS_POPUP:
		props->has_popup = g_value_get_boolean (value);
		break;
	case PROP_RECTANGLE:
		ev_rectangle_free (props->rectangle);
		props->rectangle = g_value_dup_boxed (value);
		break;
	case PROP_IS_OPEN:
		props->is_open = g_value_get_boolean (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
ev_annotation_markup_get_property (GObject    *object,
				   guint       prop_id,
				   GValue     *value,
				   GParamSpec *pspec)
{
	EvAnnotationMarkupProps *props;

	props = ev_annotation_markup_get_properties (EV_ANNOTATION_MARKUP (object));

	switch (prop_id) {
	case PROP_LABEL:
		g_value_set_string (value, props->label);
		break;
	case PROP_OPACITY:
		g_value_set_double (value, props->opacity);
		break;
	case PROP_HAS_POPUP:
		g_value_set_boolean (value, props->has_popup);
		break;
	case PROP_RECTANGLE:
		g_value_set_boxed (value, props->rectangle);
		break;
	case PROP_IS_OPEN:
		g_value_set_boolean (value, props->is_open);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
ev_annotation_markup_class_install_properties (GObjectClass *klass)
{
	klass->set_property = ev_annotation_markup_set_property;
	klass->get_property = ev_annotation_markup_get_property;

	g_object_class_override_property (klass, PROP_LABEL, "label");
	g_object_class_override_property (klass, PROP_OPACITY, "opacity");
	g_object_class_override_property (klass, PROP_HAS_POPUP, "has_popup");
	g_object_class_override_property (klass, PROP_RECTANGLE, "rectangle");
	g_object_class_override_property (klass, PROP_IS_OPEN, "is_open");
}

gchar *
ev_annotation_markup_get_label (EvAnnotationMarkup *markup)
{
	gchar *retval;

	g_return_val_if_fail (EV_IS_ANNOTATION_MARKUP (markup), NULL);

	g_object_get (G_OBJECT (markup), "label", &retval, NULL);

	return retval;
}

void
ev_annotation_markup_set_label (EvAnnotationMarkup *markup,
				const gchar        *label)
{
	g_return_if_fail (EV_IS_ANNOTATION_MARKUP (markup));
	g_return_if_fail (label != NULL);

	g_object_set (G_OBJECT (markup), "label", label, NULL);
}

gdouble
ev_annotation_markup_get_opacity (EvAnnotationMarkup *markup)
{
	gdouble retval;

	g_return_val_if_fail (EV_IS_ANNOTATION_MARKUP (markup), 0.0);

	g_object_get (G_OBJECT (markup), "opacity", &retval, NULL);

	return retval;
}

void
ev_annotation_markup_set_opacity (EvAnnotationMarkup *markup,
				  gdouble             opacity)
{
	g_return_if_fail (EV_IS_ANNOTATION_MARKUP (markup));

	g_object_set (G_OBJECT (markup), "opacity", opacity, NULL);
}

gboolean
ev_annotation_markup_has_popup (EvAnnotationMarkup *markup)
{
	gboolean retval;

	g_return_val_if_fail (EV_IS_ANNOTATION_MARKUP (markup), FALSE);

	g_object_get (G_OBJECT (markup), "has_popup", &retval, NULL);

	return retval;
}

void
ev_annotation_markup_get_rectangle (EvAnnotationMarkup *markup,
				    EvRectangle        *ev_rect)
{
	EvRectangle *r;

	g_return_if_fail (EV_IS_ANNOTATION_MARKUP (markup));
	g_return_if_fail (ev_rect != NULL);

	g_object_get (G_OBJECT (markup), "rectangle", &r, NULL);
	*ev_rect = *r;
}

gboolean
ev_annotation_markup_get_is_open (EvAnnotationMarkup *markup)
{
	gboolean retval;

	g_return_val_if_fail (EV_IS_ANNOTATION_MARKUP (markup), FALSE);

	g_object_get (G_OBJECT (markup), "is_open", &retval, NULL);

	return retval;
}

void
ev_annotation_markup_set_is_open (EvAnnotationMarkup *markup,
				  gboolean            is_open)
{
	g_return_if_fail (EV_IS_ANNOTATION_MARKUP (markup));

	g_object_set (G_OBJECT (markup), "is_open", is_open, NULL);
}

/* EvAnnotationText */
static void
ev_annotation_text_init (EvAnnotationText *annot)
{
}

static void
ev_annotation_text_class_init (EvAnnotationTextClass *klass)
{
	GObjectClass *g_object_class = G_OBJECT_CLASS (klass);

	ev_annotation_markup_class_install_properties (g_object_class);
}

static void
ev_annotation_text_markup_iface_init (EvAnnotationMarkupIface *iface)
{
}

EvAnnotation *
ev_annotation_text_new (EvPage *page)
{
	EvAnnotation *annot;

	annot = EV_ANNOTATION (g_object_new (EV_TYPE_ANNOTATION_TEXT, NULL));
	annot->page = g_object_ref (page);

	return annot;
}

/* EvAnnotationAttachment */
static void
ev_annotation_attachment_finalize (GObject *object)
{
	EvAnnotationAttachment *annot = EV_ANNOTATION_ATTACHMENT (object);

	if (annot->attachment) {
		g_object_unref (annot->attachment);
		annot->attachment = NULL;
	}

	G_OBJECT_CLASS (ev_annotation_attachment_parent_class)->finalize (object);
}

static void
ev_annotation_attachment_init (EvAnnotationAttachment *annot)
{
}

static void
ev_annotation_attachment_class_init (EvAnnotationAttachmentClass *klass)
{
	GObjectClass *g_object_class = G_OBJECT_CLASS (klass);

	ev_annotation_markup_class_install_properties (g_object_class);

	g_object_class->finalize = ev_annotation_attachment_finalize;
}

static void
ev_annotation_attachment_markup_iface_init (EvAnnotationMarkupIface *iface)
{
}

EvAnnotation *
ev_annotation_attachment_new (EvPage       *page,
			      EvAttachment *attachment)
{
	EvAnnotation *annot;

	g_return_val_if_fail (EV_IS_ATTACHMENT (attachment), NULL);

	annot = EV_ANNOTATION (g_object_new (EV_TYPE_ANNOTATION_ATTACHMENT, NULL));
	annot->page = g_object_ref (page);
	EV_ANNOTATION_ATTACHMENT (annot)->attachment = g_object_ref (attachment);

	return annot;
}
