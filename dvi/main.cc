#include <gtk/gtk.h>

#include "model.hh"
#include "view.hh"
#include "painter.hh"

int
main (int argc, char *argv[])
{
    GtkWidget *window, *scrwin;

    gtk_init (&argc, &argv);

    Model *model = new Model ("fest.dvi");
    View *view = new View (model);
    
    
    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

    scrwin = gtk_scrolled_window_new (NULL, NULL);

    gtk_container_add (GTK_CONTAINER (window), scrwin);

    gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrwin), view->get_widget());
    
    gtk_widget_show_all (GTK_WIDGET (window));
    
    gtk_main ();
    
    return 0;
}
