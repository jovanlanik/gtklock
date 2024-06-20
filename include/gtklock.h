// gtklock
// Copyright (c) 2022 Kenny Levinsen, Jovan Lanik

// gtklock application

#pragma once

#include <gtk/gtk.h>
#include <gtk-session-lock.h>

struct Window;

struct GtkLock {
	GtkApplication *app;
	GtkSessionLockLock *lock;

	GArray *windows;
	GArray *messages;
	GArray *errors;

	struct Window *focused_window;
	gboolean hidden;
	guint idle_timeout;

	guint draw_clock_source;
	guint draw_date_source;
	guint idle_hide_source;

	gboolean use_idle_hide;

	char *time;
	char *date;
	char *time_format;
	char *date_format;
	char *config_path;
	char *layout_path;

	GArray *modules;
};

void gtklock_remove_window(struct GtkLock *gtklock, struct Window *win);
void gtklock_focus_window(struct GtkLock *gtklock, struct Window *win);
void gtklock_update_clocks(struct GtkLock *gtklock);
void gtklock_update_dates(struct GtkLock *gtklock);
void gtklock_idle_hide(struct GtkLock *gtklock);
void gtklock_idle_show(struct GtkLock *gtklock);
struct GtkLock *create_gtklock(void);
void gtklock_activate(struct GtkLock *gtklock);
void gtklock_shutdown(struct GtkLock *gtklock);
void gtklock_destroy(struct GtkLock *gtklock);

