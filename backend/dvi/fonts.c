#include "config.h"
#include "fonts.h"
#include "mdvi.h"

static int registered = 0;

extern DviFontInfo pk_font_info;
extern DviFontInfo pkn_font_info;
extern DviFontInfo gf_font_info;
extern DviFontInfo vf_font_info;
extern DviFontInfo ovf_font_info;
#if 0
extern DviFontInfo tt_font_info;
#endif
extern DviFontInfo afm_font_info;
extern DviFontInfo tfm_font_info;
extern DviFontInfo ofm_font_info;

static struct fontinfo {
	DviFontInfo *info;
	char	*desc;
	int	klass;
} known_fonts[] = {
	{&vf_font_info, "Virtual fonts", 0},
	{&ovf_font_info, "Omega's virtual fonts", 0},
#if 0
	{&tt_font_info, "TrueType fonts", 0},
#endif
	{&pk_font_info, "Packed bitmap (auto-generated)", 1},
	{&pkn_font_info, "Packed bitmap", -2},
	{&gf_font_info, "Metafont's generic font format", 1},
	{&ofm_font_info, "Omega font metrics", -1},
	{&tfm_font_info, "TeX font metrics", -1},
	{&afm_font_info, "Adobe font metrics", -1},
	{0, 0}
};

void mdvi_register_fonts (void)
{
    struct fontinfo *type;

    if (!registered) {
	for(type = known_fonts; type->info; type++) {
			mdvi_register_font_type(type->info, type->klass);
	}
	registered = 1;
    }
	return;
}


