#ifndef DL_DVI_STACK_HH
#define DL_DVI_STACK_HH

#include "dl-refcounted.hh"
#include "dl-dvi-fontdefinition.hh"
#include <string>
#include <map>

namespace DviLib {
    class DviRuntime : public RefCounted
    {
    public:
	virtual void set_char (int ch) = 0;		// typeset ch, move w
	virtual void put_char (int ch) = 0;		// typeset ch, don't move
	virtual void set_rule (int height, 
			       int width) = 0;		// rule, move (height, width)
	virtual void put_rule (int height, 
			       int width) = 0;		// rule, don't move
	virtual void push (void) = 0;			// push current context
	virtual void pop (void) = 0;			// pop ccontext
	virtual void right (int len) = 0;		// move right len
	virtual void w (int len) = 0;			// move right len, set w = len
	virtual void w_rep () = 0;			// move right w
	virtual void x (int len) = 0;			// move right len, set x = len
	virtual void x_rep () = 0;			// move right x
	virtual void down (int len) = 0;		// move down len
	virtual void y (int len) = 0;			// move down len, set y = len
	virtual void y_rep () = 0;			// move down y
	virtual void z (int len) = 0;			// move down len, set z = len
	virtual void z_rep () = 0;			// move down z	
	virtual void push_fontmap (std::map<int, DviFontdefinition *> fontmap) = 0;
	virtual void font_num (int num) = 0;		// f = num
	virtual void special (std::string spc) = 0;	// do something special
	
	virtual void paint_bitmap (const unsigned char *data, 
				   uint width, 
				   uint height,
				   int hoffset,
				   int voffseth) = 0;
	
	virtual ~DviRuntime () {};
    };
}

#endif
