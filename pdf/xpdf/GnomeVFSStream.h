//========================================================================
//
// GnomeVFSStream.cc
//
// Copyright 1996-2002 Glyph & Cog, LLC
// Copyright 2003 Martin Kretzschmar
//
//========================================================================

#ifndef GNOME_VFS_STREAM_H
#define GNOME_VFS_STREAM_H

#include "gpdf-g-switch.h"
#  include <libgnomevfs/gnome-vfs-handle.h>
#include "gpdf-g-switch.h"
#include "Object.h"
#include "Stream.h"

#define gnomeVFSStreamBufSize fileStreamBufSize

class GnomeVFSStream: public BaseStream {
public:

  GnomeVFSStream(GnomeVFSHandle *handleA, Guint startA, GBool limitedA,
                 Guint lengthA, Object *dictA);
  virtual ~GnomeVFSStream();
  virtual Stream *makeSubStream(Guint startA, GBool limitedA,
				Guint lengthA, Object *dictA);
  virtual StreamKind getKind() { return strFile; }
  virtual void reset();
  virtual void close();
  virtual int getChar()
    { return (bufPtr >= bufEnd && !fillBuf()) ? EOF : (*bufPtr++ & 0xff); }
  virtual int lookChar()
    { return (bufPtr >= bufEnd && !fillBuf()) ? EOF : (*bufPtr & 0xff); }
  virtual int getPos() { return bufPos + (bufPtr - buf); }
  virtual void setPos(Guint pos, int dir = 0);
  virtual GBool isBinary(GBool last = gTrue) { return last; }
  virtual Guint getStart() { return start; }
  virtual void moveStart(int delta);

private:

  GBool fillBuf();

  GnomeVFSHandle *handle;
  Guint start;
  GBool limited;
  Guint length;
  char buf[gnomeVFSStreamBufSize];
  char *bufPtr;
  char *bufEnd;
  Guint bufPos;
  int savePos;
  GBool saved;
};

#endif /* GNOME_VFS_STREAM_H */
