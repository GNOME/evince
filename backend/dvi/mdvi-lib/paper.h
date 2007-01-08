#ifndef MDVI_PAPER
#define MDVI_PAPER

typedef struct _DviPaper DviPaper;
typedef struct _DviPaperSpec DviPaperSpec;

typedef enum {
	MDVI_PAPER_CLASS_ISO,
	MDVI_PAPER_CLASS_US,
	MDVI_PAPER_CLASS_ANY,
	MDVI_PAPER_CLASS_CUSTOM
} DviPaperClass;

struct _DviPaper {
	DviPaperClass pclass;
	const char *name;
	double	inches_wide;
	double	inches_tall;
};

struct _DviPaperSpec {
	const char *name;
	const char *width;
	const char *height;
};


extern int 	mdvi_get_paper_size __PROTO((const char *, DviPaper *));
extern DviPaperSpec* mdvi_get_paper_specs __PROTO((DviPaperClass));
extern void	mdvi_free_paper_specs __PROTO((DviPaperSpec *));

#endif
