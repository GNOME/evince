/* imposter (OO.org Impress viewer)
** Copyright (C) 2003-2005 Gurer Ozen
** This code is free software; you can redistribute it and/or
** modify it under the terms of GNU General Public License.
*/

#ifndef IMPOSTER_H
#define IMPOSTER_H

#include <sys/types.h>

enum {
	IMP_OK = 0,
	IMP_NOMEM,
	IMP_NOTZIP,
	IMP_BADZIP,
	IMP_BADDOC,
	IMP_NOTIMP
};

struct ImpDoc_struct;
typedef struct ImpDoc_struct ImpDoc;

struct ImpPage_struct;
typedef struct ImpPage_struct ImpPage;

typedef struct ImpPointStruct {
	int x;
	int y;
} ImpPoint;

typedef struct ImpColorStruct {
	int red;
	int green;
	int blue;
} ImpColor;

#define IMP_NORMAL 0
#define IMP_BOLD 1
#define IMP_ITALIC 2
#define IMP_UNDERLINE 4

typedef struct ImpDrawer_struct {
	void (*get_size)(void *drw_data, int *w, int *h);
	void (*set_fg_color)(void *drw_data, ImpColor *color);
	void (*draw_line)(void *drw_data, int x1, int y1, int x2, int y2);
	void (*draw_rect)(void *drw_data, int fill, int x, int y, int w, int h);
	void (*draw_polygon)(void *drw_data, int fill, ImpPoint *pts, int nr_pts);
	void (*draw_arc)(void *drw_data, int fill, int x, int y, int w, int h, int sa, int ea);
	void (*draw_bezier)(void *drw_data, int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3);
	void *(*open_image)(void *drw_data, const unsigned char *pix, size_t size);
	void (*get_image_size)(void *drw_data, void *img_data, int *w, int *h);
	void *(*scale_image)(void *drw_data, void *img_data, int w, int h);
	void (*draw_image)(void *drw_data, void *img_data, int x, int y, int w, int h);
	void (*close_image)(void *drw_data, void *img_data);
	void (*get_text_size)(void *drw_data, const char *text, size_t len, int size, int styles, int *w, int *h);
	void (*draw_text)(void *drw_data, int x, int y, const char *text, size_t len, int size, int styles);
} ImpDrawer;

struct ImpRenderCtx_struct;
typedef struct ImpRenderCtx_struct ImpRenderCtx;

#define IMP_LAST_PAGE -1

ImpDoc *imp_open(const char *filename, int *err);
int imp_nr_pages(ImpDoc *doc);
ImpPage *imp_get_page(ImpDoc *doc, int page_no);
void imp_close(ImpDoc *doc);

void *imp_get_xml(ImpDoc *doc, const char *filename);

ImpPage *imp_next_page(ImpPage *page);
ImpPage *imp_prev_page(ImpPage *page);
int imp_get_page_no(ImpPage *page);
const char *imp_get_page_name(ImpPage *page);

ImpRenderCtx *imp_create_context(const ImpDrawer *drw);
void imp_context_set_page(ImpRenderCtx *ctx, ImpPage *page);
void imp_context_set_step(ImpRenderCtx *ctx, int step);
void imp_render(ImpRenderCtx *ctx, void *drw_data);
void imp_delete_context(ImpRenderCtx *ctx);


#endif	/* IMPOSTER_H */
