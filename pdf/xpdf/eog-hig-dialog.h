#ifndef _EOG_HIG_DIALOG_H_
#define _EOG_HIG_DIALOG_H_

#include <gtk/gtkdialog.h>

G_BEGIN_DECLS

#define EOG_TYPE_HIG_DIALOG              (eog_hig_dialog_get_type ())
#define EOG_HIG_DIALOG(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), EOG_TYPE_HIG_DIALOG, EogHigDialog))
#define EOG_HIG_DIALOG_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), EOG_TYPE_HIG_DIALOG, EogHigDialogClass))
#define EOG_IS_HIG_DIALOG(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EOG_TYPE_HIG_DIALOG))
#define EOG_IS_HIG_DIALOG_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), EOG_TYPE_HIG_DIALOG))

typedef struct _EogHigDialog EogHigDialog;
typedef struct _EogHigDialogClass EogHigDialogClass;
typedef struct _EogHigDialogPrivate EogHigDialogPrivate;


struct _EogHigDialog {
	GtkDialog dialog;
};

struct _EogHigDialogClass {
	GtkDialogClass parent_class;
};


GType       eog_hig_dialog_get_type         (void);
GtkWidget*  eog_hig_dialog_new              (const char *stock_id, const char *header, const char *body, gboolean modal);


G_END_DECLS

#endif /* _EOG_HIG_DIALOG_H_ */
