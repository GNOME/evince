/* ev-annotation.h
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

#if !defined (__EV_EVINCE_DOCUMENT_H_INSIDE__) && !defined (EVINCE_COMPILATION)
#error "Only <evince-document.h> can be included directly."
#endif

#ifndef EV_ANNOTATION_H
#define EV_ANNOTATION_H

#include <glib-object.h>

#include "ev-document.h"
#include "ev-attachment.h"

G_BEGIN_DECLS

/* EvAnnotation */
#define EV_TYPE_ANNOTATION                      (ev_annotation_get_type())
#define EV_ANNOTATION(object)                   (G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_ANNOTATION, EvAnnotation))
#define EV_ANNOTATION_CLASS(klass)              (G_TYPE_CHECK_CLASS_CAST((klass), EV_TYPE_ANNOTATION, EvAnnotationClass))
#define EV_IS_ANNOTATION(object)                (G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_ANNOTATION))
#define EV_IS_ANNOTATION_CLASS(klass)           (G_TYPE_CHECK_CLASS_TYPE((klass), EV_TYPE_ANNOTATION))
#define EV_ANNOTATION_GET_CLASS(object)         (G_TYPE_INSTANCE_GET_CLASS((object), EV_TYPE_ANNOTATION, EvAnnotationClass))

/* EvAnnotationMarkup */
#define EV_TYPE_ANNOTATION_MARKUP               (ev_annotation_markup_get_type ())
#define EV_ANNOTATION_MARKUP(o)                 (G_TYPE_CHECK_INSTANCE_CAST ((o), EV_TYPE_ANNOTATION_MARKUP, EvAnnotationMarkup))
#define EV_ANNOTATION_MARKUP_IFACE(k)           (G_TYPE_CHECK_CLASS_CAST((k), EV_TYPE_ANNOTATION_MARKUP, EvAnnotationMarkupIface))
#define EV_IS_ANNOTATION_MARKUP(o)              (G_TYPE_CHECK_INSTANCE_TYPE ((o), EV_TYPE_ANNOTATION_MARKUP))
#define EV_IS_ANNOTATION_MARKUP_IFACE(k)        (G_TYPE_CHECK_CLASS_TYPE ((k), EV_TYPE_ANNOTATION_MARKUP))
#define EV_ANNOTATION_MARKUP_GET_IFACE(inst)    (G_TYPE_INSTANCE_GET_INTERFACE ((inst), EV_TYPE_ANNOTATION_MARKUP, EvAnnotationMarkupIface))

/* EvAnnotationText */
#define EV_TYPE_ANNOTATION_TEXT                 (ev_annotation_text_get_type())
#define EV_ANNOTATION_TEXT(object)              (G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_ANNOTATION_TEXT, EvAnnotationText))
#define EV_ANNOTATION_TEXT_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST((klass), EV_TYPE_ANNOTATION_TEXT, EvAnnotationTextClass))
#define EV_IS_ANNOTATION_TEXT(object)           (G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_ANNOTATION_TEXT))
#define EV_IS_ANNOTATION_TEXT_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE((klass), EV_TYPE_ANNOTATION_TEXT))
#define EV_ANNOTATION_TEXT_GET_CLASS(object)    (G_TYPE_INSTANCE_GET_CLASS((object), EV_TYPE_ANNOTATION_TEXT, EvAnnotationTextClass))

/* EvAnnotationText */
#define EV_TYPE_ANNOTATION_ATTACHMENT              (ev_annotation_attachment_get_type())
#define EV_ANNOTATION_ATTACHMENT(object)           (G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_ANNOTATION_ATTACHMENT, EvAnnotationAttachment))
#define EV_ANNOTATION_ATTACHMENT_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), EV_TYPE_ANNOTATION_ATTACHMENT, EvAnnotationAttachmentClass))
#define EV_IS_ANNOTATION_ATTACHMENT(object)        (G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_ANNOTATION_ATTACHMENT))
#define EV_IS_ANNOTATION_ATTACHMENT_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass), EV_TYPE_ANNOTATION_ATTACHMENT))
#define EV_ANNOTATION_ATTACHMENT_GET_CLASS(object) (G_TYPE_INSTANCE_GET_CLASS((object), EV_TYPE_ANNOTATION_ATTACHMENT, EvAnnotationAttachmentClass))

typedef struct _EvAnnotation                EvAnnotation;
typedef struct _EvAnnotationClass           EvAnnotationClass;

typedef struct _EvAnnotationMarkup          EvAnnotationMarkup;
typedef struct _EvAnnotationMarkupIface     EvAnnotationMarkupIface;

typedef struct _EvAnnotationText            EvAnnotationText;
typedef struct _EvAnnotationTextClass       EvAnnotationTextClass;

typedef struct _EvAnnotationAttachment      EvAnnotationAttachment;
typedef struct _EvAnnotationAttachmentClass EvAnnotationAttachmentClass;

struct _EvAnnotation
{
	GObject parent;

	EvPage  *page;
	gboolean changed;

	gchar   *contents;
	gchar   *name;
	gchar   *modified;
	GdkColor color;

};

struct _EvAnnotationClass
{
	GObjectClass parent_class;
};

struct _EvAnnotationMarkupIface
{
	GTypeInterface base_iface;
};

struct _EvAnnotationText
{
	EvAnnotation parent;

	gboolean is_open : 1;
};

struct _EvAnnotationTextClass
{
	EvAnnotationClass parent_class;
};

struct _EvAnnotationAttachment
{
	EvAnnotation parent;

	EvAttachment *attachment;
};

struct _EvAnnotationAttachmentClass
{
	EvAnnotationClass parent_class;
};

/* EvAnnotation */
GType         ev_annotation_get_type             (void) G_GNUC_CONST;

/* EvAnnotationMarkup */
GType         ev_annotation_markup_get_type      (void) G_GNUC_CONST;
gchar        *ev_annotation_markup_get_label     (EvAnnotationMarkup *markup);
void          ev_annotation_markup_set_label     (EvAnnotationMarkup *markup,
						  const gchar        *label);
gdouble       ev_annotation_markup_get_opacity   (EvAnnotationMarkup *markup);
void          ev_annotation_markup_set_opacity   (EvAnnotationMarkup *markup,
						  gdouble             opacity);
gboolean      ev_annotation_markup_has_popup     (EvAnnotationMarkup *markup);
void          ev_annotation_markup_get_rectangle (EvAnnotationMarkup *markup,
						  EvRectangle        *ev_rect);
gboolean      ev_annotation_markup_get_is_open   (EvAnnotationMarkup *markup);
void          ev_annotation_markup_set_is_open   (EvAnnotationMarkup *markup,
						  gboolean            is_open);

/* EvAnnotationText */
GType         ev_annotation_text_get_type        (void) G_GNUC_CONST;
EvAnnotation *ev_annotation_text_new             (EvPage             *page);

/* EvAnnotationText */
GType         ev_annotation_attachment_get_type  (void) G_GNUC_CONST;
EvAnnotation *ev_annotation_attachment_new       (EvPage             *page,
						  EvAttachment       *attachment);

G_END_DECLS

#endif /* EV_ANNOTATION_H */
