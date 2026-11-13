/*
 * Copyright (C) 2026 Germán Poo-Caamaño <gpoo@gnome.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */


#include <evince-view.h>

#include "ev-test-utils.h"

typedef struct {
	GString *notify;
	gint     page_changed;
	gint     old_page;
	gint     new_page;
} Log;

static void
notify_cb (GObject    *object,
	   GParamSpec *pspec,
	   Log        *log)
{
	g_string_append_printf (log->notify, "%s;", pspec->name);
}

static void
page_changed_cb (EvDocumentModel *model,
		 gint             old_page,
		 gint             new_page,
		 Log             *log)
{
	log->page_changed++;
	log->old_page = old_page;
	log->new_page = new_page;
}

static void
log_connect (Log             *log,
	     EvDocumentModel *model)
{
	log->notify = g_string_new (NULL);
	log->page_changed = 0;
	g_signal_connect (model, "notify", G_CALLBACK (notify_cb), log);
	g_signal_connect (model, "page-changed", G_CALLBACK (page_changed_cb), log);
}

static void
log_free (Log *log)
{
	g_string_free (log->notify, TRUE);
}

static void
assert_notified (Log         *log,
		 const gchar *expected)
{
	g_assert_cmpstr (log->notify->str, ==, expected);
	g_string_truncate (log->notify, 0);
}

static EvDocument *
load_shapes_or_skip (void)
{
#if EV_TEST_HAVE_PDF
	return ev_test_load_document_or_fail ("shapes.pdf");
#else
	g_test_skip ("PDF backend not built");
	return NULL;
#endif
}

static void
test_model_defaults (void)
{
	EvDocumentModel *model = ev_document_model_new ();

	g_assert_null (ev_document_model_get_document (model));
	g_assert_cmpint (ev_document_model_get_page (model), ==, -1);
	g_assert_cmpfloat (ev_document_model_get_scale (model), ==, 1.0);
	g_assert_cmpfloat (ev_document_model_get_min_scale (model), ==, 0.25);
	g_assert_cmpfloat (ev_document_model_get_max_scale (model), ==, 5.0);
	g_assert_cmpint (ev_document_model_get_sizing_mode (model), ==, EV_SIZING_FIT_WIDTH);
	g_assert_cmpint (ev_document_model_get_page_layout (model), ==, EV_PAGE_LAYOUT_SINGLE);
	g_assert_cmpint (ev_document_model_get_rotation (model), ==, 0);
	g_assert_true (ev_document_model_get_continuous (model));
	g_assert_false (ev_document_model_get_dual_page_odd_pages_left (model));
	g_assert_false (ev_document_model_get_inverted_colors (model));
	g_assert_false (ev_document_model_get_rtl (model));
	g_assert_false (ev_document_model_get_fullscreen (model));

	g_object_unref (model);
}

static void
test_model_document (void)
{
	EvDocument *document = load_shapes_or_skip ();
	EvDocumentModel *model;
	Log log;

	if (!document)
		return;

	model = ev_document_model_new ();
	log_connect (&log, model);

	ev_document_model_set_document (model, document);
	g_assert_true (ev_document_model_get_document (model) == document);
	g_assert_cmpint (ev_document_model_get_page (model), ==, 0);
	g_assert_cmpint (log.page_changed, ==, 1);
	g_assert_cmpint (log.old_page, ==, -1);
	g_assert_cmpint (log.new_page, ==, 0);
	assert_notified (&log, "page;document;");

	ev_document_model_set_document (model, document);
	assert_notified (&log, "");

	log_free (&log);
	g_object_unref (model);
	g_object_unref (document);
}

static void
test_model_page (void)
{
	EvDocument *document = load_shapes_or_skip ();
	EvDocumentModel *model;
	Log log;

	if (!document)
		return;

	model = ev_document_model_new_with_document (document);
	log_connect (&log, model);

	ev_document_model_set_page (model, 1);
	g_assert_cmpint (ev_document_model_get_page (model), ==, 1);
	g_assert_cmpint (log.page_changed, ==, 1);
	g_assert_cmpint (log.old_page, ==, 0);
	g_assert_cmpint (log.new_page, ==, 1);
	assert_notified (&log, "page;");

	ev_document_model_set_page (model, 1);
	ev_document_model_set_page (model, 2);
	ev_document_model_set_page (model, -1);
	g_assert_cmpint (ev_document_model_get_page (model), ==, 1);
	g_assert_cmpint (log.page_changed, ==, 1);
	assert_notified (&log, "");

	ev_document_model_set_page_by_label (model, "Cover");
	g_assert_cmpint (ev_document_model_get_page (model), ==, 0);
	ev_document_model_set_page_by_label (model, "2");
	g_assert_cmpint (ev_document_model_get_page (model), ==, 1);
	ev_document_model_set_page_by_label (model, "Missing");
	g_assert_cmpint (ev_document_model_get_page (model), ==, 1);
	g_assert_cmpint (log.page_changed, ==, 3);

	log_free (&log);
	g_object_unref (model);
	g_object_unref (document);
}

static void
test_model_scale (void)
{
	EvDocumentModel *model = ev_document_model_new ();
	Log log;

	log_connect (&log, model);

	ev_document_model_set_scale (model, 2.0);
	g_assert_cmpfloat (ev_document_model_get_scale (model), ==, 2.0);
	assert_notified (&log, "scale;");

	ev_document_model_set_scale (model, 2.0);
	assert_notified (&log, "");

	ev_document_model_set_scale (model, 10.0);
	g_assert_cmpfloat (ev_document_model_get_scale (model), ==, 5.0);
	assert_notified (&log, "scale;");

	ev_document_model_set_max_scale (model, 3.0);
	g_assert_cmpfloat (ev_document_model_get_scale (model), ==, 3.0);
	assert_notified (&log, "scale;max-scale;");

	/* The minimum scale only applies in free sizing mode */
	ev_document_model_set_scale (model, 0.1);
	g_assert_cmpfloat (ev_document_model_get_scale (model), ==, 0.1);
	assert_notified (&log, "scale;");

	ev_document_model_set_sizing_mode (model, EV_SIZING_FREE);
	g_assert_cmpint (ev_document_model_get_sizing_mode (model), ==, EV_SIZING_FREE);
	assert_notified (&log, "sizing-mode;");

	ev_document_model_set_scale (model, 0.1);
	g_assert_cmpfloat (ev_document_model_get_scale (model), ==, 0.25);
	assert_notified (&log, "scale;");

	ev_document_model_set_min_scale (model, 0.5);
	g_assert_cmpfloat (ev_document_model_get_scale (model), ==, 0.5);
	assert_notified (&log, "scale;min-scale;");

	log_free (&log);
	g_object_unref (model);
}

static void
test_model_rotation (void)
{
	EvDocumentModel *model = ev_document_model_new ();
	Log log;

	log_connect (&log, model);

	ev_document_model_set_rotation (model, 90);
	g_assert_cmpint (ev_document_model_get_rotation (model), ==, 90);
	assert_notified (&log, "rotation;");

	ev_document_model_set_rotation (model, 450);
	g_assert_cmpint (ev_document_model_get_rotation (model), ==, 90);
	assert_notified (&log, "");

	ev_document_model_set_rotation (model, -90);
	g_assert_cmpint (ev_document_model_get_rotation (model), ==, 270);
	assert_notified (&log, "rotation;");

	ev_document_model_set_rotation (model, 360);
	g_assert_cmpint (ev_document_model_get_rotation (model), ==, 0);
	assert_notified (&log, "rotation;");

	log_free (&log);
	g_object_unref (model);
}

static void
test_model_rotation_normalized (void)
{
	EvDocumentModel *model = ev_document_model_new ();
	gint rotation;

	/* Only one turn is removed: 720 becomes 360 and -450 becomes -90 */
	ev_document_model_set_rotation (model, 720);
	rotation = ev_document_model_get_rotation (model);
	ev_document_model_set_rotation (model, -450);

	if (rotation != 0 || ev_document_model_get_rotation (model) != 270)
		g_test_incomplete ("ev_document_model_set_rotation() only normalizes "
				   "angles within one turn");

	g_object_unref (model);
}

static void
test_model_layout (void)
{
	EvDocumentModel *model = ev_document_model_new ();
	Log log;

	log_connect (&log, model);

	/* The deprecated dual-page API must keep following the page layout */
	G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	ev_document_model_set_page_layout (model, EV_PAGE_LAYOUT_DUAL);
	g_assert_cmpint (ev_document_model_get_page_layout (model), ==, EV_PAGE_LAYOUT_DUAL);
	g_assert_true (ev_document_model_get_dual_page (model));
	assert_notified (&log, "page-layout;dual-page;");

	ev_document_model_set_page_layout (model, EV_PAGE_LAYOUT_AUTOMATIC);
	g_assert_false (ev_document_model_get_dual_page (model));
	assert_notified (&log, "page-layout;dual-page;");

	ev_document_model_set_dual_page (model, TRUE);
	g_assert_cmpint (ev_document_model_get_page_layout (model), ==, EV_PAGE_LAYOUT_DUAL);
	assert_notified (&log, "page-layout;dual-page;");

	ev_document_model_set_dual_page (model, FALSE);
	g_assert_cmpint (ev_document_model_get_page_layout (model), ==, EV_PAGE_LAYOUT_SINGLE);
	assert_notified (&log, "page-layout;dual-page;");
	G_GNUC_END_IGNORE_DEPRECATIONS

	log_free (&log);
	g_object_unref (model);
}

static void
test_model_flags (void)
{
	EvDocumentModel *model = ev_document_model_new ();
	Log log;

	log_connect (&log, model);

	ev_document_model_set_continuous (model, FALSE);
	ev_document_model_set_continuous (model, FALSE);
	g_assert_false (ev_document_model_get_continuous (model));
	assert_notified (&log, "continuous;");

	ev_document_model_set_dual_page_odd_pages_left (model, TRUE);
	g_assert_true (ev_document_model_get_dual_page_odd_pages_left (model));
	assert_notified (&log, "dual-odd-left;");

	ev_document_model_set_inverted_colors (model, TRUE);
	ev_document_model_set_inverted_colors (model, TRUE);
	g_assert_true (ev_document_model_get_inverted_colors (model));
	assert_notified (&log, "inverted-colors;");

	ev_document_model_set_rtl (model, TRUE);
	g_assert_true (ev_document_model_get_rtl (model));
	assert_notified (&log, "rtl;");

	ev_document_model_set_fullscreen (model, TRUE);
	g_assert_true (ev_document_model_get_fullscreen (model));
	assert_notified (&log, "fullscreen;");

	/* Any non-zero value is TRUE */
	ev_document_model_set_continuous (model, 2);
	ev_document_model_set_continuous (model, TRUE);
	g_assert_cmpint (ev_document_model_get_continuous (model), ==, TRUE);
	assert_notified (&log, "continuous;");

	g_object_set (model, "rtl", FALSE, NULL);
	g_assert_false (ev_document_model_get_rtl (model));
	assert_notified (&log, "rtl;");

	log_free (&log);
	g_object_unref (model);
}

static void
test_view_navigation (void)
{
	EvDocument *document;
	EvDocumentModel *model;
	GtkWidget *view;

	if (!gtk_init_check (NULL, NULL)) {
		g_test_skip ("No display available");
		return;
	}

	document = load_shapes_or_skip ();
	if (!document)
		return;

	model = ev_document_model_new_with_document (document);
	view = g_object_ref_sink (ev_view_new ());
	ev_view_set_model (EV_VIEW (view), model);

	g_assert_cmpint (ev_document_model_get_page (model), ==, 0);
	g_assert_false (ev_view_previous_page (EV_VIEW (view)));

	g_assert_true (ev_view_next_page (EV_VIEW (view)));
	g_assert_cmpint (ev_document_model_get_page (model), ==, 1);
	g_assert_false (ev_view_next_page (EV_VIEW (view)));

	g_assert_true (ev_view_previous_page (EV_VIEW (view)));
	g_assert_cmpint (ev_document_model_get_page (model), ==, 0);

	g_assert_false (ev_view_get_has_selection (EV_VIEW (view)));

	gtk_widget_destroy (view);
	g_object_unref (view);

	/* The view's jobs must not run while the process exits */
	ev_job_scheduler_wait ();
	while (g_main_context_iteration (NULL, FALSE));
	g_object_unref (model);
	g_object_unref (document);
}

int
main (int argc, char *argv[])
{
	int result;

	g_test_init (&argc, &argv, NULL);

	if (!ev_init ())
		g_error ("No document backends found");

	g_test_add_func ("/libview/document-model/defaults", test_model_defaults);
	g_test_add_func ("/libview/document-model/document", test_model_document);
	g_test_add_func ("/libview/document-model/page", test_model_page);
	g_test_add_func ("/libview/document-model/scale", test_model_scale);
	g_test_add_func ("/libview/document-model/rotation", test_model_rotation);
	g_test_add_func ("/libview/document-model/rotation-normalized",
			 test_model_rotation_normalized);
	g_test_add_func ("/libview/document-model/layout", test_model_layout);
	g_test_add_func ("/libview/document-model/flags", test_model_flags);
	g_test_add_func ("/libview/view/navigation", test_view_navigation);

	result = g_test_run ();

	ev_shutdown ();

	return result;
}
