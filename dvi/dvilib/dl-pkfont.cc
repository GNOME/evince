#include "dl-pkfont.hh"
#include <algorithm>
#include <iostream>

using namespace DviLib;

enum PkOpcode {
    DL_PK_FIRST_COMMAND = 240,
    DL_PK_XXX1 = 240,
    DL_PK_XXX2,
    DL_PK_XXX3,
    DL_PK_XXX4,
    DL_PK_YYY,
    DL_PK_POST,
    DL_PK_NOP,
    DL_PK_PRE
};

enum Color {
    BLACK,
    WHITE
};

class Bitmap {
    uint *data;
    uint width;
public:
    Bitmap (uint width_arg, uint height)
    {
	width = width_arg;
	data = new uint [width * height];
	std::fill (data, data + width * height, 0x00000000);
    }
    uchar *steal_pixels (void)
    {
	uchar *r = (uchar *)data;
	data = 0;
	return r;
    }
    ~Bitmap () 
    { 
    }
    void fill_black (uint index, uint len)
    {
#if 0
	cout << "filling: " << len << endl;
#endif
	std::fill (data + index,
		   data + index + len,
		   0xff000000);
    }
    void copy (uint src_index, uint len, uint dest_index)
    {
	std::copy (data + src_index, 
		   data + (src_index + len),
		   data + dest_index);
    }
    uint x (uint index)
    {
	return index % width;
    }
#if 0
    uint y (uint index)
    {
	return (index * 4) / (width * 4);
    }
#endif
    bool pixel (uint index) { 
	return *(data + index) == 0xff000000;
    }
};

class DviLib::RleContext {
    uchar *data;
    bool first;
    uchar nyb0 (uchar x) { return x >> 4; };
    uchar nyb1 (uchar x) { return x & 15; };
    Bitmap& bitmap;
public:
    uint index;
    Color color;
    RleContext (uchar *data_arg,
		Color color_arg,
		Bitmap &bitmap_arg) :
	data (data_arg),
	first (true),
	bitmap (bitmap_arg),
	
	index (0),
	color (color_arg)
    { }
    uchar get_nybble (void)
    {
	if (first)
	{
	    first = false;
	    return nyb0 (*data);
	}
	else
	{
	    first = true;
	    return nyb1 (*data++);
	}
    }
    Bitmap& get_bitmap () { return bitmap; }
};

inline CountType
PkChar::get_count (RleContext& nr, uint *count)
{
    CountType result = RUN_COUNT;
    uint i;
    
    i = nr.get_nybble();
    if (i == 15)
    {
	*count = 1;
	return REPEAT_COUNT;
    }
    else if (i == 14)
    {
	result = REPEAT_COUNT;
	i = nr.get_nybble();
    }
    switch (i) {
    case 15: case 14:
	throw string ("Duplicated repeat count");
	break;
    case 0:
	for (i = 1; (*count = nr.get_nybble()) == 0; ++i)
	    ;
	while (i-- > 0)
	    *count = (*count << 4) + nr.get_nybble();
	*count = *count - 15 + (13 - dyn_f)*16 + dyn_f;
	break;
    default:
	if (i <= dyn_f)
	    *count = i;
	else
	    *count = (i - dyn_f - 1)*16 + nr.get_nybble() + dyn_f + 1;
	break;
    }
    return result;
}

void
PkChar::unpack_rle (RleContext& nr)
{
    uint count;
    
    while (nr.index < height * width)
    {
	CountType count_type = get_count (nr, &count);
	Bitmap& bm = nr.get_bitmap ();
	
	switch (count_type) {
	case RUN_COUNT:
	    if (nr.color == BLACK)		    
	    {
		bm.fill_black (nr.index, count);
		nr.color = WHITE;
	    }
	    else
		nr.color = BLACK;
	    nr.index += count;
	    break;
	case REPEAT_COUNT:
	    uint temp = nr.index;
	    
	    nr.index += count * width;
	    unpack_rle (nr);
	    
	    uint x = bm.x(temp);
	    
	    if (bm.pixel (temp - x))
		bm.fill_black (temp + count * width - x, x);
	    
	    for (uint i = 0; i < count; ++i)
		bm.copy (temp + count * width - x, width, 
			 temp - x + i * width);
	    return;
	    break;
	}
    }
}

void
PkChar::unpack_bitmap (void)
{
    uint i, weight;
    
    uint *bitmap 
	= new uint [width * height];
    fill (bitmap, bitmap + width * height, 0x00000000);
    
    weight = 128;
    
    for (i=0; i < height * width; i++)
    {
	if ((*data.packed & weight) != 0)
	{
	    bitmap[i] = 0xff000000;
	}

	if (weight == 1)
	{
	    weight = 128;
	    data.packed++;
	}
	else
	{
	    weight >>= 1;
	}
    }
    
    data.bitmap = (uchar *)bitmap;
}

void
PkChar::unpack (void)
{
    if (unpacked)
	return;
    
    if (dyn_f == 14)
    {
	unpack_bitmap();
    }
    else
    {
	Bitmap bitmap (width, height);
	
	RleContext nr (data.packed, 
		       first_is_black? BLACK : WHITE,
		       bitmap);
	unpack_rle (nr);
	data.bitmap = bitmap.steal_pixels ();

#if 0
	if (character_code == '6')
	    cout << "HERER" << endl;
	else
	    cout << '[' << character_code << " not " << (int)'6' << ']' << endl;
#endif
    }
    
    unpacked = true;
}

PkChar::PkChar (AbstractLoader &loader)
{
    uint flag_byte = loader.get_uint8 ();
    
    dyn_f = flag_byte >> 4;
    if (dyn_f == 15)
	throw string ("Corrupt .pk file");
    
    first_is_black = (flag_byte & 8)? true : false;
    
    uint length = 0; // to quiet gcc

    switch (flag_byte & 7)
    {
    case 0: case 1: case 2: case 3:
	/* short preamble */
	length = loader.get_uint8 () + ((flag_byte & 3) << 8) - 8;
	character_code = loader.get_uint8 ();
	tfm_width = loader.get_uint24 ();
	dx = loader.get_uint8 () << 16;
	dy = 0;
	width = loader.get_uint8 ();
	height = loader.get_uint8 ();
	hoffset = loader.get_int8 ();
	voffset = loader.get_int8 ();
	break;

    case 4: case 5: case 6:
	/* extended short preamble */
	length = loader.get_uint16 () + ((flag_byte & 3) << 16) - 13;
#if 0
	cout << length;
#endif
	character_code = loader.get_uint8 ();
#if 0
	cout << ',' << character_code;
#endif
	tfm_width = loader.get_uint24 ();
	dx = loader.get_uint16 () << 16;
	dy = 0;
	width = loader.get_uint16 ();
	height = loader.get_uint16 ();
	hoffset = loader.get_int16 ();
	voffset = loader.get_int16 ();
	break;

    case 7:
	/* long preamble */
	length = loader.get_int32 () - 28;
	character_code = loader.get_int32 ();
	tfm_width = loader.get_int32 ();
	dx = loader.get_int32 ();
	dy = loader.get_int32 ();
	width = loader.get_int32 ();
	height = loader.get_int32 ();
	hoffset = loader.get_int32 ();
	voffset = loader.get_int32 ();
	break;

    default:
	/* should not be reached */
	break;
    }
    unpacked = false;
    data.packed = new uchar[length];
    loader.get_n (length, data.packed);
}

void
PkChar::paint (DviRuntime &runtime) 
{
    const unsigned char *bitmap;
    bitmap = get_bitmap ();
    runtime.paint_bitmap (bitmap, 
			  get_width(), get_height(),
			  get_hoffset(), get_voffset());
    
}
void
PkFont::load (void)
{
    PkOpcode c;
    
    c = (PkOpcode) loader.get_uint8 ();
    if (c != DL_PK_PRE)
	throw string ("Not a .pk file (no pre)");
    
    id = loader.get_uint8 ();
    if (id != 89)
	throw string ("Not a .pk file (incorrect id)");
    
    comment = loader.get_string8 ();
    design_size = loader.get_uint32 (); 
    checksum = loader.get_uint32 ();
    hppp = loader.get_uint32 ();
    vppp = loader.get_uint32 ();
    
    do
    {
	c = (PkOpcode)loader.get_uint8 ();
	switch (c)
	{
	case DL_PK_XXX1:
	    loader.skip_string8 ();
	    break;
	case DL_PK_XXX2:
	    loader.skip_string16 ();
	    break;
	case DL_PK_XXX3:
	    loader.skip_string24 ();
	    break;
	case DL_PK_XXX4:
	    loader.skip_string32 ();
	    break;
	case DL_PK_YYY:
	    loader.get_uint32 ();
	    break;
	case DL_PK_POST:
	    break;
	case DL_PK_NOP:
	    break;
	case DL_PK_PRE:
	    throw string ("Unexpected PRE");
	    break;
	default:
	    loader.goto_from_current (-1);
	    if (c <= DL_PK_FIRST_COMMAND)
	    {
		PkChar *pkc = new PkChar (loader);
		chars[pkc->get_character_code()] = pkc;
#if 0
		cout << '[' << pkc->get_character_code() << ']';
#endif
	    }
	    else
		throw string ("Undefined PK command");
	    break;
	}
    } while (c != DL_PK_POST);
}

PkFont::PkFont (AbstractLoader& l, int at_size_arg) :
    loader (l),
    at_size (at_size_arg)
{
    load ();
}

PkFont::PkFont (AbstractLoader& l) :
    loader (l)
{
    load ();
    at_size = design_size;
}
