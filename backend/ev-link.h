/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2005 Red Hat, Inc.
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef EV_LINK_H
#define EV_LINK_H

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _EvLink EvLink;
typedef struct _EvLinkClass EvLinkClass;
typedef struct _EvLinkPrivate EvLinkPrivate;

#define EV_TYPE_LINK		  (ev_link_get_type())
#define EV_LINK(object)		  (G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_LINK, EvLink))
#define EV_LINK_CLASS(klass)	  (G_TYPE_CHACK_CLASS_CAST((klass), EV_TYPE_LINK, EvLinkClass))
#define EV_IS_LINK(object)	  (G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_LINK))
#define EV_IS_LINK_CLASS(klass)	  (G_TYPE_CHECK_CLASS_TYPE((klass), EV_TYPE_LINK))
#define EV_LINK_GET_CLASS(object) (G_TYPE_INSTANCE_GET_CLASS((object), EV_TYPE_LINK, EvLinkClass))

#define EV_TYPE_LINK_TYPE	  (ev_link_type_get_type ())



typedef enum
{
	EV_LINK_TYPE_TITLE,
	EV_LINK_TYPE_PAGE,
	EV_LINK_TYPE_PAGE_XYZ,
	EV_LINK_TYPE_PAGE_FIT,
	EV_LINK_TYPE_PAGE_FITH,
	EV_LINK_TYPE_PAGE_FITV,
	EV_LINK_TYPE_PAGE_FITR,
	EV_LINK_TYPE_EXTERNAL_URI,
	EV_LINK_TYPE_LAUNCH
	/* We'll probably fill this in more as we support the other types of
	 * links */
} EvLinkType;

GType           ev_link_type_get_type	(void);
GType		ev_link_get_type	(void);

EvLink	       *ev_link_new_title	(const char     *title);
EvLink	       *ev_link_new_page	(const char     *title,
					 int             page);
EvLink	       *ev_link_new_page_xyz	(const char     *title,
					 int             page,
					 double          top,
					 double          left,
					 double          zoom);
EvLink	       *ev_link_new_page_fith	(const char     *title,
					 int             page,
					 double          top);
EvLink	       *ev_link_new_page_fitv	(const char     *title,
					 int             page,
					 double          left);
EvLink	       *ev_link_new_page_fitr	(const char     *title,
					 int             page,
					 double          left,
					 double          bottom,
					 double          right,
					 double          top);
EvLink	       *ev_link_new_page_fit	(const char     *title,
					 int             page);
EvLink	       *ev_link_new_external	(const char     *title,
					 const char     *uri);
EvLink	       *ev_link_new_launch	(const char     *title,
					 const char     *filename,
					 const char     *params);

const char     *ev_link_get_title	(EvLink     *link);
const char     *ev_link_get_uri		(EvLink     *link);
EvLinkType	ev_link_get_link_type	(EvLink     *link);
int		ev_link_get_page	(EvLink     *link);
double		ev_link_get_top		(EvLink     *link);
double		ev_link_get_left	(EvLink     *link);
double		ev_link_get_bottom	(EvLink     *link);
double		ev_link_get_right	(EvLink     *link);
double		ev_link_get_zoom	(EvLink     *link);
const char     *ev_link_get_filename    (EvLink     *link);
const char     *ev_link_get_params      (EvLink     *link);

/* Link Mapping stuff */

typedef struct _EvLinkMapping	  EvLinkMapping;
struct _EvLinkMapping
{
	EvLink *link;
	gdouble x1;
	gdouble y1;
	gdouble x2;
	gdouble y2;
};

void    ev_link_mapping_free (GList   *link_mapping);
EvLink *ev_link_mapping_find (GList   *link_mapping,
			      gdouble  x,
			      gdouble  y);
G_END_DECLS

#endif /* !EV_LINK_H */
