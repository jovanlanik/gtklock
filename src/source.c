// gtklock
// Copyright (c) 2022 Kenny Levinsen, Jovan Lanik

#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <signal.h>
#include <sys/wait.h>
#include <glib-unix.h>
#include <gtk/gtk.h>

#include "util.h"
#include "auth.h"
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
static gboolean no_layer_shell = FALSE;
static gboolean no_input_inhibit = FALSE;
static gboolean idle_hide = FALSE;
static gboolean start_hidden = FALSE;

static gint idle_timeout = 15;

static gchar *gtk_theme = NULL;
static gchar *config_path = NULL;
static gchar *style_path = NULL;
static gchar **module_path = NULL;
static gchar *background_path = NULL;
static gchar *time_format = NULL;
static gchar *lock_command = NULL;
static gchar *unlock_command = NULL;

static GOptionEntry main_entries[] = {
	{ "version", 'v', 0, G_OPTION_ARG_NONE, &show_version, "Show version", NULL },
	{ "config", 'c', 0, G_OPTION_ARG_FILENAME, &config_path, "Load config file", NULL },
	{ "daemonize", 'd', 0, G_OPTION_ARG_NONE, &should_daemonize, "Detach from controlling terminal", NULL },
	{ NULL },
};

static GOptionEntry config_entries[] = {
	{ "gtk-theme", 'g', 0, G_OPTION_ARG_STRING, &gtk_theme, "Set GTK theme", NULL },
	{ "style", 's', 0, G_OPTION_ARG_FILENAME, &style_path, "Load CSS style file", NULL },
	{ "modules", 'm', 0, G_OPTION_ARG_FILENAME_ARRAY, &module_path, "Load gtklock modules", NULL },
	{ "background", 'b', 0, G_OPTION_ARG_FILENAME, &background_path, "Load background", NULL },
	{ "time-format", 't', 0, G_OPTION_ARG_STRING, &time_format, "Set time format", NULL },
	{ "idle-hide", 'H', 0, G_OPTION_ARG_NONE, &idle_hide, "Hide form when idle", NULL },
	{ "idle-timeout", 'T', 0, G_OPTION_ARG_INT, &idle_timeout, "Idle timeout in seconds", NULL },
	{ "start-hidden", 'S', 0, G_OPTION_ARG_NONE, &start_hidden, "Start with hidden form", NULL },
	{ "lock-command", 'L', 0, G_OPTION_ARG_STRING, &lock_command, "Command to execute before locking", NULL },
	{ "unlock-command", 'U', 0, G_OPTION_ARG_STRING, &unlock_command, "Command to execute after unlocking", NULL },
	{ NULL },
};

static GOptionEntry debug_entries[] = {
	{ "no-layer-shell", 'l', 0, G_OPTION_ARG_NONE, &no_layer_shell, "Don't use wlr-layer-shell", NULL },
	{ "no-input-inhibit", 'i', 0, G_OPTION_ARG_NONE, &no_input_inhibit, "Don't use wlr-input-inhibitor", NULL },
	{ NULL },
};

static pid_t parent = -2;

static void reload_outputs(void) {
	GdkDisplay *display = gdk_display_get_default();

	// Make note of all existing windows
	GArray *dead_windows = g_array_new(FALSE, TRUE, sizeof(struct Window*));
	for(guint idx = 0; idx < gtklock->windows->len; idx++) {
		struct Window *ctx = g_array_index(gtklock->windows, struct Window*, idx);
		g_array_append_val(dead_windows, ctx);
	}

	// Go through all monitors
	struct Window *new = NULL;
	for(int i = 0; i < gdk_display_get_n_monitors(display); i++) {
		GdkMonitor *monitor = gdk_display_get_monitor(display, i);
		struct Window *w = window_by_monitor(monitor);
		if(w != NULL) {
			// We already have this monitor, remove from dead_windows list
			for(guint idx = 0; idx < dead_windows->len; idx++) {
				if(w == g_array_index(dead_windows, struct Window*, idx)) {
					g_array_remove_index_fast(dead_windows, idx);
					break;
				}
			}
		} else {
			w = create_window(monitor);
			gtklock_focus_window(gtklock, w);
		}
		new = w;
	}

	// Remove all windows left behind
	for(guint idx = 0; idx < dead_windows->len; idx++) {
		struct Window *w = g_array_index(dead_windows, struct Window*, idx);
		if(gtklock->focused_window == w) {
			gtklock->focused_window = NULL;
			if(new) window_swap_focus(new, w);
		}
		gtk_widget_destroy(w->window);
	}

	g_array_unref(dead_windows);
	module_on_output_change(gtklock);
}

static void monitors_changed(GdkDisplay *display, GdkMonitor *monitor) {
	reload_outputs();
}

static gboolean setup_layer_shell(void) {
	if(!gtklock->use_layer_shell) return FALSE;

	reload_outputs();

	GdkDisplay *display = gdk_display_get_default();
	g_signal_connect(display, "monitor-added", G_CALLBACK(monitors_changed), NULL);
	g_signal_connect(display, "monitor-removed", G_CALLBACK(monitors_changed), NULL);
	return TRUE;
}

static void activate(GtkApplication *app, gpointer user_data) {
	gtklock_activate(gtklock);
	module_on_activation(gtklock);
	if(!setup_layer_shell()) {
		struct Window *win = create_window(NULL);
		gtklock_focus_window(gtklock, win);
	}
	if(parent > 0) kill(parent, SIGINT);
}

static void shutdown(GtkApplication *app, gpointer user_data) {
	for(guint idx = 0; idx < gtklock->modules->len; idx++) {
		GModule *module = g_array_index(gtklock->modules, GModule *, idx);
		g_module_close(module);
	}
	gtklock_shutdown(gtklock);
}

static void attach_style(const gchar *format, ...) G_GNUC_PRINTF(1, 2);
static void attach_style(const gchar *format, ...) {
	GtkCssProvider *provider = gtk_css_provider_new();
	GError *err = NULL;
	va_list args;
	va_start(args, format);
	char *buff = g_strdup_vprintf(format, args);
	va_end(args);

	gtk_css_provider_load_from_data(provider, buff, -1, &err);
	if(err != NULL) {
		g_warning("Style loading failed: %s", err->message);
		g_error_free(err);
	} else {
		gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
			GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	}

	g_object_unref(provider);
	g_free(buff);
}

static void attach_custom_style(const gchar *path) {
	GtkCssProvider *provider = gtk_css_provider_new();
	GError *err = NULL;

	gtk_css_provider_load_from_path(provider, path, &err);
	if(err != NULL) {
		g_warning("Custom style loading failed: %s", err->message);
		g_error_free(err);
	} else {
		gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
			GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION+1);
	}
	g_object_unref(provider);
}

static void daemonize(void) {
	parent = getpid();
	pid_t pid = fork();
	if(pid == -1) report_error_and_exit("Failed to daemonize!\n");
	else if(pid != 0) {
		int status;
		waitpid(pid, &status, 0);
		if(WIFEXITED(status) && WEXITSTATUS(status) == EXIT_SUCCESS)
			exit(EXIT_SUCCESS);
		report_error_and_exit("Failed to daemonize!\n");
	}

	if(setsid() == -1) exit(EXIT_FAILURE);
	pid = fork();
	if(pid == -1) exit(EXIT_FAILURE);
	else if(pid != 0) exit(EXIT_SUCCESS);
}

static void exec_command(const gchar *command) {
	GError *err = NULL;

	g_spawn_command_line_async(command, &err);
	if(err != NULL) {
		g_warning("Executing `%s` failed: %s", command, err->message);
		g_error_free(err);
	}
}

static gboolean signal_handler(gpointer data) {
	g_application_quit(G_APPLICATION(gtklock->app));
	return G_SOURCE_REMOVE;
}

int main(int argc, char **argv) {
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
	g_option_context_add_group(option_context, gtk_get_option_group(TRUE));
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

	if(lock_command) exec_command(lock_command);

	if(gtk_theme) {
		GtkSettings *settings = gtk_settings_get_default();
		g_object_set(settings, "gtk-theme-name", gtk_theme, NULL);
	}

	gtklock = create_gtklock();
	gtklock->use_layer_shell = !no_layer_shell;
	gtklock->use_input_inhibit = !no_input_inhibit;
	gtklock->use_idle_hide = idle_hide;
	gtklock->idle_timeout = (guint)idle_timeout;
	gtklock->hidden = start_hidden;

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
		"#error-label {"
		"color: red;"
		"}"
	);

	if(style_path == NULL) style_path = xdg_get_config_path("style.css");
	if(style_path != NULL) {
		attach_custom_style(style_path);
		g_free(style_path);
	}

	gtklock->modules = modules;

	gtklock->time_format = time_format;
	gtklock->config_path = config_path;

	g_signal_connect(gtklock->app, "activate", G_CALLBACK(activate), NULL);
	g_signal_connect(gtklock->app, "shutdown", G_CALLBACK(shutdown), NULL);
	g_unix_signal_add(SIGTERM, G_SOURCE_FUNC(signal_handler), NULL);
	int status = g_application_run(G_APPLICATION(gtklock->app), argc, argv);

	gtklock_destroy(gtklock);
	if(unlock_command) exec_command(unlock_command);
	return status;
}

