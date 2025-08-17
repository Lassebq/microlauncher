#include "gdk/gdkkeysyms.h"
#include "gio/gio.h"
#include "gio/gmenumodel.h"
#include "glib-object.h"
#include "gtk_util.h"
#include "json_object.h"
#include "json_types.h"
#include "json_util.h"
#include "microlauncher.h"
#include "microlauncher_msa.h"
#include "microlauncher_version_item.h"
#include "util.h"
#include <curl/curl.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <linux/limits.h>
#include <math.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define APPID "io.github.lassebq.microlauncher"

static GtkApplication *app;
static GtkWindow *window;
static GtkWindow *popupWindow;
static GtkButton *playButton;
static GtkFrame *instanceFrame;
static GtkFrame *userFrame;
static GtkLabel *progressStage;
static GtkProgressBar *progressBar;
static GtkStack *launcherStack;
static GtkCheckButton *checkFullscreen;
static GtkCheckButton *checkDemo;
static GtkEntry *widthEntry;
static GtkEntry *heightEntry;
static GtkRevealer *revealer;

static GtkWidget *accountsPage;
static GtkStringList *accountsList;
static GtkWidget *instancesPage;
static GtkStringList *instancesList;

static GtkLabel *msaCodeLabel;
static GtkLabel *msaHint;
static GtkProgressBar *msaTimeout;
static GtkSpinner *msaSpinner;
static int msaSecondsLeft;

static pid_t instancePid = 0;

static struct Settings *settings;

static void microlauncher_gui_refresh_instance(void);

static void remove_account(struct User *account) {
	GSList **accounts = microlauncher_get_accounts();
	gtk_string_list_remove(accountsList, gtk_string_list_find(accountsList, account->id));
	*accounts = g_slist_remove(*accounts, account);
	if(settings->user == account) {
		settings->user = NULL;
	}
	free(account);
	microlauncher_gui_refresh_instance();
}

static void remove_instance(struct Instance *instance) {
	GSList **instances = microlauncher_get_instances();
	gtk_string_list_remove(instancesList, gtk_string_list_find(instancesList, instance->name));
	*instances = g_slist_remove(*instances, instance);
	if(settings->instance == instance) {
		settings->instance = NULL;
	}
	free(instance);
	microlauncher_gui_refresh_instance();
}

static void add_instance(struct Instance *instance) {
	GSList **instances = microlauncher_get_instances();
	*instances = g_slist_append(*instances, instance);
	gtk_string_list_append(instancesList, instance->name);
}

static void add_account(struct User *user) {
	GSList **accounts = microlauncher_get_accounts();
	*accounts = g_slist_append(*accounts, user);
	gtk_string_list_append(accountsList, user->id);
}

static void browse_directory_click(GtkButton *button, const gpointer p) {
	gtk_open_file(p);
}

static void setup_column_cb(GtkSignalListItemFactory *factory, GObject *listitem) {
	GtkWidget *label = gtk_label_new(NULL);
	gtk_widget_set_halign(label, GTK_ALIGN_START);
	gtk_list_item_set_child(GTK_LIST_ITEM(listitem), label);
}

static void bind_column_cb(GtkSignalListItemFactory *factory, GtkListItem *listitem, void *userdata) {
	GtkWidget *label = gtk_list_item_get_child(listitem);
	VersionItem *item = gtk_list_item_get_item(GTK_LIST_ITEM(listitem));
	g_object_bind_property(item, userdata, label, "label", G_BINDING_SYNC_CREATE);
}

static void select_instance_directory(GtkFileDialog *dialog, GAsyncResult *res, gpointer data) {
	GtkEntry *entry = GTK_ENTRY(data);
	GFile *file = gtk_file_dialog_select_folder_finish(dialog, res, NULL);
	if(file) {
		gtk_entry_set_text(entry, g_file_peek_path(file));
		g_object_unref(file);
	}
}

static void select_instance_dir_event(GtkButton *button, gpointer user_data) {
	GtkEntry *entry = GTK_ENTRY(user_data);
	gtk_file_dialog_select_folder(gtk_file_dialog_new(), window, NULL, (GAsyncReadyCallback)select_instance_directory, entry);
}

static void select_instance_java(GtkFileDialog *dialog, GAsyncResult *res, gpointer data) {
	GtkEntry *entry = GTK_ENTRY(data);
	GFile *file = gtk_file_dialog_open_finish(dialog, res, NULL);
	if(file) {
		gtk_entry_set_text(entry, g_file_peek_path(file));
		g_object_unref(file);
	}
}

static void select_instance_java_event(GtkButton *button, gpointer user_data) {
	GtkEntry *entry = GTK_ENTRY(user_data);
	gtk_file_dialog_open(gtk_file_dialog_new(), window, NULL, (GAsyncReadyCallback)select_instance_java, entry);
}

static void select_instance_icon(GtkFileDialog *dialog, GAsyncResult *res, gpointer data) {
	GtkImage *image = GTK_IMAGE(data);
	GFile *file = gtk_file_dialog_open_finish(dialog, res, NULL);
	if(file) {
		gtk_image_set_from_file(image, g_file_peek_path(file));
		g_object_set_data_full(G_OBJECT(image), "icon-location", g_file_get_path(file), g_free);
		g_object_unref(file);
	}
}

static void icon_released(GtkGestureClick *self, gint n_press, gdouble x, gdouble y, gpointer user_data) {
	GtkImage *image = GTK_IMAGE(user_data);
	gtk_file_dialog_open(gtk_file_dialog_new(), window, NULL, (GAsyncReadyCallback)select_instance_icon, image);
}

static void cancel_callback(GtkButton *button, GtkWindow *data) {
	gtk_window_destroy(data);
}

static void add_version(gpointer key, struct Version *value, GListStore *store) {
	g_list_store_append(store, microlauncher_version_item_new(value->id, value->type, value->releaseTime));
}

struct CreateInstance {
	GtkEntry *instanceName;
	GtkEntry *instanceDir;
	GtkEntry *instanceJvm;
	GtkImage *instanceIcon;
	GtkColumnView *versionView;
	GtkWindow *dialog;
	struct Instance *toReplace;
};

static void create_or_modify_instance(GtkButton *button, void *userdata) {
	struct CreateInstance *createInstance = (struct CreateInstance *)userdata;
	struct Instance *inst = g_new0(struct Instance, 1);
	inst->name = g_strdup(gtk_entry_get_text(createInstance->instanceName));
	inst->location = g_strdup(gtk_entry_get_text(createInstance->instanceDir));
	inst->javaLocation = g_strdup(gtk_entry_get_text(createInstance->instanceJvm));
	inst->icon = g_strdup(g_object_get_data(G_OBJECT(createInstance->instanceIcon), "icon-location"));
	GtkSingleSelection *selection = GTK_SINGLE_SELECTION(gtk_column_view_get_model(createInstance->versionView));
	VersionItem *item = gtk_single_selection_get_selected_item(selection);
	inst->version = g_strdup(item->version);
	if(createInstance->toReplace) {
		remove_instance(createInstance->toReplace);
	}
	add_instance(inst);
	gtk_window_destroy(createInstance->dialog);
}

static void microlauncher_modify_instance_window(GtkButton *button, struct Instance *instance) {
	GtkWidget *widget;
	GtkScrolledWindow *scrolledWindow;
	GtkBox *box;
	GtkGrid *grid;
	GtkEntry *entry;
	struct CreateInstance *createInstance = g_new(struct CreateInstance, 1);
	GtkWindow *newWindow = gtk_modal_dialog_new(window);
	createInstance->toReplace = instance;
	createInstance->dialog = newWindow;
	gtk_window_set_title(newWindow, instance ? "Modify instance" : "New instance");
	gtk_window_set_default_size(newWindow, 600, 400);
	widget = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
	gtk_widget_set_margin(widget, 10, 10, 10, 10);
	box = GTK_BOX(widget);
	gtk_window_set_child(newWindow, widget);

	widget = gtk_grid_new();
	gtk_widget_set_margin_bottom(widget, 10);
	grid = GTK_GRID(widget);
	gtk_grid_set_row_spacing(grid, 10);
	gtk_grid_set_column_spacing(grid, 10);
	gtk_box_append(box, widget);

	widget = gtk_image_new();
	createInstance->instanceIcon = GTK_IMAGE(widget);
	gtk_image_set_pixel_size(GTK_IMAGE(widget), 64);
	if(instance && instance->icon) {
		gtk_image_set_from_file(GTK_IMAGE(widget), instance->icon);
		g_object_set_data_full(G_OBJECT(widget), "icon-location", g_strdup(instance->icon), g_free);
	} else {
		gtk_image_set_from_icon_name(GTK_IMAGE(widget), "insert-image");
	}
	GtkGesture *controller = gtk_gesture_click_new();
	g_signal_connect(controller, "released", G_CALLBACK(icon_released), widget);
	gtk_widget_add_controller(widget, GTK_EVENT_CONTROLLER(controller));
	gtk_grid_attach(grid, widget, 0, 0, 1, 2);

	widget = gtk_label_new("Instance name:");
	gtk_widget_set_halign(widget, GTK_ALIGN_START);
	gtk_grid_attach(grid, widget, 1, 0, 1, 1);

	widget = gtk_entry_new();
	if(instance) {
		gtk_entry_set_text(GTK_ENTRY(widget), instance->name);
	}
	gtk_grid_attach(grid, widget, 2, 0, 1, 1);
	createInstance->instanceName = GTK_ENTRY(widget);

	widget = gtk_label_new("Game directory:");
	gtk_widget_set_halign(widget, GTK_ALIGN_START);
	gtk_grid_attach(grid, widget, 1, 1, 1, 1);

	widget = gtk_entry_new();
	entry = GTK_ENTRY(widget);
	if(instance) {
		gtk_entry_set_text(entry, instance->location);
	}
	createInstance->instanceDir = entry;
	gtk_widget_set_hexpand(widget, true);
	GtkBox *container = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10));
	gtk_box_append(container, widget);
	widget = gtk_button_new_with_label("Select");
	g_signal_connect(widget, "clicked", G_CALLBACK(select_instance_dir_event), entry);
	gtk_box_append(container, widget);
	gtk_grid_attach(grid, GTK_WIDGET(container), 2, 1, 1, 1);

	widget = gtk_label_new("Java executable:");
	gtk_widget_set_halign(widget, GTK_ALIGN_START);
	gtk_grid_attach(grid, widget, 1, 2, 1, 1);

	widget = gtk_entry_new();
	entry = GTK_ENTRY(widget);
	if(instance) {
		gtk_entry_set_text(entry, instance->javaLocation);
	}
	createInstance->instanceJvm = entry;
	gtk_widget_set_hexpand(widget, true);
	container = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10));
	gtk_box_append(container, widget);
	widget = gtk_button_new_with_label("Select");
	g_signal_connect(widget, "clicked", G_CALLBACK(select_instance_java_event), entry);
	gtk_box_append(container, widget);
	gtk_grid_attach(grid, GTK_WIDGET(container), 2, 2, 1, 1);
	widget = gtk_label_new("Select version:");
	gtk_widget_set_halign(widget, GTK_ALIGN_START);
	gtk_box_append(box, widget);
	widget = gtk_scrolled_window_new();
	gtk_widget_set_hexpand(widget, true);
	gtk_widget_set_vexpand(widget, true);
	scrolledWindow = GTK_SCROLLED_WINDOW(widget);

	GListStore *store = g_list_store_new(G_TYPE_OBJECT);
	GHashTable *manifest = microlauncher_get_manifest();
	g_hash_table_foreach(manifest, (GHFunc)add_version, store);
	widget = gtk_column_view_new(NULL);
	createInstance->versionView = GTK_COLUMN_VIEW(widget);
	gtk_column_view_set_show_column_separators(GTK_COLUMN_VIEW(widget), true);

	GtkListItemFactory *factory = gtk_signal_list_item_factory_new();
	g_signal_connect(factory, "setup", G_CALLBACK(setup_column_cb), NULL);
	g_signal_connect(factory, "bind", G_CALLBACK(bind_column_cb), "version");
	GtkColumnViewColumn *column = gtk_column_view_column_new("Version", factory);
	GtkSorter *sorter = GTK_SORTER(gtk_string_sorter_new(gtk_property_expression_new(MICROLAUNCHER_VERSION_ITEM_TYPE, NULL, "version")));
	gtk_column_view_column_set_sorter(column, GTK_SORTER(sorter));
	gtk_column_view_column_set_resizable(column, true);
	gtk_column_view_append_column(GTK_COLUMN_VIEW(widget), column);

	factory = gtk_signal_list_item_factory_new();
	g_signal_connect(factory, "setup", G_CALLBACK(setup_column_cb), NULL);
	g_signal_connect(factory, "bind", G_CALLBACK(bind_column_cb), "type");
	column = gtk_column_view_column_new("Type", factory);
	sorter = GTK_SORTER(gtk_string_sorter_new(gtk_property_expression_new(MICROLAUNCHER_VERSION_ITEM_TYPE, NULL, "type")));
	gtk_column_view_column_set_sorter(column, GTK_SORTER(sorter));
	gtk_column_view_column_set_resizable(column, true);
	gtk_column_view_append_column(GTK_COLUMN_VIEW(widget), column);

	factory = gtk_signal_list_item_factory_new();
	g_signal_connect(factory, "setup", G_CALLBACK(setup_column_cb), NULL);
	g_signal_connect(factory, "bind", G_CALLBACK(bind_column_cb), "releaseTime");
	column = gtk_column_view_column_new("Release Time", factory);
	sorter = GTK_SORTER(gtk_string_sorter_new(gtk_property_expression_new(MICROLAUNCHER_VERSION_ITEM_TYPE, NULL, "releaseTime")));
	gtk_column_view_column_set_expand(column, true);
	gtk_column_view_column_set_sorter(column, GTK_SORTER(sorter));
	gtk_column_view_column_set_resizable(column, true);
	gtk_column_view_append_column(GTK_COLUMN_VIEW(widget), column);
	gtk_column_view_sort_by_column(GTK_COLUMN_VIEW(widget), column, GTK_SORT_DESCENDING);

	sorter = g_object_ref(gtk_column_view_get_sorter(GTK_COLUMN_VIEW(widget)));
	GtkSortListModel *model = gtk_sort_list_model_new(G_LIST_MODEL(store), sorter);
	GtkSingleSelection *selection = gtk_single_selection_new(G_LIST_MODEL(model));
	gtk_column_view_set_model(GTK_COLUMN_VIEW(widget), GTK_SELECTION_MODEL(selection));
	if(instance) {
		guint n = g_list_model_get_n_items(G_LIST_MODEL(model));
		for(guint i = 0; i < n; i++) {
			VersionItem *item = g_list_model_get_item(G_LIST_MODEL(model), i);
			if(strequal(instance->version, item->version)) {
				gtk_single_selection_set_selected(selection, i);
				break;
			}
		}
	}

	gtk_scrolled_window_set_child(scrolledWindow, widget);
	gtk_box_append(box, GTK_WIDGET(scrolledWindow));

	container = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10));
	gtk_box_set_homogeneous(container, true);
	widget = gtk_button_new_with_label("Cancel");
	gtk_widget_set_hexpand(widget, true);
	g_signal_connect(widget, "clicked", G_CALLBACK(cancel_callback), newWindow);
	gtk_box_append(container, widget);
	widget = gtk_button_new_with_label(instance ? "Modify instance" : "Create instance");
	gtk_widget_set_hexpand(widget, true);
	g_signal_connect_data(widget, "clicked", G_CALLBACK(create_or_modify_instance), createInstance, (GClosureNotify)g_free, 0);
	gtk_box_append(container, widget);
	gtk_box_append(box, GTK_WIDGET(container));

	gtk_window_present(newWindow);
}

static void microlauncher_gui_switch_to_tab(GtkButton *button, const char *tab) {
	gtk_stack_set_visible_child_name(launcherStack, tab);
}

static void microlauncher_gui_refresh_instance(void) {
	GtkWidget *widget, *box;
	GtkGrid *titleArea, *grid;
	int grid_row;
	char *label;

	if(settings->instance) {
		grid = GTK_GRID(gtk_grid_new());
		grid_row = 0;
		gtk_widget_set_margin(GTK_WIDGET(grid), 0, 10, 10, 10);
		gtk_widget_set_hexpand(GTK_WIDGET(grid), true);
		gtk_grid_set_row_spacing(grid, 10);
		gtk_grid_set_column_spacing(grid, 10);
		gtk_frame_set_child(instanceFrame, GTK_WIDGET(grid));

		titleArea = GTK_GRID(gtk_grid_new());
		gtk_grid_set_row_spacing(titleArea, 0);
		gtk_grid_set_column_spacing(titleArea, 0);

		gtk_grid_attach(grid, GTK_WIDGET(titleArea), 0, grid_row++, 1, 1);

		struct stat st;
		if(access(settings->instance->icon, F_OK) == 0 && stat(settings->instance->icon, &st) == 0 && S_ISREG(st.st_mode)) {
			GtkWidget *icon;
			icon = gtk_image_new();
			gtk_image_set_pixel_size(GTK_IMAGE(icon), 64);
			gtk_widget_set_margin_end(icon, 20);
			gtk_image_set_from_file(GTK_IMAGE(icon), settings->instance->icon);
			gtk_grid_attach(titleArea, GTK_WIDGET(icon), 0, 0, 1, 2);
		}

		widget = gtk_label_new(settings->instance->name);
		gtk_widget_add_css_class(widget, "title-1");
		gtk_label_set_xalign(GTK_LABEL(widget), 0.0F);
		gtk_widget_set_hexpand(widget, true);
		gtk_grid_attach(titleArea, widget, 1, 0, 1, 1);

		label = g_strdup_printf("Version %s", settings->instance->version);
		widget = gtk_label_new(label);
		free(label);
		gtk_label_set_xalign(GTK_LABEL(widget), 0.0F);
		gtk_widget_set_hexpand(widget, true);
		gtk_widget_set_margin(widget, 10, 10, 0, 0);
		gtk_grid_attach(titleArea, widget, 1, 1, 1, 1);

		widget = gtk_entry_new();
		gtk_editable_set_editable(GTK_EDITABLE(widget), false);
		gtk_widget_set_hexpand(widget, true);
		if(settings->instance->location) {
			gtk_entry_set_text(GTK_ENTRY(widget), settings->instance->location);
		}
		GtkBox *container = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10));
		gtk_box_append(container, widget);
		widget = gtk_button_new_with_label("Browse");
		g_signal_connect(widget, "clicked", G_CALLBACK(browse_directory_click), settings->instance->location);
		gtk_box_append(container, widget);
		widget = gtk_widget_with_label("Game directory:", GTK_WIDGET(container));
		gtk_grid_attach(titleArea, widget, 0, 2, 3, 1);
	} else {
		box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
		gtk_widget_set_margin(box, 10, 10, 10, 10);
		gtk_frame_set_child(instanceFrame, box);
		widget = gtk_label_new("No active instance");
		gtk_widget_add_css_class(widget, "title-2");
		gtk_box_append(GTK_BOX(box), widget);
	}

	widget = gtk_button_new_with_label("Change");
	g_signal_connect(widget, "clicked", G_CALLBACK(microlauncher_gui_switch_to_tab), "instances");
	gtk_widget_set_margin(widget, 0, 0, 10, 0);
	gtk_widget_set_valign(widget, GTK_ALIGN_CENTER);
	gtk_widget_set_halign(widget, GTK_ALIGN_END);
	gtk_widget_set_hexpand(widget, true);
	if(settings->instance) {
		gtk_grid_attach(titleArea, widget, 2, 0, 1, 2);
		gtk_frame_set_label(instanceFrame, "Instance");
	} else {
		gtk_box_append(GTK_BOX(box), widget);
		gtk_frame_set_label(instanceFrame, NULL);
	}

	box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
	gtk_widget_set_margin(box, 10, 10, 10, 10);
	if(settings->user) {
		gtk_box_append(GTK_BOX(box), gtk_label_new("User:"));
		widget = gtk_entry_new();
		gtk_box_append(GTK_BOX(box), widget);
		gtk_editable_set_editable(GTK_EDITABLE(widget), false);
		gtk_widget_set_hexpand(widget, true);
		char *str = g_strdup_printf("%s (%s)", settings->user->name, microlauncher_get_account_type_name(settings->user->type));
		gtk_entry_set_text(GTK_ENTRY(widget), str);
		free(str);
	} else {
		widget = gtk_label_new("No active user");
		gtk_widget_set_halign(widget, GTK_ALIGN_START);
		gtk_widget_set_hexpand(widget, true);
		gtk_widget_add_css_class(widget, "title-2");
		gtk_box_append(GTK_BOX(box), widget);
	}
	widget = gtk_button_new_with_label("Change");
	g_signal_connect(widget, "clicked", G_CALLBACK(microlauncher_gui_switch_to_tab), "accounts");
	gtk_widget_set_valign(widget, GTK_ALIGN_CENTER);
	gtk_widget_set_halign(widget, GTK_ALIGN_END);
	gtk_box_append(GTK_BOX(box), widget);
	gtk_frame_set_child(userFrame, box);

	gtk_widget_set_sensitive(GTK_WIDGET(playButton), settings->instance && settings->user);
}

static gboolean microlauncher_gui_enable_play(void *userdata) {
	instancePid = 0;
	gtk_button_set_label(playButton, "Play");
	gtk_widget_set_sensitive(GTK_WIDGET(playButton), true);
	gtk_widget_set_sensitive(GTK_WIDGET(instancesPage), true);
	gtk_widget_set_sensitive(GTK_WIDGET(accountsPage), true);
	return false;
}

static void launch_instance_thread(GTask *task, gpointer source_object, gpointer task_data, GCancellable *cancellable) {
	microlauncher_launch_instance(settings->instance, settings->user);
	g_idle_add(G_SOURCE_FUNC(microlauncher_gui_enable_play), NULL);
}

static void clicked_play(void) {
	if(instancePid != 0) {
		kill(instancePid, SIGKILL);
		return;
	}
	gtk_widget_set_sensitive(GTK_WIDGET(playButton), false);
	gtk_widget_set_sensitive(GTK_WIDGET(instancesPage), false);
	gtk_widget_set_sensitive(GTK_WIDGET(accountsPage), false);
	settings->width = atoi(gtk_entry_buffer_get_text(gtk_entry_get_buffer(widthEntry)));
	settings->height = atoi(gtk_entry_buffer_get_text(gtk_entry_get_buffer(heightEntry)));
	settings->fullscreen = gtk_check_button_get_active(checkFullscreen);
	settings->demo = gtk_check_button_get_active(checkDemo);
	GTask *task = g_task_new(playButton, NULL, NULL, NULL);
	g_task_run_in_thread(task, launch_instance_thread);
}

static GtkWidget *microlauncher_gui_page_launcher(void) {
	GtkWidget *widget, *box, *boxOuter, *scrolledWindow, *frame;
	GtkGrid *grid;
	int grid_row;

	boxOuter = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
	gtk_widget_set_vexpand(boxOuter, true);
	gtk_widget_set_margin(boxOuter, 10, 10, 10, 10);

	scrolledWindow = gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledWindow), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_vexpand(scrolledWindow, true);
	gtk_box_append(GTK_BOX(boxOuter), scrolledWindow);

	box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolledWindow), box);

	frame = gtk_frame_new("Instance");
	instanceFrame = GTK_FRAME(frame);
	gtk_widget_set_margin(frame, 10, 10, 10, 10);
	gtk_frame_set_label_align(GTK_FRAME(frame), 0.5F);
	gtk_box_append(GTK_BOX(box), frame);

	frame = gtk_frame_new(NULL);
	userFrame = GTK_FRAME(frame);
	gtk_widget_set_margin(frame, 10, 10, 10, 10);
	gtk_box_append(GTK_BOX(box), frame);

	frame = gtk_frame_new("Game settings");
	gtk_widget_set_margin(frame, 10, 10, 10, 10);
	grid = GTK_GRID(gtk_grid_new());
	grid_row = 0;
	gtk_grid_set_column_spacing(grid, 10);
	gtk_grid_set_row_spacing(grid, 10);
	gtk_widget_set_margin(GTK_WIDGET(grid), 10, 10, 10, 10);
	gtk_frame_set_child(GTK_FRAME(frame), GTK_WIDGET(grid));

	widget = gtk_check_button_new_with_label("Fullscreen");
	checkFullscreen = GTK_CHECK_BUTTON(widget);
	gtk_widget_set_hexpand(widget, false);
	gtk_grid_attach(grid, widget, 0, grid_row++, 2, 1);

	widget = gtk_label_new("Window sizes");
	gtk_widget_add_css_class(widget, "label");
	gtk_label_set_xalign(GTK_LABEL(widget), 0.0F);
	gtk_grid_attach(grid, widget, 0, grid_row++, 2, 1);

	widthEntry = gtk_entry_digits_only();
	widget = GTK_WIDGET(widthEntry);
	widget = gtk_widget_with_label("Width:", widget);
	gtk_entry_set_placeholder_text(widthEntry, "854");
	gtk_widget_set_margin_start(widget, 10);
	gtk_grid_attach(grid, widget, 0, grid_row, 1, 1);

	heightEntry = gtk_entry_digits_only();
	widget = GTK_WIDGET(heightEntry);
	widget = gtk_widget_with_label("Height:", widget);
	gtk_entry_set_placeholder_text(heightEntry, "480");
	gtk_grid_attach(grid, widget, 1, grid_row++, 1, 1);

	widget = gtk_check_button_new_with_label("Demo mode");
	checkDemo = GTK_CHECK_BUTTON(widget);
	gtk_widget_set_hexpand(widget, false);
	gtk_grid_attach(grid, widget, 0, grid_row++, 2, 1);

	gtk_box_append(GTK_BOX(box), frame);

	widget = gtk_button_new_with_label("Play");
	gtk_widget_set_sensitive(widget, false);
	playButton = GTK_BUTTON(widget);
	g_signal_connect(playButton, "clicked", G_CALLBACK(clicked_play), NULL);
	gtk_box_append(GTK_BOX(boxOuter), widget);
	return boxOuter;
}

struct InstanceRowWidgets {
	GtkWidget *grid;
	GtkLabel *instanceLabel;
	GtkLabel *versionLabel;
	GtkLabel *locationLabel;
	GtkImage *icon;
};

static void show_row_menu(GtkGestureClick *gesture, gint n_press, gdouble x, gdouble y, gpointer user_data) {
	GtkWidget *row_widget = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(gesture));
	GtkPopover *popover = GTK_POPOVER(gtk_popover_menu_new_from_model(G_MENU_MODEL(user_data)));

	gtk_popover_set_has_arrow(popover, false);
	gtk_popover_set_pointing_to(popover, &(GdkRectangle){(int)x, (int)y, 1, 1});
	gtk_widget_set_halign(GTK_WIDGET(popover), GTK_ALIGN_START);
	gtk_widget_set_parent(GTK_WIDGET(popover), row_widget);
	gtk_popover_popup(popover);
}

static void instance_list_view_setup_factory(GtkListItemFactory *factory, GtkListItem *list_item, gpointer user_data) {
	GtkWidget *widget;
	widget = gtk_grid_new();
	GtkGesture *click = gtk_gesture_click_new();
	gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click), GDK_BUTTON_SECONDARY);
	GMenu *menu = g_menu_new();
	g_menu_append(menu, "Edit", "instance.edit");
	g_menu_append(menu, "Delete", "instance.delete");
	g_signal_connect(click, "pressed", G_CALLBACK(show_row_menu), menu);
	gtk_widget_add_controller(widget, GTK_EVENT_CONTROLLER(click));
	gtk_widget_set_hexpand(widget, true);
	GtkGrid *grid = GTK_GRID(widget);
	struct InstanceRowWidgets *rw = g_new0(struct InstanceRowWidgets, 1);
	rw->grid = widget;

	widget = gtk_image_new();
	gtk_image_set_pixel_size(GTK_IMAGE(widget), 48);
	gtk_widget_set_margin_end(widget, 10);
	rw->icon = GTK_IMAGE(widget);
	gtk_grid_attach(grid, widget, 0, 0, 1, 2);

	widget = gtk_label_new(NULL);
	rw->instanceLabel = GTK_LABEL(widget);
	gtk_widget_set_halign(widget, GTK_ALIGN_START);
	gtk_widget_set_hexpand(widget, true);
	gtk_widget_add_css_class(widget, "title");
	gtk_grid_attach(grid, widget, 1, 0, 1, 1);

	widget = gtk_label_new(NULL);
	rw->locationLabel = GTK_LABEL(widget);
	gtk_widget_set_halign(widget, GTK_ALIGN_END);
	gtk_widget_set_vexpand(widget, true);
	gtk_widget_add_css_class(widget, "subtitle");
	gtk_grid_attach(grid, widget, 2, 0, 1, 2);

	widget = gtk_label_new(NULL);
	rw->versionLabel = GTK_LABEL(widget);
	gtk_widget_set_halign(widget, GTK_ALIGN_START);
	gtk_widget_add_css_class(widget, "subtitle");
	gtk_grid_attach(grid, widget, 1, 1, 1, 1);

	gtk_list_item_set_child(list_item, GTK_WIDGET(grid));
	g_object_set_data_full(G_OBJECT(list_item), "row-widgets", rw, g_free);
}

static void instance_action(GSimpleAction *simple_action, G_GNUC_UNUSED GVariant *parameter, gpointer *data) {
	struct Instance *inst = (struct Instance *)data;
	if(strcmp(g_action_get_name(G_ACTION(simple_action)), "edit") == 0) {
		microlauncher_modify_instance_window(NULL, inst);
	} else if(strcmp(g_action_get_name(G_ACTION(simple_action)), "delete") == 0) {
		remove_instance(inst);
	}
}

static void instance_list_view_bind_factory(GtkListItemFactory *factory, GtkListItem *list_item, gpointer user_data) {
	struct InstanceRowWidgets *rw = g_object_get_data(G_OBJECT(list_item), "row-widgets");

	const char *str = gtk_string_object_get_string(GTK_STRING_OBJECT(gtk_list_item_get_item(list_item)));
	GSList *list = *microlauncher_get_instances();
	struct Instance *instance = microlauncher_instance_get(list, str);
	GSimpleActionGroup *actions = g_simple_action_group_new();
	GSimpleAction *action;

	action = g_simple_action_new("edit", NULL);
	g_signal_connect(action, "activate", G_CALLBACK(instance_action), instance);
	g_action_map_add_action(G_ACTION_MAP(actions), G_ACTION(action));

	action = g_simple_action_new("delete", NULL);
	g_signal_connect(action, "activate", G_CALLBACK(instance_action), instance);
	g_action_map_add_action(G_ACTION_MAP(actions), G_ACTION(action));

	gtk_widget_insert_action_group(rw->grid, "instance", G_ACTION_GROUP(actions));

	gtk_image_set_from_file(rw->icon, instance->icon);
	gtk_label_set_label(rw->instanceLabel, instance->name);
	char path[PATH_MAX];
	int truncDir = MAX(0, (int)strlen(instance->location) - 40);
	const char *c = instance->location + truncDir;
	while(*c != '\0' && *c != '/') {
		c++;
	}
	snprintf(path, PATH_MAX, (truncDir == 0) ? "%s" : "...%s", c);
	gtk_label_set_label(rw->locationLabel, path);
	gtk_label_set_label(rw->versionLabel, instance->version);
}

static void instance_selection_changed(GtkSingleSelection *self, guint position, guint n_items, GtkStringList *user_data) {
	GSList *instances = *microlauncher_get_instances();
	settings->instance = microlauncher_instance_get(instances, gtk_string_list_get_string(user_data, gtk_single_selection_get_selected(self)));
	microlauncher_gui_refresh_instance();
}

static void activate_instance_row(GtkListView *self, guint position, GtkStringList *user_data) {
	GSList *instances = *microlauncher_get_instances();
	settings->instance = microlauncher_instance_get(instances, gtk_string_list_get_string(user_data, position));
	microlauncher_gui_refresh_instance();
	microlauncher_gui_switch_to_tab(NULL, "launcher");
	clicked_play();
}

static void account_selection_changed(GtkSingleSelection *self, guint position, guint n_items, GtkStringList *user_data) {
	GSList *accounts = *microlauncher_get_accounts();
	settings->user = microlauncher_account_get(accounts, gtk_string_list_get_string(user_data, gtk_single_selection_get_selected(self)));
	microlauncher_gui_refresh_instance();
}

static void activate_account_row(GtkListView *self, guint position, GtkStringList *user_data) {
	GSList *accounts = *microlauncher_get_accounts();
	settings->user = microlauncher_account_get(accounts, gtk_string_list_get_string(user_data, position));
	microlauncher_gui_refresh_instance();
	microlauncher_gui_switch_to_tab(NULL, "launcher");
}

static GtkWidget *microlauncher_gui_page_instances(void) {
	GtkWidget *widget, *box, *boxOuter, *scrolledWindow;
	boxOuter = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
	gtk_widget_set_vexpand(boxOuter, true);
	gtk_widget_set_margin(boxOuter, 10, 10, 10, 10);

	scrolledWindow = gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledWindow), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_vexpand(scrolledWindow, true);

	box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolledWindow), box);
	instancesList = gtk_string_list_new(NULL);

	GSList *list = *microlauncher_get_instances();
	for(guint i = 0; i < g_slist_length(list); i++) {
		const struct Instance *inst = g_slist_nth(list, i)->data;
		gtk_string_list_append(instancesList, inst->name);
	}

	GtkSingleSelection *selection = gtk_single_selection_new(G_LIST_MODEL(instancesList));
	GtkListItemFactory *factory = gtk_signal_list_item_factory_new();
	g_signal_connect(factory, "setup", G_CALLBACK(instance_list_view_setup_factory), NULL);
	g_signal_connect(factory, "bind", G_CALLBACK(instance_list_view_bind_factory), NULL);

	widget = gtk_list_view_new(GTK_SELECTION_MODEL(selection), factory);
	if(settings->instance) {
		gtk_single_selection_set_selected(selection, gtk_string_list_find(instancesList, settings->instance->name));
	}
	g_signal_connect(selection, "selection-changed", G_CALLBACK(instance_selection_changed), instancesList);
	g_signal_connect(widget, "activate", G_CALLBACK(activate_instance_row), instancesList);
	gtk_widget_add_css_class(widget, "rich-list");
	gtk_widget_add_css_class(widget, "boxed-list");

	gtk_box_append(GTK_BOX(box), widget);
	gtk_box_append(GTK_BOX(boxOuter), scrolledWindow);

	widget = gtk_button_new_with_label("New instance");
	g_signal_connect(widget, "clicked", G_CALLBACK(microlauncher_modify_instance_window), NULL);
	gtk_box_append(GTK_BOX(boxOuter), widget);
	instancesPage = boxOuter;
	return boxOuter;
}

struct AccountRowWidgets {
	GtkWidget *grid;
	GtkLabel *nameLabel;
	GtkLabel *typeLabel;
};

static void account_list_view_setup_factory(GtkListItemFactory *factory, GtkListItem *list_item, gpointer user_data) {
	GtkWidget *widget;
	widget = gtk_grid_new();
	gtk_widget_set_hexpand(widget, true);
	GtkGrid *grid = GTK_GRID(widget);
	GtkGesture *click = gtk_gesture_click_new();
	gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click), GDK_BUTTON_SECONDARY);
	GMenu *menu = g_menu_new();
	g_menu_append(menu, "Edit", "account.edit");
	g_menu_append(menu, "Delete", "account.delete");
	g_signal_connect(click, "pressed", G_CALLBACK(show_row_menu), menu);
	gtk_widget_add_controller(widget, GTK_EVENT_CONTROLLER(click));
	struct AccountRowWidgets *rw = g_new0(struct AccountRowWidgets, 1);
	rw->grid = widget;

	widget = gtk_label_new(NULL);
	rw->nameLabel = GTK_LABEL(widget);
	gtk_widget_set_halign(widget, GTK_ALIGN_START);
	gtk_widget_set_hexpand(widget, true);
	gtk_widget_set_vexpand(widget, true);
	gtk_widget_add_css_class(widget, "title");
	gtk_grid_attach(grid, widget, 0, 0, 1, 1);

	widget = gtk_label_new(NULL);
	rw->typeLabel = GTK_LABEL(widget);
	gtk_widget_set_halign(widget, GTK_ALIGN_END);
	gtk_widget_add_css_class(widget, "subtitle");
	gtk_grid_attach(grid, widget, 1, 0, 1, 1);

	gtk_list_item_set_child(list_item, GTK_WIDGET(grid));
	g_object_set_data_full(G_OBJECT(list_item), "row-widgets", rw, g_free);
}

static void account_action(GSimpleAction *simple_action, G_GNUC_UNUSED GVariant *parameter, gpointer *data) {
	struct User *account = (struct User *)data;
	if(strcmp(g_action_get_name(G_ACTION(simple_action)), "edit") == 0) {

	} else if(strcmp(g_action_get_name(G_ACTION(simple_action)), "delete") == 0) {
		remove_account(account);
	}
}

static void account_list_view_bind_factory(GtkListItemFactory *factory, GtkListItem *list_item, gpointer user_data) {
	struct AccountRowWidgets *rw = g_object_get_data(G_OBJECT(list_item), "row-widgets");

	const char *str = gtk_string_object_get_string(GTK_STRING_OBJECT(gtk_list_item_get_item(list_item)));
	GSList *list = *microlauncher_get_accounts();
	struct User *account = microlauncher_account_get(list, str);
	GSimpleActionGroup *actions = g_simple_action_group_new();
	GSimpleAction *action;

	// action = g_simple_action_new("edit", NULL);
	// g_signal_connect(action, "activate", G_CALLBACK(account_action), account);
	// g_action_map_add_action(G_ACTION_MAP(actions), G_ACTION(action));

	action = g_simple_action_new("delete", NULL);
	g_signal_connect(action, "activate", G_CALLBACK(account_action), account);
	g_action_map_add_action(G_ACTION_MAP(actions), G_ACTION(action));

	gtk_widget_insert_action_group(rw->grid, "account", G_ACTION_GROUP(actions));

	gtk_label_set_label(rw->nameLabel, account->name);
	gtk_label_set_label(rw->typeLabel, microlauncher_get_account_type_name(account->type));
}

static gboolean check_running = false;

struct DeviceCodeOAuthResponse *oAuthResponse = NULL;
static int checkDeviceCode = 0;

static gboolean reload_code(void *data) {
	struct DeviceCodeOAuthResponse *response = data;
	if(!response || !msaTimeout || !response->user_code) {
		return false;
	}
	gtk_progress_bar_set_fraction(msaTimeout, (double)msaSecondsLeft / response->expires_in);
	gtk_spinner_stop(msaSpinner);
	gtk_widget_set_visible(GTK_WIDGET(msaSpinner), false);
	if(msaCodeLabel) {
		gtk_label_set_text(msaCodeLabel, response->user_code);
	}
	char *str = g_strdup_printf("Open <a href=\"%s\">%s</a> and paste the above code to authenticate.", response->verification_uri, response->verification_uri);
	gtk_label_set_text(msaHint, str);
	gtk_label_set_use_markup(msaHint, true);
	gtk_widget_set_visible(GTK_WIDGET(msaHint), true);
	free(str);
	return false;
}

static gboolean add_account_in_main(struct User *user) {
	add_account(user);
	return false;
}

static void check_device_code(GTask *task, gpointer source_object, gpointer task_data, GCancellable *cancellable) {
	struct DeviceCodeOAuthResponse *response = (struct DeviceCodeOAuthResponse *)task_data;
	if(!response || !response->device_code) {
		return;
	}
	struct MicrosoftUser *userdata = malloc(sizeof(struct MicrosoftUser));
	int result = microlauncher_msa_device_token(response->device_code, userdata);
	if(result == 1) { // pending
		checkDeviceCode = response->interval;
	} else if(result == -1) { // cancel/error
		msaSecondsLeft = 0;	  // Request new code
	} else if(result == 0) {
		struct User *user = g_new0(struct User, 1);
		user->data = userdata;
		user->type = ACCOUNT_TYPE_MSA;
		user->id = random_uuid();
		g_idle_add(G_SOURCE_FUNC(gtk_window_close), popupWindow);
		microlauncher_auth_user(user);
		g_idle_add(G_SOURCE_FUNC(add_account_in_main), user);
		msaSecondsLeft = 1;
	}
}

static void get_new_code(GTask *task, gpointer source_object, gpointer task_data, GCancellable *cancellable) {
	struct DeviceCodeOAuthResponse response = microlauncher_msa_get_device_code();
	if(oAuthResponse) {
		microlauncher_msa_destroy_device_code(oAuthResponse);
	}
	oAuthResponse = malloc(sizeof(struct DeviceCodeOAuthResponse));
	memcpy(oAuthResponse, &response, sizeof(struct DeviceCodeOAuthResponse));

	msaSecondsLeft = response.expires_in;
	g_idle_add(reload_code, oAuthResponse);
	g_object_unref(task);
}

static gboolean update_msa_timeout(void *data) {
	if(msaSecondsLeft > 0) { /* underflow protection */
		msaSecondsLeft--;
	}
	if(checkDeviceCode > 0) { /* underflow protection */
		checkDeviceCode--;
	}
	if(!msaTimeout) {
		// check_running = false;
		// msaSecondsLeft = 0;
		return true;
	}
	if(msaSecondsLeft == 0) {
		GTask *task = g_task_new(window, NULL, NULL, NULL);
		g_task_set_task_data(task, oAuthResponse, NULL /* (GDestroyNotify)microlauncher_msa_destroy_device_code */);
		g_task_run_in_thread(task, get_new_code);
	}
	if(checkDeviceCode == 0) {
		GTask *task = g_task_new(window, NULL, NULL, NULL);
		g_task_set_task_data(task, oAuthResponse, NULL /* (GDestroyNotify)microlauncher_msa_destroy_device_code */);
		g_task_run_in_thread(task, check_device_code);
	}
	if(oAuthResponse && oAuthResponse->expires_in > 0)
		gtk_progress_bar_set_fraction(msaTimeout, (double)msaSecondsLeft / oAuthResponse->expires_in);
	return true;
}

struct OnAddOffline {
	GtkWindow *parent;
	GtkEntry *usernameEntry;
};

static void on_add_offline(GtkButton *button, struct OnAddOffline *data) {
	struct User *user = g_new0(struct User, 1);
	user->name = strdup(gtk_entry_buffer_get_text(gtk_entry_get_buffer(data->usernameEntry)));
	user->type = ACCOUNT_TYPE_OFFLINE;
	user->id = random_uuid();
	user->accessToken = "-";
	add_account(user);
	GtkWindow *window = data->parent;
	gtk_window_close(window);
}

static void offline_username_entry_changed(GtkEntry *entry, gpointer userdata) {
	GtkWidget *addAccountButton = GTK_WIDGET(userdata);
	gtk_widget_set_sensitive(addAccountButton, strlen(gtk_entry_get_text(entry)) > 0);
}

static GtkWidget *microlauncher_gui_add_offline(GtkWindow *newWindow) {
	GtkWidget *widget;
	GtkBox *box;
	GtkEntry *entry;
	widget = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
	box = GTK_BOX(widget);

	widget = gtk_entry_new();
	entry = GTK_ENTRY(widget);
	gtk_widget_set_hexpand(widget, true);
	widget = gtk_widget_with_label("Username:", widget);
	gtk_widget_set_margin(widget, 10, 10, 10, 10);
	gtk_box_append(box, widget);

	widget = gtk_button_new_with_label("Add account");
	gtk_widget_set_sensitive(widget, false);
	g_signal_connect(entry, "changed", G_CALLBACK(offline_username_entry_changed), widget);
	struct OnAddOffline *cbdata = g_new0(struct OnAddOffline, 1);
	cbdata->parent = newWindow;
	cbdata->usernameEntry = entry;
	g_signal_connect_data(widget, "clicked", G_CALLBACK(on_add_offline), cbdata, (GClosureNotify)g_free, 0);
	gtk_widget_set_vexpand(widget, true);
	gtk_widget_set_valign(widget, GTK_ALIGN_END);
	gtk_widget_set_margin(widget, 10, 10, 10, 10);
	gtk_box_append(box, widget);

	return GTK_WIDGET(box);
}

static void on_stack_page_changed(GObject *obj, GParamSpec *pspec, gpointer user_data) {
	GtkStack *stack = GTK_STACK(obj);
	const char *t = gtk_stack_get_visible_child_name(stack);
	if(strcmp(t, "ms") == 0) {
		reload_code(oAuthResponse);
		if(!check_running) {
			check_running = true;
			update_msa_timeout(NULL);
			g_timeout_add_seconds(1, update_msa_timeout, NULL);
		}
	}
}

static GtkWidget *microlauncher_gui_add_msa(GtkWindow *newWindow) {
	GtkWidget *widget;
	GtkBox *box;
	widget = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
	box = GTK_BOX(widget);
	if(!oAuthResponse) {
		oAuthResponse = malloc(sizeof(struct DeviceCodeOAuthResponse));
		memset(oAuthResponse, 0, sizeof(struct DeviceCodeOAuthResponse));
	}

	widget = gtk_spinner_new();
	msaSpinner = GTK_SPINNER(widget);
	gtk_spinner_start(msaSpinner);
	gtk_widget_set_size_request(widget, 48, 48);
	gtk_box_append(box, widget);

	widget = gtk_label_new("");
	msaCodeLabel = GTK_LABEL(widget);
	gtk_widget_add_css_class(widget, "monospace");
	gtk_widget_add_css_class(widget, "msa-code");
	gtk_label_set_selectable(msaCodeLabel, true);
	gtk_box_append(box, widget);

	widget = gtk_label_new("");
	msaHint = GTK_LABEL(widget);
	gtk_widget_set_margin(widget, 10, 10, 10, 10);
	gtk_label_set_use_markup(GTK_LABEL(widget), true);
	gtk_label_set_wrap(GTK_LABEL(widget), true);
	gtk_label_set_selectable(GTK_LABEL(widget), true);
	gtk_widget_set_valign(widget, GTK_ALIGN_START);
	gtk_widget_set_vexpand(widget, true);
	gtk_box_append(box, widget);

	widget = gtk_progress_bar_new();
	gtk_widget_set_margin(widget, 10, 10, 10, 10);
	msaTimeout = GTK_PROGRESS_BAR(widget);
	gtk_progress_bar_set_fraction(msaTimeout, 1);
	gtk_box_append(box, widget);
	return GTK_WIDGET(box);
}

static void close_accounts_dialog(void) {
	msaTimeout = NULL;
}

static void microlauncher_gui_add_account(void) {
	GtkBox *box;
	GtkWidget *widget;
	GtkWindow *newWindow = gtk_modal_dialog_new(window);
	popupWindow = newWindow;
	gtk_window_set_default_size(newWindow, 600, 400);
	gtk_window_set_title(newWindow, "Add account");

	widget = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
	box = GTK_BOX(widget);
	gtk_window_set_child(newWindow, widget);

	widget = gtk_stack_switcher_new();
	GtkStackSwitcher *stackSwitcher = GTK_STACK_SWITCHER(widget);
	gtk_widget_set_margin(widget, 10, 0, 10, 10);
	gtk_box_append(box, widget);

	widget = gtk_stack_new();
	GtkStack *stack = GTK_STACK(widget);
	g_signal_connect(stack, "notify::visible-child", G_CALLBACK(on_stack_page_changed), NULL);
	g_signal_connect(newWindow, "destroy", G_CALLBACK(close_accounts_dialog), NULL);
	gtk_stack_switcher_set_stack(stackSwitcher, stack);
	gtk_box_append(box, widget);

	gtk_stack_add_titled(stack, microlauncher_gui_add_offline(newWindow), "offline", "Offline");
	gtk_stack_add_titled(stack, microlauncher_gui_add_msa(newWindow), "ms", "Microsoft");

	gtk_window_present(newWindow);
}

static GtkWidget *microlauncher_gui_page_accounts(void) {
	GtkWidget *widget, *box, *boxOuter, *scrolledWindow;
	boxOuter = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
	gtk_widget_set_vexpand(boxOuter, true);
	gtk_widget_set_margin(boxOuter, 10, 10, 10, 10);

	scrolledWindow = gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledWindow), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_vexpand(scrolledWindow, true);

	box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolledWindow), box);
	accountsList = gtk_string_list_new(NULL);
	GSList *list = *microlauncher_get_accounts();
	for(guint i = 0; i < g_slist_length(list); i++) {
		const struct User *user = g_slist_nth(list, i)->data;
		gtk_string_list_append(accountsList, user->id);
	}

	GtkSingleSelection *selection = gtk_single_selection_new(G_LIST_MODEL(accountsList));
	GtkListItemFactory *factory = gtk_signal_list_item_factory_new();
	g_signal_connect(factory, "setup", G_CALLBACK(account_list_view_setup_factory), NULL);
	g_signal_connect(factory, "bind", G_CALLBACK(account_list_view_bind_factory), NULL);

	widget = gtk_list_view_new(GTK_SELECTION_MODEL(selection), factory);
	if(settings->user) {
		gtk_single_selection_set_selected(selection, gtk_string_list_find(accountsList, settings->user->id));
	}
	g_signal_connect(selection, "selection-changed", G_CALLBACK(account_selection_changed), accountsList);
	g_signal_connect(widget, "activate", G_CALLBACK(activate_account_row), accountsList);
	gtk_widget_add_css_class(widget, "rich-list");
	gtk_widget_add_css_class(widget, "boxed-list");

	gtk_box_append(GTK_BOX(box), widget);

	gtk_box_append(GTK_BOX(boxOuter), scrolledWindow);
	widget = gtk_button_new_with_label("Add account");
	g_signal_connect(widget, "clicked", G_CALLBACK(microlauncher_gui_add_account), NULL);
	gtk_box_append(GTK_BOX(boxOuter), widget);
	accountsPage = boxOuter;
	return boxOuter;
}

struct AsyncStatusInfo {
	char *stage;
	char *label;
	double progress;
};

static void microlauncher_gui_close_launcher(void *data) {
	settings->width = atoi(gtk_entry_buffer_get_text(gtk_entry_get_buffer(widthEntry)));
	settings->height = atoi(gtk_entry_buffer_get_text(gtk_entry_get_buffer(heightEntry)));
	settings->fullscreen = gtk_check_button_get_active(checkFullscreen);
	settings->demo = gtk_check_button_get_active(checkDemo);
	gtk_window_destroy(window);
}

static gboolean close_request(GtkWindow *self, gpointer user_data) {
	microlauncher_gui_close_launcher(NULL);
	return true;
}

static gboolean microlauncher_gui_enable_kill(void *userdata) {
	gtk_button_set_label(playButton, "Kill");
	gtk_widget_set_sensitive(GTK_WIDGET(playButton), true);
	return false;
}

static void scheduled_launcher_set_pid(pid_t pid, void *userdata) {
	instancePid = pid;
	g_idle_add(G_SOURCE_FUNC(microlauncher_gui_enable_kill), userdata);
}

static void scheduled_launcher_enable_play(void *userdata) {
	g_idle_add(G_SOURCE_FUNC(microlauncher_gui_enable_play), userdata);
}

struct ProgressUpdate {
	double progress;
	char *label;
};

static gboolean microlauncher_gui_progress_update(struct ProgressUpdate *data) {
	char *str;
	if(progressBar && data->label) {
		int percent = (int)round(data->progress * 100);
		gtk_progress_bar_set_fraction(progressBar, data->progress);
		str = g_strdup_printf("%d%%: %s", percent, data->label);
		gtk_progress_bar_set_text(progressBar, str);
		free(str);
	}
	free(data->label);
	free(data);
	return false;
}

static void scheduled_progress_update(double progress, const char *label, void *userdata) {
	struct ProgressUpdate *data = g_new(struct ProgressUpdate, 1);
	data->progress = progress;
	data->label = g_strdup(label);

	g_idle_add(G_SOURCE_FUNC(microlauncher_gui_progress_update), data);
}

static gboolean microlauncher_gui_set_stage(char *data) {
	gtk_revealer_set_reveal_child(revealer, data != NULL);
	if(!data) {
		return false;
	}
	gtk_label_set_text(progressStage, data);
	free(data);
	gtk_progress_bar_set_fraction(progressBar, 0);
	return false;
}

static void scheduled_set_stage(const char *stage, void *userdata) {
	char *data = g_strdup(stage);
	g_idle_add(G_SOURCE_FUNC(microlauncher_gui_set_stage), data);
}

static void activate(GtkApplication *app, gpointer user_data) {
	GtkWidget *widget, *box;
	GtkCssProvider *provider = gtk_css_provider_new();
	gtk_css_provider_load_from_string(provider, ".msa-code { font-size: 64px; }");
	gtk_style_context_add_provider_for_display(gdk_display_get_default(), GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_THEME);

	window = GTK_WINDOW(gtk_window_new());
	gtk_application_add_window(app, window);
	gtk_window_set_default_size(window, 0, 480);
	gtk_window_set_title(window, "MicroLauncher - Minecraft Launcher");

	box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_window_set_child(window, box);

	widget = gtk_stack_switcher_new();
	gtk_widget_set_margin(widget, 10, 10, 10, 10);
	gtk_box_append(GTK_BOX(box), widget);
	GtkStackSwitcher *stackSwitcher = GTK_STACK_SWITCHER(widget);

	widget = gtk_stack_new();
	GtkStack *stack = GTK_STACK(widget);
	launcherStack = stack;
	gtk_stack_switcher_set_stack(stackSwitcher, stack);
	gtk_stack_set_transition_type(stack, GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);
	gtk_stack_add_titled(stack, microlauncher_gui_page_launcher(), "launcher", "Launcher");
	gtk_stack_add_titled(stack, microlauncher_gui_page_instances(), "instances", "Instances");
	gtk_stack_add_titled(stack, microlauncher_gui_page_accounts(), "accounts", "Accounts");
	gtk_widget_set_vexpand(widget, true);
	gtk_box_append(GTK_BOX(box), widget);

	widget = gtk_revealer_new();
	revealer = GTK_REVEALER(widget);
	gtk_box_append(GTK_BOX(box), widget);

	widget = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
	gtk_revealer_set_child(revealer, widget);
	gtk_revealer_set_transition_type(revealer, GTK_REVEALER_TRANSITION_TYPE_SWING_UP);
	gtk_revealer_set_reveal_child(revealer, false);
	GtkBox *progressBox = GTK_BOX(widget);

	widget = gtk_label_new("");
	progressStage = GTK_LABEL(widget);
	gtk_box_append(progressBox, widget);

	widget = gtk_progress_bar_new();
	progressBar = GTK_PROGRESS_BAR(widget);
	gtk_widget_set_margin(widget, 0, 10, 10, 10);
	gtk_progress_bar_set_show_text(progressBar, true);
	gtk_box_append(progressBox, widget);

	microlauncher_gui_refresh_instance();
	struct Callbacks callbacks = {
		.instance_finished = scheduled_launcher_enable_play,
		.instance_started = scheduled_launcher_set_pid,
		.progress_update = scheduled_progress_update,
		.stage_update = scheduled_set_stage,
		.userdata = NULL,
	};
	char *str;
	if(settings->width) {
		str = g_strdup_printf("%d", settings->width);
		gtk_entry_set_text(widthEntry, str);
		free(str);
	}

	if(settings->height) {
		str = g_strdup_printf("%d", settings->height);
		gtk_entry_set_text(heightEntry, str);
		free(str);
	}

	gtk_check_button_set_active(checkFullscreen, settings->fullscreen);
	gtk_check_button_set_active(checkDemo, settings->demo);
	microlauncher_set_callbacks(callbacks);

	g_signal_connect(window, "close-request", G_CALLBACK(close_request), NULL);
	gtk_window_set_focus(window, GTK_WIDGET(playButton));
	gtk_window_present(window);
}

int microlauncher_gui_show(void) {
	int status;
	int argc = 1;
	char *argv[] = {APPID, NULL};
	settings = microlauncher_get_settings();

	app = gtk_application_new(APPID, G_APPLICATION_NON_UNIQUE);
	g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
	status = g_application_run(G_APPLICATION(app), argc, argv);
	g_object_unref(app);

	return status;
}
