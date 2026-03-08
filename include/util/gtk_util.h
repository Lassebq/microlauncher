#pragma once
#include <gtk/gtk.h>

enum EDialogType {
	DIALOG_NONE,
	DIALOG_YN,
	DIALOG_YN_CANCEL,
	DIALOG_OK,
	DIALOG_OK_CANCEL
};

GtkWidget *gtk_widget_with_label(gchar *labelStr, GtkWidget *widget);

void gtk_widget_set_margin(GtkWidget *widget, int top, int bottom, int left, int right);

void gtk_entry_set_text(GtkEntry *entry, const char *text);

const char *gtk_entry_get_text(GtkEntry *entry);

GtkWindow *gtk_modal_dialog_new(GtkWindow *parent);

GtkEntry *gtk_entry_digits_only(void);

void gtk_open_file(const char *path);

GtkWidget *gtk_drop_down_simple_new(GtkStringList *list, GtkExpression *expression);

void gtk_image_set_from_file_pixbuf(GtkImage *image, const char *path);

void gtk_show_modal_dialog(const char *message, enum EDialogType dialogType, GtkMessageType type, GtkWindow *parent, GCallback callback, gpointer user_data);