#ifndef PAINTER_HH
#define PAINTER_HH

#include "dl-dvi-program.hh"
#include "dl-dvi-file.hh"
#include <gdk-pixbuf/gdk-pixbuf.h>
#include "font.hh"
#include <gdk/gdk.h>
#include <cmath>
#include <list>

class AbstractDviPainter : public DviLib::DviRuntime
{
public:
    virtual void paint_bitmap (const unsigned char *data, 
			       uint width, 
			       uint height,
			       int hoffset,
			       int voffseth) = 0;
    virtual ~AbstractDviPainter () {}
};

class DviFrame : public DviLib::RefCounted
{
public:
    DviLib::DviFontMap *fontmap;
    int h, v, w, x, y, z;		// in dvi units
    DviFrame *next;
    DviFrame *copy ();
    DviLib::AbstractFont *font;
    ~DviFrame();
};

class DviPainter : public AbstractDviPainter
{
public:
    virtual void set_char (int ch);		// typeset ch, move w
    virtual void put_char (int ch);		// typeset ch, don't move
    virtual void set_rule (int height, 
			   int width);		// rule, move (height, width)
    virtual void put_rule (int height, 
			   int width);		// rule, don't move
    virtual void push (void);			// push current context
    virtual void pop (void);			// pop ccontext
    virtual void right (int len);		// move right len
    virtual void w (int len);			// move right len, set w = len
    virtual void w_rep ();			// move right w
    virtual void x (int len);			// move right len, set x = len
    virtual void x_rep ();			// move right x
    virtual void down (int len);		// move down len
    virtual void y (int len);			// move down len, set y = len
    virtual void y_rep ();			// move down y
    virtual void z (int len);			// move down len, set z = len
    virtual void z_rep ();			// move down z
    virtual void fontmap (DviLib::DviFontMap *fontmap); // set fontmap
    virtual void font_num (int font_num);	// current_font = fd
    virtual void special (string spc);		// do something special
    virtual void paint_bitmap (const unsigned char *data, 
			       uint width, 
			       uint height,
			       int voffset,
			       int hoffset);
private:
    GdkPixmap		*pixmap;
    GdkGC               *gc;
    DviLib::DviFile     *dvi_file;
    uint		 base_dpi;
    AbstractFontFactory *font_factory;

    // runtime
    DviFrame *current_frame;		// stack of DVI frames

    double scale;		// convert dvi units to pixels
    int dvi_to_pixels (int du) 
    { 
	// We add base_dpi horizontally and vertically. This
	// has the effect of adding an inch horizontally and
	// vertically. This is just how .dvi files work ...
	return (int)floor (0.5 + scale * du) + base_dpi; 
    }
    int dvi_to_pixels_no_offset (int du) 
    { 
	return (int)floor (0.5 + scale * du); 
    }
    int tfm_to_dvi (uint tfm, int at_size);
    
public:
    DviPainter (GdkPixmap		*pixmap_arg,
		GdkGC			*gc_arg,
		DviLib::DviFile		*dvi_file_arg,
		uint			 base_dpi_arg,
		AbstractFontFactory	*font_factory_arg);
    virtual ~DviPainter ();
};

#endif
