//========================================================================
//
// xpdf.cc
//
// Copyright 1996 Derek B. Noonburg
// Copyright 1999 Miguel de Icaza
//
//========================================================================


#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <gnome.h>
#include "gtypes.h"
#include "GString.h"
#include "parseargs.h"
#include "gfile.h"
#include "gmem.h"
#include "Object.h"
#include "Stream.h"
#include "Array.h"
#include "Dict.h"
#include "XRef.h"
#include "Catalog.h"
#include "Page.h"
#include "Link.h"
#include "PDFDoc.h"
#include "XOutputDev.h"
#include "PSOutputDev.h"
#include "TextOutputDev.h"
#include "Params.h"
#include "Error.h"
#include "config.h"

int
main (int argc, char *argv [])
{
	gnome_init ("GPDF", "1.0", argv, argv);

	gtk_main ();
}
