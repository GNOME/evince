#include "dl-vffont.hh"
#include "dl-dvi-parser.hh"

using namespace DviLib;

void
VfFont::fixup_fontmap (DviFontMap *fontmap)
{
    typedef std::map<int, DviFontdefinition *>::iterator It;
    for (It i = fontmap->fontmap.begin(); i != fontmap->fontmap.end(); ++i)
    {
	(*i).second->at_size = ((*i).second->at_size / 1048576.0) * preamble->design_size;
#if 0
	(*i).second->design_size = 1048576;
#endif
    }
}

VfFont::VfFont (AbstractLoader &l,
		int at_size_arg)
{
    at_size = at_size_arg;
    DviParser parser (l);
    preamble = parser.parse_vf_font_preamble();
    
    VfChar *ch;
    while ((ch = parser.parse_vf_char()) != NULL)
    {
	chars[ch->character_code] = ch;
	ch->fontmap = preamble->fontmap;
    }

    /* fixup fontmap
     *
     * FIXME: I don't think this is correct, but vftovp.web isn't
     * totally clear on the issue
     */

    fixup_fontmap (preamble->fontmap);
}

