#ifndef DL_FONT_DEFINITION_HH__
#define DL_FONT_DEFINITION_HH__

#include <string>

#include "dl-refcounted.hh"

namespace DviLib {
    
    class DviFontdefinition : public RefCounted {
    public:
	uint fontnum;
	uint checksum;
	uint at_size;           /* if 300 dpi base, 
				 * load at at_size * 300 / 1000 */
	uint design_size;
	string directory;
	string name;
    };

}    
#endif // DL_FONT_DEFINITION_HH__
