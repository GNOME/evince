#include <config.h>
#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#ifndef USE_STABLE_LIBGNOMEUI
#include <libgnomeui/gnome-icon-theme.h>
#include <libgnomeui/gnome-icon-lookup.h>
#endif
#include <math.h>
#include "egg-recent-util.h"

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

#ifndef USE_STABLE_LIBGNOMEUI
static GdkPixbuf *
scale_icon (GdkPixbuf *pixbuf,
	    double *scale)
{
	guint width, height;

	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);

	width = floor (width * *scale + 0.5);
	height = floor (height * *scale + 0.5);
	
	return gdk_pixbuf_scale_simple (pixbuf, width, height, GDK_INTERP_BILINEAR);
}

static GdkPixbuf *
load_icon_file (char          *filename,
		guint          base_size,
		guint          nominal_size)
{
	GdkPixbuf *pixbuf, *scaled_pixbuf;
	guint width, height, size;
	double scale;

	pixbuf = gdk_pixbuf_new_from_file (filename, NULL);

	if (pixbuf == NULL) {
		return NULL;
	}
	
	if (base_size == 0) {
		width = gdk_pixbuf_get_width (pixbuf); 
		height = gdk_pixbuf_get_height (pixbuf);
		size = MAX (width, height);
		if (size > nominal_size) {
			base_size = size;
		} else {
			/* Don't scale up small icons */
			base_size = nominal_size;
		}
	}
	
	if (base_size != nominal_size) {
		scale = (double)nominal_size/base_size;
		scaled_pixbuf = scale_icon (pixbuf, &scale);
		g_object_unref (pixbuf);
		pixbuf = scaled_pixbuf;
	}

	return pixbuf;
}

GdkPixbuf *
egg_recent_util_get_icon (GnomeIconTheme *theme, const gchar *uri,
			  const gchar *mime_type, int size)
{
	gchar *icon;
	gchar *filename;
	const GnomeIconData *icon_data;
	int base_size;
	GdkPixbuf *pixbuf;
	
	icon = gnome_icon_lookup (theme, NULL, uri, NULL, NULL,
				  mime_type, 0, NULL);
	

	g_return_val_if_fail (icon != NULL, NULL);

	filename = gnome_icon_theme_lookup_icon (theme, icon,
						 size,
						 &icon_data,
						 &base_size);
	g_free (icon);

	if (filename == NULL) {
		return NULL;
	}

	pixbuf = load_icon_file (filename, base_size, size);
	g_free (filename);
	
	
	return pixbuf;
}
#endif /* !USE_STABLE_LIBGNOMEUI */

gchar *
egg_recent_util_get_unique_id (void)
{
	char hostname[EGG_RECENT_UTIL_HOSTNAME_SIZE];
	time_t the_time;
	guint32 rand;
	int pid;
	
	gethostname (hostname, EGG_RECENT_UTIL_HOSTNAME_SIZE);
	
	time (&the_time);
	rand = g_random_int ();
	pid = getpid ();

	return g_strdup_printf ("%s-%d-%d-%d", hostname, (int)time, (int)rand, (int)pid);
}
