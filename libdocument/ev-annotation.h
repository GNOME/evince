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
#include "ev-macros.h"

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
#define EV_ANNOTATION_MARKUP_IFACE(k)           (G_TYPE_CHECK_CLASS_CAST((k), EV_TYPE_ANNOTATION_MARKUP, EvAnnotationMarkupInterface))
#define EV_IS_ANNOTATION_MARKUP(o)              (G_TYPE_CHECK_INSTANCE_TYPE ((o), EV_TYPE_ANNOTATION_MARKUP))
#define EV_IS_ANNOTATION_MARKUP_IFACE(k)        (G_TYPE_CHECK_CLASS_TYPE ((k), EV_TYPE_ANNOTATION_MARKUP))
#define EV_ANNOTATION_MARKUP_GET_IFACE(inst)    (G_TYPE_INSTANCE_GET_INTERFACE ((inst), EV_TYPE_ANNOTATION_MARKUP, EvAnnotationMarkupInterface))

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
typedef struct _EvAnnotationMarkupInterface EvAnnotationMarkupInterface;

typedef struct _EvAnnotationText            EvAnnotationText;
typedef struct _EvAnnotationTextClass       EvAnnotationTextClass;

typedef struct _EvAnnotationAttachment      EvAnnotationAttachment;
typedef struct _EvAnnotationAttachmentClass EvAnnotationAttachmentClass;

typedef enum {
	EV_ANNOTATION_TYPE_UNKNOWN,
	EV_ANNOTATION_TYPE_TEXT,
	EV_ANNOTATION_TYPE_ATTACHMENT
} EvAnnotationType;

typedef enum {
	EV_ANNOTATION_TEXT_ICON_NOTE,
	EV_ANNOTATION_TEXT_ICON_COMMENT,
	EV_ANNOTATION_TEXT_ICON_KEY,
	EV_ANNOTATION_TEXT_ICON_HELP,
	EV_ANNOTATION_TEXT_ICON_NEW_PARAGRAPH,
	EV_ANNOTATION_TEXT_ICON_PARAGRAPH,
	EV_ANNOTATION_TEXT_ICON_INSERT,
	EV_ANNOTATION_TEXT_ICON_CROSS,
	EV_ANNOTATION_TEXT_ICON_CIRCLE,
	EV_ANNOTATION_TEXT_ICON_UNKNOWN
} EvAnnotationTextIcon;

/* EvAnnotation */
GType                ev_annotation_get_type                  (void) G_GNUC_CONST;
EvAnnotationType     ev_annotation_get_annotation_type       (EvAnnotation           *annot);
EvPage              *ev_annotation_get_page                  (EvAnnotation           *annot);
guint                ev_annotation_get_page_index            (EvAnnotation           *annot);
gboolean             ev_annotation_equal                     (EvAnnotation           *annot,
							      EvAnnotation           *other);
const gchar         *ev_annotation_get_contents              (EvAnnotation           *annot);
gboolean             ev_annotation_set_contents              (EvAnnotation           *annot,
							      const gchar            *contents);
const gchar         *ev_annotation_get_name                  (EvAnnotation           *annot);
gboolean             ev_annotation_set_name                  (EvAnnotation           *annot,
							      const gchar            *name);
const gchar         *ev_annotation_get_modified              (EvAnnotation           *annot);
gboolean             ev_annotation_set_modified              (EvAnnotation           *annot,
							      const gchar            *modified);
gboolean             ev_annotation_set_modified_from_time    (EvAnnotation           *annot,
							      GTime                   utime);
EV_DEPRECATED_FOR(ev_annotaion_get_rgba)
void                 ev_annotation_get_color                 (EvAnnotation           *annot,
							      GdkColor               *color);
EV_DEPRECATED_FOR(ev_annotaion_set_rgba)
gboolean             ev_annotation_set_color                 (EvAnnotation           *annot,
							      const GdkColor         *color);
void                 ev_annotation_get_rgba                  (EvAnnotation           *annot,
                                                              GdkRGBA                *rgba);
gboolean             ev_annotation_set_rgba                  (EvAnnotation           *annot,
                                                              const GdkRGBA          *rgba);

/* EvAnnotationMarkup */
GType                ev_annotation_markup_get_type           (void) G_GNUC_CONST;
const gchar         *ev_annotation_markup_get_label          (EvAnnotationMarkup     *markup);
gboolean             ev_annotation_markup_set_label          (EvAnnotationMarkup     *markup,
							      const gchar            *label);
gdouble              ev_annotation_markup_get_opacity        (EvAnnotationMarkup     *markup);
gboolean             ev_annotation_markup_set_opacity        (EvAnnotationMarkup     *markup,
							      gdouble                 opacity);
gboolean             ev_annotation_markup_has_popup          (EvAnnotationMarkup     *markup);
gboolean             ev_annotation_markup_set_has_popup      (EvAnnotationMarkup     *markup,
							      gboolean                has_popup);
void                 ev_annotation_markup_get_rectangle      (EvAnnotationMarkup     *markup,
							      EvRectangle            *ev_rect);
gboolean             ev_annotation_markup_set_rectangle      (EvAnnotationMarkup     *markup,
							      const EvRectangle      *ev_rect);
gboolean             ev_annotation_markup_get_popup_is_open  (EvAnnotationMarkup     *markup);
gboolean             ev_annotation_markup_set_popup_is_open  (EvAnnotationMarkup     *markup,
							      gboolean                is_open);

/* EvAnnotationText */
GType                ev_annotation_text_get_type             (void) G_GNUC_CONST;
EvAnnotation        *ev_annotation_text_new                  (EvPage                 *page);
EvAnnotationTextIcon ev_annotation_text_get_icon             (EvAnnotationText       *text);
gboolean             ev_annotation_text_set_icon             (EvAnnotationText       *text,
							      EvAnnotationTextIcon    icon);
gboolean             ev_annotation_text_get_is_open          (EvAnnotationText       *text);
gboolean             ev_annotation_text_set_is_open          (EvAnnotationText       *text,
							      gboolean                is_open);

/* EvAnnotationAttachment */
GType                ev_annotation_attachment_get_type       (void) G_GNUC_CONST;
EvAnnotation        *ev_annotation_attachment_new            (EvPage                 *page,
							      EvAttachment           *attachment);
EvAttachment        *ev_annotation_attachment_get_attachment (EvAnnotationAttachment *annot);
gboolean             ev_annotation_attachment_set_attachment (EvAnnotationAttachment *annot,
							      EvAttachment           *attachment);

G_END_DECLS

#endif /* EV_ANNOTATION_H */
