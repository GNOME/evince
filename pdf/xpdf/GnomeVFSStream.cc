//========================================================================
//
// GnomeVFSStream.cc
//
// Copyright 1996-2002 Glyph & Cog, LLC
// Copyright 2003 Martin Kretzschmar
//
//========================================================================

#ifdef __GNUC__
#pragma implementation
#endif

#include <aconf.h>
#include "config.h"

#include "GnomeVFSStream.h"
#include "gpdf-g-switch.h"
#  include <libgnomevfs/gnome-vfs.h>
#include "gpdf-g-switch.h"

#ifndef NO_DECRYPTION
#include "Decrypt.h"
#endif

GnomeVFSStream::GnomeVFSStream(GnomeVFSHandle *handleA, Guint startA,
                               GBool limitedA, Guint lengthA, Object *dictA):
  BaseStream(dictA) {
  handle = handleA;
  start = startA;
  limited = limitedA;
  length = lengthA;
  bufPtr = bufEnd = buf;
  bufPos = start;
  savePos = 0;
  saved = gFalse;
}

GnomeVFSStream::~GnomeVFSStream() {
  close();
}

Stream *GnomeVFSStream::makeSubStream(Guint startA, GBool limitedA,
                                      Guint lengthA, Object *dictA) {
  return new GnomeVFSStream(handle, startA, limitedA, lengthA, dictA);
}

void GnomeVFSStream::reset() {
  GnomeVFSFileSize offsetReturn;
  gnome_vfs_tell(handle, &offsetReturn);
  savePos = (Guint)offsetReturn;
  saved = gTrue;
  gnome_vfs_seek(handle, GNOME_VFS_SEEK_START, start);
  bufPtr = bufEnd = buf;
  bufPos = start;
#ifndef NO_DECRYPTION
  if (decrypt)
    decrypt->reset();
#endif
}

void GnomeVFSStream::close() {
  if (saved) {
    gnome_vfs_seek(handle, GNOME_VFS_SEEK_START, savePos);
    saved = gFalse;
  }
}

GBool GnomeVFSStream::fillBuf() {
  int n;
  GnomeVFSFileSize bytesRead;
#ifndef NO_DECRYPTION
  char *p;
#endif

  bufPos += bufEnd - buf;
  bufPtr = bufEnd = buf;
  if (limited && bufPos >= start + length) {
    return gFalse;
  }
  if (limited && bufPos + gnomeVFSStreamBufSize > start + length) {
    n = start + length - bufPos;
  } else {
    n = gnomeVFSStreamBufSize;
  }
  gnome_vfs_read(handle, buf, n, &bytesRead);
  bufEnd = buf + bytesRead;
  if (bufPtr >= bufEnd) {
    return gFalse;
  }
#ifndef NO_DECRYPTION
  if (decrypt) {
    for (p = buf; p < bufEnd; ++p) {
      *p = (char)decrypt->decryptByte((Guchar)*p);
    }
  }
#endif
  return gTrue;
}

void GnomeVFSStream::setPos(Guint pos, int dir) {
  Guint size;

  if (dir >= 0) {
    gnome_vfs_seek(handle, GNOME_VFS_SEEK_START, pos);
    bufPos = pos;
  } else {
    GnomeVFSFileSize offsetReturn;
    gnome_vfs_seek(handle, GNOME_VFS_SEEK_END, 0);
    gnome_vfs_tell(handle, &offsetReturn);
    size = (Guint)offsetReturn;
    if (pos > size)
      pos = (Guint)size;
    gnome_vfs_seek(handle, GNOME_VFS_SEEK_END, -(int)pos);
    gnome_vfs_tell(handle, &offsetReturn);
    bufPos = (Guint)offsetReturn;
  }
  bufPtr = bufEnd = buf;
}

void GnomeVFSStream::moveStart(int delta) {
  start += delta;
  bufPtr = bufEnd = buf;
  bufPos = start;
}
