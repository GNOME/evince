#include "font.hh"

#include "painter.hh"
#include "dl-pkfont.hh"
#include "dl-vffont.hh"

using DviLib::FileLoader;
using DviLib::AbstractFont;
using DviLib::VfChar;
using DviLib::PkChar;
using DviLib::PkFont;
using DviLib::VfFont;

static char *
do_run_program (const char *program,
		GList *args,
		GError **err)
{
    char **argv = g_new (char *, g_list_length (args) + 2);
    GList *l;
    int i;
    char *result;

    i = 0;
    argv[i++] = g_strdup (program);
    for (l = args; l; l = l->next)
	argv[i++] = g_strdup ((char *)l->data);
    argv[i++] = NULL;
    
    g_list_free (args);


    /* run it */
    g_spawn_sync         (NULL, /* working directory */
			  argv, /* arguments */
			  NULL, /* environment */
			  G_SPAWN_SEARCH_PATH, /* flags */
			  NULL, /* child setup */
			  NULL, /* user data for child setup */
			  &result, /* stdout */
			  NULL, /* stderr */
			  NULL, /* exit status */
			  err /* GError */);

    g_strfreev (argv);
    
    return result;
}


static char *
run_program (const char *program,
	     GError **err,
	     const char *arg1,
	     ...)
{
    va_list arg_list;
    GList *arguments = NULL;
    char *s;

    va_start (arg_list, arg1);

    arguments = g_list_append (arguments, (gpointer)arg1);
    
    s = va_arg (arg_list, gchar*);
    while (s)
    {
	arguments = g_list_append (arguments, s);
	s = va_arg (arg_list, gchar*);
    }

    va_end (arg_list);

    return do_run_program (program, arguments, err);
}

static char *
run_kpsewhich (int dpi,
	       string format,
	       string name)
{
    char *dpistr = g_strdup_printf ("--dpi=%d", dpi);
    char *formatstr = g_strdup_printf ("--format=%s", format.c_str());
    char *namestr = g_strdup (name.c_str());
    GError *err = NULL;
    char *result;

    result = run_program ("kpsewhich", &err, dpistr, formatstr, namestr, NULL);

    if (!result)
    {
	cout << err->message << endl;
    }
    else
    {
	g_strstrip (result);

	if (*result == '\0')
	{
	    /* Nothing useful returned */
	    g_free (result);
	    result = NULL;
	}
    }

    cout << "kpsewhich " << dpistr << " " << formatstr << " " << namestr << " " << endl;
    
    g_free (dpistr);
    g_free (formatstr);
    g_free (namestr);

    return result;
}

static void
run_mktexpk (int dpi, string name)
{
    char *dpistr = g_strdup_printf ("--bdpi=%d", dpi);
    char *bdpistr = g_strdup_printf ("--dpi=%d", dpi);
    char *namestr = g_strdup (name.c_str());
    char *result;

    cout << "mktexpk " << bdpistr << " " << dpistr << " " << " " << namestr << " " << endl;
    
    result = run_program ("mktexpk", NULL, bdpistr, dpistr, namestr, NULL);
    if (result)
	g_free (result);

    g_free (dpistr);
    g_free (bdpistr);
    g_free (namestr);
}

AbstractFont *
FontFactory::create_font (std::string name, 
			  int dpi,
			  int at_size)
{
    char *filename;

    cout << "at size: " << at_size << endl;
    
    /* Find VF */
    filename = run_kpsewhich (dpi, "vf", name);

    if (filename)
	return new VfFont (*new FileLoader (filename), at_size);

    /* Try PK */
    
    filename = run_kpsewhich (dpi, "pk", name);

    if (filename)
	return new PkFont (*new FileLoader (filename), at_size);

    /* Generate PK */
    
    run_mktexpk (dpi, name);

    cout << "birnan" << endl;
    
    /* Try PK again */
    filename = run_kpsewhich (dpi, "pk", name);

    if (filename)
	return new PkFont (*new FileLoader (filename), at_size);

    cout << "no luck" << endl;
    
    throw (string ("bad font"));
    
    return NULL;
}
