#ifndef DL_DVI_PARSER_HH
#define DL_DVI_PARSER_HH

#include "dl-loader.hh"
#include "dl-refcounted.hh"
#include "dl-dvi-program.hh"
#include "dl-dvi-fontdefinition.hh"
#include "dl-dvi-file.hh"
#include "dl-vffont.hh"

namespace DviLib {
    
    class DviParser : public RefCounted {
	AbstractLoader& loader;
    public:
	DviParser (AbstractLoader& l) : loader (l)
	{
	};
	
	DviFontdefinition *	parse_fontdefinition	(void);
	DviProgram *		parse_program           (void);
	DviProgram *		parse_program           (uint max);
	DviPageHeader *		parse_page_header       (uint *prev_page);
	DviFilePreamble *	parse_preamble          (void);
	DviFilePostamble *	parse_postamble         (void);
	VfFontPreamble *	parse_vf_font_preamble  (void);
	VfChar *		parse_vf_char           (void);
	
	~DviParser (void) 
	{
	};
    };
}

#endif
