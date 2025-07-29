#include <gtk/gtk.h>

GtkApplication *app;
GtkWindow *window;

static void gtk_widget_set_margin(GtkWidget *widget, int top, int bottom, int left, int right) {
	gtk_widget_set_margin_top(widget, top);
	gtk_widget_set_margin_bottom(widget, bottom);
	gtk_widget_set_margin_start(widget, left);
	gtk_widget_set_margin_end(widget, right);
}

static void activate(GtkApplication *app, gpointer user_data) {
    GtkWidget *widget, *boxOuter, *box, *scrolledWindow, *frame;
	GtkGrid *grid;
	int grid_row;
	
	window = GTK_WINDOW(gtk_window_new());
	gtk_application_add_window(app, window);
	gtk_window_set_default_size(window, 0, 380);
	gtk_window_set_title(window, "MicroLauncher - Minecraft Launcher");

	boxOuter = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
	gtk_widget_set_margin(boxOuter, 10, 10, 10, 10);
	gtk_window_set_child(window, boxOuter);

	scrolledWindow = gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledWindow), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_hexpand(scrolledWindow, true);
	gtk_box_append(GTK_BOX(boxOuter), scrolledWindow);

	box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
	gtk_widget_set_vexpand(GTK_WIDGET(box), true);
	gtk_widget_set_hexpand(GTK_WIDGET(box), true);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolledWindow), box);

	grid = GTK_GRID(gtk_grid_new());
	grid_row = 0;
	gtk_widget_set_margin(GTK_WIDGET(grid), 10, 10, 10, 10);
	gtk_widget_set_hexpand(GTK_WIDGET(grid), true);
	gtk_grid_set_row_spacing(grid, 10);
	gtk_grid_set_column_spacing(grid, 10);
	gtk_box_append(GTK_BOX(box), GTK_WIDGET(grid));

	gtk_box_append(GTK_BOX(boxOuter), GTK_WIDGET(box));

	// g_signal_connect(window, "close-request", G_CALLBACK(confirm_exit), NULL);
	// gtk_window_set_focus(window, playButton);

	gtk_window_present(window);
}

int microlauncher_gui_show(void) {
	int status;
	int argc = 1;
	char *argv[] = {"io.github.lassebq.microlauncher", NULL};

	app = gtk_application_new("io.github.lassebq.microlauncher", G_APPLICATION_NON_UNIQUE);
	g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
	status = g_application_run(G_APPLICATION(app), argc, argv);
	g_object_unref(app);

	return status;
}