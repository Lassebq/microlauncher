#pragma once
#include "gtk/gtk.h"

GtkWidget *gtk_widget_with_label(gchar *labelStr, GtkWidget *widget);
void gtk_widget_set_margin(GtkWidget *widget, int top, int bottom, int left, int right);
void gtk_entry_set_text(GtkEntry *entry, const char *text);
const char *gtk_entry_get_text(GtkEntry *entry);
GtkWindow *gtk_modal_dialog_new(GtkWindow *parent);
GtkEntry *gtk_entry_digits_only(void);
gboolean gtk_open_file(const char *path);