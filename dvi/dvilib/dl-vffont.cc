#include "dl-vffont.hh"
#include "dl-dvi-parser.hh"

using namespace DviLib;

VfFont::VfFont (AbstractLoader &l, int at_size_arg) :
    at_size (at_size_arg)
{
    DviParser parser (l);
    preamble = parser.parse_vf_font_preamble ();

    VfChar *ch;
    while ((ch = parser.parse_vf_char ()) != NULL)
	chars[ch->character_code] = ch;
}

