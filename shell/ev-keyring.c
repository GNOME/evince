/* ev-keyring.c
 *  this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2008 Carlos Garcia Campos <carlosgc@gnome.org>
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

#include <glib/gi18n.h>

#include "ev-keyring.h"

#ifdef WITH_KEYRING
#include <libsecret/secret.h>

static const SecretSchema doc_password_schema = {
	"org.gnome.Evince.Document",
	SECRET_SCHEMA_DONT_MATCH_NAME,
	{
		{ "type", SECRET_SCHEMA_ATTRIBUTE_STRING },
		{ "uri",  SECRET_SCHEMA_ATTRIBUTE_STRING },
		{ NULL, 0 }
	}
};
const SecretSchema *EV_DOCUMENT_PASSWORD_SCHEMA = &doc_password_schema;
#endif /* WITH_KEYRING */

gboolean
ev_keyring_is_available (void)
{
#ifdef WITH_KEYRING
	return TRUE;
#else
	return FALSE;
#endif
}

gchar *
ev_keyring_lookup_password (const gchar *uri)
{
#ifdef WITH_KEYRING
	g_return_val_if_fail (uri != NULL, NULL);

	return secret_password_lookup_sync (EV_DOCUMENT_PASSWORD_SCHEMA,
                                            NULL, NULL,
                                            "type", "document_password",
                                            "uri", uri,
                                            NULL);
#else
        return NULL;
#endif /* WITH_KEYRING */
}

gboolean
ev_keyring_save_password (const gchar  *uri,
			  const gchar  *password,
			  GPasswordSave flags)
{
#ifdef WITH_KEYRING
	const gchar *keyring;
	gchar       *name;
	gchar       *unescaped_uri;
        gboolean     retval;

	g_return_val_if_fail (uri != NULL, FALSE);

	if (flags == G_PASSWORD_SAVE_NEVER)
		return FALSE;

	keyring = (flags == G_PASSWORD_SAVE_FOR_SESSION) ? SECRET_COLLECTION_SESSION : NULL;
	unescaped_uri = g_uri_unescape_string (uri, NULL);
	name = g_strdup_printf (_("Password for document %s"), unescaped_uri);
	g_free (unescaped_uri);

	retval = secret_password_store_sync (EV_DOCUMENT_PASSWORD_SCHEMA, keyring,
                                             name, password, NULL, NULL,
                                             "type", "document_password",
                                             "uri", uri,
                                             NULL);
	g_free (name);

	return retval;
#else
	return FALSE;
#endif /* WITH_KEYRING */
}
