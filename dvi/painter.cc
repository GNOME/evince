#include "painter.hh"
#include "dl-dvi-fontdefinition.hh"

using DviLib::DviFontdefinition;
using DviLib::DviFontMap;
using DviLib::AbstractCharacter;

// paint a bitmap
void
DviPainter::paint_bitmap (const unsigned char *data,
			  uint width, uint height, 
			  int hoffset, int voffset)
{
    GdkPixbuf *pixbuf = 
	gdk_pixbuf_new_from_data (data,
				  GDK_COLORSPACE_RGB, 
				  TRUE,        // has_alpha,
				  8,
				  width,
				  height,
				  width * 4,  // rowstride
				  NULL,	    // destroy_fn
				  NULL);    // destroy_fn_data
    
    uint x = dvi_to_pixels (current_frame->h);
    uint y = dvi_to_pixels (current_frame->v);
    
    gdk_pixbuf_render_to_drawable (pixbuf,
				   pixmap,
				   gc,
				   0, 0,
				   x-hoffset,
				   y-voffset,
				   //y+(height - voffset),
				   width,
				   height,
				   GDK_RGB_DITHER_NONE,
				   0, 0);
}

// typeset ch, move w
void 
DviPainter::set_char (int ch)
{
    AbstractCharacter *character = current_frame->font->get_char (ch);
    character->paint (* this);
    
    int tfm_width = character->get_tfm_width ();
    int at_size = current_frame->font->get_at_size ();
    int dvi_width = tfm_to_dvi (tfm_width, at_size);
    
    current_frame->h += dvi_width;
}

// typeset ch, don't move
void 
DviPainter::put_char (int ch)
{
    AbstractCharacter *character = current_frame->font->get_char (ch);
    character->paint (* this);
}

void 
// rule, move (height, width)
DviPainter::set_rule (int height, 
		      int width)
{
    int width_p = dvi_to_pixels_no_offset (width);
    int height_p = dvi_to_pixels_no_offset (height);
    int x = dvi_to_pixels (current_frame->h);
    int y = dvi_to_pixels (current_frame->v);
    
#if 0
    cout << "BIRNAN\n" << endl;
#endif
    
    gdk_draw_rectangle (pixmap,	gc, TRUE, 
			x, y, 
			width_p + 1, height_p + 1);
    
    current_frame->h += width;
}

// rule, don't move
void 
DviPainter::put_rule (int height, 
		      int width)
{
    cout << "w h " << width << " " << height << " " << endl;
    
    int width_p = dvi_to_pixels_no_offset (width);
    int height_p = dvi_to_pixels_no_offset (height);
    int x = dvi_to_pixels (current_frame->h);
    int y = dvi_to_pixels (current_frame->v);
    
    cout << "EMFLE\n" << endl;
    
    cout << "x y h w " << x << " " << y << " " << height_p << " " 
	 << width_p << endl;
    
    gdk_draw_rectangle (pixmap,	gc, TRUE, 
			x, y, 
			width_p + 1, height_p + 1);
}

// push current context
DviFrame *
DviFrame::copy (void)
{
    DviFrame *frame = new DviFrame ();

    frame->fontmap = this->fontmap;
    if (frame->fontmap)
	frame->fontmap->ref();
    frame->h = this->h;
    frame->v = this->v;
    frame->w = this->w;
    frame->x = this->x;
    frame->y = this->y;
    frame->z = this->z;
    frame->font = this->font;
    if (frame->font)
	frame->font->ref();
    
    return frame;
}

DviFrame::~DviFrame()
{
    if (this->fontmap)
	this->fontmap->unref();
    if (this->font)
	this->font->unref();
}

void 
DviPainter::push (void)
{
    DviFrame *new_frame = current_frame->copy();
    new_frame->next = current_frame;
    current_frame = new_frame;
    if (current_frame->font)
	cout << "push: there is a font" << endl;
    else
	cout << "push: there is not a font" << endl;
}

// pop ccontext
void 
DviPainter::pop (void)
{
    DviFrame *old_frame = current_frame;

    // FIXME: dvi assumes that fonts survive pops
    // FIXME: however, do they also survive the final pop of a vfchar?
    
    current_frame = current_frame->next;

    if (current_frame && current_frame->font)
	cout << "pop: there is a font" << endl;
    else if (current_frame)
	cout << "pop: there is not font" << endl;
    else
	cout << "no current" << endl;
    
    old_frame->unref();
}

// move right len
void 
DviPainter::right (int len)
{
    current_frame->h += len;
}

// move right len, set w = len
void 
DviPainter::w (int len)
{
    right (len);
    current_frame->w = len;
}

// move right w
void 
DviPainter::w_rep ()
{
    right (current_frame->w);
}

// move right len, set x = len
void 
DviPainter::x (int len)
{
    right (len);
    current_frame->x = len;
}

// move right x
void 
DviPainter::x_rep ()
{
    right (current_frame->x);
}

// move down len
void 
DviPainter::down (int len)
{
    current_frame->v += len;
}

// move down len, set y = len
void 
DviPainter::y (int len)
{
    down (len);
    current_frame->y = len;
}

// move down y
void 
DviPainter::y_rep ()
{
    down (current_frame->y);
}

// move down len, set z = len
void 
DviPainter::z (int len)
{
    down (len);
    current_frame->z = len;
}

// move down z
void 
DviPainter::z_rep ()
{
    down (current_frame->z);
}

// f = font_num 
void 
DviPainter::font_num (int font_num)
{
    DviFontdefinition *fd = current_frame->fontmap->get_fontdefinition (font_num);

    g_assert (fd);
    if (fd)
    {
	// gtkdvi:
	int dpi = (int)floor( 0.5 + 1.0 * base_dpi * 
			      dvi_file->get_magnification() * fd->at_size /
			      ( 1000.0 * fd->design_size));

	cout << fd->name << " design size: " << fd->design_size << " at size " << fd->at_size << endl;
	
	if (current_frame->font)
	    current_frame->font->unref();
	
	current_frame->font = 
	    font_factory->create_font (fd->name, dpi, fd->at_size);

	g_assert (current_frame->font);
	cout << "there is now a font"<< endl;
    
    }
}

// do something special
void 
DviPainter::special (string spc)
{
    cout << "warning: special " << spc << " " << "not handled" << endl;
}

int 
DviPainter::tfm_to_dvi (uint tfm, int at_size)
{
    // this is from gtkdvi:
    int alpha, z, beta, b0, b1, b2, b3, r;
    
    alpha = 16;
    z = at_size;
    while (z >= (1<<23))
    {
	z >>= 1;
	alpha <<= 1;
    }
    beta = 256/alpha;
    alpha *= z;
    
#if 0
    b0 = tfm & (0xFF << 24);
    b1 = tfm & (0xFF << 16);
    b2 = tfm & (0xFF << 8);
    b3 = tfm & (0xFF << 0);
#endif
    
    b0 = tfm >> 24;  
    b1 = (tfm >> 16) & 255;
    b2 = (tfm >> 8) & 255;
    b3 = tfm & 255;
    
#if 0
    r = (((((b3 * z) / 256) + (b2 * z)) / 256) + (b1 * z))/beta;
#endif
    
    b1 *= z;
    b2 *= z;
    b3 *= z;
    
    r = (((b3 / 256 + b2) / 256) + b1) / beta;
    
    if (b0 > 0)
    {
	if ((b0 > 0) != (tfm > 0))
	    cout << "b0: " << b0 << "tfm: " << tfm << endl;
	r -= alpha;
    }
    
    return r;
}

DviPainter::DviPainter (GdkPixmap              *pixmap_arg,
			 GdkGC		       *gc_arg,
			 DviLib::DviFile       *dvi_file_arg,
			 uint			base_dpi_arg,
			 AbstractFontFactory   *font_factory_arg)
{
    pixmap		= (GdkPixmap *)g_object_ref (pixmap_arg);
    gc			= (GdkGC *)g_object_ref (gc_arg);
    dvi_file		= dvi_file_arg;
    base_dpi		= base_dpi_arg;
    font_factory	= font_factory_arg;

    dvi_file->ref();
    font_factory->ref();
    
    current_frame = new DviFrame;
    current_frame->h = 0;
    current_frame->v = 0;
    current_frame->w = 0;
    current_frame->x = 0;
    current_frame->y = 0;
    current_frame->z = 0;
    current_frame->fontmap = NULL;
    current_frame->font = NULL;
    
    // from gtkdvi:
    scale =  dvi_file->get_numerator() / 254000.0;
    scale *= 1.0 * base_dpi / dvi_file->get_denominator ();
    scale *= dvi_file->get_magnification () / 1000.0;
}

DviPainter::~DviPainter ()
{
    g_object_unref (pixmap);
    g_object_unref (gc);
    dvi_file->unref();
    font_factory->unref();
    while (current_frame)
	pop();
}

void
DviPainter::fontmap (DviFontMap *fontmap)
{
    fontmap->ref();

    if (current_frame->fontmap)
	current_frame->fontmap->unref();

    current_frame->fontmap = fontmap;
}
