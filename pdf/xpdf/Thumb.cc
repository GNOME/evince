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

#include <aconf.h>
#include <string.h>

#ifdef USE_GCC_PRAGMAS
#pragma implementation
#endif

#include <gpdf-g-switch.h>
#  include <glib.h>
#include <gpdf-g-switch.h>
#include "gmem.h"
#include "Object.h"
#include "Gfx.h"
#include "GfxState.h"
#include "Thumb.h"

static GHashTable *cmhash = NULL;

/*
 * ThumbColorMap
 */
ThumbColorMap::ThumbColorMap(int bitsA,
                             Object *obj,
                             GfxColorSpace *csA) :
  bits(bitsA),
  str(NULL),
  length(0), 
  cs(csA)
{
        Object obj1, obj2; 
        GfxIndexedColorSpace *iCS;
        GfxSeparationColorSpace *sepCS;
        int maxPixel, indexHigh;
        Dict *streamDict;
        int n;
        int baseNComps; 
        /* LZW params */
        int colors = 0, early = 0;
        /* CCITTFax params */
        int encoding = 0, rows = 0;
        GBool eol = gFalse, byteAlign = gFalse, eob = gFalse, black = gFalse;
        /* Common params */
        int pred = 0, cols = 0; 

        ok = gFalse;
        maxPixel = (1 << bits) - 1;

        do {
                if (!obj->isStream ()) {
                        printf ("Error: Invalid object of type %s\n",
                                obj->getTypeName ()); 
                        break; 
                }
                str = obj->getStream(); 

                streamDict = obj->streamGetDict ();

                streamDict->lookupNF ("Filter", &obj1);
                if (!obj1.isArray ()) {
                        printf ("Error: Invalid filter object of type %s\n",
                                obj1.getTypeName ()); 
                        break;                         
                }

                str = str->addFilters(obj);

		streamDict->lookup ("Length", &obj1);
		if (obj1.isNull ())
		{
			printf ("Error: No Length object\n"); 
                        break; 
		}
		if (!obj1.isInt ()) {
			printf ("Error: Invalid Width object %s\n",
				obj1.getTypeName ());
			obj1.free ();
			break;
		}
		length = obj1.getInt ();
		obj1.free ();

                nComps = cs->getNComps();

                if (cs->getMode () == csIndexed) {
                        iCS = (GfxIndexedColorSpace *)cs;
                        baseNComps = iCS->getBase ()->getNComps ();
                        str->reset(); 
                        if (iCS->getBase ()->getMode () == csDeviceGray) {
                                gray = (double *)gmalloc(sizeof(double) * (iCS->getIndexHigh () + 1));
                                for (n = 0; n <= iCS->getIndexHigh (); n++) {
                                        double comp = (double)str->getChar(); 
                                        //printf ("Gray pixel [%03d] = %02x\n", n, (int)comp); 
                                        gray[n] = comp / (double)iCS->getIndexHigh (); 
                                }
                        }
                        else if (iCS->getBase ()->getMode () == csDeviceRGB) {
                                rgb = (GfxRGB *)gmalloc(sizeof(GfxRGB) * (iCS->getIndexHigh () + 1));
                                for (n = 0; n <= iCS->getIndexHigh (); n++) {
                                        double comp_r = (double)str->getChar(); 
                                        double comp_g = (double)str->getChar(); 
                                        double comp_b = (double)str->getChar(); 
//                                        printf ("RGB pixel [0x%02x] = (%02x,%02x,%02x)\n",
//                                                n, (int)comp_r, (int)comp_g, (int)comp_b); 
                                        rgb[n].r = comp_r / (double)iCS->getIndexHigh (); 
                                        rgb[n].g = comp_g / (double)iCS->getIndexHigh (); 
                                        rgb[n].b = comp_b / (double)iCS->getIndexHigh (); 
                                }
                        }
                        else if (iCS->getBase ()->getMode () == csDeviceCMYK) {
                                cmyk = (GfxCMYK *)gmalloc(sizeof(GfxCMYK) * (iCS->getIndexHigh () + 1));
                                for (n = 0; n <= iCS->getIndexHigh (); n++) {
                                        double comp_c = (double)str->getChar(); 
                                        double comp_m = (double)str->getChar(); 
                                        double comp_y = (double)str->getChar(); 
                                        double comp_k = (double)str->getChar(); 
                                        //printf ("CMYK pixel [%03d] = (%02x,%02x,%02x,%02x)\n",
                                        //        n, (int)comp_c, (int)comp_m, (int)comp_y, (int)comp_k); 
                                        cmyk[n].c = comp_c / (double)iCS->getIndexHigh (); 
                                        cmyk[n].m = comp_m / (double)iCS->getIndexHigh (); 
                                        cmyk[n].y = comp_y / (double)iCS->getIndexHigh (); 
                                        cmyk[n].k = comp_k / (double)iCS->getIndexHigh (); 
                                }
                        }
                }
                else if (cs->getMode () == csSeparation) {
                        sepCS = (GfxSeparationColorSpace *)cs; 
                        /* FIXME: still to do */
                }

                ok = gTrue;
        }
        while (0); 
}

ThumbColorMap *
ThumbColorMap::lookupColorMap(XRef *xref, int bits, Object *obj, GfxColorSpace *cs)
{
	Object obj1; 
	ThumbColorMap *cm; 
	gchar *key;
	
	if (!cmhash)
		cmhash = g_hash_table_new(NULL, g_int_equal); 

	key = g_strdup_printf ("%d %d R", obj->getRefNum (), obj->getRefGen ());

	if (!(cm = (ThumbColorMap *)g_hash_table_lookup (cmhash, &key))) {
		cm = new ThumbColorMap(bits, obj->fetch(xref, &obj1), cs);
		obj1.free(); 
		g_hash_table_insert(cmhash, &key, cm); 
	}

	g_free (key); 

	return cm; 
}

void
ThumbColorMap::getGray(Guchar *x, double *outgray)
{
	*outgray = gray[*x];
}

void
ThumbColorMap::getRGB(Guchar *x, GfxRGB *outrgb)
{
	outrgb->r = rgb[*x].r;
	outrgb->g = rgb[*x].g;
	outrgb->b = rgb[*x].b;
}

void
ThumbColorMap::getCMYK(Guchar *x, GfxCMYK *outcmyk)
{
	outcmyk->c = cmyk[*x].c;
	outcmyk->m = cmyk[*x].m;
	outcmyk->y = cmyk[*x].y;
	outcmyk->k = cmyk[*x].k;
}

ThumbColorMap::~ThumbColorMap()
{
        delete str;
        gfree((void *)gray); 
}

/*
 * Thumb
 */

Thumb::Thumb(XRef *xrefA, Object *obj) :
  xref(xrefA),
  str(NULL)
{
	Object obj1, obj2;
	Dict *dict;
	unsigned int dsize;
	int row, col, comp;

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
			printf ("Error: Invalid Width object %s\n",
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
			printf ("Error: Invalid Height object %s\n",
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
			printf ("Error: Invalid BitsPerComponent object %s\n",
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
		if (!(gfxCS = GfxColorSpace::parse (&obj1)))
		{
			printf ("Error: Cannot parse color space\n");
			obj1.free ();
			break;
		}
		if (gfxCS->getMode () == csIndexed)			
			thumbCM = ThumbColorMap::lookupColorMap (xref, bits, obj1.arrayGetNF(3, &obj2), gfxCS);
		else if (gfxCS->getMode () == csSeparation)
			printf ("Not yet implemented\n");
		  
		
		dict->lookup ("Length", &obj1);
		if (!obj1.isInt ()) {
			printf ("Error: Invalid Length Object %s\n",
				obj1.getTypeName ());
			obj1.free ();
			break;
		}
		length = obj1.getInt ();
		obj1.free ();

		str->addFilters(obj);
	}
	while (0);	
}

unsigned char *
Thumb::getPixbufData()
{
	ImageStream *imgstr;
	unsigned char *pixbufdata;
	unsigned int pixbufdatasize;
	int row, col, i;
	unsigned char *p;

	/* RGB Pixbuf data */
	pixbufdatasize = width * height * 3;
	pixbufdata =(unsigned char *)g_malloc(pixbufdatasize);
	p = pixbufdata;

	imgstr = new ImageStream(str, width, thumbCM->getNumPixelComps(), thumbCM->getBits());
	imgstr->reset();
	for (row = 0; row < height; ++row) {
	    for (col = 0; col < width; ++col) {
		Guchar pix[gfxColorMaxComps];
		GfxRGB rgb;

		imgstr->getPixel(pix);
		thumbCM->getRGB(pix, &rgb);

		*p++ = (guchar)(rgb.r * 255.99999);
		*p++ = (guchar)(rgb.g * 255.99999);
		*p++ = (guchar)(rgb.b * 255.99999);
	    }
	}
	delete imgstr;

	return pixbufdata;
}

Thumb::~Thumb() {
        delete thumbCM;
        delete str;
}

