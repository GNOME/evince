//========================================================================
//
// BonoboFile.h
//
// Copyright 1999 Derek B. Noonburg assigned by Michael Meeks.
//
//========================================================================

#ifndef BONOBOFILE_H
#define BONOBOFILE_H

extern "C" {
#define GString G_String
#include <gnome.h>
#include <libgnorba/gnorba.h>
#include <bonobo/gnome-bonobo.h>
#undef GString
}

typedef GNOME_Stream BaseFile;

/* The open/close is done for you by the Bonobo server */

extern void bfclose (BaseFile file);
extern size_t bfread (void *ptr, size_t size, size_t nmemb, BaseFile file);
extern int bfseek (BaseFile file, long offset, int whence);
extern void brewind (BaseFile file);
extern long bftell (BaseFile file);

#endif /* BONOBOFILE_H */
