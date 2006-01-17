
#ifndef __EGG_RECENT_UTIL__
#define __EGG_RECENT_UTIL__

#include <gtk/gtk.h>

G_BEGIN_DECLS

gchar * egg_recent_util_escape_underlines (const gchar *uri);
gchar * egg_recent_util_get_unique_id (void);
GdkPixbuf * egg_recent_util_get_icon (GtkIconTheme *theme,
				      const gchar *uri,
				      const gchar *mime_type,
				      int size);

G_END_DECLS

#endif /* __EGG_RECENT_UTIL__ */
