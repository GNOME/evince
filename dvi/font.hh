#ifndef FONT_HH
#define FONT_HH

#include "dl-dvi-program.hh"
#include "dl-font.hh"

// Font factories

class AbstractFontFactory : public DviLib::RefCounted {
public:
    virtual DviLib::AbstractFont *create_font (std::string name, 
					       int dpi, 
					       int at_size) = 0;
};

class FontFactory : public AbstractFontFactory {
public:
    virtual DviLib::AbstractFont *create_font (std::string name, 
					       int dpi,
					       int at_size);
};

#endif
