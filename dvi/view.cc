#include "view.hh"
#include "dl-dvi-fontdefinition.hh"

using DviLib::DviPage;
using DviLib::DviFontdefinition;

View::View (Model *model_arg)
{
    model = model_arg;
    
    model->add_observer (*this);
    
    drawing_area = gtk_drawing_area_new ();
    gtk_widget_show (drawing_area);
    gtk_drawing_area_size (GTK_DRAWING_AREA (drawing_area), 
			   BASE_DPI * PAPER_WIDTH,
			   BASE_DPI * PAPER_HEIGHT);
    gtk_signal_connect (GTK_OBJECT (drawing_area), "realize",
			(GtkSignalFunc) on_da_realize, this);
    gtk_signal_connect (GTK_OBJECT (drawing_area), "expose_event",
			(GtkSignalFunc) on_da_expose, this);
}

void
View::create_pixmap (void)
{
    pixmap = gdk_pixmap_new(drawing_area->window,
			    BASE_DPI * PAPER_WIDTH,
			    BASE_DPI * PAPER_HEIGHT,
			    -1);
}

void
View::expose (GdkEventExpose *event)
{
    gdk_draw_pixmap (
	drawing_area->window,
	drawing_area->style->fg_gc[GTK_WIDGET_STATE (drawing_area)],
	pixmap,
	event->area.x,     event->area.y,
	event->area.x,     event->area.y,
	event->area.width, event->area.height);
}

void
View::redraw (void) const
{
    // clear page
    gdk_draw_rectangle (pixmap, 
			drawing_area->style->white_gc,
			TRUE, 0, 0,
			BASE_DPI * PAPER_WIDTH,
			BASE_DPI * PAPER_HEIGHT);
    
    cout << "width " << BASE_DPI * PAPER_WIDTH << endl;
    cout << "height " << BASE_DPI * PAPER_HEIGHT << endl;
    
    // create a painter
    DviPainter *painter = 				
	new DviPainter (pixmap,
			drawing_area->style->fg_gc[GTK_WIDGET_STATE 
						   (drawing_area)],
			model->get_dvi_file (),
			BASE_DPI,
			new FontFactory());
    // get page
    DviPage *page;
    try 
    {
	page = model->get_dvi_file ()->get_page (0);
    }
    catch (string s)
    {
	cout << s;
	abort ();
    }
    
    // draw it with the painter
    page->execute (* painter);
    
}

void
View::notify (void) const
{
    ModelState state = model->get_state ();
    
    switch (state) {
    case HAS_FILE: 
	redraw ();
	break;
	
    case NO_FILE:
	break;
	
    case ERROR:
	break;
    }
}

void
on_da_realize (GtkDrawingArea *da, View *v)
{
    v->create_pixmap ();
    v->notify ();
}

gint
on_da_expose (GtkWidget *widget, GdkEventExpose *event, View *v)
{
    v->expose (event);
    return FALSE;
}
