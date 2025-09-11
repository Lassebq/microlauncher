#include "gtk_util.h"
#include "json_object.h"
#include "json_types.h"
#include "json_util.h"
#include "microlauncher.h"
#include "microlauncher_instance.h"
#include "microlauncher_account.h"
#include "microlauncher_msa.h"
#include "microlauncher_version_item.h"
#include "util.h"
#include "xdgutil.h"
#include <curl/curl.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <stdatomic.h>
#ifdef _WIN32
#include "shlobj.h"
#include "windows.h"
#endif
#ifdef __unix
#include <pci/pci.h>
#endif
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
static GListStore *accountsList;
static GtkWidget *instancesPage;
static GListStore *instancesList;
static GtkListView *instancesView;
static GtkListView *accountsView;

static GtkLabel *msaCodeLabel;
static GtkLabel *msaHint;
static GtkProgressBar *msaTimeout;
static GtkSpinner *msaSpinner;
static int msaSecondsLeft;

static GPid instancePid = 0;
static GCancellable *instanceCancellable;

static GSList *gpuIds;
static GtkStringList *gpuLabels;

static struct Settings *settings;

static void microlauncher_gui_refresh_instance(void);

static void remove_account(MicrolauncherAccount *account) {
	GSList **accounts = microlauncher_get_accounts();
	guint index = G_MAXUINT;
	g_list_store_find(accountsList, account, &index);
	g_list_store_remove(accountsList, index);
	*accounts = g_slist_remove(*accounts, account);
	if(settings->user == account) {
		settings->user = NULL;
	}
	g_object_unref(account);
	microlauncher_gui_refresh_instance();
}

static void remove_instance(MicrolauncherInstance *instance) {
	GSList **instances = microlauncher_get_instances();
	guint index = G_MAXUINT;
	g_list_store_find(instancesList, instance, &index);
	g_list_store_remove(instancesList, index);
	*instances = g_slist_remove(*instances, instance);
	if(settings->instance == instance) {
		settings->instance = NULL;
	}
	g_object_unref(instance);
	microlauncher_gui_refresh_instance();
}

static void add_instance(MicrolauncherInstance *instance) {
	GSList **instances = microlauncher_get_instances();
	*instances = g_slist_append(*instances, instance);
	g_list_store_append(instancesList, instance);
}

static void add_account(MicrolauncherAccount *user) {
	GSList **accounts = microlauncher_get_accounts();
	*accounts = g_slist_append(*accounts, user);
	g_list_store_append(accountsList, user);
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
	GtkFileDialog *dialog = gtk_file_dialog_new();
	GFile *file = g_file_new_for_path(gtk_entry_get_text(entry));
	if(file && g_file_query_file_type(file, G_FILE_QUERY_INFO_NONE, NULL) == G_FILE_TYPE_DIRECTORY) {
		gtk_file_dialog_set_initial_folder(dialog, file);
	}
	gtk_file_dialog_select_folder(dialog, window, NULL, (GAsyncReadyCallback)select_instance_directory, entry);
	g_object_unref(file);
}

static void select_instance_java(GtkFileDialog *dialog, GAsyncResult *res, gpointer data) {
	GtkEntry *entry = GTK_ENTRY(data);
	GFile *file = gtk_file_dialog_open_finish(dialog, res, NULL);
	if(file) {
		gtk_entry_set_text(entry, g_file_peek_path(file));
	}
	g_object_unref(file);
}

static void select_instance_java_event(GtkButton *button, gpointer user_data) {
	GtkEntry *entry = GTK_ENTRY(user_data);
	GtkFileDialog *dialog = gtk_file_dialog_new();
	GFile *file = g_file_new_for_path(gtk_entry_get_text(entry));
	if(file && g_file_query_file_type(file, G_FILE_QUERY_INFO_NONE, NULL) == G_FILE_TYPE_REGULAR) {
		gtk_file_dialog_set_initial_file(dialog, file);
	}
	gtk_file_dialog_open(dialog, window, NULL, (GAsyncReadyCallback)select_instance_java, entry);
	g_object_unref(file);
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
	GtkFileDialog *dialog = gtk_file_dialog_new();
	GFile *file = g_file_new_for_path(g_object_get_data(G_OBJECT(image), "icon-location"));
	if(file && g_file_query_file_type(file, G_FILE_QUERY_INFO_NONE, NULL) == G_FILE_TYPE_REGULAR) {
		gtk_file_dialog_set_initial_file(dialog, file);
	}
	gtk_file_dialog_open(dialog, window, NULL, (GAsyncReadyCallback)select_instance_icon, image);
	g_object_unref(file);
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
	GtkEntry *instanceGameArgs;
	GtkEntry *instanceJvmArgs;
	GtkColumnView *versionView;
	GtkWindow *dialog;
	MicrolauncherInstance *toReplace;
};

static void create_or_modify_instance(GtkButton *button, void *userdata) {
	struct CreateInstance *createInstance = (struct CreateInstance *)userdata;
	GValue strVal = G_VALUE_INIT;
	g_value_init(&strVal, G_TYPE_STRING);

	MicrolauncherInstance *inst = createInstance->toReplace ? createInstance->toReplace : microlauncher_instance_new();

	g_value_set_string(&strVal, gtk_entry_get_text(createInstance->instanceName));
	g_object_set_property(G_OBJECT(inst), "name", &strVal);

	g_value_set_string(&strVal, gtk_entry_get_text(createInstance->instanceDir));
	g_object_set_property(G_OBJECT(inst), "location", &strVal);

	g_value_set_string(&strVal, gtk_entry_get_text(createInstance->instanceJvm));
	g_object_set_property(G_OBJECT(inst), "java-location", &strVal);

	g_value_set_string(&strVal, g_object_get_data(G_OBJECT(createInstance->instanceIcon), "icon-location"));
	g_object_set_property(G_OBJECT(inst), "icon", &strVal);

	inst->extraGameArgs = NULL;
	inst->jvmArgs = NULL;
	char *str = g_strdup(gtk_entry_get_text(createInstance->instanceGameArgs));
	char **args = get_commandv(str);
	int i = 0;
	while(args && args[i]) {
		inst->extraGameArgs = g_slist_append(inst->extraGameArgs, g_strdup(args[i]));
		i++;
	}
	free(str);
	free(args);
	i = 0;
	str = g_strdup(gtk_entry_get_text(createInstance->instanceJvmArgs));
	args = get_commandv(str);
	while(args && args[i]) {
		inst->jvmArgs = g_slist_append(inst->jvmArgs, g_strdup(args[i]));
		i++;
	}
	free(str);
	free(args);

	GtkSingleSelection *selection = GTK_SINGLE_SELECTION(gtk_column_view_get_model(createInstance->versionView));
	VersionItem *item = gtk_single_selection_get_selected_item(selection);
	g_value_set_string(&strVal, item->version);
	g_object_set_property(G_OBJECT(inst), "version", &strVal);
	if(!createInstance->toReplace) {
		add_instance(inst);
	}
	microlauncher_gui_refresh_instance();
	gtk_window_destroy(createInstance->dialog);
}

// I should not need to workaround this once the approriate GTK bug is fixed. https://gitlab.gnome.org/GNOME/gtk/-/issues/6747
struct TryScroll {
	GtkColumnView *colView;
	guint row;
};

static gboolean try_scroll_to(struct TryScroll *tryScroll) {
	GtkScrollInfo *scrollInfo = gtk_scroll_info_new();
	gtk_scroll_info_set_enable_vertical(scrollInfo, true);
	gtk_column_view_scroll_to(tryScroll->colView, tryScroll->row, NULL, GTK_LIST_SCROLL_SELECT | GTK_LIST_SCROLL_FOCUS, scrollInfo);
	return false;
}

struct ModifyInstanceName {
	char *instanceName;
	GtkWidget *createButton;
};

const MicrolauncherInstance *get_instance_by_name(const char *name) {
	GSList *node = *microlauncher_get_instances();
	while(node) {
		MicrolauncherInstance *inst = node->data;
		if(strequal(inst->name, name)) {
			return node->data;
		}
		node = node->next;
	}
	return NULL;
}

void instance_name_changed(GtkEntry *self, gpointer user_data) {
	struct ModifyInstanceName instName = *(struct ModifyInstanceName *)user_data;
	const char *text = gtk_entry_get_text(self);
	if(strlen(text) == 0) {
		gtk_widget_add_css_class(GTK_WIDGET(self), "error");
		gtk_widget_set_tooltip_text(GTK_WIDGET(self), "Name cannot be empty");
		gtk_widget_set_sensitive(instName.createButton, false);
	} else if(get_instance_by_name(text) && !strequal(instName.instanceName, text)) {
		gtk_widget_add_css_class(GTK_WIDGET(self), "error");
		gtk_widget_set_tooltip_text(GTK_WIDGET(self), "Instance name conflicts with existing instance");
		gtk_widget_set_sensitive(instName.createButton, false);
	} else {
		gtk_widget_remove_css_class(GTK_WIDGET(self), "error");
		gtk_widget_set_tooltip_text(GTK_WIDGET(self), "");
		gtk_widget_set_sensitive(instName.createButton, true);
	}
}

static void microlauncher_modify_instance_window(GtkButton *button, MicrolauncherInstance *instance) {
	GtkWidget *widget;
	GtkScrolledWindow *scrolledWindow;
	GtkBox *box;
	GtkGrid *grid;
	GtkEntry *entry;
	int row = 0;
	struct CreateInstance *createInstance = g_new(struct CreateInstance, 1);
	GtkWindow *newWindow = gtk_modal_dialog_new(window);
	createInstance->toReplace = instance;
	createInstance->dialog = newWindow;
	gtk_window_set_title(newWindow, instance ? "Modify instance" : "New instance");
	gtk_window_set_default_size(newWindow, 640, 700);
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
	gtk_grid_attach(grid, widget, 0, row, 1, 2);

	widget = gtk_label_new("Instance name:");
	gtk_widget_set_halign(widget, GTK_ALIGN_START);
	gtk_grid_attach(grid, widget, 1, row, 1, 1);

	widget = gtk_entry_new();
	if(instance) {
		gtk_entry_set_text(GTK_ENTRY(widget), instance->name);
	}
	struct ModifyInstanceName *instName = g_new(struct ModifyInstanceName, 1);
	instName->instanceName = instance ? instance->name : NULL;
	g_signal_connect_data(widget, "changed", G_CALLBACK(instance_name_changed), instName, (GClosureNotify)g_free, 0);
	gtk_grid_attach(grid, widget, 2, row++, 1, 1);
	createInstance->instanceName = GTK_ENTRY(widget);

	widget = gtk_label_new("Game directory:");
	gtk_widget_set_halign(widget, GTK_ALIGN_START);
	gtk_grid_attach(grid, widget, 1, row, 1, 1);

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
	gtk_grid_attach(grid, GTK_WIDGET(container), 2, row++, 1, 1);

	widget = gtk_label_new("Java executable:");
	gtk_widget_set_halign(widget, GTK_ALIGN_START);
	gtk_grid_attach(grid, widget, 1, row, 1, 1);

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
	gtk_grid_attach(grid, GTK_WIDGET(container), 2, row++, 1, 1);

	widget = gtk_label_new("Game arguments:");
	gtk_widget_set_halign(widget, GTK_ALIGN_START);
	gtk_grid_attach(grid, widget, 1, row, 1, 1);

	widget = gtk_entry_new();
	entry = GTK_ENTRY(widget);
	createInstance->instanceGameArgs = entry;
	if(instance) {
		String str = string_new(NULL);
		GSList *node = instance->extraGameArgs;
		while(node) {
			string_append(&str, node->data);
			if(node->next) {
				string_append_char(&str, ' ');
			}
			node = node->next;
		}
		gtk_entry_set_text(entry, str.data);
		string_destroy(&str);
	}
	gtk_grid_attach(grid, widget, 2, row++, 1, 1);

	widget = gtk_label_new("JVM arguments:");
	gtk_widget_set_halign(widget, GTK_ALIGN_START);
	gtk_grid_attach(grid, widget, 1, row, 1, 1);

	widget = gtk_entry_new();
	entry = GTK_ENTRY(widget);
	createInstance->instanceJvmArgs = entry;
	if(instance) {
		String str = string_new(NULL);
		GSList *node = instance->jvmArgs;
		while(node) {
			string_append(&str, node->data);
			if(node->next) {
				string_append_char(&str, ' ');
			}
			node = node->next;
		}
		gtk_entry_set_text(entry, str.data);
		string_destroy(&str);
	}
	gtk_grid_attach(grid, widget, 2, row++, 1, 1);

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
	GtkColumnView *columnView = GTK_COLUMN_VIEW(widget);
	createInstance->versionView = columnView;
	gtk_column_view_set_show_column_separators(columnView, true);

	GtkListItemFactory *factory = gtk_signal_list_item_factory_new();
	g_signal_connect(factory, "setup", G_CALLBACK(setup_column_cb), NULL);
	g_signal_connect(factory, "bind", G_CALLBACK(bind_column_cb), "version");
	GtkColumnViewColumn *column = gtk_column_view_column_new("Version", factory);
	GtkSorter *sorter = GTK_SORTER(gtk_string_sorter_new(gtk_property_expression_new(MICROLAUNCHER_VERSION_ITEM_TYPE, NULL, "version")));
	gtk_column_view_column_set_sorter(column, GTK_SORTER(sorter));
	gtk_column_view_column_set_resizable(column, true);
	gtk_column_view_append_column(columnView, column);

	factory = gtk_signal_list_item_factory_new();
	g_signal_connect(factory, "setup", G_CALLBACK(setup_column_cb), NULL);
	g_signal_connect(factory, "bind", G_CALLBACK(bind_column_cb), "type");
	column = gtk_column_view_column_new("Type", factory);
	sorter = GTK_SORTER(gtk_string_sorter_new(gtk_property_expression_new(MICROLAUNCHER_VERSION_ITEM_TYPE, NULL, "type")));
	gtk_column_view_column_set_sorter(column, GTK_SORTER(sorter));
	gtk_column_view_column_set_resizable(column, true);
	gtk_column_view_append_column(columnView, column);

	factory = gtk_signal_list_item_factory_new();
	g_signal_connect(factory, "setup", G_CALLBACK(setup_column_cb), NULL);
	g_signal_connect(factory, "bind", G_CALLBACK(bind_column_cb), "releaseTime");
	column = gtk_column_view_column_new("Release Time", factory);
	sorter = GTK_SORTER(gtk_string_sorter_new(gtk_property_expression_new(MICROLAUNCHER_VERSION_ITEM_TYPE, NULL, "releaseTime")));
	gtk_column_view_column_set_expand(column, true);
	gtk_column_view_column_set_sorter(column, GTK_SORTER(sorter));
	gtk_column_view_column_set_resizable(column, true);
	gtk_column_view_append_column(columnView, column);
	gtk_column_view_sort_by_column(columnView, column, GTK_SORT_DESCENDING);

	sorter = g_object_ref(gtk_column_view_get_sorter(columnView));
	GtkSortListModel *model = gtk_sort_list_model_new(G_LIST_MODEL(store), sorter);
	GtkSingleSelection *selection = gtk_single_selection_new(G_LIST_MODEL(model));
	gtk_column_view_set_model(columnView, GTK_SELECTION_MODEL(selection));
	gtk_scrolled_window_set_child(scrolledWindow, widget);
	gtk_box_append(box, GTK_WIDGET(scrolledWindow));

	container = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10));
	gtk_box_set_homogeneous(container, true);
	widget = gtk_button_new_with_label("Cancel");
	gtk_widget_set_hexpand(widget, true);
	g_signal_connect(widget, "clicked", G_CALLBACK(cancel_callback), newWindow);
	gtk_box_append(container, widget);
	widget = gtk_button_new_with_label(instance ? "Modify instance" : "Create instance");
	gtk_widget_set_sensitive(widget, instance != NULL);
	instName->createButton = widget;
	gtk_widget_set_hexpand(widget, true);
	g_signal_connect_data(widget, "clicked", G_CALLBACK(create_or_modify_instance), createInstance, (GClosureNotify)g_free, 0);
	gtk_box_append(container, widget);

	struct TryScroll *tryScroll = g_new0(struct TryScroll, 1);
	tryScroll->colView = columnView;
	if(instance) {
		guint n = g_list_model_get_n_items(G_LIST_MODEL(model));
		for(guint i = 0; i < n; i++) {
			VersionItem *item = g_list_model_get_item(G_LIST_MODEL(model), i);
			if(strequal(instance->version, item->version)) {
				// gtk_single_selection_set_selected(selection, i);
				tryScroll->row = i;
				break;
			}
		}
	}
	g_idle_add_full(G_PRIORITY_DEFAULT_IDLE, (GSourceFunc)try_scroll_to, tryScroll, g_free);

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
		gtk_label_set_ellipsize(GTK_LABEL(widget), PANGO_ELLIPSIZE_END);
		gtk_widget_add_css_class(widget, "title-1");
		gtk_label_set_xalign(GTK_LABEL(widget), 0.0F);
		gtk_widget_set_hexpand(widget, true);
		gtk_grid_attach(titleArea, widget, 1, 0, 1, 1);

		label = g_strdup_printf("Version %s", settings->instance->version);
		widget = gtk_label_new(label);
		gtk_label_set_ellipsize(GTK_LABEL(widget), PANGO_ELLIPSIZE_END);
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

static gboolean microlauncher_gui_enable_cancel(void *userdata) {
	gtk_button_set_label(playButton, "Cancel");
	gtk_widget_set_sensitive(GTK_WIDGET(playButton), true);
	return false;
}

static gboolean microlauncher_gui_enable_play(void *userdata) {
	instancePid = 0;
	if(instanceCancellable) {
		g_object_unref(instanceCancellable);
		instanceCancellable = NULL;
	}
	gtk_button_set_label(playButton, "Play");
	gtk_widget_set_sensitive(GTK_WIDGET(playButton), true);
	gtk_widget_set_sensitive(GTK_WIDGET(instancesPage), true);
	gtk_widget_set_sensitive(GTK_WIDGET(accountsPage), true);
	return false;
}

static void launch_instance_thread(GTask *gtask, gpointer source_object, gpointer task_data, GCancellable *cancellable) {
	g_application_hold(G_APPLICATION(app));
	g_idle_add(microlauncher_gui_enable_cancel, NULL);
	microlauncher_launch_instance(settings->instance, settings->user, cancellable);
	g_idle_add(microlauncher_gui_enable_play, NULL);
	g_application_release(G_APPLICATION(app));
}

static void clicked_play(void) {
	if(instancePid != 0) {
		util_kill_process(instancePid);
		return;
	}
	if(instanceCancellable) {
		g_cancellable_cancel(instanceCancellable);
		g_object_unref(instanceCancellable);
		instanceCancellable = NULL;
		return;
	}
	gtk_widget_set_sensitive(GTK_WIDGET(playButton), false);
	gtk_widget_set_sensitive(GTK_WIDGET(instancesPage), false);
	gtk_widget_set_sensitive(GTK_WIDGET(accountsPage), false);
	settings->width = atoi(gtk_entry_buffer_get_text(gtk_entry_get_buffer(widthEntry)));
	settings->height = atoi(gtk_entry_buffer_get_text(gtk_entry_get_buffer(heightEntry)));
	settings->fullscreen = gtk_check_button_get_active(checkFullscreen);
	settings->demo = gtk_check_button_get_active(checkDemo);
	instanceCancellable = g_cancellable_new();
	GTask *task = g_task_new(playButton, instanceCancellable, NULL, NULL);
	g_task_run_in_thread(task, launch_instance_thread);
}

static void notify_gpu_change(GtkDropDown *dropDown, GParamSpec *pspec, GSList *gpuList) {
	free(settings->gpu_id);
	settings->gpu_id = g_strdup(g_slist_nth_data(gpuList, gtk_drop_down_get_selected(dropDown)));
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

#ifdef __unix
	widget = gtk_drop_down_simple_new(gpuLabels, NULL);
	g_signal_connect(widget, "notify::selected", G_CALLBACK(notify_gpu_change), gpuIds);
	gtk_drop_down_set_selected(GTK_DROP_DOWN(widget), 0);
	GSList *node;
	if(settings->gpu_id && (node = g_slist_find_custom(gpuIds, settings->gpu_id, (GCompareFunc)g_strcmp0))) {
		gtk_drop_down_set_selected(GTK_DROP_DOWN(widget), g_slist_index(gpuIds, node->data));
	}
	widget = gtk_widget_with_label("Preferred GPU:", widget);
	gtk_grid_attach(grid, widget, 0, grid_row++, 2, 1);
#endif

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
	gtk_label_set_ellipsize(GTK_LABEL(widget), PANGO_ELLIPSIZE_START);
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

static void microlauncher_create_launcher(MicrolauncherInstance *inst) {
	char path[PATH_MAX];
#ifndef _WIN32
	snprintf(path, PATH_MAX, "%s/applications/microlauncher-%s.desktop", XDG_DATA_HOME, inst->name);
	FILE *fd = fopen_mkdir(path, "wb");
	fprintf(fd,
			"#!/usr/bin/env xdg-open\n"
			"[Desktop Entry]\n"
			"Name=%s\n"
			"Exec=microlauncher --instance \"%s\" --saved-user\n",
			inst->name, inst->name);
	if(inst->icon) {
		fprintf(fd, "Icon=%s\n", inst->icon);
	}
	fprintf(fd,
			"Terminal=false\n"
			"Type=Application\n"
			"Categories=Game;\n");
	fclose(fd);
#else
	char dir[PATH_MAX];
	if(SHGetFolderPathA(NULL, CSIDL_STARTMENU, NULL, 0, dir) == S_OK) {
		snprintf(path, PATH_MAX, "%s/Programs/MicroLauncher/%s.lnk", dir, inst->name);
		char *dirname = g_path_get_dirname(path);
		if(g_mkdir_with_parents(dirname, 0755) == 0) {

			char *cmdline = g_strdup_printf("--instance \"%s\" --saved-user", inst->name);
			g_print("%s\n", path);
			free(dirname);
			dirname = g_path_get_dirname(EXEC_BINARY);
			CreateShortcut(EXEC_BINARY, cmdline, path, SW_SHOWNORMAL, dirname, EXEC_BINARY, 0);
		}
		free(dirname);
	}
#endif
}

static void instance_action(GSimpleAction *simple_action, G_GNUC_UNUSED GVariant *parameter, gpointer *data) {
	MicrolauncherInstance *inst = (MicrolauncherInstance *)data;
	if(strcmp(g_action_get_name(G_ACTION(simple_action)), "edit") == 0) {
		microlauncher_modify_instance_window(NULL, inst);
	} else if(strcmp(g_action_get_name(G_ACTION(simple_action)), "create-launcher") == 0) {
		microlauncher_create_launcher(inst);
	} else if(strcmp(g_action_get_name(G_ACTION(simple_action)), "delete") == 0) {
		remove_instance(inst);
	}
}

void reorder_list(GSList **list, GListStore *orderList) {
	GSList *newList = NULL;
	for(guint i = 0; i < g_list_model_get_n_items(G_LIST_MODEL(orderList)); i++) {
		newList = g_slist_append(newList, g_list_model_get_item(G_LIST_MODEL(orderList), i));
	}
	*list = newList;
}

struct DropUserData {
	GtkListItem *list_item;
	GSList **list;
	GtkSingleSelection *selection;
};

static gboolean on_drop(GtkDropTarget *target,
					const GValue *value,
					double x,
					double y,
					gpointer user_data) {
	struct DropUserData *ondrop = user_data;
	GtkListItem *list_item = GTK_LIST_ITEM(ondrop->list_item);
	GListStore *store = g_object_get_data(G_OBJECT(list_item), "store");
	guint pos = gtk_list_item_get_position(list_item);
	if(!G_VALUE_HOLDS(value, G_TYPE_POINTER)) {
		return false;
	}
	GtkListItem *dropItem = g_value_get_pointer(value);

	guint old_pos = gtk_list_item_get_position(dropItem);
	gboolean wasSelected = gtk_list_item_get_selected(dropItem);

	if(old_pos != G_MAXUINT && old_pos != pos) {
		g_list_store_remove(store, old_pos);
		g_list_store_insert(store, pos, gtk_list_item_get_item(dropItem));
		reorder_list(ondrop->list, store);
		if(wasSelected) {
			gtk_single_selection_set_selected(ondrop->selection, pos);
		}
	}
	return true;
}

static GdkContentProvider *on_drag_prepare(GtkDragSource *src,
										   double x, double y,
										   gpointer user_data) {
	graphene_point_t point = GRAPHENE_POINT_INIT(x, y);
	g_object_set_data_full(G_OBJECT(src), "drag-point", g_memdup2(&point, sizeof(graphene_point_t)), g_free);
	GtkListItem *li = GTK_LIST_ITEM(user_data);
	return gdk_content_provider_new_typed(G_TYPE_POINTER, li);
}

static void on_drag_begin(GtkDragSource *source,
						  GdkDrag *drag,
						  GtkWidget *self) {
	// Set the widget as the drag icon
	graphene_point_t point = *(graphene_point_t *)g_object_get_data(G_OBJECT(source), "drag-point");
	GdkPaintable *paintable = gtk_widget_paintable_new(self);
	int width = gtk_widget_get_width(self);
	int height = gtk_widget_get_height(self);

	GtkSnapshot *snapshot = gtk_snapshot_new();

	gdk_paintable_snapshot(paintable, snapshot, width, height);

	paintable = gtk_snapshot_free_to_paintable(snapshot, NULL);
	gtk_drag_source_set_icon(source, paintable, point.x, point.y);
	g_object_unref(paintable);
}

static void update_icon(GObject* self, GParamSpec* pspec, gpointer user_data) {
	GValue value = G_VALUE_INIT;
	g_value_init(&value, pspec->value_type);
	g_object_get_property(self, pspec->name, &value);
	if(G_VALUE_HOLDS(&value, G_TYPE_STRING)) {
		gtk_image_set_from_file(GTK_IMAGE(user_data), g_value_get_string(&value));
	}
}

static void instance_list_view_bind_factory(GtkListItemFactory *factory, GtkListItem *list_item, gpointer user_data) {
	struct InstanceRowWidgets *rw = g_object_get_data(G_OBJECT(list_item), "row-widgets");
	GtkWidget *parent = gtk_widget_get_parent(rw->grid);

	GtkGesture *click = gtk_gesture_click_new();
	gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click), GDK_BUTTON_SECONDARY);
	GMenu *menu = g_menu_new();
	g_menu_append(menu, "Edit", "instance.edit");
#ifdef _WIN32
	g_menu_append(menu, "Create shortcut", "instance.create-launcher");
#else
	g_menu_append(menu, "Create launcher", "instance.create-launcher");
#endif
	g_menu_append(menu, "Delete", "instance.delete");
	g_signal_connect(click, "pressed", G_CALLBACK(show_row_menu), menu);
	gtk_widget_add_controller(parent, GTK_EVENT_CONTROLLER(click));

	MicrolauncherInstance *instance = gtk_list_item_get_item(list_item);
	GSimpleActionGroup *actions = g_simple_action_group_new();
	GSimpleAction *action;

	const char *action_names[] = {"edit", "create-launcher", "delete", NULL};
	int i = 0;
	while(action_names[i]) {
		action = g_simple_action_new(action_names[i], NULL);
		g_signal_connect(action, "activate", G_CALLBACK(instance_action), instance);
		g_action_map_add_action(G_ACTION_MAP(actions), G_ACTION(action));
		i++;
	}
	gtk_widget_insert_action_group(parent, "instance", G_ACTION_GROUP(actions));

	GtkDragSource *drag_source = gtk_drag_source_new();
	gtk_drag_source_set_actions(drag_source, GDK_ACTION_MOVE);

	g_signal_connect(drag_source, "prepare", G_CALLBACK(on_drag_prepare), list_item);
	g_signal_connect(drag_source, "drag-begin", G_CALLBACK(on_drag_begin), rw->grid);

	gtk_widget_add_controller(parent, GTK_EVENT_CONTROLLER(drag_source));

	g_object_set_data(G_OBJECT(list_item), "store", user_data);
	GtkDropTarget *target = gtk_drop_target_new(G_TYPE_POINTER, GDK_ACTION_MOVE);
	struct DropUserData *ondrop = g_new(struct DropUserData, 1);
	ondrop->list_item = list_item;
	ondrop->list = microlauncher_get_instances();
	ondrop->selection = GTK_SINGLE_SELECTION(gtk_list_view_get_model(instancesView));
	g_signal_connect_data(target, "drop", G_CALLBACK(on_drop), ondrop, (GClosureNotify)g_free, 0);
	gtk_widget_add_controller(parent, GTK_EVENT_CONTROLLER(target));

	g_signal_connect(instance, "notify::icon", G_CALLBACK(update_icon), rw->icon);
	gtk_image_set_from_file(rw->icon, instance->icon);
	g_object_bind_property(instance, "name", rw->instanceLabel, "label", G_BINDING_SYNC_CREATE);
	gtk_label_set_label(rw->instanceLabel, instance->name);
	g_object_bind_property(instance, "location", rw->locationLabel, "label", G_BINDING_SYNC_CREATE);
	gtk_label_set_label(rw->locationLabel, instance->location);
	g_object_bind_property(instance, "version", rw->versionLabel, "label", G_BINDING_SYNC_CREATE);
	gtk_label_set_label(rw->versionLabel, instance->version);
}

static void instance_selection_changed(GtkSingleSelection *self, guint position, guint n_items, GListStore *user_data) {
	settings->instance = gtk_single_selection_get_selected_item(self);
	microlauncher_gui_refresh_instance();
}

static void activate_instance_row(GtkListView *self, guint position, GListStore *user_data) {
	settings->instance = g_list_model_get_item(G_LIST_MODEL(user_data), position);
	microlauncher_gui_refresh_instance();
	microlauncher_gui_switch_to_tab(NULL, "launcher");
	clicked_play();
}

static void account_selection_changed(GtkSingleSelection *self, guint position, guint n_items, GtkStringList *user_data) {
	settings->user = gtk_single_selection_get_selected_item(self);
	microlauncher_gui_refresh_instance();
}

static void activate_account_row(GtkListView *self, guint position, GtkStringList *user_data) {
	settings->user = g_list_model_get_item(G_LIST_MODEL(user_data), position);
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
	instancesList = g_list_store_new(G_TYPE_OBJECT);

	GSList *node = *microlauncher_get_instances();
	while(node) {
		MicrolauncherInstance *inst = node->data;
		g_list_store_append(instancesList, inst);
		node = node->next;
	}

	GtkSingleSelection *selection = gtk_single_selection_new(G_LIST_MODEL(instancesList));
	gtk_single_selection_set_can_unselect(selection, true);
	gtk_single_selection_set_autoselect(selection, false);
	GtkListItemFactory *factory = gtk_signal_list_item_factory_new();
	g_signal_connect(factory, "setup", G_CALLBACK(instance_list_view_setup_factory), NULL);
	g_signal_connect(factory, "bind", G_CALLBACK(instance_list_view_bind_factory), instancesList);

	widget = gtk_list_view_new(GTK_SELECTION_MODEL(selection), factory);
	instancesView = GTK_LIST_VIEW(widget);
	gtk_single_selection_set_selected(selection, GTK_INVALID_LIST_POSITION);
	if(settings->instance) {
		guint pos = G_MAXUINT;
		if(g_list_store_find(instancesList, settings->instance, &pos)) {
			gtk_single_selection_set_selected(selection, pos);
		}
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
	MicrolauncherAccount *account = MICROLAUNCHER_ACCOUNT(data);
	if(strcmp(g_action_get_name(G_ACTION(simple_action)), "edit") == 0) {

	} else if(strcmp(g_action_get_name(G_ACTION(simple_action)), "delete") == 0) {
		remove_account(account);
	}
}

static void account_list_view_bind_factory(GtkListItemFactory *factory, GtkListItem *list_item, gpointer user_data) {
	struct AccountRowWidgets *rw = g_object_get_data(G_OBJECT(list_item), "row-widgets");
	GtkWidget *parent = gtk_widget_get_parent(rw->grid);

	GtkGesture *click = gtk_gesture_click_new();
	gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click), GDK_BUTTON_SECONDARY);
	GMenu *menu = g_menu_new();
	g_menu_append(menu, "Edit", "account.edit");
	g_menu_append(menu, "Delete", "account.delete");
	g_signal_connect(click, "pressed", G_CALLBACK(show_row_menu), menu);
	gtk_widget_add_controller(parent, GTK_EVENT_CONTROLLER(click));

	MicrolauncherAccount *account = gtk_list_item_get_item(list_item);
	GSimpleActionGroup *actions = g_simple_action_group_new();
	GSimpleAction *action;

	// action = g_simple_action_new("edit", NULL);
	// g_signal_connect(action, "activate", G_CALLBACK(account_action), account);
	// g_action_map_add_action(G_ACTION_MAP(actions), G_ACTION(action));

	action = g_simple_action_new("delete", NULL);
	g_signal_connect(action, "activate", G_CALLBACK(account_action), account);
	g_action_map_add_action(G_ACTION_MAP(actions), G_ACTION(action));

	gtk_widget_insert_action_group(parent, "account", G_ACTION_GROUP(actions));

	GtkDragSource *drag_source = gtk_drag_source_new();
	gtk_drag_source_set_actions(drag_source, GDK_ACTION_MOVE);

	g_signal_connect(drag_source, "prepare", G_CALLBACK(on_drag_prepare), list_item);
	g_signal_connect(drag_source, "drag-begin", G_CALLBACK(on_drag_begin), rw->grid);

	gtk_widget_add_controller(parent, GTK_EVENT_CONTROLLER(drag_source));

	g_object_set_data(G_OBJECT(list_item), "store", user_data);
	GtkDropTarget *target = gtk_drop_target_new(G_TYPE_POINTER, GDK_ACTION_MOVE);
	struct DropUserData *ondrop = g_new(struct DropUserData, 1);
	ondrop->list_item = list_item;
	ondrop->list = microlauncher_get_accounts();
	ondrop->selection = GTK_SINGLE_SELECTION(gtk_list_view_get_model(accountsView));
	g_signal_connect_data(target, "drop", G_CALLBACK(on_drop), ondrop, (GClosureNotify)g_free, 0);
	gtk_widget_add_controller(parent, GTK_EVENT_CONTROLLER(target));

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

static gboolean add_account_in_main(MicrolauncherAccount *user) {
	add_account(user);
	return false;
}

static void check_device_code(GTask *task, gpointer source_object, gpointer task_data, GCancellable *cancellable) {
	struct DeviceCodeOAuthResponse *response = (struct DeviceCodeOAuthResponse *)task_data;
	if(!response || !response->device_code) {
		return;
	}
	struct MicrosoftUser *userdata = g_new0(struct MicrosoftUser, 1);
	int result = microlauncher_msa_device_token(response->device_code, userdata);
	if(result == 1) { // pending
		checkDeviceCode = response->interval;
	} else if(result == -1) { // cancel/error
		msaSecondsLeft = 0;	  // Request new code
	} else if(result == 0) {
		MicrolauncherAccount *user = microlauncher_account_new(NULL, ACCOUNT_TYPE_MSA);
		microlauncher_account_set_userdata(user, userdata);
		g_idle_add(G_SOURCE_FUNC(gtk_window_close), popupWindow);
		if(microlauncher_auth_user(user, NULL)) {
			g_idle_add(G_SOURCE_FUNC(add_account_in_main), user);
		} else {
			free(user);
		}
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
	const char *name = gtk_entry_buffer_get_text(gtk_entry_get_buffer(data->usernameEntry));
	MicrolauncherAccount *user = microlauncher_account_new(name, ACCOUNT_TYPE_OFFLINE);
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
	accountsList = g_list_store_new(G_TYPE_OBJECT);
	GSList *node = *microlauncher_get_accounts();
	while(node) {
		MicrolauncherAccount *user = node->data;
		g_list_store_append(accountsList, user);
		node = node->next;
	}

	GtkSingleSelection *selection = gtk_single_selection_new(G_LIST_MODEL(accountsList));
	gtk_single_selection_set_can_unselect(selection, true);
	gtk_single_selection_set_autoselect(selection, false);
	GtkListItemFactory *factory = gtk_signal_list_item_factory_new();
	g_signal_connect(factory, "setup", G_CALLBACK(account_list_view_setup_factory), NULL);
	g_signal_connect(factory, "bind", G_CALLBACK(account_list_view_bind_factory), accountsList);

	widget = gtk_list_view_new(GTK_SELECTION_MODEL(selection), factory);
	accountsView = GTK_LIST_VIEW(widget);
	gtk_single_selection_set_selected(selection, GTK_INVALID_LIST_POSITION);
	if(settings->user) {
		guint pos = G_MAXUINT;
		if(g_list_store_find(accountsList, settings->user, &pos)) {
			gtk_single_selection_set_selected(selection, pos);
		}
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

struct AsyncPid {
	GPid pid;
	void *userdata;
};

static gboolean microlauncher_gui_enable_kill(struct AsyncPid *userdata) {
	instancePid = userdata->pid;
	g_object_unref(instanceCancellable);
	instanceCancellable = NULL;
	gtk_button_set_label(playButton, "Kill");
	gtk_widget_set_sensitive(GTK_WIDGET(playButton), true);
	return false;
}

static void scheduled_launcher_set_pid(GPid pid, void *userdata) {
	struct AsyncPid *data = g_new(struct AsyncPid, 1);
	data->pid = pid;
	data->userdata = userdata;
	g_idle_add(G_SOURCE_FUNC(microlauncher_gui_enable_kill), data);
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

static gboolean microlauncher_gui_show_err(char *data) {
	if(!data) {
		return false;
	}
	GtkWindow *dialog = gtk_modal_dialog_new(window);
	// gtk_window_set_resizable(dialog, false);
	gtk_window_set_title(dialog, "Error");
	GtkBox *box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 10));
	gtk_widget_set_margin(GTK_WIDGET(box), 10, 10, 10, 10);
	GtkWidget *widget = gtk_label_new(data);
	gtk_label_set_wrap(GTK_LABEL(widget), true);
	gtk_label_set_yalign(GTK_LABEL(widget), 0.0F);
	gtk_widget_set_vexpand(widget, true);
	gtk_box_append(box, widget);
	widget = gtk_button_new_with_label("Ok");
	gtk_widget_set_halign(widget, GTK_ALIGN_END);
	g_signal_connect(widget, "clicked", G_CALLBACK(cancel_callback), dialog);
	gtk_box_append(box, widget);
	gtk_window_set_child(dialog, GTK_WIDGET(box));
	gtk_window_present(dialog);
	free(data);
	return false;
}

static void scheduled_show_error(const char *msg, void *userdata) {
	char *data = g_strdup(msg);
	g_idle_add(G_SOURCE_FUNC(microlauncher_gui_show_err), data);
}

static void init_gpus(void) {
#ifdef __unix
	struct pci_access *acc = pci_alloc();
	struct pci_filter filter;
	pci_filter_init(acc, &filter);
	// filter.device_class = PCI_BASE_CLASS_DISPLAY << 8;
	filter.device_class = PCI_CLASS_DISPLAY_VGA;
	filter.device_class_mask = 0xFF00;
	char devbuf[256];
	char bdfbuf[256];
	struct pci_dev *p;
	gpuIds = g_slist_append(NULL, NULL);
	gpuLabels = gtk_string_list_new(NULL);
	gtk_string_list_append(gpuLabels, "Default");

	pci_init(acc);
	pci_scan_bus(acc);
	for(p = acc->devices; p; p = p->next) {
		if(!pci_filter_match(&filter, p)) {
			continue;
		}
		snprintf(bdfbuf, sizeof(bdfbuf), "pci-%04d_%02u_%02u_%01u", p->domain, p->bus, p->dev, p->func);
		pci_fill_info(p, PCI_FILL_IDENT);
		char *s = pci_lookup_name(acc, devbuf, sizeof(devbuf), PCI_LOOKUP_VENDOR | PCI_LOOKUP_DEVICE, p->vendor_id, p->device_id);
		gtk_string_list_append(gpuLabels, strdup(s));
		gpuIds = g_slist_append(gpuIds, strdup(bdfbuf));
	}
	int i = 0;
	const char *s;
	while((s = gtk_string_list_get_string(gpuLabels, i++))) {
		g_print("%s\n", s);
	}
	pci_cleanup(acc);
#endif
}

static void activate(GtkApplication *app, gpointer user_data) {
	GtkWidget *widget, *box;
	GtkCssProvider *provider = gtk_css_provider_new();
	gtk_css_provider_load_from_string(provider, ".msa-code { font-size: 64px; }");
	gtk_style_context_add_provider_for_display(gdk_display_get_default(), GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_THEME);

	window = GTK_WINDOW(gtk_window_new());
	gtk_application_add_window(app, window);
	gtk_window_set_default_size(window, 0, 480);
	gtk_window_set_title(window, LAUNCHER_NAME " " LAUNCHER_VERSION " - Minecraft Launcher");

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
		.show_error = scheduled_show_error,
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
	init_gpus();

	app = gtk_application_new(APPID, G_APPLICATION_NON_UNIQUE);
	g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
	status = g_application_run(G_APPLICATION(app), argc, argv);
	g_object_unref(app);

	return status;
}
