#ifndef _GTKGREET_H
#define _GTKGREET_H

#include <gtk/gtk.h>

// Defined in window.h
struct Window;

struct GtkLock {
	GtkApplication *app;
	GArray *windows;
	GArray *messages;
	GArray *errors;

	struct Window *focused_window;
	guint draw_clock_source;

	gboolean use_layer_shell;
	gboolean use_input_inhibit;
	char time[8];
};

extern struct GtkLock *gtklock;

struct Window *gtklock_window_by_widget(struct GtkLock *gtklock, GtkWidget *window);
struct Window *gtklock_window_by_monitor(struct GtkLock *gtklock, GdkMonitor *monitor);
void gtklock_remove_window(struct GtkLock *gtklock, struct Window *win);
void gtklock_focus_window(struct GtkLock *gtklock, struct Window *win);
void gtklock_update_clocks(struct GtkLock *gtklock);
struct GtkLock *create_gtklock(void);
void gtklock_activate(struct GtkLock *gtklock);
void gtklock_destroy(struct GtkLock *gtklock);

#endif
