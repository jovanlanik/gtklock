#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <gtk/gtk.h>

#include "window.h"
#include "gtklock.h"

struct GtkLock *gtklock = NULL;

static char* style = NULL;

static gboolean should_daemonize = FALSE;
static gboolean no_layer_shell = FALSE;
static gboolean no_input_inhibit = FALSE;
static gboolean show_user_info = FALSE;

static GOptionEntry entries[] = {
	{ "daemonize", 'd', 0, G_OPTION_ARG_NONE, &should_daemonize, "Detach from the controlling terminal after locking", NULL },
	{ "no-layer-shell", 'l', 0, G_OPTION_ARG_NONE, &no_layer_shell, "Don't use wlr-layer-shell", NULL },
	{ "no-input-inhibit", 'i', 0, G_OPTION_ARG_NONE, &no_input_inhibit, "Don't use wlr-input-inhibitor", NULL },
	{ "style", 's', 0, G_OPTION_ARG_FILENAME, &style, "CSS style to use", NULL },
	{ "show-user-info", 'u', 0, G_OPTION_ARG_NONE, &show_user_info, "Display users profile picture and name", NULL },
	{ NULL },
};

static void reload_outputs() {
	GdkDisplay *display = gdk_display_get_default();

	// Make note of all existing windows
	GArray *dead_windows = g_array_new(FALSE, TRUE, sizeof(struct Window*));
	for(guint idx = 0; idx < gtklock->windows->len; idx++) {
		struct Window *ctx = g_array_index(gtklock->windows, struct Window*, idx);
		g_array_append_val(dead_windows, ctx);
	}

	// Go through all monitors
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
		} else create_window(monitor);
	}

	// Remove all windows left behind
	for(guint idx = 0; idx < dead_windows->len; idx++) {
		struct Window *w = g_array_index(dead_windows, struct Window*, idx);
		gtk_widget_destroy(w->window);
		if(gtklock->focused_window == w) gtklock->focused_window = NULL;
	}

	for(guint idx = 0; idx < gtklock->windows->len; idx++) {
		struct Window *win = g_array_index(gtklock->windows, struct Window*, idx);
		window_configure(win);
	}

	g_array_unref(dead_windows);
}

static void monitors_changed(GdkDisplay *display, GdkMonitor *monitor) {
	reload_outputs();
}

static gboolean setup_layer_shell() {
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
		window_configure(win);
	}
}

static void attach_custom_style(const char* path) {
	GtkCssProvider *provider = gtk_css_provider_new();
	GError *err = NULL;

	gtk_css_provider_load_from_path(provider, path, &err);
	if(err != NULL) {
		g_warning("Style loading failed: %s", err->message);
		g_error_free(err);
	} else
		gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
			GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	g_object_unref(provider);
}

static void daemonize(void) {
	pid_t pid = fork();
	if(pid == -1) g_error("Failed to daemonize!\n");
	else if(pid != 0) exit(0);

	if(setsid() == -1) exit(1);
	pid = fork();
	if(pid == -1) exit(1);
	else if(pid != 0) exit(0);
}

int main(int argc, char **argv) {
	GError *error = NULL;
	GOptionContext *option_context = g_option_context_new("- GTK-based lockscreen for sway");
	g_option_context_add_main_entries(option_context, entries, NULL);
	g_option_context_set_help_enabled(option_context, FALSE);
	g_option_context_set_ignore_unknown_options(option_context, TRUE);
	g_option_context_parse(option_context, &argc, &argv, &error);

	if(should_daemonize) daemonize();

	g_option_context_add_group(option_context, gtk_get_option_group(TRUE));
	g_option_context_set_help_enabled(option_context, TRUE);
	g_option_context_set_ignore_unknown_options(option_context, FALSE);
	if(!g_option_context_parse(option_context, &argc, &argv, &error))
		g_error("Option parsing failed: %s\n", error->message);

	gtklock = create_gtklock();
	gtklock->use_layer_shell = !no_layer_shell;
	gtklock->use_input_inhibit = !no_input_inhibit;
	gtklock->show_user_info = show_user_info;

	if(style != NULL) attach_custom_style(style);

	g_signal_connect(gtklock->app, "activate", G_CALLBACK(activate), NULL);
	int status = g_application_run(G_APPLICATION(gtklock->app), argc, argv);
	gtklock_destroy(gtklock);
	return status;
}
