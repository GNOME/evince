#include "dl-dvi-fontdefinition.hh"

#include <iostream>

using namespace DviLib;

DviFontdefinition *
DviFontMap::get_fontdefinition (int fontnum)
{
    cout << "getting fontnum " << fontnum << endl;
    return fontmap[fontnum];
}

void
DviFontMap::set_fontdefinition (int fontnum,
				DviFontdefinition *fd)
{
    fd->ref();

    
    cout << "froot " << fontnum << (int)this << endl;
    
    if (fontmap[fontnum])
    {
	cout << "blah" << endl;
	fontmap[fontnum]->unref();
    }
    
    fontmap[fontnum] = fd;
}

DviFontMap::~DviFontMap ()
{
    typedef map <int, DviFontdefinition *>::iterator It;

    for (It i = fontmap.begin(); i != fontmap.end(); ++i)
    {
	(*i).second->unref();
    }
}
