// notes:
/*
 * hold en gdkpixbuf ved lige, og tegn den på en gtkdrawingarea 
 * ved passende lejligheder
 */

#include "model.hh"
#include <gtk/gtk.h>
#include "painter.hh"

enum {
    BASE_DPI = 300,
    PAPER_WIDTH = 7,		// inches
    PAPER_HEIGHT = 17		// inches
};

class View : public Observer {
private:
    Model *model;
    GtkWidget *drawing_area;
    GdkPixmap *pixmap;
public:
    View (Model *model_arg);
    
    GtkWidget *get_widget (void) { return drawing_area; }
    
    void create_pixmap (void);
    void expose (GdkEventExpose *event);
    void notify (void) const;
    void redraw (void) const;
};

void on_da_realize (GtkDrawingArea *da,
		    View           *v);
int  on_da_expose  (GtkWidget      *widget,
		    GdkEventExpose *event,
		    View           *v);
