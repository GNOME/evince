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

#pragma once

#if !defined (__EV_EVINCE_DOCUMENT_H_INSIDE__) && !defined (EVINCE_COMPILATION)
#error "Only <evince-document.h> can be included directly."
#endif

#include <glib-object.h>
#include <gdk/gdk.h>

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

EV_PUBLIC
G_DECLARE_INTERFACE (EvAnnotationMarkup, ev_annotation_markup, EV, ANNOTATION_MARKUP, GObject)

/* EvAnnotationText */
#define EV_TYPE_ANNOTATION_TEXT                 (ev_annotation_text_get_type())
#define EV_ANNOTATION_TEXT(object)              (G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_ANNOTATION_TEXT, EvAnnotationText))
#define EV_ANNOTATION_TEXT_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST((klass), EV_TYPE_ANNOTATION_TEXT, EvAnnotationTextClass))
#define EV_IS_ANNOTATION_TEXT(object)           (G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_ANNOTATION_TEXT))
#define EV_IS_ANNOTATION_TEXT_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE((klass), EV_TYPE_ANNOTATION_TEXT))
#define EV_ANNOTATION_TEXT_GET_CLASS(object)    (G_TYPE_INSTANCE_GET_CLASS((object), EV_TYPE_ANNOTATION_TEXT, EvAnnotationTextClass))

/* EvAnnotationAttachment */
#define EV_TYPE_ANNOTATION_ATTACHMENT              (ev_annotation_attachment_get_type())
#define EV_ANNOTATION_ATTACHMENT(object)           (G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_ANNOTATION_ATTACHMENT, EvAnnotationAttachment))
#define EV_ANNOTATION_ATTACHMENT_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), EV_TYPE_ANNOTATION_ATTACHMENT, EvAnnotationAttachmentClass))
#define EV_IS_ANNOTATION_ATTACHMENT(object)        (G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_ANNOTATION_ATTACHMENT))
#define EV_IS_ANNOTATION_ATTACHMENT_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass), EV_TYPE_ANNOTATION_ATTACHMENT))
#define EV_ANNOTATION_ATTACHMENT_GET_CLASS(object) (G_TYPE_INSTANCE_GET_CLASS((object), EV_TYPE_ANNOTATION_ATTACHMENT, EvAnnotationAttachmentClass))

/* EvAnnotationTextMarkup */
#define EV_TYPE_ANNOTATION_TEXT_MARKUP              (ev_annotation_text_markup_get_type ())
#define EV_ANNOTATION_TEXT_MARKUP(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), EV_TYPE_ANNOTATION_TEXT_MARKUP, EvAnnotationTextMarkup))
#define EV_ANNOTATION_TEXT_MARKUP_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), EV_TYPE_ANNOTATION_TEXT_MARKUP, EvAnnotationTextMarkupClass))
#define EV_IS_ANNOTATION_TEXT_MARKUP(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), EV_TYPE_ANNOTATION_TEXT_MARKUP))
#define EV_IS_ANNOTATION_TEXT_MARKUP_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), EV_TYPE_ANNOTATION_TEXT_MARKUP))
#define EV_ANNOTATION_TEXT_MARKUP_GET_CLASS(object) (G_TYPE_INSTANCE_GET_CLASS ((object), EV_TYPE_ANNOTATION_TEXT_MARKUP, EvAnnotationTextMarkupClass))

typedef struct _EvAnnotation                EvAnnotation;
typedef struct _EvAnnotationClass           EvAnnotationClass;

typedef struct _EvAnnotationText            EvAnnotationText;
typedef struct _EvAnnotationTextClass       EvAnnotationTextClass;

typedef struct _EvAnnotationAttachment      EvAnnotationAttachment;
typedef struct _EvAnnotationAttachmentClass EvAnnotationAttachmentClass;

typedef struct _EvAnnotationTextMarkup      EvAnnotationTextMarkup;
typedef struct _EvAnnotationTextMarkupClass EvAnnotationTextMarkupClass;

typedef enum {
	EV_ANNOTATION_TYPE_UNKNOWN,
	EV_ANNOTATION_TYPE_TEXT,
	EV_ANNOTATION_TYPE_ATTACHMENT,
	EV_ANNOTATION_TYPE_TEXT_MARKUP
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

typedef enum {
        EV_ANNOTATION_TEXT_MARKUP_HIGHLIGHT,
        EV_ANNOTATION_TEXT_MARKUP_STRIKE_OUT,
        EV_ANNOTATION_TEXT_MARKUP_UNDERLINE,
        EV_ANNOTATION_TEXT_MARKUP_SQUIGGLY
} EvAnnotationTextMarkupType;

#define EV_ANNOTATION_DEFAULT_COLOR ((const GdkRGBA) { 1., 1., 0, 1.});

/* EvAnnotation */
EV_PUBLIC
GType                ev_annotation_get_type                  (void) G_GNUC_CONST;
EV_PUBLIC
EvAnnotationType     ev_annotation_get_annotation_type       (EvAnnotation           *annot);
EV_PUBLIC
EvPage              *ev_annotation_get_page                  (EvAnnotation           *annot);
EV_PUBLIC
guint                ev_annotation_get_page_index            (EvAnnotation           *annot);
EV_PUBLIC
gboolean             ev_annotation_equal                     (EvAnnotation           *annot,
							      EvAnnotation           *other);
EV_PUBLIC
const gchar         *ev_annotation_get_contents              (EvAnnotation           *annot);
EV_PUBLIC
gboolean             ev_annotation_set_contents              (EvAnnotation           *annot,
							      const gchar            *contents);
EV_PUBLIC
const gchar         *ev_annotation_get_name                  (EvAnnotation           *annot);
EV_PUBLIC
gboolean             ev_annotation_set_name                  (EvAnnotation           *annot,
							      const gchar            *name);
EV_PUBLIC
const gchar         *ev_annotation_get_modified              (EvAnnotation           *annot);
EV_PUBLIC
gboolean             ev_annotation_set_modified              (EvAnnotation           *annot,
							      const gchar            *modified);
EV_PUBLIC
gboolean             ev_annotation_set_modified_from_time_t  (EvAnnotation           *annot,
							      time_t                  utime);
EV_PUBLIC
void                 ev_annotation_get_rgba                  (EvAnnotation           *annot,
                                                              GdkRGBA                *rgba);
EV_PUBLIC
gboolean             ev_annotation_set_rgba                  (EvAnnotation           *annot,
                                                              const GdkRGBA          *rgba);
EV_PUBLIC
void                 ev_annotation_get_area                  (EvAnnotation           *annot,
                                                              EvRectangle            *area);
EV_PUBLIC
gboolean             ev_annotation_set_area                  (EvAnnotation           *annot,
                                                              const EvRectangle      *area);

/* EvAnnotationMarkup */
EV_PUBLIC
const gchar         *ev_annotation_markup_get_label          (EvAnnotationMarkup     *markup);
EV_PUBLIC
gboolean             ev_annotation_markup_set_label          (EvAnnotationMarkup     *markup,
							      const gchar            *label);
EV_PUBLIC
gdouble              ev_annotation_markup_get_opacity        (EvAnnotationMarkup     *markup);
EV_PUBLIC
gboolean             ev_annotation_markup_set_opacity        (EvAnnotationMarkup     *markup,
							      gdouble                 opacity);
EV_PUBLIC
gboolean             ev_annotation_markup_can_have_popup     (EvAnnotationMarkup     *markup);
EV_PUBLIC
gboolean             ev_annotation_markup_has_popup          (EvAnnotationMarkup     *markup);
EV_PUBLIC
gboolean             ev_annotation_markup_set_has_popup      (EvAnnotationMarkup     *markup,
							      gboolean                has_popup);
EV_PUBLIC
void                 ev_annotation_markup_get_rectangle      (EvAnnotationMarkup     *markup,
							      EvRectangle            *ev_rect);
EV_PUBLIC
gboolean             ev_annotation_markup_set_rectangle      (EvAnnotationMarkup     *markup,
							      const EvRectangle      *ev_rect);
EV_PUBLIC
gboolean             ev_annotation_markup_get_popup_is_open  (EvAnnotationMarkup     *markup);
EV_PUBLIC
gboolean             ev_annotation_markup_set_popup_is_open  (EvAnnotationMarkup     *markup,
							      gboolean                is_open);

/* EvAnnotationText */
EV_PUBLIC
GType                ev_annotation_text_get_type             (void) G_GNUC_CONST;
EV_PUBLIC
EvAnnotation        *ev_annotation_text_new                  (EvPage                 *page);
EV_PUBLIC
EvAnnotationTextIcon ev_annotation_text_get_icon             (EvAnnotationText       *text);
EV_PUBLIC
gboolean             ev_annotation_text_set_icon             (EvAnnotationText       *text,
							      EvAnnotationTextIcon    icon);
EV_PUBLIC
gboolean             ev_annotation_text_get_is_open          (EvAnnotationText       *text);
EV_PUBLIC
gboolean             ev_annotation_text_set_is_open          (EvAnnotationText       *text,
							      gboolean                is_open);

/* EvAnnotationAttachment */
EV_PUBLIC
GType                ev_annotation_attachment_get_type       (void) G_GNUC_CONST;
EV_PUBLIC
EvAnnotation        *ev_annotation_attachment_new            (EvPage                 *page,
							      EvAttachment           *attachment);
EV_PUBLIC
EvAttachment        *ev_annotation_attachment_get_attachment (EvAnnotationAttachment *annot);
EV_PUBLIC
gboolean             ev_annotation_attachment_set_attachment (EvAnnotationAttachment *annot,
							      EvAttachment           *attachment);

/* EvAnnotationTextMarkup */
EV_PUBLIC
GType                      ev_annotation_text_markup_get_type        (void) G_GNUC_CONST;
EV_PUBLIC
EvAnnotation              *ev_annotation_text_markup_highlight_new   (EvPage                    *page);
EV_PUBLIC
EvAnnotation              *ev_annotation_text_markup_strike_out_new  (EvPage                    *page);
EV_PUBLIC
EvAnnotation              *ev_annotation_text_markup_underline_new   (EvPage                    *page);
EV_PUBLIC
EvAnnotation              *ev_annotation_text_markup_squiggly_new    (EvPage                    *page);
EV_PUBLIC
EvAnnotationTextMarkupType ev_annotation_text_markup_get_markup_type (EvAnnotationTextMarkup    *annot);
EV_PUBLIC
gboolean                   ev_annotation_text_markup_set_markup_type (EvAnnotationTextMarkup    *annot,
                                                                      EvAnnotationTextMarkupType markup_type);

G_END_DECLS
