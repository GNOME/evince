#ifndef DL_DVI_FILE_HH
#define DL_DVI_FILE_HH

#include "dl-dvi-program.hh"
#include <map>
#include <list>
#include <algorithm>
#include "dl-dvi-fontdefinition.hh"
#include "dl-loader.hh"

namespace DviLib
{
    const uint N_PAGE_COUNTERS = 10;          // \count0 ... \count9
    
    class DviPageHeader : public RefCounted
    {
    public:
	int count[N_PAGE_COUNTERS];
	uint address;          // address of this page, not the preceding
    };
    
    class DviPage : public AbstractDviProgram
    { 
	DviProgram& program;
	DviFontMap *fontmap;
	int count[N_PAGE_COUNTERS];	   // \count0 ... \count9
    public:
	DviPage (DviProgram& p, int c[N_PAGE_COUNTERS], DviFontMap *fontmap) :
	    program (p)
	{
	    this->fontmap = fontmap;
	    this->fontmap->ref();
	    for (uint i = 0; i < N_PAGE_COUNTERS; ++i)
		count[i] = c[i];
	}
	virtual void execute (DviRuntime& runtime)
	{
	    runtime.push();
	    runtime.fontmap (fontmap);
	    cout << "page " << (int)fontmap << endl;
	    program.execute (runtime);
	    runtime.pop();
	}
	int get_page_count (int i) { return count[i]; }
    };
    
    enum DviType
    {
	NORMAL_DVI  = 2, // FIXME: this should be 2
	TEX_XET_DVI = 2  // FIXME: is this correct?
    };
    
    class DviFilePreamble : public RefCounted
    {
    public:
	// preamble
	DviType type;
	uint magnification;
	uint numerator;
	uint denominator;
	string comment;
    };
    
    class DviFilePostamble : public RefCounted
    {
    public:
	// postamble
	DviType type;
	uint magnification;
	uint numerator;
	uint denominator;
	
	uint last_page_address;
	uint max_height;
	uint max_width;
	uint stack_height;

	DviFontMap *fontmap;
    };
    
    class DviFile : public RefCounted
    {
	AbstractLoader &loader;
	
	DviFilePreamble *preamble;
	DviFilePostamble *postamble;
	
	uint n_pages;
	map <uint, DviPageHeader *> page_headers;
	map <uint, DviPage *> pages;
	
    public:
	DviFile (AbstractLoader& l);
	DviPage *get_page (uint n);	/* unref it when done */
	~DviFile (void) {}
	uint get_n_pages () { return n_pages; }
	DviFontdefinition *get_fontdefinition (uint n) 
	{
	    return postamble->fontmap->get_fontdefinition (n);
	}
	uint get_numerator () { return postamble->numerator; }
	uint get_denominator () { return postamble->denominator; }
	uint get_magnification () { return postamble->magnification; }
    };
    
}    
#endif // DL_DVI_FILE_HH

