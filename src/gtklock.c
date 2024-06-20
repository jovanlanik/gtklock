// gtklock
// Copyright (c) 2022 Kenny Levinsen, Jovan Lanik

// gtklock application

#include <gtk/gtk.h>

#include "util.h"
#include "window.h"
#include "gtklock.h"
#include "module.h"

void gtklock_remove_window(struct GtkLock *gtklock, struct Window *win) {
	for(guint idx = 0; idx < gtklock->windows->len; idx++) {
		struct Window *ctx = g_array_index(gtklock->windows, struct Window *, idx);
		if(ctx == win) {
			g_array_remove_index(gtklock->windows, idx);
			g_free(ctx);
			return;
		}
	}
}

void gtklock_focus_window(struct GtkLock *gtklock, struct Window* win) {
	struct Window *old = gtklock->focused_window;
	gtklock->focused_window = win;
	window_swap_focus(win, old);
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
	return G_SOURCE_CONTINUE;
}

void gtklock_update_dates(struct GtkLock *gtklock) {
	GDateTime *date = g_date_time_new_now_local();
	if(date == NULL) return;
	if(gtklock->date) g_free(gtklock->date);
	gtklock->date = g_date_time_format(date, gtklock->date_format ? gtklock->date_format : "%a, %d %b %Y");
	g_date_time_unref(date);

	for(guint idx = 0; idx < gtklock->windows->len; idx++) {
		struct Window *ctx = g_array_index(gtklock->windows, struct Window *, idx);
		window_update_date(ctx);
	}
}

static int gtklock_update_dates_handler(gpointer data) {
	struct GtkLock *gtklock = (struct GtkLock *)data;
	gtklock_update_dates(gtklock);
	return G_SOURCE_CONTINUE;
}

static int gtklock_idle_handler(gpointer data) {
	struct GtkLock *gtklock = (struct GtkLock *)data;
	gtklock_idle_hide(gtklock);
	return G_SOURCE_CONTINUE;
}

void gtklock_idle_hide(struct GtkLock *gtklock) {
	if(!gtklock->use_idle_hide || gtklock->hidden || g_application_get_is_busy(G_APPLICATION(gtklock->app)))
		return;
	gtklock->hidden = TRUE;
	module_on_idle_hide(gtklock);

	for(guint idx = 0; idx < gtklock->windows->len; idx++) {
		struct Window *ctx = g_array_index(gtklock->windows, struct Window *, idx);
		window_idle_hide(ctx);
	}
}

void gtklock_idle_show(struct GtkLock *gtklock) {
	if(gtklock->hidden) {
		gtklock->hidden = FALSE;
		module_on_idle_show(gtklock);
	}

	for(guint idx = 0; idx < gtklock->windows->len; idx++) {
		struct Window *ctx = g_array_index(gtklock->windows, struct Window *, idx);
		window_idle_show(ctx);
	}

	if(!gtklock->use_idle_hide) return;
	if(gtklock->idle_hide_source > 0) g_source_remove(gtklock->idle_hide_source);
	gtklock->idle_hide_source = g_timeout_add_seconds(gtklock->idle_timeout, G_SOURCE_FUNC(gtklock_idle_handler), gtklock);
}

#if GLIB_CHECK_VERSION(2, 74, 0)
	#define GTKLOCK_FLAGS G_APPLICATION_DEFAULT_FLAGS
#else
	#define GTKLOCK_FLAGS G_APPLICATION_FLAGS_NONE
#endif

struct GtkLock* create_gtklock(void) {
	struct GtkLock *gtklock = g_malloc0(sizeof(struct GtkLock));
	if(!gtklock) report_error_and_exit("Failed allocation");
	gtklock->app = gtk_application_new(NULL, GTKLOCK_FLAGS);
	gtklock->windows = g_array_new(FALSE, TRUE, sizeof(struct Window *));
	gtklock->messages = g_array_new(FALSE, TRUE, sizeof(char *));
	gtklock->errors = g_array_new(FALSE, TRUE, sizeof(char *));
	return gtklock;
}

void gtklock_activate(struct GtkLock *gtklock) {
	g_application_hold(G_APPLICATION(gtklock->app));

	if(!gtk_session_lock_is_supported())
		report_error_and_exit("Your compositor doesn't support ext-session-lock");
	gtklock->lock = gtk_session_lock_prepare_lock();
	gtk_session_lock_lock_lock(gtklock->lock);

	gtklock->draw_clock_source = g_timeout_add(1000, G_SOURCE_FUNC(gtklock_update_clocks_handler), gtklock);
	gtklock_update_clocks(gtklock);
	gtklock->draw_date_source = g_timeout_add(18000000, G_SOURCE_FUNC(gtklock_update_dates_handler), gtklock);
	gtklock_update_dates(gtklock);
	if(gtklock->use_idle_hide) gtklock->idle_hide_source =
		g_timeout_add_seconds(gtklock->idle_timeout, G_SOURCE_FUNC(gtklock_idle_handler), gtklock);
}

void gtklock_shutdown(struct GtkLock *gtklock) {
	if(gtklock->draw_clock_source > 0) {
		g_source_remove(gtklock->draw_clock_source);
		gtklock->draw_clock_source = 0;
	}
	if(gtklock->idle_hide_source > 0) {
		g_source_remove(gtklock->idle_hide_source);
		gtklock->idle_hide_source = 0;
	}
	gtk_session_lock_lock_unlock_and_destroy(gtklock->lock);
	gdk_display_sync(gdk_display_get_default());
}

void gtklock_destroy(struct GtkLock *gtklock) {
	g_object_unref(gtklock->app);
	g_array_unref(gtklock->windows);
	g_free(gtklock);
}

