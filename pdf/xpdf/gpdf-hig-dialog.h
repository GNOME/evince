#ifndef _GPDF_HIG_DIALOG_H_
#define _GPDF_HIG_DIALOG_H_

#include <gtk/gtkdialog.h>

G_BEGIN_DECLS

#define GPDF_TYPE_HIG_DIALOG              (gpdf_hig_dialog_get_type ())
#define GPDF_HIG_DIALOG(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), GPDF_TYPE_HIG_DIALOG, GpdfHigDialog))
#define GPDF_HIG_DIALOG_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GPDF_TYPE_HIG_DIALOG, GpdfHigDialogClass))
#define GPDF_IS_HIG_DIALOG(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GPDF_TYPE_HIG_DIALOG))
#define GPDF_IS_HIG_DIALOG_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GPDF_TYPE_HIG_DIALOG))

typedef struct _GpdfHigDialog GpdfHigDialog;
typedef struct _GpdfHigDialogClass GpdfHigDialogClass;
typedef struct _GpdfHigDialogPrivate GpdfHigDialogPrivate;


struct _GpdfHigDialog {
	GtkDialog dialog;
};

struct _GpdfHigDialogClass {
	GtkDialogClass parent_class;
};


GType       gpdf_hig_dialog_get_type         (void);
GtkWidget*  gpdf_hig_dialog_new              (const char *stock_id, const char *header, const char *body, gboolean modal);


G_END_DECLS

#endif /* _GPDF_HIG_DIALOG_H_ */
