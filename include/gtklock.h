// gtklock
// Copyright (c) 2022 Kenny Levinsen, Jovan Lanik

// gtklock application

#pragma once

#include <gtk/gtk.h>

struct Window;

struct GtkLock {
	GtkApplication *app;
	GArray *windows;
	GArray *messages;
	GArray *errors;

	struct Window *focused_window;
	gboolean hidden;
	guint idle_timeout;

	guint draw_clock_source;
	guint idle_hide_source;

	gboolean use_layer_shell;
	gboolean use_input_inhibit;
	gboolean use_idle_hide;

	char *time;
	char *time_format;
};

extern struct GtkLock *gtklock;

struct Window *gtklock_window_by_widget(struct GtkLock *gtklock, GtkWidget *window);
struct Window *gtklock_window_by_monitor(struct GtkLock *gtklock, GdkMonitor *monitor);
void gtklock_remove_window(struct GtkLock *gtklock, struct Window *win);
void gtklock_focus_window(struct GtkLock *gtklock, struct Window *win);
void gtklock_update_clocks(struct GtkLock *gtklock);
void gtklock_idle_hide(struct GtkLock *gtklock);
void gtklock_idle_show(struct GtkLock *gtklock);
struct GtkLock *create_gtklock(void);
void gtklock_activate(struct GtkLock *gtklock);
void gtklock_destroy(struct GtkLock *gtklock);

