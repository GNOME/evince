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

#ifndef THUMB_H
#define THUMB_H

#include <aconf.h>

class XRef;
class GfxColorSpace;
class GfxRGB;
class GfxCMYK;
class GfxColor;

/*
 * ThumbColorMap
 */
class ThumbColorMap {

      public:
        ThumbColorMap(int bitsA, Object *obj, GfxColorSpace *csA);
        ~ThumbColorMap();
        
        GBool isOk() {return ok; };
        
        GfxColorSpace *getColorSpace() { return cs; }; 
        
        int getNumPixelComps() { return nComps; }; 
        int getBits() { return bits; }; 
        
        void getGray(Guchar *x, double *gray);
        void getRGB(Guchar *x, GfxRGB *rgb);
        void getCMYK(Guchar *x, GfxCMYK *cmyk);
        //void getColor(Guchar *x, GfxColor *color);

	static ThumbColorMap *lookupColorMap(XRef *xref, int bitsA, Object *obj, GfxColorSpace *csA); 

      private:
        GBool ok; 
        int bits;
        Stream *str;
        GfxColorSpace *cs;
        int nComps;
        int length;
        union {
                double *gray;
                GfxRGB *rgb;
                GfxCMYK *cmyk;
                GfxColor *colors; 
        };
};

/*
 * ThumbColorMaps
 */

/* FIXME: Should have a class to avoid reading same colormap for every thumb */

/*
 * Thumb
 */

class Thumb {

      public:
        Thumb(XRef *xrefA, Object *obj);
        ~Thumb();

        int getWidth(void) {return width; };
        int getHeight(void) {return height; };
        GfxColorSpace *getColorSpace(void) {return gfxCS; };
        int getBitsPerComponent(void) {return bits; };
	int getLength(void) {return length; };

	Stream *getStream() {return str; };

	unsigned char *getPixbufData();

      private:
        XRef *xref;
	Stream *str;
	GfxColorSpace *gfxCS;
        ThumbColorMap *thumbCM; 
        int width, height, bits;
	int length;
};

#endif

