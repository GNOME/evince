/* imposter (OO.org Impress viewer)
** Copyright (C) 2003-2005 Gurer Ozen
** This code is free software; you can redistribute it and/or
** modify it under the terms of GNU General Public License.
*/

#include <config.h>
#include "common.h"
#include "internal.h"

struct Span {
	struct Span *next;
	int x, y;
	int w, h;
	char *text;
	int len;
	int size;
	int styles;
	ImpColor fg;
};

struct Line {
	struct Line *next;
	struct Span *spans;
	struct Span *last_span;
	int x, y;
	int w, h;
};

struct Layout {
	ikstack *s;
	int x, y, w, h;
	int tw, th;
	struct Line *lines;
	struct Line *last_line;
	char spaces[128];
};

static struct Line *
add_line(struct Layout *lay)
{
	struct Line *line;

	line = iks_stack_alloc(lay->s, sizeof(struct Line));
	memset(line, 0, sizeof(struct Line));

	if (!lay->lines) lay->lines = line;
	if (lay->last_line) lay->last_line->next = line;
	lay->last_line = line;

	return line;
}

static struct Span *
add_span(struct Layout *lay, char *text, int len, int size, int styles)
{
	struct Line *line;
	struct Span *span;

	span = iks_stack_alloc(lay->s, sizeof(struct Span));
	memset(span, 0, sizeof(struct Span));
	span->text = text;
	span->len = len;
	span->size = size;
	span->styles = styles;

	line = lay->last_line;
	if (!line) line = add_line(lay);
	if (line->spans) {
		span->x = line->last_span->x + line->last_span->w;
		span->y = line->last_span->y;
	} else {
		span->x = line->x;
		span->y = line->y;
	}

	if (!line->spans) line->spans = span;
	if (line->last_span) line->last_span->next = span;
	line->last_span = span;

	return span;
}

static void
calc_sizes(ImpRenderCtx *ctx, void *drw_data, struct Layout *lay)
{
	struct Line *line;
	struct Span *span;

	for (line = lay->lines; line; line = line->next) {
		for (span = line->spans; span; span = span->next) {
			ctx->drw->get_text_size(drw_data,
				span->text, span->len,
				span->size, span->styles,
				&span->w, &span->h
			);
			line->w += span->w;
			if (span->h > line->h) line->h = span->h;
		}
		if (line->w > lay->tw) lay->tw = line->w;
		lay->th += line->h;
	}
}

static void
calc_pos(ImpRenderCtx *ctx, struct Layout *lay)
{
	struct Line *line;
	struct Span *span;
	int x, y, x2;

	x = lay->x;
	y = lay->y;
	for (line = lay->lines; line; line = line->next) {
		line->x = x;
		line->y = y;
		y += line->h;
		x2 = x;
		for (span = line->spans; span; span = span->next) {
			span->x = x2;
			span->y = y;
			x2 += span->w;
		}
	}
}

static void
_imp_draw_layout(ImpRenderCtx *ctx, void *drw_data, struct Layout *lay)
{
	struct Line *line;
	struct Span *span;

	for (line = lay->lines; line; line = line->next) {
		for (span = line->spans; span; span = span->next) {
			ctx->drw->set_fg_color(drw_data, &span->fg);
			ctx->drw->draw_text(drw_data,
				span->x, span->y,
				span->text, span->len,
				span->size,
				span->styles
			);
		}
	}
}

static void
text_span(ImpRenderCtx *ctx, struct Layout *lay, iks *node, char *text, size_t len)
{
	struct Span *span;
	double cm;
	char *attr, *t, *s;
	int px = 0, cont = 1;
	int styles = IMP_NORMAL;

	attr = r_get_style(ctx, node, "fo:font-size");
	if (attr) {
		cm = atof(attr);
		if (strstr(attr, "pt")) cm = cm * 2.54 / 102;
		px = cm * ctx->fact_y;
	}
	attr = r_get_style(ctx, node, "fo:font-weight");
	if (attr && strcmp(attr, "bold") == 0) styles |= IMP_BOLD;
	attr = r_get_style(ctx, node, "style:text-underline");
	if (attr && strcmp(attr, "single") == 0) styles |= IMP_UNDERLINE;
	attr = r_get_style(ctx, node, "fo:font-style");
	if (attr && strcmp(attr, "italic") == 0) styles |= IMP_ITALIC;

	t = text;
	while (cont) {
		s = strchr(t, '\n');
		if (s) {
			int len2 = s - t;
			span = add_span(lay, t, len2, px, styles);
			t = s + 1;
			len -= len2;
			add_line(lay);
		} else {
			span = add_span(lay, text, len, px, styles);
			cont = 0;
		}
		r_get_color(ctx, node, "fo:color", &span->fg);
	}
}

static void
text_p(ImpRenderCtx *ctx, struct Layout *lay, iks *node)
{
	iks *n, *n2;

	add_line(lay);
	for (n = iks_child(node); n; n = iks_next(n)) {
		if (iks_type(n) == IKS_CDATA) {
			text_span(ctx, lay, node, iks_cdata(n), iks_cdata_size(n));
		} else if (iks_strcmp(iks_name(n), "text:span") == 0) {
			for (n2 = iks_child(n); n2; n2 = iks_next(n2)) {
				if (iks_type(n2) == IKS_CDATA) {
					text_span(ctx, lay, n2, iks_cdata(n2), iks_cdata_size(n2));
				} else if (iks_strcmp(iks_name(n2), "text:s") == 0) {
					char *attr;
					int c = 1;
					attr = iks_find_attrib(n2, "text:c");
					if (attr) c = atoi(attr);
					if (c > 127) {
						c = 127;
						puts("bork bork");
					}
					text_span(ctx, lay, n, lay->spaces, c);
				} else if (iks_strcmp(iks_name(n2), "text:a") == 0) {
					text_span(ctx, lay, n, iks_cdata(iks_child(n2)), iks_cdata_size(iks_child(n2)));
				} else if (iks_strcmp(iks_name(n2), "text:tab-stop") == 0) {
					text_span(ctx, lay, n, "\t", 1);
				} else if (iks_strcmp(iks_name(n2), "text:page-number") == 0) {
					char buf[8];
					sprintf(buf, "%d", ctx->page->nr);
					text_span(ctx, lay, n, iks_stack_strdup(lay->s, buf, 0), strlen(buf));
				}
			}
		} else if (iks_strcmp(iks_name(n), "text:line-break") == 0) {
			add_line(lay);
		} else if (iks_strcmp(iks_name(n), "text:a") == 0) {
			text_span(ctx, lay, n, iks_cdata(iks_child(n)), iks_cdata_size(iks_child(n)));
		} else if (iks_strcmp(iks_name(n), "text:page-number") == 0) {
			char buf[8];
			sprintf(buf, "%d", ctx->page->nr);
			text_span(ctx, lay, n, iks_stack_strdup(lay->s, buf, 0), strlen(buf));
		}
	}
}

static void
text_list(ImpRenderCtx *ctx, struct Layout *lay, iks *node)
{
	iks *n, *n2;

	for (n = iks_first_tag(node); n; n = iks_next_tag(n)) {
		for (n2 = iks_first_tag(n); n2; n2 = iks_next_tag(n2)) {
			if (strcmp(iks_name(n2), "text:p") == 0) {
				text_p(ctx, lay, n2);
			} else if (strcmp(iks_name(n2), "text:ordered-list") == 0) {
				text_list(ctx, lay, n2);
			} else if (strcmp(iks_name(n2), "text:unordered-list") == 0) {
				text_list(ctx, lay, n2);
			} else if (strcmp(iks_name(n2), "text:list") == 0) {
				text_list(ctx, lay, n2);
			}
		}
	}
}

void
r_text(ImpRenderCtx *ctx, void *drw_data, iks *node)
{
	struct Layout lay;
	iks *n;

	memset(&lay, 0, sizeof(struct Layout));
	memset(&lay.spaces, ' ', 128);
	lay.s = iks_stack_new(sizeof(struct Span) * 16, 0);
	lay.x = r_get_x(ctx, node, "svg:x");
	lay.y = r_get_y(ctx, node, "svg:y");
	lay.w = r_get_y(ctx, node, "svg:width");
	lay.h = r_get_y(ctx, node, "svg:height");

	for (n = iks_first_tag(node); n; n = iks_next_tag(n)) {
		if (strcmp(iks_name(n), "text:p") == 0) {
			text_p(ctx, &lay, n);
		} else if (strcmp(iks_name(n), "text:ordered-list") == 0) {
			text_list(ctx, &lay, n);
		} else if (strcmp(iks_name(n), "text:unordered-list") == 0) {
			text_list(ctx, &lay, n);
		} else if (strcmp(iks_name(n), "text:list") == 0) {
			text_list(ctx, &lay, n);
		}
	}

	calc_sizes(ctx, drw_data, &lay);
	calc_pos(ctx, &lay);
	_imp_draw_layout(ctx, drw_data, &lay);

	iks_stack_delete(lay.s);
}
/*
static void
text_span (render_ctx *ctx, text_ctx *tc, struct layout_s *lout, iks *node, char *text, int len)
{
	if (tc->bullet_flag && tc->bullet_sz) size = tc->bullet_sz; else size = r_get_font_size (ctx, tc, node);
}

static int
is_animated (render_ctx *ctx, text_ctx *tc, iks *node)
{
	if (!ctx->step_mode) return 0;
	if (!tc->id) return 0;
	while (strcmp (iks_name (node), "draw:page") != 0
		&& strcmp (iks_name (node), "style:master-page") != 0)
			node = iks_parent (node);
	node = iks_find (node, "presentation:animations");
	if (!node) return 0;
	if (iks_find_with_attrib (node, "presentation:show-text", "draw:shape-id", tc->id)) return 1;
	return 0;
}

static void
text_p (render_ctx *ctx, text_ctx *tc, iks *node)
{
	if (is_animated (ctx, tc, node) && ctx->step_cnt >= ctx->step) lout->flag = 0;
	ctx->step_cnt++;

	attr = r_get_style (ctx, node, "text:enable-numbering");
	if (attr && strcmp (attr, "true") == 0) {
		if (iks_child (node) && tc->bullet) {
			tc->bullet_flag = 1;
			text_span (ctx, tc, lout, node, tc->bullet, strlen (tc->bullet));
			text_span (ctx, tc, lout, node, " ", 1);
			tc->bullet_flag = 0;
		}
	}

	if (!lout->text) {
lout->h = 0;
attr = r_get_style (ctx, node, "fo:line-height");
if (attr) {
	int ratio = atoi (attr);
	lout->lh = ratio;
} else {
	lout->lh = 100;
}
tc->layouts = g_list_append (tc->layouts, lout);
//		g_object_unref (lout->play);
//		iks_stack_delete (s);
		return;
	}

	attr = r_get_style (ctx, node, "fo:text-align");
	if (attr) {
		if (strcmp (attr, "center") == 0)
			pango_layout_set_alignment (lout->play, PANGO_ALIGN_CENTER);
		else if (strcmp (attr, "end") == 0)
			pango_layout_set_alignment (lout->play, PANGO_ALIGN_RIGHT);
	}
	pango_layout_set_width (lout->play, tc->w * PANGO_SCALE);
	pango_layout_set_markup (lout->play, lout->text, lout->text_len);
	pango_layout_get_pixel_size (lout->play, &lout->w, &lout->h);
	attr = r_get_style (ctx, node, "fo:line-height");
	if (attr) {
		int ratio = atoi (attr);
		lout->lh = ratio;
	} else {
		lout->lh = 100;
	}
	tc->layouts = g_list_append (tc->layouts, lout);
}

static void
find_bullet (render_ctx *ctx, text_ctx *tc, iks *node)
{
	iks *x;
	char *t;
	x = r_get_bullet (ctx, node, "text:list-level-style-bullet");
	x = iks_find (x, "text:list-level-style-bullet");
	t = iks_find_attrib (x, "text:bullet-char");
	if (t) tc->bullet = t; else tc->bullet = "*";
	x = iks_find (x, "style:properties");
	t = iks_find_attrib (x, "fo:font-size");
	if (t) tc->bullet_sz = tc->last_sz * atoi (t) / 100;
	else tc->bullet_sz = 0;
}

void
r_text (render_ctx *ctx, iks *node)
{
	tc.id = iks_find_attrib (node, "draw:id");
	ctx->step_cnt = 0;
	for (n = iks_first_tag (node); n; n = iks_next_tag (n)) {
		if (strcmp (iks_name (n), "text:p") == 0) {
			text_p (ctx, &tc, n);
		} else if (strcmp (iks_name (n), "text:ordered-list") == 0) {
			text_list (ctx, &tc, n);
		} else if (strcmp (iks_name (n), "text:unordered-list") == 0) {
			find_bullet (ctx, &tc, n);
			text_list (ctx, &tc, n);
			tc.bullet = 0;
		}
	}

*/
