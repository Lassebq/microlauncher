#include "gio/gio.h"
#include "glib-object.h"
#include "glib.h"
#include "gtk/gtk.h"
#include "gtk/gtkdropdown.h"
#include "gtk/gtkexpression.h"
#include "gtk/gtkshortcut.h"
#include "pango/pango-layout.h"

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
		gtk_editable_set_text(GTK_EDITABLE(entry), "");
		return;
	}
	gtk_editable_set_text(GTK_EDITABLE(entry), text);
}

const char *gtk_entry_get_text(GtkEntry *entry) {
	return gtk_editable_get_text(GTK_EDITABLE(entry));
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

static void simple_drop_down_setup(GtkListItemFactory *factory, GtkListItem *list_item, gpointer user_data) {
	GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	GtkWidget *label = gtk_label_new(NULL);
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_box_append(GTK_BOX(box), label);
	GtkWidget *icon = g_object_new(GTK_TYPE_IMAGE,
								   "icon-name", "object-select-symbolic",
								   "accessible-role", GTK_ACCESSIBLE_ROLE_PRESENTATION,
								   NULL);
	gtk_box_append(GTK_BOX(box), icon);
	gtk_list_item_set_child(list_item, box);
}

static void simple_drop_down_selection_changed(GtkDropDown *self, GParamSpec *pspec, GtkListItem *list_item) {
	GtkWidget *box;
	GtkWidget *icon;

	box = gtk_list_item_get_child(list_item);
	icon = gtk_widget_get_last_child(box);

	if(gtk_drop_down_get_selected_item(self) == gtk_list_item_get_item(list_item))
		gtk_widget_set_opacity(icon, 1.0);
	else
		gtk_widget_set_opacity(icon, 0.0);
}

static void simple_drop_down_root_changed(GtkWidget *box, GParamSpec *pspec, GtkDropDown *self) {
	GtkWidget *label = gtk_widget_get_first_child(box);
	GtkWidget *icon = gtk_widget_get_last_child(box);

	if(gtk_widget_get_ancestor(box, GTK_TYPE_POPOVER) != NULL) {
		gtk_widget_set_visible(icon, TRUE);
		gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_NONE); // Drop down menu items shouldn't be trimmed down
	} else {
		gtk_widget_set_visible(icon, FALSE);
		gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
	}
}

static void simple_drop_down_bind(GtkListItemFactory *factory, GtkListItem *list_item, gpointer user_data) {
	GtkDropDown *self = user_data;
	GtkStringObject *obj = gtk_list_item_get_item(list_item);
	GtkWidget *box = gtk_list_item_get_child(list_item);
	GtkWidget *label = gtk_widget_get_first_child(box);
	gtk_label_set_label(GTK_LABEL(label), gtk_string_object_get_string(obj));

	g_signal_connect(self, "notify::selected-item", G_CALLBACK(simple_drop_down_selection_changed), list_item);
	simple_drop_down_selection_changed(self, NULL, list_item);
	g_signal_connect(box, "notify::root", G_CALLBACK(simple_drop_down_root_changed), self);
	simple_drop_down_root_changed(box, NULL, self);
}

GtkWidget *gtk_drop_down_simple_new(GtkStringList *list, GtkExpression *expression) {
	GtkDropDown *dropDown = GTK_DROP_DOWN(gtk_drop_down_new(G_LIST_MODEL(list), expression));
	GtkListItemFactory *factory = gtk_signal_list_item_factory_new();
	g_signal_connect(factory, "setup", G_CALLBACK(simple_drop_down_setup), NULL);
	g_signal_connect(factory, "bind", G_CALLBACK(simple_drop_down_bind), dropDown);
	gtk_drop_down_set_factory(dropDown, factory);
	return GTK_WIDGET(dropDown);
}
