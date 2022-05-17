#define _POSIX_C_SOURCE 200809L

#include <gtk/gtk.h>

#include "window.h"
#include "gtklock.h"
#include "input-inhibitor.h"

struct Window* gtklock_window_by_widget(struct GtkLock *gtklock, GtkWidget *window) {
	for(guint idx = 0; idx < gtklock->windows->len; idx++) {
		struct Window *ctx = g_array_index(gtklock->windows, struct Window*, idx);
		if(ctx->window == window) return ctx;
	}
	return NULL;
}

struct Window* gtklock_window_by_monitor(struct GtkLock *gtklock, GdkMonitor *monitor) {
	for(guint idx = 0; idx < gtklock->windows->len; idx++) {
		struct Window *ctx = g_array_index(gtklock->windows, struct Window*, idx);
		if(ctx->monitor == monitor) return ctx;
	}
	return NULL;
}

void gtklock_remove_window_by_widget(struct GtkLock *gtklock, GtkWidget *widget) {
	for(guint idx = 0; idx < gtklock->windows->len; idx++) {
		struct Window *ctx = g_array_index(gtklock->windows, struct Window*, idx);
		if(ctx->window == widget) {
			if(gtklock->focused_window) gtklock->focused_window = NULL;
			free(ctx);
			g_array_remove_index_fast(gtklock->windows, idx);
			return;
		}
	}
}

void gtklock_focus_window(struct GtkLock *gtklock, struct Window* win) {
	struct Window *old = gtklock->focused_window;
	gtklock->focused_window = win;
	window_swap_focus(win, old);
	for(guint idx = 0; idx < gtklock->windows->len; idx++) {
		struct Window *ctx = g_array_index(gtklock->windows, struct Window*, idx);
		window_configure(ctx);
	}
}

void gtklock_update_clocks(struct GtkLock *gtklock) {
	time_t now = time(&now);
	struct tm *now_tm = localtime(&now);
	if(now_tm == NULL) return;
	snprintf(gtklock->time, 8, "%02d:%02d", now_tm->tm_hour, now_tm->tm_min);
	for(guint idx = 0; idx < gtklock->windows->len; idx++) {
		struct Window *ctx = g_array_index(gtklock->windows, struct Window*, idx);
		window_update_clock(ctx);
	}
}

static int gtklock_update_clocks_handler(gpointer data) {
	struct GtkLock *gtklock = (struct GtkLock*)data;
	gtklock_update_clocks(gtklock);
	return TRUE;
}

struct GtkLock* create_gtklock() {
	gtklock = calloc(1, sizeof(struct GtkLock));
	gtklock->app = gtk_application_new(NULL, G_APPLICATION_FLAGS_NONE);
	gtklock->windows = g_array_new(FALSE, TRUE, sizeof(struct Window*));
	return gtklock;
}

void gtklock_activate(struct GtkLock *gtklock) {
	gtklock->draw_clock_source = g_timeout_add_seconds(5, gtklock_update_clocks_handler, gtklock);
	gtklock_update_clocks(gtklock);
	if(gtklock->use_input_inhibit) input_inhibitor_get();
}

void gtklock_destroy(struct GtkLock *gtklock) {
	input_inhibitor_destroy();

	if(gtklock->error != NULL) {
		free(gtklock->error);
		gtklock->error = NULL;
	}

	g_object_unref(gtklock->app);
	g_array_unref(gtklock->windows);

	if(gtklock->draw_clock_source > 0) {
		g_source_remove(gtklock->draw_clock_source);
		gtklock->draw_clock_source = 0;
	}

	free(gtklock);
}

