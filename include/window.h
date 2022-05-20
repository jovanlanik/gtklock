#ifndef _WINDOW_H
#define _WINDOW_H

#include <gtk/gtk.h>
#include "act/act-user-manager.h"
#include "act/act-user.h"

struct Window {
    GdkMonitor *monitor;

    GtkWidget *window;
    GtkWidget *revealer;
    GtkWidget *window_box;
    GtkWidget *body;
    GtkWidget *user_revealer;
    GtkWidget *user_box;
    GtkWidget *user_icon;
    GtkWidget *user_name;
    GtkWidget *input_box;
    GtkWidget *input_field;
    GtkWidget *unlock_button;
    GtkWidget *error_label;
    GtkWidget *clock_label;

	ActUserManager* manager;
	ActUser* user;

    gulong enter_notify_handler;
};

struct Window *create_window(GdkMonitor *monitor);
void window_configure(struct Window *win);
void window_update_clock(struct Window *ctx);
void window_swap_focus(struct Window *win, struct Window *old);

#endif
