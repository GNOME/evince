/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-indent-level: 8; c-basic-offset: 8 -*- */
/* 
 *  Copyright (C) 2003 Remi Cohen-Scali
 *
 *  Author:
 *    Remi Cohen-Scali <Remi@Cohen-Scali.com>
 *
 * GPdf is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * GPdf is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU General Public
 * License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#include <string.h>

#ifdef USE_GCC_PRAGMAS
#pragma implementation
#endif

#include <glib.h>
#include <goo/gmem.h>
#include <Object.h>
#include <Gfx.h>
#include <GfxState.h>
#include "Thumb.h"

/*
 * Thumb
 */

Thumb::Thumb(XRef *xrefA, Object *obj) :
  xref(xrefA),
  str(NULL),
  ok_flag(0)
{
	Object obj1, obj2;
	Dict *dict;
	GfxColorSpace *colorSpace;

	do {
		/* Get stream dict */
		dict = obj->streamGetDict ();
                str = obj->getStream(); 
		
		/* Get width */
		dict->lookup ("Width", &obj1);
		if (obj1.isNull ())
		{
			obj1.free ();
			dict->lookup ("W", &obj1);
		}
		if (!obj1.isInt ()) {
			fprintf (stderr, "Error: Invalid Width object %s\n",
				obj1.getTypeName ());
			obj1.free ();
			break;
		}
		width = obj1.getInt ();
		obj1.free ();
		
		/* Get heigth */
		dict->lookup ("Height", &obj1);
		if (obj1.isNull ()) 
		{
			obj1.free ();
			dict->lookup ("H", &obj1);
		}
		if (!obj1.isInt ()) {
			fprintf (stderr, "Error: Invalid Height object %s\n",
				obj1.getTypeName ());
			obj1.free ();
			break;
		}
		height = obj1.getInt ();
		obj1.free ();
		
		/* bit depth */
		dict->lookup ("BitsPerComponent", &obj1);
		if (obj1.isNull ())
		{
			obj1.free ();
			dict->lookup ("BPC", &obj1);
		}
		if (!obj1.isInt ()) {
			fprintf (stderr, "Error: Invalid BitsPerComponent object %s\n",
				obj1.getTypeName ());
			obj1.free ();
			break;
		}
		bits = obj1.getInt ();
		obj1.free ();
		
		/* Get color space */
		dict->lookup ("ColorSpace", &obj1);
		if (obj1.isNull ()) 
		{
			obj1.free ();
			dict->lookup ("CS", &obj1);
		}
		colorSpace = GfxColorSpace::parse(&obj1);
		obj1.free();
		if (!colorSpace) {
			fprintf (stderr, "Error: Cannot parse color space\n");
			break;
		}

		dict->lookup("Decode", &obj1);
		if (obj1.isNull()) {
			obj1.free();
			dict->lookup("D", &obj1);
		}
		colorMap = new GfxImageColorMap(bits, &obj1, colorSpace);
		obj1.free();
		if (!colorMap->isOk()) {
			fprintf (stderr, "Error: invalid colormap\n");
			delete colorMap;
			colorMap = NULL;
		}

		dict->lookup ("Length", &obj1);
		if (!obj1.isInt ()) {
			fprintf (stderr, "Error: Invalid Length Object %s\n",
				obj1.getTypeName ());
			obj1.free ();
			break;
		}
		length = obj1.getInt ();
		obj1.free ();

		str->addFilters(obj);

		ok_flag = 1; 
	}
	while (0);	
}

unsigned char *
Thumb::getPixbufData()
{
	ImageStream *imgstr;
	unsigned char *pixbufdata;
	unsigned int pixbufdatasize;
	int row, col;
	unsigned char *p;

	/* RGB Pixbuf data */
	pixbufdatasize = width * height * 3;
	if (colorMap) {
		pixbufdata =(unsigned char *)g_malloc(pixbufdatasize);
	} else {
		pixbufdata =(unsigned char *)g_malloc0(pixbufdatasize);
		return pixbufdata;
	}

	p = pixbufdata;

	imgstr = new ImageStream(str, width, colorMap->getNumPixelComps(), colorMap->getBits());
	imgstr->reset();
	for (row = 0; row < height; ++row) {
	    for (col = 0; col < width; ++col) {
		Guchar pix[gfxColorMaxComps];
		GfxRGB rgb;

		imgstr->getPixel(pix);
		colorMap->getRGB(pix, &rgb);

		*p++ = (guchar)(rgb.r * 255.99999);
		*p++ = (guchar)(rgb.g * 255.99999);
		*p++ = (guchar)(rgb.b * 255.99999);
	    }
	}
	delete imgstr;

	return pixbufdata;
}

Thumb::~Thumb() {
        delete colorMap;
        delete str;
}

