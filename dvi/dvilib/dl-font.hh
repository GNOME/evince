#ifndef DL_FONT_HH__
#define DL_FONT_HH__

#include "dl-loader.hh"
#include "dl-refcounted.hh"
#include "dl-dvi-runtime.hh"

#include <vector>
#include <map>

namespace DviLib {

    class AbstractCharacter : public RefCounted {
    public:
	virtual void paint (DviRuntime &runtime) = 0;
	virtual int get_tfm_width () = 0;
    };

    class AbstractFont : public RefCounted {
    public:
	virtual int get_at_size () = 0;
	virtual int get_design_size () = 0;
	virtual AbstractCharacter *get_char (int ccode) = 0;
    };
}

#endif /* DL_PKFONT_HH__ */
