/*
  This file is not licenced under the GPL like the rest of the code.
  Its is under the MIT license, to encourage reuse by cut-and-paste.

  Copyright (c) 2007 Red Hat, inc

  Permission is hereby granted, free of charge, to any person
  obtaining a copy of this software and associated documentation files
  (the "Software"), to deal in the Software without restriction,
  including without limitation the rights to use, copy, modify, merge,
  publish, distribute, sublicense, and/or sell copies of the Software,
  and to permit persons to whom the Software is furnished to do so,
  subject to the following conditions: 

  The above copyright notice and this permission notice shall be
  included in all copies or substantial portions of the Software. 

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
  ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
  CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


char *
xdg_user_dir_lookup (const char *type)
{
  FILE *file;
  char *home_dir, *config_home, *config_file;
  char buffer[512];
  char *user_dir;
  char *p, *d;
  int len;
  int relative;
  
  home_dir = getenv ("HOME");

  if (home_dir == NULL)
    return strdup ("/tmp");

  config_home = getenv ("XDG_CONFIG_HOME");
  if (config_home == NULL || config_home[0] == 0)
    {
      config_file = malloc (strlen (home_dir) + strlen ("/.config/user-dirs.dirs") + 1);
      strcpy (config_file, home_dir);
      strcat (config_file, "/.config/user-dirs.dirs");
    }
  else
    {
      config_file = malloc (strlen (config_home) + strlen ("/user-dirs.dirs") + 1);
      strcpy (config_file, config_home);
      strcat (config_file, "/user-dirs.dirs");
    }

  file = fopen (config_file, "r");
  free (config_file);
  if (file == NULL)
    goto error;

  user_dir = NULL;
  while (fgets (buffer, sizeof (buffer), file))
    {
      /* Remove newline at end */
      len = strlen (buffer);
      if (len > 0 && buffer[len-1] == '\n')
	buffer[len-1] = 0;
      
      p = buffer;
      while (*p == ' ' || *p == '\t')
	p++;
      
      if (strncmp (p, "XDG_", 4) != 0)
	continue;
      p += 4;
      if (strncmp (p, type, strlen (type)) != 0)
	continue;
      p += strlen (type);
      if (strncmp (p, "_DIR", 4) != 0)
	continue;
      p += 4;

      while (*p == ' ' || *p == '\t')
	p++;

      if (*p != '=')
	continue;
      p++;
      
      while (*p == ' ' || *p == '\t')
	p++;

      if (*p != '"')
	continue;
      p++;
      
      relative = 0;
      if (strncmp (p, "$HOME/", 6) == 0)
	{
	  p += 6;
	  relative = 1;
	}
      else if (*p != '/')
	continue;
      
      if (relative)
	{
	  user_dir = malloc (strlen (home_dir) + 1 + strlen (p) + 1);
	  strcpy (user_dir, home_dir);
	  strcat (user_dir, "/");
	}
      else
	{
	  user_dir = malloc (strlen (p) + 1);
	  *user_dir = 0;
	}
      
      d = user_dir + strlen (user_dir);
      while (*p && *p != '"')
	{
	  if ((*p == '\\') && (*(p+1) != 0))
	    p++;
	  *d++ = *p++;
	}
      *d = 0;
    }  
  fclose (file);

  if (user_dir)
    return user_dir;

 error:
  /* Special case desktop for historical compatibility */
  if (strcmp (type, "DESKTOP") == 0)
    {
      user_dir = malloc (strlen (home_dir) + strlen ("/Desktop") + 1);
      strcpy (user_dir, home_dir);
      strcat (user_dir, "/Desktop");
      return user_dir;
    }
  else
    return strdup (home_dir);
}

