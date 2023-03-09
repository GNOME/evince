/* ev-document-links.h
 *  this file is part of evince, a gnome document_links viewer
 *
 * Copyright (C) 2004 Red Hat, Inc.
 *
 * Author:
 *   Jonathan Blandford <jrb@alum.mit.edu>
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

#include "ev-document-security.h"

G_DEFINE_INTERFACE (EvDocumentSecurity, ev_document_security, 0)

static void
ev_document_security_default_init (EvDocumentSecurityInterface *klass)
{
}

gboolean
ev_document_security_has_document_security (EvDocumentSecurity *document_security)
{
	EvDocumentSecurityInterface *iface = EV_DOCUMENT_SECURITY_GET_IFACE (document_security);
	return iface->has_document_security (document_security);
}

void
ev_document_security_set_password (EvDocumentSecurity *document_security,
				   const char         *password)
{
	EvDocumentSecurityInterface *iface = EV_DOCUMENT_SECURITY_GET_IFACE (document_security);
	iface->set_password (document_security, password);
}
