/* 
 * When including goo and glib headers in one file:
 *
 * #include "GList.h"
 * #include "gpdf-g-switch.h"
 * #include "glib.h"
 * < more glib/gtk+/gnome headers >
 * #include "gpdf-g-switch.h"
 */


#ifdef GPDF_GOO
#  undef GString
#  undef GList
#  undef GDir
#  undef GMutex
#  undef GPDF_GOO
#else
#  define GString G_String
#  define GList   G_List
#  define GDir    G_Dir
#  define GMutex  G_Mutex
#  define GPDF_GOO
#endif
