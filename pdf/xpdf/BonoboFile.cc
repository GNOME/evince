//========================================================================
//
// BonoboFile.cc
//
// Copyright 1999 Derek B. Noonburg assigned by Michael Meeks.
//
//========================================================================

#ifdef __GNUC__
#pragma implementation
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#ifndef WIN32
#include <unistd.h>
#endif
#include <string.h>
#include <ctype.h>

#include "BaseFile.h"

//define HARD_DEBUG

/* The open/close is done for you by the Bonobo server */

void
bfclose (BaseFile file)
{
}

size_t
bfread (void *ptr, size_t size, size_t nmemb, BaseFile file)
{
  CORBA_long len;
  CORBA_Environment ev;
  GNOME_Stream_iobuf *buffer = NULL;

  g_return_val_if_fail (ptr != NULL, 0);

#ifdef HARD_DEBUG
  printf ("read %p %d %d to %p\n", file, size, nmemb, ptr);
#endif
  len = GNOME_Stream_read (file, size*nmemb, &buffer, &ev);
  g_return_val_if_fail (ev._major == CORBA_NO_EXCEPTION, 0);

#ifdef HARD_DEBUG
  printf ("Read %d bytes %p %d\n", len, buffer->_buffer, buffer->_length);
#endif
  memcpy (ptr, buffer->_buffer, buffer->_length);

  return len;
}

int
bfseek (BaseFile file, long offset, int whence)
{
  CORBA_Environment ev;
  GNOME_Stream_SeekType t;
#ifdef HARD_DEBUG
  printf ("Seek %p %d %d\n", file, offset, whence);
#endif
  if (whence == SEEK_SET)
    t = GNOME_Stream_SEEK_SET;
  else if (whence == SEEK_CUR)
    t = GNOME_Stream_SEEK_CUR;
  else
    t = GNOME_Stream_SEEK_END;
  
  return GNOME_Stream_seek (file, offset, t, &ev);
}

void
brewind (BaseFile file)
{
  CORBA_Environment ev;
#ifdef HARD_DEBUG
  printf ("rewind %p\n", file);
#endif
  GNOME_Stream_seek (file, 0, GNOME_Stream_SEEK_SET, &ev);
}

long
bftell (BaseFile file)
{
  CORBA_Environment ev;
  CORBA_long pos;
#ifdef HARD_DEBUG
  printf ("tell %p\n", file);
#endif
  pos = GNOME_Stream_seek (file, 0, GNOME_Stream_SEEK_CUR, &ev);
#ifdef HARD_DEBUG
  printf ("tell returns %d\n", pos);
#endif

  return pos;
}
