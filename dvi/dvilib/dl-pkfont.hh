#ifndef DL_PKFONT_HH__
#define DL_PKFONT_HH__

#include "dl-loader.hh"
#include "dl-refcounted.hh"
#include "dl-font.hh"

#include <vector>
#include <map>

namespace DviLib {
    
    class RleContext;
    
    enum CountType {
	RUN_COUNT,
	REPEAT_COUNT
    };
    
    class PkChar : public AbstractCharacter
    {
	uint dyn_f;
	bool first_is_black;	// if first run count is black or white
	int character_code;
	int tfm_width;          // in what units? FIXME
	uint dx;		// escapement - what is this? FIXME
	uint dy;
	uint width;		// in pixels
	uint height;		// in pixels
	int hoffset;
	int voffset;
	
	bool unpacked;
	union {
	    unsigned char *bitmap;  // 32 bit/pixel ARGB format
	    unsigned char *packed;
	} data;
	
	CountType get_count (RleContext& nr, uint *count);
	void unpack_rle (RleContext& nr);
	void unpack_bitmap (void);
	void unpack (void);

    public:
	PkChar (AbstractLoader &l);
	virtual void paint (DviRuntime &runtime);
	const unsigned char *get_bitmap (void) 
	{ 
	    if (!unpacked)
		unpack ();
	    return data.bitmap;
	}
	uint get_width (void)
	{
	    return width;
	}
	uint get_height (void)
	{
	    return height;
	}
	virtual int get_tfm_width (void)
	{
	    return tfm_width;
	}
	int get_hoffset (void) 
	{
	    return hoffset;
	}
	int get_voffset (void)
	{
	    return voffset;
	}
	int get_character_code (void) { return character_code; }
    };
    
    class PkFont : public AbstractFont {
	AbstractLoader& loader;
	uint id;
	string comment;
	uint design_size;
	uint checksum;
	uint hppp;		/* horizontal pixels per point */
	uint vppp;		/* vertical  pixels per point */
	map <uint, PkChar *> chars;
	int at_size;
	void load (void);
    public:
	PkFont (AbstractLoader& l);
	PkFont (AbstractLoader& l, int at_size);
	virtual PkChar *get_char (int ccode)
	{
	    return chars[ccode]; 
	}
	virtual int get_design_size (void)
	{
	    return design_size; 
	}
	virtual int get_at_size (void)
	{
	    return at_size;
	}
	virtual ~PkFont () {}
    };
}

#endif /* DL_PKFONT_HH__ */
