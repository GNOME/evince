#include <config.h>
#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>
#ifndef USE_STABLE_LIBGNOMEUI
#include <libgnomeui/gnome-icon-lookup.h>
#endif
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <math.h>
#include "egg-recent-util.h"

#ifdef G_OS_WIN32
#include <windows.h>
#endif

#define EGG_RECENT_UTIL_HOSTNAME_SIZE 512

/* ripped out of gedit2 */
gchar* 
egg_recent_util_escape_underlines (const gchar* text)
{
	GString *str;
	gint length;
	const gchar *p;
 	const gchar *end;

  	g_return_val_if_fail (text != NULL, NULL);

    	length = strlen (text);

	str = g_string_new ("");

	p = text;
	end = text + length;

	while (p != end)
	{
		const gchar *next;
		next = g_utf8_next_char (p);

		switch (*p)
		{
			case '_':
				g_string_append (str, "__");
				break;
			default:
				g_string_append_len (str, p, next - p);
			break;
		}

		p = next;
	}

	return g_string_free (str, FALSE);
}

GdkPixbuf *
egg_recent_util_get_icon (GtkIconTheme *theme, const gchar *uri,
			  const gchar *mime_type, int size)
{
#ifndef USE_STABLE_LIBGNOMEUI
	gchar *icon;
	GdkPixbuf *pixbuf;
	
	icon = gnome_icon_lookup (theme, NULL, uri, NULL, NULL,
				  mime_type, 0, NULL);

	g_return_val_if_fail (icon != NULL, NULL);

	pixbuf = gtk_icon_theme_load_icon (theme, icon, size, 0, NULL);
	g_free (icon);

	return pixbuf;
#endif
	return NULL;
}

gchar *
egg_recent_util_get_unique_id (void)
{
	char hostname[EGG_RECENT_UTIL_HOSTNAME_SIZE];
	time_t the_time;
	guint32 rand;
	int pid;
	
#ifndef G_OS_WIN32
	gethostname (hostname, EGG_RECENT_UTIL_HOSTNAME_SIZE);
#else
	{
		DWORD size = EGG_RECENT_UTIL_HOSTNAME_SIZE;
		GetComputerName (hostname, &size);
	}
#endif
	
	time (&the_time);
	rand = g_random_int ();
	pid = getpid ();

	return g_strdup_printf ("%s-%d-%d-%d", hostname, (int)time, (int)rand, (int)pid);
}
