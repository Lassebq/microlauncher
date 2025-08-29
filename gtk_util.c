#include "glib-object.h"
#include "glib.h"
#include "gtk/gtk.h"

GtkWidget *gtk_widget_with_label(gchar *labelStr, GtkWidget *widget) {
	GtkBox *box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10));
	gtk_widget_set_hexpand(GTK_WIDGET(box), true);

	gtk_box_append(box, gtk_label_new(labelStr));
	gtk_widget_set_hexpand(widget, true);
	gtk_box_append(box, widget);

	return GTK_WIDGET(box);
}

void gtk_widget_set_margin(GtkWidget *widget, int top, int bottom, int left, int right) {
	gtk_widget_set_margin_top(widget, top);
	gtk_widget_set_margin_bottom(widget, bottom);
	gtk_widget_set_margin_start(widget, left);
	gtk_widget_set_margin_end(widget, right);
}

void gtk_entry_set_text(GtkEntry *entry, const char *text) {
	if(!text) {
		gtk_entry_buffer_set_text(gtk_entry_get_buffer(entry), "", 0);
		return;
	}
	gtk_entry_buffer_set_text(gtk_entry_get_buffer(entry), text, strlen(text));
}

const char *gtk_entry_get_text(GtkEntry *entry) {
	return gtk_entry_buffer_get_text(gtk_entry_get_buffer(entry));
}

GtkWindow *gtk_modal_dialog_new(GtkWindow *parent) {
	GtkWindow *newWindow = GTK_WINDOW(gtk_window_new());
	gtk_window_set_modal(newWindow, true);
	gtk_window_set_transient_for(newWindow, parent);
	GtkShortcutController *controller = GTK_SHORTCUT_CONTROLLER(gtk_shortcut_controller_new());
	GtkShortcutTrigger *trigger = gtk_keyval_trigger_new(GDK_KEY_Escape, 0);
	gtk_shortcut_controller_set_scope(controller, GTK_SHORTCUT_SCOPE_LOCAL);
	gtk_shortcut_controller_add_shortcut(controller,
										 gtk_shortcut_new(trigger, gtk_named_action_new("window.close")));

	gtk_widget_add_controller(GTK_WIDGET(newWindow), GTK_EVENT_CONTROLLER(controller));
	return newWindow;
}

void on_format_number(GtkEditable *editable,
					  const gchar *text,
					  gint length,
					  gint *position,
					  gpointer user_data) {
	if(g_unichar_isdigit(g_utf8_get_char(text))) {
		g_signal_handlers_block_by_func(editable,
										(gpointer)on_format_number,
										user_data);

		gtk_editable_insert_text(editable,
								 text,
								 length,
								 position);

		g_signal_handlers_unblock_by_func(editable,
										  (gpointer)on_format_number,
										  user_data);
	}

	g_signal_stop_emission_by_name(editable, "insert_text");
}

GtkEntry *gtk_entry_digits_only(void) {
	GtkEntry *entry = GTK_ENTRY(gtk_entry_new());
	g_signal_connect(gtk_editable_get_delegate(GTK_EDITABLE(entry)), "insert-text", G_CALLBACK(on_format_number), NULL);
	gtk_entry_set_input_purpose(entry, GTK_INPUT_PURPOSE_DIGITS);
	return entry;
}

void gtk_open_file(const char *path) {
	GFile *file = g_file_new_for_path(path);
	GtkFileLauncher *launcher = gtk_file_launcher_new(file);
	gtk_file_launcher_launch(launcher, NULL, NULL, NULL, NULL);
	g_object_unref(file);
}
