#ifndef DL_FONT_DEFINITION_HH__
#define DL_FONT_DEFINITION_HH__

#include <string>
#include <map>

#include "dl-refcounted.hh"

namespace DviLib {

    class DviFontdefinition : public RefCounted
    {
    public:
	uint fontnum;
	uint checksum;
	uint at_size;           /* if 300 dpi base, 
				 * load at at_size * 300 / 1000 */
	uint design_size;
	string directory;
	string name;
    };

    class DviFontMap : public RefCounted
    {
    public:
	std::map <int, DviFontdefinition *> fontmap;
	DviFontdefinition *get_fontdefinition (int fontnum);
	void set_fontdefinition (int fontnum, DviFontdefinition *fd);
	DviFontMap::~DviFontMap ();
    };
}    
#endif // DL_FONT_DEFINITION_HH__
