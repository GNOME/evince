#include <string.h>
#include <gtk/gtk.h>
#include <libgnome/gnome-macros.h>
#include "gpdf-hig-dialog.h"


GNOME_CLASS_BOILERPLATE (GpdfHigDialog, 
			 gpdf_hig_dialog,
			 GtkDialog,
			 GTK_TYPE_DIALOG);

void
gpdf_hig_dialog_class_init (GpdfHigDialogClass *klass)
{
}

void 
gpdf_hig_dialog_instance_init (GpdfHigDialog *dlg)
{
}


GtkWidget*  
gpdf_hig_dialog_new (const char *stock_id, const char *header, const char *body, gboolean modal)
{
	GtkWidget *dlg;
	GtkWidget *hbox;
	GtkWidget *image;
	GtkWidget *label;
	int header_len;
	int body_len;
	int message_len;
	char *message;
	
	g_return_val_if_fail (stock_id != NULL, NULL);
	g_return_val_if_fail (header != NULL, NULL);
	
	dlg = gtk_widget_new (GPDF_TYPE_HIG_DIALOG, 
			      "border-width", 6, 
			      "resizable", FALSE,
			      "has-separator", FALSE,
			      "modal", modal,
			      "title", "",
			      NULL);
			      
	hbox = gtk_widget_new (GTK_TYPE_HBOX, 
			       "homogeneous", FALSE,
			       "spacing", 12,
			       "border-width", 6,
			       NULL);

	image = gtk_widget_new (GTK_TYPE_IMAGE,
				"stock", stock_id,
				"icon-size", GTK_ICON_SIZE_DIALOG,
				"yalign", 0.0,
				NULL);
	gtk_container_add (GTK_CONTAINER (hbox), image);


	header_len = strlen (header);
	body_len = body ? strlen (body) : 0;
	message_len = header_len + body_len + 64;

	message = g_new0 (char, message_len);

	if (body != NULL) {
		g_snprintf (message, message_len,
			    "<span weight=\"bold\" size=\"larger\">%s</span>\n\n%s", header, body);
	}
	else {
		g_snprintf (message, message_len,
			    "<span weight=\"bold\" size=\"larger\">%s</span>\n", header);
	}
	
	label = gtk_label_new (message);
	g_object_set (G_OBJECT (label),
		      "use-markup", TRUE, 
		      "wrap", TRUE,
		      "yalign", 0.0,
		      NULL); 
			       
	gtk_container_add (GTK_CONTAINER (hbox), label);
	gtk_widget_show_all (hbox);

	gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dlg)->vbox), hbox);
	g_object_set (G_OBJECT (GTK_DIALOG (dlg)->vbox), "spacing", 12, NULL);

	return dlg;
}
