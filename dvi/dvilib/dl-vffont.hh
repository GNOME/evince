#ifndef DL_VFFONT_HH__
#define DL_VFFONT_HH__

#include "dl-dvi-file.hh"
#include "dl-dvi-fontdefinition.hh"
#include "dl-font.hh"

namespace DviLib {
    
    class VfChar : public AbstractCharacter {
    public:
	int tfm_width;
	DviProgram *program;
	int character_code;

	virtual void paint (DviRuntime& runtime)
	{
	    runtime.push();
	    program->execute (runtime); // FIXME push, pop, etc.
	    runtime.pop();
	}
	virtual int get_tfm_width () { return tfm_width; }
    };
    
    class VfFontPreamble : public RefCounted {
    public:
	string comment;
	uint checksum;
	uint design_size;
	vector <DviFontdefinition *> fontdefinitions;
    };
    
    class VfFont : public AbstractFont {
	VfFontPreamble *preamble;
	map <int, VfChar *> chars;
	int at_size;
    public:
	VfFont (AbstractLoader& l, int at_size);
	virtual VfChar *get_char (int ccode) 
	{ 
	    return chars[ccode]; 
	};
	int get_design_size () 
	{ 
	    return preamble->design_size; 
	}
	virtual int get_at_size ()
	{
	    /* FIXME (what is the correct thing to do here?) */
	    return at_size;
	}
	virtual ~VfFont () {}
    };
}
#endif /* DL_VFFONT_HH__ */
