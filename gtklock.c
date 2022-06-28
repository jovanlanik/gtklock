// gtklock
// Copyright (c) 2022 Kenny Levinsen, Jovan Lanik

// gtklock application

#include <gtk/gtk.h>

#include "window.h"
#include "gtklock.h"
#include "module.h"
#include "input-inhibitor.h"

struct Window* gtklock_window_by_widget(struct GtkLock *gtklock, GtkWidget *window) {
	for(guint idx = 0; idx < gtklock->windows->len; idx++) {
		struct Window *ctx = g_array_index(gtklock->windows, struct Window *, idx);
		if(ctx->window == window) return ctx;
	}
	return NULL;
}

struct Window* gtklock_window_by_monitor(struct GtkLock *gtklock, GdkMonitor *monitor) {
	for(guint idx = 0; idx < gtklock->windows->len; idx++) {
		struct Window *ctx = g_array_index(gtklock->windows, struct Window *, idx);
		if(ctx->monitor == monitor) return ctx;
	}
	return NULL;
}

void gtklock_remove_window(struct GtkLock *gtklock, struct Window *win) {
	for(guint idx = 0; idx < gtklock->windows->len; idx++) {
		struct Window *ctx = g_array_index(gtklock->windows, struct Window *, idx);
		if(ctx == win) {
			g_array_remove_index_fast(gtklock->windows, idx);
			free(ctx);
			return;
		}
	}
}

void gtklock_focus_window(struct GtkLock *gtklock, struct Window* win) {
	struct Window *old = gtklock->focused_window;
	gtklock->focused_window = win;
	window_swap_focus(win, old);
	for(guint idx = 0; idx < gtklock->windows->len; idx++) {
		struct Window *ctx = g_array_index(gtklock->windows, struct Window *, idx);
		if(ctx != win) window_configure(ctx);
	}
	module_on_focus_change(gtklock, win, old);
}

void gtklock_update_clocks(struct GtkLock *gtklock) {
	GDateTime *time = g_date_time_new_now_local();
	if(time == NULL) return;
	if(gtklock->time) g_free(gtklock->time);
	gtklock->time = g_date_time_format(time, gtklock->time_format ? gtklock->time_format : "%R");
	g_date_time_unref(time);

	for(guint idx = 0; idx < gtklock->windows->len; idx++) {
		struct Window *ctx = g_array_index(gtklock->windows, struct Window *, idx);
		window_update_clock(ctx);
	}
}

static int gtklock_update_clocks_handler(gpointer data) {
	struct GtkLock *gtklock = (struct GtkLock *)data;
	gtklock_update_clocks(gtklock);
	return TRUE;
}

struct GtkLock* create_gtklock(void) {
	gtklock = calloc(1, sizeof(struct GtkLock));
	gtklock->app = gtk_application_new(NULL, G_APPLICATION_FLAGS_NONE);
	g_application_hold(G_APPLICATION(gtklock->app));
	gtklock->windows = g_array_new(FALSE, TRUE, sizeof(struct Window *));
	gtklock->messages = g_array_new(FALSE, TRUE, sizeof(char *));
	gtklock->errors = g_array_new(FALSE, TRUE, sizeof(char *));
	return gtklock;
}

void gtklock_activate(struct GtkLock *gtklock) {
	gtklock->draw_clock_source = g_timeout_add_seconds(1, gtklock_update_clocks_handler, gtklock);
	gtklock_update_clocks(gtklock);
	if(gtklock->use_input_inhibit) input_inhibitor_get();
}

void gtklock_destroy(struct GtkLock *gtklock) {
	g_object_unref(gtklock->app);
	g_array_unref(gtklock->windows);

	if(gtklock->draw_clock_source > 0) {
		g_source_remove(gtklock->draw_clock_source);
		gtklock->draw_clock_source = 0;
	}

	if(gtklock->use_input_inhibit) input_inhibitor_destroy();
	free(gtklock);
}

