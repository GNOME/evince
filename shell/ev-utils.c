/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-indent-level: 8 -*- */
/*
 *  Copyright (C) 2004 Anders Carlsson <andersca@gnome.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include "ev-utils.h"
#include <math.h>

typedef struct
{
  int size;
  double *data;
} ConvFilter;

static double
gaussian (double x, double y, double r)
{
    return ((1 / (2 * M_PI * r)) *
	    exp ((- (x * x + y * y)) / (2 * r * r)));
}

static ConvFilter *
create_blur_filter (int radius)
{
  ConvFilter *filter;
  int x, y;
  double sum;
  
  filter = g_new0 (ConvFilter, 1);
  filter->size = radius * 2 + 1;
  filter->data = g_new (double, filter->size * filter->size);

  sum = 0.0;
  
  for (y = 0 ; y < filter->size; y++)
    {
      for (x = 0 ; x < filter->size; x++)
	{
	  sum += filter->data[y * filter->size + x] = gaussian (x - (filter->size >> 1),
								y - (filter->size >> 1),
								radius);
	}
    }

  for (y = 0; y < filter->size; y++)
    {
      for (x = 0; x < filter->size; x++)
	{
	  filter->data[y * filter->size + x] /= sum;
	}
    }

  return filter;
  
}

static GdkPixbuf *
create_shadow (GdkPixbuf *src, int blur_radius,
	       int x_offset, int y_offset, double opacity)
{
  int x, y, i, j;
  int width, height;
  GdkPixbuf *dest;
  static ConvFilter *filter = NULL;
  int src_rowstride, dest_rowstride;
  int src_bpp, dest_bpp;
  
  guchar *src_pixels, *dest_pixels;

  if (!filter)
    filter = create_blur_filter (blur_radius);

  if (x_offset < 0)
	  x_offset = (blur_radius * 4) / 5;
  
  if (y_offset < 0)
	  y_offset = (blur_radius * 4) / 5;

  
  width = gdk_pixbuf_get_width (src) + blur_radius * 2 + x_offset;
  height = gdk_pixbuf_get_height (src) + blur_radius * 2 + y_offset;

  dest = gdk_pixbuf_new (gdk_pixbuf_get_colorspace (src), TRUE,
			 gdk_pixbuf_get_bits_per_sample (src),
			 width, height);
  gdk_pixbuf_fill (dest, 0);  
  src_pixels = gdk_pixbuf_get_pixels (src);
  src_rowstride = gdk_pixbuf_get_rowstride (src);
  src_bpp = gdk_pixbuf_get_has_alpha (src) ? 4 : 3;
  
  dest_pixels = gdk_pixbuf_get_pixels (dest);
  dest_rowstride = gdk_pixbuf_get_rowstride (dest);
  dest_bpp = gdk_pixbuf_get_has_alpha (dest) ? 4 : 3;
  
  for (y = 0; y < height; y++)
    {
      for (x = 0; x < width; x++)
	{
	  int sumr = 0, sumg = 0, sumb = 0, suma = 0;

	  for (i = 0; i < filter->size; i++)
	    {
	      for (j = 0; j < filter->size; j++)
		{
		  int src_x, src_y;

		  src_y = -(blur_radius + x_offset) + y - (filter->size >> 1) + i;
		  src_x = -(blur_radius + y_offset) + x - (filter->size >> 1) + j;

		  if (src_y < 0 || src_y > gdk_pixbuf_get_height (src) ||
		      src_x < 0 || src_x > gdk_pixbuf_get_width (src))
		    continue;

		  sumr += src_pixels [src_y * src_rowstride +
				      src_x * src_bpp + 0] *
		    filter->data [i * filter->size + j];
		  sumg += src_pixels [src_y * src_rowstride +
				      src_x * src_bpp + 1] * 
		    filter->data [i * filter->size + j];

		  sumb += src_pixels [src_y * src_rowstride +
				      src_x * src_bpp + 2] * 
		    filter->data [i * filter->size + j];
		  
		  if (src_bpp == 4)
		    suma += src_pixels [src_y * src_rowstride +
					src_x * src_bpp + 3] *
		    filter->data [i * filter->size + j];
		  else
			  suma += 0xff;
		    
		}
	    }

	  if (dest_bpp == 4)
	    dest_pixels [y * dest_rowstride +
			 x * dest_bpp + 3] = (suma * opacity) / (filter->size * filter->size);

	}
    }
  
  return dest;
}

GdkPixbuf *
ev_pixbuf_add_shadow (GdkPixbuf *src, int size,
		      int x_offset, int y_offset, double opacity)
{
  GdkPixbuf *dest;
  
  dest = create_shadow (src, size, x_offset, y_offset, opacity);

  gdk_pixbuf_composite (src, dest,
			size, size,
			gdk_pixbuf_get_width (src),
			gdk_pixbuf_get_height (src),
 			size, size,
			1.0, 1.0,
			GDK_INTERP_NEAREST, 255);

  return dest;
}


#ifndef HAVE_G_FILE_SET_CONTENTS

#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>

static gboolean
rename_file (const char *old_name,
	     const char *new_name,
	     GError **err)
{
  errno = 0;
  if (g_rename (old_name, new_name) == -1)
    {
      return FALSE;
    }
  
  return TRUE;
}

static gboolean
set_umask_permissions (int	     fd,
		       GError      **err)
{
  /* All of this function is just to work around the fact that
   * there is no way to get the umask without changing it.
   *
   * We can't just change-and-reset the umask because that would
   * lead to a race condition if another thread tried to change
   * the umask in between the getting and the setting of the umask.
   * So we have to do the whole thing in a child process.
   */

  int save_errno;
  pid_t pid;

  pid = fork ();
  
  if (pid == -1)
    {
      return FALSE;
    }
  else if (pid == 0)
    {
      /* child */
      mode_t mask = umask (0666);

      errno = 0;
      if (fchmod (fd, 0666 & ~mask) == -1)
	_exit (errno);
      else
	_exit (0);

      return TRUE; /* To quiet gcc */
    }
  else
    { 
      /* parent */
      int status;

      errno = 0;
      if (waitpid (pid, &status, 0) == -1)
	{
	  return FALSE;
	}

      if (WIFEXITED (status))
	{
	  save_errno = WEXITSTATUS (status);

	  if (save_errno == 0)
	    {
	      return TRUE;
	    }
	  else
	    {
	      return FALSE;
	    }
	}
      else if (WIFSIGNALED (status))
	{
	  return FALSE;
	}
      else
	{
	  return FALSE;
	}
    }
}

static gchar *
write_to_temp_file (const gchar *contents,
		    gssize length,
		    const gchar *template,
		    GError **err)
{
  gchar *tmp_name;
  gchar *display_name;
  gchar *retval;
  FILE *file;
  gint fd;
  int save_errno;

  retval = NULL;
  
  tmp_name = g_strdup_printf ("%s.XXXXXX", template);

  errno = 0;
  fd = g_mkstemp (tmp_name);
  display_name = g_filename_display_name (tmp_name);
      
  if (fd == -1)
    {
      goto out;
    }

  if (!set_umask_permissions (fd, err))
    {
      close (fd);
      g_unlink (tmp_name);

      goto out;
    }
  
  errno = 0;
  file = fdopen (fd, "wb");
  if (!file)
    {
      close (fd);
      g_unlink (tmp_name);
      
      goto out;
    }

  if (length > 0)
    {
      size_t n_written;
      
      errno = 0;

      n_written = fwrite (contents, 1, length, file);

      if (n_written < length)
	{
	  fclose (file);
	  g_unlink (tmp_name);
	  
	  goto out;
	}
    }
   
  errno = 0;
  if (fclose (file) == EOF)
    { 
      g_unlink (tmp_name);
      
      goto out;
    }

  retval = g_strdup (tmp_name);
  
 out:
  g_free (tmp_name);
  g_free (display_name);
  
  return retval;
}

gboolean
ev_file_set_contents (const gchar *filename,
		      const gchar *contents,
		      gssize	     length,
		      GError	   **error)
{
  gchar *tmp_filename;
  gboolean retval;
  GError *rename_error = NULL;
  
  g_return_val_if_fail (filename != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  g_return_val_if_fail (contents != NULL || length == 0, FALSE);
  g_return_val_if_fail (length >= -1, FALSE);
  
  if (length == -1)
    length = strlen (contents);

  tmp_filename = write_to_temp_file (contents, length, filename, error);
  
  if (!tmp_filename)
    {
      retval = FALSE;
      goto out;
    }

  if (!rename_file (tmp_filename, filename, &rename_error))
    {
      g_unlink (tmp_filename);
      g_propagate_error (error, rename_error);
      retval = FALSE;
      goto out;
    }

  retval = TRUE;
  
 out:
  g_free (tmp_filename);
  return retval;
}

#endif /* HAVE_G_FILE_SET_CONTENTS */

