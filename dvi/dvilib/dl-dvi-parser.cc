#include "dl-dvi-parser.hh"

using namespace DviLib;

enum DviOpcode {
    DVI_SETCHAR0 = 0, /* 128 of these */
    DVI_SETCHAR127 = 127,
    DVI_SET1,
    DVI_SET2, 
    DVI_SET3, 
    DVI_SET4,
    DVI_SETRULE,
    DVI_PUT1,
    DVI_PUT2,
    DVI_PUT3,
    DVI_PUT4,
    DVI_PUTRULE,
    DVI_NOP,
    DVI_BOP,
    DVI_EOP,
    DVI_PUSH,
    DVI_POP,
    DVI_RIGHT1,
    DVI_RIGHT2,
    DVI_RIGHT3,
    DVI_RIGHT4,
    DVI_W0,
    DVI_W1,
    DVI_W2,
    DVI_W3,
    DVI_W4,
    DVI_X0,
    DVI_X1,
    DVI_X2,
    DVI_X3,
    DVI_X4,
    DVI_DOWN1,
    DVI_DOWN2,
    DVI_DOWN3,
    DVI_DOWN4,
    DVI_Y0,
    DVI_Y1,
    DVI_Y2,
    DVI_Y3,
    DVI_Y4,
    DVI_Z0,
    DVI_Z1,
    DVI_Z2,
    DVI_Z3,
    DVI_Z4, 
    DVI_FONTNUM0 = 171, /* 64 of these */
    DVI_FONTNUM63 = 234,
    DVI_FNT1,
    DVI_FNT2,
    DVI_FNT3,
    DVI_FNT4,
    DVI_XXX1,
    DVI_XXX2,
    DVI_XXX3,
    DVI_XXX4,
    DVI_FNTDEF1,
    DVI_FNTDEF2,
    DVI_FNTDEF3,
    DVI_FNTDEF4,
    DVI_PRE,
    DVI_POST,
    DVI_POSTPOST = 249
};

static void 
skip_font_definition (AbstractLoader& l, uint *count)
{ 
    *count += 12;
    l.skip_n (12);
    
    *count += 2;
    uint dl = l.get_uint8();
    uint nl = l.get_uint8();
    
    *count += dl+nl;
    l.skip_n (dl+nl);
}

static DviCommand *
parse_command (AbstractLoader &l, uint *count, DviOpcode *opcode)
{
    int h, w;
    string error;
    string s;
    
    *opcode = (DviOpcode)l.get_uint8 ();
    *count += 1;
    
    if (DVI_SETCHAR0 <= *opcode && *opcode <= DVI_SETCHAR127)
	return new DviSetCharCommand (*opcode);
    else if (DVI_FONTNUM0 <= *opcode && *opcode <= DVI_FONTNUM63)
	return new DviFontNumCommand (*opcode - DVI_FONTNUM0);
    else switch (*opcode) {
    case DVI_SET1:
	*count += 1;
	return new DviSetCharCommand (l.get_uint8());
	break;
    case DVI_SET2:
	*count += 2;
	return new DviSetCharCommand (l.get_uint16());
	break;
    case DVI_SET3:
	*count += 3;
	return new DviSetCharCommand (l.get_uint24());
	break;
    case DVI_SET4:
	*count += 4;
	return new DviSetCharCommand (l.get_uint32());
	break;
    case DVI_SETRULE:
	*count += 8;
	h = l.get_int32 ();
	w = l.get_int32 ();
	return new DviSetRuleCommand (h, w);
	break;
    case DVI_PUT1:
	*count += 1;
	return new DviPutCharCommand (l.get_uint8());
	break;
    case DVI_PUT2:
	*count += 2;
	return new DviPutCharCommand (l.get_uint16());
	break;
    case DVI_PUT3:
	*count += 3;
	return new DviPutCharCommand (l.get_uint24());
	break;
    case DVI_PUT4:
	*count += 4;
	return new DviPutCharCommand (l.get_uint32());
	break;
    case DVI_PUTRULE:
	*count += 8;
	h = l.get_int32();
	w = l.get_int32();
	return new DviPutRuleCommand (h, w);
	break;
    case DVI_PUSH:
	return new DviPushCommand();
	break;
    case DVI_POP:
	return new DviPopCommand();
	break;
    case DVI_RIGHT1:
	*count += 1;
	return new DviRightCommand (l.get_int8());
	break;
    case DVI_RIGHT2:
	*count += 2;
	return new DviRightCommand (l.get_int16());
	break;
    case DVI_RIGHT3:
	*count += 3;
	return new DviRightCommand (l.get_int24());
	break;
    case DVI_RIGHT4:
	*count += 4;
	return new DviRightCommand (l.get_int32());
	break;
    case DVI_W0:
	return new DviWRepCommand ();
	break;
    case DVI_W1:
	*count += 1;
	return new DviWCommand (l.get_int8());
	break;
    case DVI_W2:
	*count += 2;
	return new DviWCommand (l.get_int16());
	break;
    case DVI_W3:
	*count += 3;
	return new DviWCommand (l.get_int24());
	break;
    case DVI_W4:
	*count += 4;
	return new DviWCommand (l.get_int32());
	break;
    case DVI_X0:
	return new DviXRepCommand ();
	break;
    case DVI_X1:
	*count += 1;
	return new DviXCommand (l.get_int8());
	break;
    case DVI_X2:
	*count += 2;
	return new DviXCommand (l.get_int16());
	break;
    case DVI_X3:
	*count += 3;
	return new DviXCommand (l.get_int24());
	break;
    case DVI_X4:
	*count += 4;
	return new DviXCommand (l.get_int32());
	break;
    case DVI_DOWN1:
	*count += 1;
	return new DviDownCommand (l.get_int8());
	break;
    case DVI_DOWN2:
	*count += 2;
	return new DviDownCommand (l.get_int16());
	break;
    case DVI_DOWN3:
	*count += 3;
	return new DviDownCommand (l.get_int24());
	break;
    case DVI_DOWN4:
	*count += 4;
	return new DviDownCommand (l.get_int32());
	break;
    case DVI_Y0:
	return new DviYRepCommand ();
	break;
    case DVI_Y1:
	*count += 1;
	return new DviYCommand (l.get_int8());
	break;
    case DVI_Y2:
	*count += 2;
	return new DviYCommand (l.get_int16());
	break;
    case DVI_Y3:
	*count += 3;
	return new DviYCommand (l.get_int24());
	break;
    case DVI_Y4:
	*count += 4;
	return new DviYCommand (l.get_int32());
	break;
    case DVI_Z0:
	return new DviZRepCommand ();
	break;
    case DVI_Z1:
	*count += 1;
	return new DviZCommand (l.get_int8());
	break;
    case DVI_Z2:
	*count += 2;
	return new DviZCommand (l.get_int16());
	break;
    case DVI_Z3:
	*count += 3;
	return new DviZCommand (l.get_int24());
	break;
    case DVI_Z4:
	*count += 4;
	return new DviZCommand (l.get_int32());
	break;
    case DVI_FNT1:
	*count += 1;
	return new DviFontNumCommand (l.get_uint8());
	break;
    case DVI_FNT2:
	*count += 2;
	return new DviFontNumCommand (l.get_uint16());
	break;
    case DVI_FNT3:
	*count += 3;
	return new DviFontNumCommand (l.get_uint24());
	break;
    case DVI_FNT4:
	*count += 4;
	return new DviFontNumCommand (l.get_uint32());
	break;
    case DVI_XXX1:
	s = l.get_string8();
	*count += s.length() + 1;
	return new DviSpecialCommand (s);
	break;
    case DVI_XXX2:
	s = l.get_string16();
	*count += s.length() + 2;
	return new DviSpecialCommand (s);
	break;
    case DVI_XXX3:
	s = l.get_string24();
	*count += s.length() + 3;
	return new DviSpecialCommand (s);
	break;
    case DVI_XXX4:
	s = l.get_string32();
	*count += s.length() + 4;
	return new DviSpecialCommand (s);
	break;
    case DVI_FNTDEF1:
	l.get_uint8 ();
	skip_font_definition (l, count);
	break;
    case DVI_FNTDEF2:
	l.get_uint16 ();
	skip_font_definition (l, count);
	break;
    case DVI_FNTDEF3: 
	l.get_uint24 ();
	skip_font_definition (l, count);
	break;
    case DVI_FNTDEF4:
	l.get_uint32 ();
	skip_font_definition (l, count);
	break;
    case DVI_BOP:	// BOP and EOP are not considered commands
    case DVI_EOP:
    case DVI_NOP:       // NOP is ignored
    case DVI_PRE:       // PRE, POST and POSTPOST are not supposed to happen
    case DVI_POST:
    case DVI_POSTPOST:
	break;
    default:
	printf ("%u\n", *opcode);
	throw string ("Unknown command");
	break;
    }
    return 0;
}

DviProgram *
DviParser::parse_program (void)
{
    DviProgram *program = new DviProgram ();
    DviOpcode opcode;
    
    do
    { 
	DviCommand *cmd;
	uint dummy;
	
	cmd = parse_command (loader, &dummy, &opcode);
	if (cmd)
	{
	    program->add_command (cmd);
	    cmd->unref();
	}
	
    } while (opcode != DVI_EOP);
    
    return program;
}

DviProgram *
DviParser::parse_program (uint n_bytes)
{
    DviProgram *program = new DviProgram ();
    uint count = 0;
    
    while (count < n_bytes)
    {
	DviOpcode opcode;
	DviCommand *cmd;
	
	cmd = parse_command (loader, &count, &opcode);
	if (cmd)
	{
#if 0
	    cout << opcode << endl;
#endif
	    program->add_command (cmd);
	    cmd->unref();
	}
    }
    
    return program;
}

DviPageHeader *
DviParser::parse_page_header (uint *page_pointer)
{
    DviOpcode c;
    
    DviPageHeader *header = new DviPageHeader();
    
    header->address = *page_pointer;
    
    c = (DviOpcode)loader.get_uint8();
    if (c != DVI_BOP)
	throw string ("Expected BOP not found");
    for (uint i=0; i<N_PAGE_COUNTERS; ++i)
	header->count[i] = loader.get_uint32 ();
    
    *page_pointer = loader.get_uint32 ();
    
    return header;
}

DviFontdefinition *
DviParser::parse_fontdefinition (void)
{
    DviFontdefinition *fontdef = new DviFontdefinition;
    DviOpcode c = (DviOpcode)loader.get_uint8 ();
    
    switch (c) {
    case DVI_FNTDEF1:
	fontdef->fontnum = loader.get_uint8 ();
	break;
    case DVI_FNTDEF2:
	fontdef->fontnum = loader.get_uint16 ();
	break;
    case DVI_FNTDEF3:
	fontdef->fontnum = loader.get_uint24 ();
	break;
    case DVI_FNTDEF4:
	fontdef->fontnum = loader.get_uint32 ();
	break;
    default:
	throw string ("DVI_FNTDEF? expected");
	break;
    }
    fontdef->checksum = loader.get_uint32 ();
    fontdef->at_size = loader.get_uint32 ();
    fontdef->design_size = loader.get_uint32 ();
    
    uint dirlength = loader.get_uint8 ();
    uint namelength = loader.get_uint8 ();
    
    fontdef->directory = "";
    fontdef->name = "";
    
    for (uint i=0; i < dirlength; ++i)
	fontdef->directory += loader.get_uint8();
    for (uint i=0; i < namelength; ++i)
	fontdef->name += loader.get_uint8();
    
#if 0
    cout << "parsed fd: " << fontdef->name << " " << fontdef->fontnum << endl;
#endif
    
    return fontdef;
}

DviFilePreamble *
DviParser::parse_preamble (void)
{
    DviFilePreamble *preamble = new DviFilePreamble;
    
    DviOpcode c = (DviOpcode)loader.get_uint8 ();
    if (c != DVI_PRE)
    {
	string asdf ("asdf");
	throw string ("Corrupt .dvi file - first byte is not DVI_PRE" + asdf);
    }
    
    preamble->type = (DviType)loader.get_uint8 ();
    if (preamble->type != NORMAL_DVI)
    {
	string asdf ("asdf");
	cout << asdf;
	throw string ("Unknown .dvi format" + asdf);
    }
    
    preamble->numerator = loader.get_uint32 ();
    preamble->denominator = loader.get_uint32 ();
    preamble->magnification = loader.get_uint32 ();
    preamble->comment = loader.get_string8 ();
    
    return preamble;
}

DviFilePostamble *
DviParser::parse_postamble (void)
{
    DviFilePostamble *postamble = new DviFilePostamble;

    postamble->fontmap = new DviFontMap;
    
    loader.goto_from_end (-5);
    
    int i;
    do {
	i = loader.get_uint8 ();
	loader.goto_from_current (-2);
    } while (i == 223);
    
    postamble->type = (DviType)i;
    
    loader.goto_from_current (-3);
    loader.goto_from_start (loader.get_uint32() + 1);
    
    postamble->last_page_address = loader.get_uint32();
    postamble->numerator = loader.get_uint32();
    postamble->denominator = loader.get_uint32();
    postamble->magnification = loader.get_uint32();
    postamble->max_height = loader.get_uint32();
    postamble->max_width = loader.get_uint32();
    postamble->stack_height = loader.get_uint16();
    
    loader.get_uint16 (); // skip number of pages (we count them instead)
    
    while (true)
    {
	DviOpcode c = (DviOpcode)loader.get_uint8 ();
	
	if (c == DVI_NOP)
	    continue;
	else if (DVI_FNTDEF1 <= c  &&  c <= DVI_FNTDEF4)
	{
	    loader.goto_from_current (-1);
	    DviFontdefinition *fd = parse_fontdefinition ();
	    
	    postamble->fontmap->set_fontdefinition (fd->fontnum, fd);
	    cout << fd->name << endl;
	    cout << postamble->fontmap->get_fontdefinition(fd->fontnum)->name;
	}
	else
	{
	    loader.goto_from_current (-1);
	    break;
	}
    }
    return postamble;
}

VfFontPreamble *
DviParser::parse_vf_font_preamble (void)
{
    DviOpcode c;
    VfFontPreamble *preamble = new VfFontPreamble;

    preamble->fontmap = new DviFontMap;
    
    c = (DviOpcode)loader.get_uint8 ();
    if (c != DVI_PRE)
	throw string ("Not a .vf file");
    c = (DviOpcode)loader.get_uint8 ();
    if (c != 202)
	throw string ("Not a .vf file");
    
    preamble->comment = loader.get_string8 ();
    preamble->checksum = loader.get_uint32 ();
    preamble->design_size = loader.get_uint32 ();
    
    int i = 0;
    while (true)
    {
	DviOpcode c = (DviOpcode)loader.get_uint8 ();
	
	if (DVI_FNTDEF1 <= c  &&  c <= DVI_FNTDEF4)
	{
	    loader.goto_from_current (-1);
	    DviFontdefinition *fd = parse_fontdefinition ();
	    
	    preamble->fontmap->set_fontdefinition (i++, fd);
	}
	else
	{
	    loader.goto_from_current (-1);
	    break;
	}
    }
    return preamble;
}

VfChar *
DviParser::parse_vf_char (void)
{
    DviOpcode c;
    
    c = (DviOpcode)loader.get_uint8 ();
    
    VfChar *ch = new VfChar;
    
    if (c == DVI_POST)
	return 0;
    else if (c > 242)
	throw string ("Corrupt .vf file");
    else 
    {
	uint packet_length;
	if (c == 242)
	{
	    packet_length = loader.get_uint32 ();
	    ch->character_code = loader.get_uint32 ();
	    ch->tfm_width = loader.get_uint32 ();
	}
	else
	{
	    packet_length = c;
	    ch->character_code = loader.get_uint8 ();
	    ch->tfm_width = loader.get_uint24 ();
	}
	ch->program = parse_program (packet_length);
    }
    return ch;
}
