// gtklock
// Copyright (c) 2022 Kenny Levinsen, Jovan Lanik

// Window functions

#pragma once

#include <gtk/gtk.h>

struct Window {
	GdkMonitor *monitor;

	GtkWidget *window;
	GtkWidget *overlay;
	GtkWidget *window_box;
	GtkWidget *body_revealer;
	GtkWidget *body_grid;
	GtkWidget *input_label;
	GtkWidget *input_field;
	GtkWidget *message_box;
	GtkWidget *unlock_button;
	GtkWidget *error_label;
	GtkWidget *warning_label;
	GtkWidget *clock_label;
	GtkWidget *date_label;

	void *module_data[];
};

struct Window *window_by_widget(GtkWidget *window);
struct Window *window_by_monitor(GdkMonitor *monitor);
struct Window *window_last_active(void);
struct Window *create_window(GdkMonitor *monitor);
void window_idle_hide(struct Window *win);
void window_idle_show(struct Window *win);
void window_update_clock(struct Window *ctx);
void window_update_date(struct Window *ctx);
void window_swap_focus(struct Window *win, struct Window *old);

