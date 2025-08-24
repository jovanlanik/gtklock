// gtklock
// Copyright (c) 2022 Kenny Levinsen, Jovan Lanik, Bhaskar Khoraja

#define _POSIX_C_SOURCE 200809L

#include <signal.h>
#include <sys/wait.h>
#include <locale.h>
#include <glib-unix.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include "util.h"
#include "window.h"
#include "gtklock.h"
#include "config.h"
#include "module.h"
#include "xdg.h"

#ifndef MAJOR_VERSION
#warning MAJOR_VERSION not defined.
#define MAJOR_VERSION 0
#endif
#ifndef MINOR_VERSION
#warning MINOR_VERSION not defined.
#define MINOR_VERSION 0
#endif
#ifndef MICRO_VERSION
#warning MICRO_VERSION not defined.
#define MICRO_VERSION 0
#endif

#define _STR(x) #x
#define STR(x) _STR(x)

struct GtkLock *gtklock = NULL;

static gboolean show_version = FALSE;
static gboolean should_daemonize = FALSE;
static gboolean follow_focus = FALSE;
static gboolean idle_hide = FALSE;
static gboolean start_hidden = FALSE;

static gint idle_timeout = 15;

static gchar *gtk_theme = NULL;
static gchar *config_path = NULL;
static gchar *style_path = NULL;
static gchar *layout_path = NULL;
static gchar **module_path = NULL;
static gchar *background_path = NULL;
static gchar *time_format = NULL;
static gchar *date_format = NULL;
static gchar *lock_command = NULL;
static gchar *unlock_command = NULL;
static gchar **monitor_priority = NULL;

static GOptionEntry main_entries[] = {
	{ "version", 'v', 0, G_OPTION_ARG_NONE, &show_version, "Show version", NULL },
	{ "config", 'c', 0, G_OPTION_ARG_FILENAME, &config_path, "Load config file", NULL },
	{ "daemonize", 'd', 0, G_OPTION_ARG_NONE, &should_daemonize, "Detach from controlling terminal", NULL },
	{ NULL },
};

static GOptionEntry config_entries[] = {
	{ "gtk-theme", 'g', 0, G_OPTION_ARG_STRING, &gtk_theme, "Set GTK theme", NULL },
	{ "style", 's', 0, G_OPTION_ARG_FILENAME, &style_path, "Load CSS style file", NULL },
	{ "layout", 'x', 0, G_OPTION_ARG_FILENAME, &layout_path, "Load XML layout file", NULL },
	{ "modules", 'm', 0, G_OPTION_ARG_FILENAME_ARRAY, &module_path, "Load gtklock modules", NULL },
	{ "background", 'b', 0, G_OPTION_ARG_FILENAME, &background_path, "Load background", NULL },
	{ "time-format", 't', 0, G_OPTION_ARG_STRING, &time_format, "Set time format", NULL },
	{ "date-format", 'D', 0, G_OPTION_ARG_STRING, &date_format, "Set date format", NULL },
	{ "follow-focus", 'f', 0, G_OPTION_ARG_NONE, &follow_focus, "Follow focus between monitors", NULL },
	{ "idle-hide", 'H', 0, G_OPTION_ARG_NONE, &idle_hide, "Hide form when idle", NULL },
	{ "idle-timeout", 'T', 0, G_OPTION_ARG_INT, &idle_timeout, "Idle timeout in seconds", NULL },
	{ "start-hidden", 'S', 0, G_OPTION_ARG_NONE, &start_hidden, "Start with hidden form", NULL },
	{ "lock-command", 'L', 0, G_OPTION_ARG_STRING, &lock_command, "Command to execute after locking", NULL },
	{ "unlock-command", 'U', 0, G_OPTION_ARG_STRING, &unlock_command, "Command to execute after unlocking", NULL },
	{ "monitor-priority", 'M', 0, G_OPTION_ARG_STRING_ARRAY, &monitor_priority, "Set monitor focus priority", NULL },
	{ NULL },
};

static GOptionEntry debug_entries[] = {
	{ NULL },
};

static pid_t parent = -2;

static void monitors_added(GdkDisplay *display, GdkMonitor *monitor, gpointer user_data) {
	struct Window *w = NULL;
	if(window_by_monitor(monitor) == NULL) w = create_window(monitor);
	if(w == g_array_index(gtklock->windows, struct Window*, 0)) gtklock_focus_window(gtklock, w);
	module_on_output_change(gtklock);
}

static void monitors_removed(GdkDisplay *display, GdkMonitor *monitor, gpointer user_data) {
	struct Window *w = window_by_monitor(monitor);
	if(w != NULL) {
		if(gtklock->focused_window == w) {
			struct Window *any_window = g_array_index(gtklock->windows, struct Window*, 0);
			if(any_window == w) any_window = g_array_index(gtklock->windows, struct Window*, 1);

			gtklock->focused_window = NULL;
			if(any_window) window_swap_focus(any_window, w);
		}

		// gtk_session_lock_unmap_lock_window(GTK_WINDOW(w->window));
		gtk_window_destroy(GTK_WINDOW(w->window));
	}
	module_on_output_change(gtklock);
}

static gboolean find_priority_monitor(char *name) {
	if(monitor_priority) {
		for(guint i = 0; monitor_priority[i] != NULL; ++i)
			if(g_strcmp0(name, monitor_priority[i]) == 0) return TRUE;
	}
	return FALSE;
}

static gint compare_priority_monitors(gconstpointer first_ptr, gconstpointer second_ptr, gpointer user_data) {
	const void *first = *(const void **)first_ptr;
	const void *second = *(const void **)second_ptr;
	if(first != second && monitor_priority) {
		GHashTable *names = user_data;
		char *first_name = g_hash_table_lookup(names, first);
		char *second_name = g_hash_table_lookup(names, second);
		gint first_index = -1;
		gint second_index = -1;
		for(guint i = 0; monitor_priority[i] != NULL; ++i) {
			if(g_strcmp0(first_name, monitor_priority[i]) != 0) continue;
			first_index = i;
			break;
		}
		for(guint i = 0; monitor_priority[i] != NULL; ++i) {
			if(g_strcmp0(second_name, monitor_priority[i]) != 0) continue;
			second_index = i;
			break;
		}
		return first_index - second_index;
	}
	return 0;
}

// See comment in window.c
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

static void activate(GtkApplication *app, gpointer user_data) {
	gtklock_activate(gtklock);
	module_on_activation(gtklock);

	GdkDisplay *display = gdk_display_get_default();
	g_signal_connect(display, "monitor-added", G_CALLBACK(monitors_added), NULL);
	g_signal_connect(display, "monitor-removed", G_CALLBACK(monitors_removed), NULL);

	GArray *monitors = g_array_new(FALSE, TRUE, sizeof(GdkMonitor *));
	GArray *priority_monitors = g_array_new(FALSE, TRUE, sizeof(GdkMonitor *));
	GHashTable *priority_monitor_names = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, g_free);

	GListModel* monitors2 = gdk_display_get_monitors(display);
	for(int i = 0; i < g_list_model_get_n_items(monitors2); ++i) {
		GdkMonitor *monitor = g_list_model_get_item (monitors2, i);
		char *name = gdk_monitor_get_model(monitor);

		if(find_priority_monitor(name)) {
			g_hash_table_insert(priority_monitor_names, monitor, name);
			g_array_append_val(priority_monitors, monitor);
		} else {
			g_array_append_val(monitors, monitor);
			g_free(name);
		}
	}

	g_array_sort_with_data(priority_monitors, compare_priority_monitors, priority_monitor_names);
	gsize len;
	gpointer data = g_array_steal(priority_monitors, &len);
	g_array_prepend_vals(monitors, data, len);

	gtk_session_lock_instance_lock(gtklock->lock);

	for(guint idx = 0; idx < monitors->len; idx++) create_window(g_array_index(monitors, GdkMonitor *, idx));
	if(gtklock->windows->len) gtklock_focus_window(gtklock, g_array_index(gtklock->windows, struct Window *, 0));

	g_array_unref(monitors);
	g_array_unref(priority_monitors);
	g_hash_table_unref(priority_monitor_names);


}

static void shutdown(GtkApplication *app, gpointer user_data) {
	gtklock_shutdown(gtklock);
	for(guint idx = 0; idx < gtklock->modules->len; idx++) {
		GModule *module = g_array_index(gtklock->modules, GModule *, idx);
		g_module_close(module);
	}
}

static void attach_style(const gchar *format, ...) G_GNUC_PRINTF(1, 2);
static void attach_style(const gchar *format, ...) {
	GtkCssProvider *provider = gtk_css_provider_new();
	GError *err = NULL;
	va_list args;
	va_start(args, format);
	char *buff = g_strdup_vprintf(format, args);
	va_end(args);
	gtk_css_provider_load_from_string(provider, buff);
	// Now we need to connect to the CssProvider::parsing-error signal to check error!
    /*
	if(err != NULL) {
		g_warning("Style loading failed: %s", err->message);
		g_error_free(err);
	}
	*/
	GdkDisplay *display = gdk_display_get_default();
	if(display == NULL){
		display = gdk_display_open(NULL); // NULL uses the default display name
	}
	if(display == NULL){
		g_warning("Failed to open display");
		return;
	}
	gtk_style_context_add_provider_for_display(gdk_display_get_default(),
		GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

	g_object_unref(provider);
	g_free(buff);
}

static void attach_custom_style(const gchar *path) {
	GtkCssProvider *provider = gtk_css_provider_new();
	GError *err = NULL;

	gtk_css_provider_load_from_path(provider, path);
	/*
	// Again we need to actually connect to a signal now!
	if(err != NULL) {
		g_warning("Custom style loading failed: %s", err->message);
		g_error_free(err);
	}
	*/
	gtk_style_context_add_provider_for_display(gdk_display_get_default(),
			GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION+1);
	g_object_unref(provider);
}

static void daemonize(void) {
	parent = getpid();
	pid_t pid = fork();
	if(pid == -1) report_error_and_exit("Failed to daemonize!\n");
	else if(pid != 0) {
		int status;
		waitpid(pid, &status, 0);
		if(WIFEXITED(status) && WEXITSTATUS(status) == EXIT_SUCCESS) {
			sigset_t set;
			sigemptyset(&set);
			sigaddset(&set, SIGUSR2);
			sigprocmask(SIG_BLOCK, &set, NULL);
			int ret = sigtimedwait(&set, NULL, &(struct timespec){ 1, 0 });
			if(ret == SIGUSR2)
				exit(EXIT_SUCCESS);
			exit(EXIT_FAILURE);
		}
		report_error_and_exit("Failed to daemonize!\n");
	}

	if(setsid() == -1) exit(EXIT_FAILURE);
	pid = fork();
	if(pid == -1) exit(EXIT_FAILURE);
	else if(pid != 0) exit(EXIT_SUCCESS);
}

static gboolean signal_handler(gpointer data) {
	g_application_quit(G_APPLICATION(gtklock->app));
	return G_SOURCE_REMOVE;
}

#if GLIB_CHECK_VERSION(2, 74, 0)
	#define GTKLOCK_FLAGS G_APPLICATION_DEFAULT_FLAGS
#else
	#define GTKLOCK_FLAGS G_APPLICATION_FLAGS_NONE
#endif

int main(int argc, char **argv) {
	gtk_init();
	setlocale(LC_ALL, "");
	bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);

	GOptionContext *option_context = g_option_context_new("- GTK-based lockscreen for sway");
	g_option_context_add_main_entries(option_context, main_entries, NULL);
	g_option_context_set_help_enabled(option_context, FALSE);
	g_option_context_set_ignore_unknown_options(option_context, TRUE);
	g_option_context_parse(option_context, &argc, &argv, NULL);

	if(show_version) {
		g_print("gtklock %s\n", "v" STR(MAJOR_VERSION) "." STR(MINOR_VERSION) "." STR(MICRO_VERSION));
		exit(EXIT_SUCCESS);
	}

	if(should_daemonize) daemonize();

	if(config_path == NULL) config_path = xdg_get_config_path("config.ini");
	if(config_path) config_load(config_path, "main", config_entries);

	GOptionGroup *config_group =
		g_option_group_new("config", "Config options", "Show options available in the config", NULL, NULL);
	GOptionGroup *debug_group =
		g_option_group_new("debug", "Debug options", "Show options for debugging and styling", NULL, NULL);

	g_option_group_add_entries(config_group, config_entries);
	g_option_group_add_entries(debug_group, debug_entries);
	g_option_context_add_group(option_context, config_group);
	g_option_context_add_group(option_context, debug_group);
	//g_option_context_add_group(option_context, gtk_get_option_group(TRUE));
	g_option_context_parse(option_context, &argc, &argv, NULL);

	GArray *modules = g_array_new(FALSE, TRUE, sizeof(GModule *));
	if(module_path) {
		for(guint i = 0; module_path[i] != NULL; ++i) {
			GModule *module = module_load(module_path[i]);
			if(!module) continue;
			g_array_append_val(modules, module);

			gchar *module_name = NULL;
			GOptionEntry *module_entries = NULL;
			gboolean has_name = g_module_symbol(module, "module_name", (gpointer *)&module_name);
			gboolean has_entries = g_module_symbol(module, "module_entries", (gpointer *)&module_entries);
			if(has_name && has_entries) {
				GOptionGroup *module_group =
					g_option_group_new(module_name, module_name, "Show module options", NULL, NULL);
				g_option_group_add_entries(module_group, module_entries);
				g_option_context_add_group(option_context, module_group);

				if(config_path) config_load(config_path, module_name, module_entries);
			}
		}
	}

	GError *error = NULL;
	g_option_context_set_help_enabled(option_context, TRUE);
	g_option_context_set_ignore_unknown_options(option_context, FALSE);
	if(!g_option_context_parse(option_context, &argc, &argv, &error))
		report_error_and_exit("Option parsing failed: %s\n", error->message);

	if(gtk_theme) {
		GtkSettings *settings = gtk_settings_get_default();
		g_object_set(settings, "gtk-theme-name", gtk_theme, NULL);
	}

	struct GtkLock g = {};
	gtklock = &g;

	gtklock->app = gtk_application_new(NULL, GTKLOCK_FLAGS);
	gtklock->parent = parent;

	gtklock->windows = g_array_new(FALSE, TRUE, sizeof(struct Window *));
	gtklock->messages = g_array_new(FALSE, TRUE, sizeof(char *));
	gtklock->errors = g_array_new(FALSE, TRUE, sizeof(char *));

	gtklock->hidden = start_hidden;
	gtklock->idle_timeout = (guint)idle_timeout;

	gtklock->follow_focus = follow_focus;
	gtklock->use_idle_hide = idle_hide;

	gtklock->time_format = time_format;
	gtklock->date_format = date_format;
	gtklock->config_path = config_path;
	gtklock->layout_path = layout_path;
	gtklock->lock_command = lock_command;
	gtklock->unlock_command = unlock_command;

	gtklock->modules = modules;

	if(background_path != NULL) {
		GFile *file = g_file_new_for_path(background_path);
		char *path = g_file_get_path(file);
		g_object_unref(file);
		attach_style(
			"window {"
			"background-color: black;"
			"background-image: url(\"%s\");"
			"background-size: 100%% 100%%;"
			"}",
			path
		);
	}

	attach_style(
		"window #clock-label {"
		"font-size: 96pt;"
		"}"
		"window.focused:not(.hidden) #clock-label {"
		"font-size: 32pt;"
		"}"
		"window #date-label {"
		"font-size: 28px;"
		"}"
		"window.focused:not(.hidden) #date-label {"
		"font-size: 12pt;"
		"}"
		"#error-label {"
		"color: red;"
		"}"
	);

	if(style_path == NULL) style_path = xdg_get_config_path("style.css");
	if(style_path != NULL) {
		attach_custom_style(style_path);
		g_free(style_path);
	}

	g_signal_connect(gtklock->app, "activate", G_CALLBACK(activate), NULL);
	g_signal_connect(gtklock->app, "shutdown", G_CALLBACK(shutdown), NULL);
	g_unix_signal_add(SIGTERM, G_SOURCE_FUNC(signal_handler), NULL);
	int status = g_application_run(G_APPLICATION(gtklock->app), argc, argv);

	g_object_unref(gtklock->app);
	g_array_unref(gtklock->windows);

	return status;
}

