// gtklock
// Copyright (c) 2022 Kenny Levinsen, Jovan Lanik

#define _POSIX_C_SOURCE

#include <assert.h>
#include <signal.h>
#include <sys/wait.h>
#include <gtk/gtk.h>
#include <glib/gprintf.h>

#include "auth.h"
#include "window.h"
#include "gtklock.h"
#include "config.h"
#include "module.h"
#include "xdg.h"

struct GtkLock *gtklock = NULL;

static gboolean should_daemonize = FALSE;
static gboolean no_layer_shell = FALSE;
static gboolean no_input_inhibit = FALSE;
static gboolean idle_hide = FALSE;
static gboolean start_hidden = FALSE;

static gint idle_timeout = 15;

static char *gtk_theme = NULL;

static char *config_path = NULL;
static char *style_path = NULL;
static char *module_path = NULL;
static char *background_path = NULL;
static char *time_format = NULL;

static GOptionEntry main_entries[] = {
	{ "daemonize", 'd', 0, G_OPTION_ARG_NONE, &should_daemonize, "Detach from controlling terminal", NULL },
	{ "config", 'c', 0, G_OPTION_ARG_FILENAME, &config_path, "Load config file", NULL },
	{ NULL },
};

static GOptionEntry config_entries[] = {
	{ "gtk-theme", 'g', 0, G_OPTION_ARG_STRING, &gtk_theme, "Set GTK theme", NULL },
	{ "style", 's', 0, G_OPTION_ARG_FILENAME, &style_path, "Load CSS style file", NULL },
	{ "module", 'm', 0, G_OPTION_ARG_FILENAME, &module_path, "Load gtklock module", NULL },
	{ "background", 'b', 0, G_OPTION_ARG_FILENAME, &background_path, "Load background", NULL },
	{ "time-format", 't', 0, G_OPTION_ARG_STRING, &time_format, "Set time format", NULL },
	{ "idle-hide", 'H', 0, G_OPTION_ARG_NONE, &idle_hide, "Hide form when idle", NULL },
	{ "idle-timeout", 'T', 0, G_OPTION_ARG_INT, &idle_timeout, "Idle timeout in seconds", NULL },
	{ "start-hidden", 'S', 0, G_OPTION_ARG_NONE, &start_hidden, "Start with hidden form", NULL },
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
		struct Window *w = gtklock_window_by_monitor(gtklock, monitor);
		if(w != NULL) {
			// We already have this monitor, remove from dead_windows list
			for(guint ydx = 0; ydx < dead_windows->len; ydx++) {
				if(w == g_array_index(dead_windows, struct Window*, ydx)) {
					g_array_remove_index_fast(dead_windows, ydx);
					break;
				}
			}
		} else w = create_window(monitor);
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

	for(guint idx = 0; idx < gtklock->windows->len; idx++) {
		struct Window *w = g_array_index(gtklock->windows, struct Window*, idx);
		window_configure(w);
	}

	g_array_unref(dead_windows);
	module_on_output_change(gtklock);
}

static void monitors_changed(GdkDisplay *display, GdkMonitor *monitor) {
	reload_outputs();
}

static gboolean setup_layer_shell(void) {
	if(gtklock->use_layer_shell) {
		reload_outputs();
		GdkDisplay *display = gdk_display_get_default();
		g_signal_connect(display, "monitor-added", G_CALLBACK(monitors_changed), NULL);
		g_signal_connect(display, "monitor-removed", G_CALLBACK(monitors_changed), NULL);
		return TRUE;
	} else return FALSE;
}

static void activate(GtkApplication *app, gpointer user_data) {
	gtklock_activate(gtklock);
	if(!setup_layer_shell()) {
		struct Window *win = create_window(NULL);
		gtklock_focus_window(gtklock, win);
	}
	module_on_activation(gtklock);
	if(parent > 0) kill(parent, SIGINT);
}

static void attach_style(const char *format, ...) G_GNUC_PRINTF(1, 2);
static void attach_style(const char *format, ...) {
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

static void attach_custom_style(const char *path) {
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
	if(pid == -1) g_error("Failed to daemonize!\n");
	else if(pid != 0) {
		int status;
		waitpid(pid, &status, 0);
		if(WIFEXITED(status) && WEXITSTATUS(status) == EXIT_SUCCESS) {
			g_usleep(G_USEC_PER_SEC);
			exit(0);
		}
		g_error("Failed to daemonize!\n");
	}

	freopen("/dev/null", "r", stdin);
	freopen("/dev/null", "w", stdout);
	freopen("/dev/null", "w", stderr);

	if(setsid() == -1) exit(1);
	pid = fork();
	if(pid == -1) exit(1);
	else if(pid != 0) exit(0);
}

int main(int argc, char **argv) {
	GError *error = NULL;
	GOptionContext *option_context = g_option_context_new("- GTK-based lockscreen for sway");
	g_option_context_add_main_entries(option_context, main_entries, NULL);
	g_option_context_set_help_enabled(option_context, FALSE);
	g_option_context_set_ignore_unknown_options(option_context, TRUE);
	g_option_context_parse(option_context, &argc, &argv, &error);

	if(config_path == NULL) config_path = xdg_get_config_path("config.ini");
	if(config_path) config_load(config_path, config_entries);
	
	if(should_daemonize) daemonize();

	GOptionGroup *config_group =
		g_option_group_new("config", "Config options", "Show options available in the config", NULL, NULL);
	GOptionGroup *debug_group =
		g_option_group_new("debug", "Debug options", "Show options for debugging and styling", NULL, NULL);

	g_option_group_add_entries(config_group, config_entries);
	g_option_group_add_entries(debug_group, debug_entries);
	g_option_context_add_group(option_context, config_group);
	g_option_context_add_group(option_context, debug_group);

	g_option_context_add_group(option_context, gtk_get_option_group(TRUE));
	g_option_context_set_help_enabled(option_context, TRUE);
	g_option_context_set_ignore_unknown_options(option_context, FALSE);
	if(!g_option_context_parse(option_context, &argc, &argv, &error))
		g_error("Option parsing failed: %s\n", error->message);

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
			"window { "
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
	);

	if(style_path == NULL) style_path = xdg_get_config_path("style.css");
	if(style_path != NULL) {
		attach_custom_style(style_path);
		free(style_path);
	}

	GModule *module = NULL;
	if(module_path != NULL) module = module_load(module_path);

	gtklock->time_format = time_format;

	g_signal_connect(gtklock->app, "activate", G_CALLBACK(activate), NULL);
	int status = g_application_run(G_APPLICATION(gtklock->app), argc, argv);

	if(module != NULL) g_module_close(module);
	gtklock_destroy(gtklock);
	return status;
}

