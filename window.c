// gtklock
// Copyright (c) 2022 Kenny Levinsen, Jovan Lanik, Erik Reider

// Window functions

#include <time.h>
#include <assert.h>

#include <gtk/gtk.h>
#include <gtk-layer-shell.h>

#include "window.h"
#include "gtklock.h"
#include "auth.h"
#include "module.h"

static void window_set_focus_layer_shell(struct Window *win, struct Window *old) {
	if(old != NULL) gtk_layer_set_keyboard_mode(GTK_WINDOW(old->window), GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);
	gtk_layer_set_keyboard_mode(GTK_WINDOW(win->window), GTK_LAYER_SHELL_KEYBOARD_MODE_EXCLUSIVE);
}

static gboolean window_enter_notify(GtkWidget *widget, gpointer data) {
	struct Window *win = gtklock_window_by_widget(gtklock, widget);
	gtklock_focus_window(gtklock, win);
	return FALSE;
}

static void window_setup_layer_shell(struct Window *ctx) {
	gtk_widget_add_events(ctx->window, GDK_ENTER_NOTIFY_MASK);
	if(ctx->enter_notify_handler > 0) {
		g_signal_handler_disconnect(ctx->window, ctx->enter_notify_handler);
		ctx->enter_notify_handler = 0;
	}
	ctx->enter_notify_handler = g_signal_connect(ctx->window, "enter-notify-event", G_CALLBACK(window_enter_notify), NULL);

	gtk_layer_init_for_window(GTK_WINDOW(ctx->window));
	gtk_layer_set_layer(GTK_WINDOW(ctx->window), GTK_LAYER_SHELL_LAYER_OVERLAY);
	gtk_layer_set_monitor(GTK_WINDOW(ctx->window), ctx->monitor);
	gtk_layer_set_exclusive_zone(GTK_WINDOW(ctx->window), -1);
	gtk_layer_set_anchor(GTK_WINDOW(ctx->window), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
	gtk_layer_set_anchor(GTK_WINDOW(ctx->window), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
	gtk_layer_set_anchor(GTK_WINDOW(ctx->window), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
	gtk_layer_set_anchor(GTK_WINDOW(ctx->window), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
}

void window_update_clock(struct Window *ctx) {
	gtk_label_set_text((GtkLabel*)ctx->clock_label, gtklock->time);
}

static void window_empty(struct Window *ctx) {
	if(ctx->window_box != NULL) {
		gtk_widget_destroy(ctx->window_box);
		ctx->window_box = NULL;
	}

	ctx->clock_label = NULL;
	ctx->body = NULL;
	ctx->input_box = NULL;
	ctx->input_label = NULL;
	ctx->input_field = NULL;
	ctx->message_box = NULL;
	ctx->unlock_button = NULL;
	ctx->error_label = NULL;
	module_on_window_empty(gtklock, ctx);
}

static void window_setup_messages(struct Window *ctx);

static void window_close_message(GtkInfoBar *bar, gint response, gpointer data) {
	struct Window *ctx = gtklock_window_by_widget(gtklock, gtk_widget_get_toplevel(GTK_WIDGET(bar)));
	gtk_widget_destroy(GTK_WIDGET(bar));
	for(guint idx = 0; idx < gtklock->errors->len; idx++) {
		char *err = g_array_index(gtklock->errors, char *, idx);
		if(err == data) {
			g_array_remove_index_fast(gtklock->errors, idx);
			free(err);
			window_setup_messages(ctx);
			return;
		}
	}
	for(guint idx = 0; idx < gtklock->messages->len; idx++) {
		char *msg = g_array_index(gtklock->messages, char *, idx);
		if(msg == data) {
			g_array_remove_index_fast(gtklock->messages, idx);
			free(msg);
			window_setup_messages(ctx);
			return;
		}
	}
}

static GtkInfoBar *window_new_message(struct Window *ctx, char *msg) {
	GtkWidget *bar = gtk_info_bar_new();
	gtk_info_bar_set_show_close_button(GTK_INFO_BAR(bar), TRUE);
	g_signal_connect(bar, "response", G_CALLBACK(window_close_message), msg);
	gtk_container_add(GTK_CONTAINER(ctx->message_box), bar);

	GtkWidget *content_area = gtk_info_bar_get_content_area(GTK_INFO_BAR(bar));
	GtkWidget *label = gtk_label_new(msg);
	gtk_container_add(GTK_CONTAINER(content_area), label);

	gtk_widget_show_all(bar);
	return GTK_INFO_BAR(bar);
}

static void window_setup_messages(struct Window *ctx) {
	if(ctx->message_box != NULL) {
		gtk_widget_destroy(ctx->message_box);
		ctx->message_box = NULL;
	}
	ctx->message_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_no_show_all(ctx->message_box, TRUE);
	gtk_grid_attach(GTK_GRID(ctx->input_box), ctx->message_box, 1, 1, 2, 1);

	for(guint idx = 0; idx < gtklock->errors->len; idx++) {
		char *err = g_array_index(gtklock->errors, char *, idx);
		GtkInfoBar *bar = window_new_message(ctx, err);
		gtk_info_bar_set_message_type(bar, GTK_MESSAGE_WARNING);
		gtk_widget_show(ctx->message_box);
	}
	for(guint idx = 0; idx < gtklock->messages->len; idx++) {
		char *msg = g_array_index(gtklock->messages, char *, idx);
		GtkInfoBar *bar = window_new_message(ctx, msg);
		gtk_info_bar_set_message_type(bar, GTK_MESSAGE_INFO);
		gtk_widget_show(ctx->message_box);
	}
}

static void window_set_busy(struct Window *ctx, gboolean busy) {
	if(busy) g_application_mark_busy(G_APPLICATION(gtklock->app));
	else g_application_unmark_busy(G_APPLICATION(gtklock->app));

	GdkCursor *cursor = gdk_cursor_new_from_name(gtk_widget_get_display(ctx->window), busy ? "wait" : "default");
	gdk_window_set_cursor(gtk_widget_get_window(ctx->window), cursor);
	g_object_unref(cursor);

	gtk_widget_set_sensitive(ctx->unlock_button, !busy);
	gtk_widget_set_sensitive(ctx->input_field, !busy);
}

static gboolean window_pw_failure(gpointer data) {
	struct Window *ctx = data;
	window_set_busy(ctx, FALSE);
	gtk_entry_set_text(GTK_ENTRY(ctx->input_field), "");
	gtk_widget_grab_focus(ctx->input_field);
	gtk_label_set_markup(GTK_LABEL(ctx->error_label), "<span color=\"red\">Login failed</span>");
	return G_SOURCE_REMOVE;
}

static gboolean window_pw_message(gpointer data) {
	window_setup_messages((struct Window *)data);
	return G_SOURCE_REMOVE;
}

static gpointer window_pw_wait(gpointer data) {
	struct Window *ctx = data;
	const char *password = gtk_entry_get_text((GtkEntry*)ctx->input_field);
	while(TRUE) {
		enum pwcheck ret = auth_pw_check(password);
		switch(ret) {
			case PW_FAILURE:
				g_main_context_invoke(NULL, window_pw_failure, ctx);
				return NULL;
			case PW_SUCCESS:
				g_application_quit(G_APPLICATION(gtklock->app));
				return NULL;
			case PW_ERROR:
				{
					char *err = auth_get_error();
					g_array_append_val(gtklock->errors, err);
					g_main_context_invoke(NULL, window_pw_message, ctx);
				}
				break;
			case PW_MESSAGE:
				{
					char *msg = auth_get_message();
					g_array_append_val(gtklock->messages, msg);
					g_main_context_invoke(NULL, window_pw_message, ctx);
				}
				break;
			case PW_WAIT:
				break;
		}
	}

}

static void window_pw_check(GtkWidget *widget, gpointer data) {
	struct Window *ctx = data;
	window_set_busy(ctx, TRUE);
	gtk_label_set_text(GTK_LABEL(ctx->error_label), NULL);
	g_thread_new(NULL, window_pw_wait, ctx);
}

static void window_pw_set_vis(GtkEntry* entry, gboolean visibility) {
	const char *icon = visibility ? "view-conceal-symbolic" : "view-reveal-symbolic";
	gtk_entry_set_icon_from_icon_name(entry, GTK_ENTRY_ICON_SECONDARY, icon);
	gtk_entry_set_visibility(entry, visibility);
}

static void window_pw_toggle_vis(GtkEntry* entry, GtkEntryIconPosition icon_pos) {
	if(icon_pos != GTK_ENTRY_ICON_SECONDARY) return;
	gboolean visibility = gtk_entry_get_visibility(entry);
	window_pw_set_vis(entry, !visibility);
}

static void window_setup_input(struct Window *ctx) {
	if(ctx->input_box != NULL) {
		gtk_widget_destroy(ctx->input_box);
		ctx->input_box = NULL;
	}
	ctx->input_box = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID(ctx->input_box), 5);
	gtk_grid_set_column_spacing(GTK_GRID(ctx->input_box), 5);
	gtk_container_add(GTK_CONTAINER(ctx->body), ctx->input_box);

	ctx->input_label = gtk_label_new("Password:");
	gtk_widget_set_name(ctx->input_label, "input-label");
	gtk_grid_attach(GTK_GRID(ctx->input_box), ctx->input_label, 0, 0, 1, 1);

	ctx->input_field = gtk_entry_new();
	gtk_entry_set_input_purpose((GtkEntry*)ctx->input_field, GTK_INPUT_PURPOSE_PASSWORD);
	g_object_set(ctx->input_field, "caps-lock-warning", FALSE, NULL);
	window_pw_set_vis((GtkEntry*)ctx->input_field, FALSE);
	g_signal_connect(ctx->input_field, "icon-release", G_CALLBACK(window_pw_toggle_vis), NULL);
	g_signal_connect(ctx->input_field, "activate", G_CALLBACK(window_pw_check), ctx);
	gtk_widget_set_size_request(ctx->input_field, 380, -1);
	gtk_grid_attach(GTK_GRID(ctx->input_box), ctx->input_field, 1, 0, 2, 1);

	GtkWidget *button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
	gtk_widget_set_halign(button_box, GTK_ALIGN_END);
	gtk_grid_attach(GTK_GRID(ctx->input_box), button_box, 1, 2, 2, 1);

	ctx->error_label = gtk_label_new(NULL);
	gtk_widget_set_name(ctx->error_label, "error-label");
	gtk_container_add(GTK_CONTAINER(button_box), ctx->error_label);

	ctx->unlock_button = gtk_button_new_with_label("Unlock");
	GtkStyleContext *unlock_button_style = gtk_widget_get_style_context(ctx->unlock_button);
	g_signal_connect(ctx->unlock_button, "clicked", G_CALLBACK(window_pw_check), ctx);
	gtk_style_context_add_class(unlock_button_style, "suggested-action");
	gtk_container_add(GTK_CONTAINER(button_box), ctx->unlock_button);

	if(ctx->input_field != NULL) gtk_widget_grab_focus(ctx->input_field);
}

static void window_setup(struct Window *ctx) {
	// Create general structure if it is missing
	if(ctx->window_box == NULL) {
		ctx->window_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
		g_object_set(ctx->window_box, "margin", 100, NULL);
		gtk_widget_set_valign(ctx->window_box, GTK_ALIGN_CENTER);
		gtk_widget_set_halign(ctx->window_box, GTK_ALIGN_CENTER);
		gtk_widget_set_name(ctx->window_box, "window-box");
		gtk_container_add(GTK_CONTAINER(ctx->window), ctx->window_box);

		ctx->clock_label = gtk_label_new("");
		gtk_widget_set_halign(ctx->clock_label, GTK_ALIGN_CENTER);
		gtk_widget_set_name(ctx->clock_label, "clock-label");
		g_object_set(ctx->clock_label, "margin-bottom", 10, NULL);
		gtk_container_add(GTK_CONTAINER(ctx->window_box), ctx->clock_label);
		window_update_clock(ctx);
	}

	// Update input area if necessary
	if(gtklock->focused_window == ctx || gtklock->focused_window == NULL) {
		if(ctx->body == NULL) {
			ctx->body = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
			gtk_widget_set_halign(ctx->body, GTK_ALIGN_CENTER);
			gtk_widget_set_name(ctx->body, "body");
			gtk_widget_set_size_request(ctx->body, 384, -1);
			gtk_container_add(GTK_CONTAINER(ctx->window_box), ctx->body);
			window_update_clock(ctx);
			window_setup_input(ctx);
			window_setup_messages(ctx);
		}
	}
	else if(ctx->body != NULL) {
		gtk_widget_destroy(ctx->body);
		ctx->body = NULL;
		ctx->input_box = NULL;
		ctx->input_label = NULL;
		ctx->input_field = NULL;
		ctx->message_box = NULL;
		ctx->unlock_button = NULL;
		ctx->error_label = NULL;
		module_on_body_empty(gtklock, ctx);
		window_update_clock(ctx);
	}
}

static void window_destroy_notify(GtkWidget *widget, gpointer data) {
	struct Window *win = gtklock_window_by_widget(gtklock, widget);
	window_empty(win);
	gtklock_remove_window(gtklock, win);
}

static void window_set_focus(struct Window *win, struct Window *old) {
	assert(win != NULL);
	window_setup(win);

	GtkStyleContext *win_context = gtk_widget_get_style_context(win->window);
	gtk_style_context_remove_class(win_context, "not-focused");
	gtk_style_context_add_class(win_context, "focused");

	if(old != NULL && old != win) {
		GtkStyleContext *old_context = gtk_widget_get_style_context(old->window);
		gtk_style_context_remove_class(old_context, "focused");
		gtk_style_context_add_class(old_context, "not-focused");

		if(old->input_field != NULL && win->input_field != NULL) {
			// Get previous cursor position
			gint cursor_pos = 0;
			g_object_get((GtkEntry*)old->input_field, "cursor-position", &cursor_pos, NULL);

			// Move content
			gtk_entry_set_text((GtkEntry*)win->input_field, gtk_entry_get_text((GtkEntry*)old->input_field));
			gtk_entry_set_text((GtkEntry*)old->input_field, "");

			// Update new cursor position
			g_signal_emit_by_name((GtkEntry*)win->input_field, "move-cursor", GTK_MOVEMENT_BUFFER_ENDS, -1, FALSE);
			g_signal_emit_by_name((GtkEntry*)win->input_field, "move-cursor", GTK_MOVEMENT_LOGICAL_POSITIONS, cursor_pos, FALSE);

			// Copy pw visibility
			window_pw_set_vis((GtkEntry*)win->input_field, gtk_entry_get_visibility((GtkEntry*)old->input_field));
		}
		gtk_widget_show_all(old->window);
	}
	gtk_widget_show_all(win->window);
}

void window_swap_focus(struct Window *win, struct Window *old) {
	if(gtklock->use_layer_shell) window_set_focus_layer_shell(win, old);
	window_set_focus(win, old);
}

void window_configure(struct Window *w) {
	window_setup(w);
	gtk_widget_show_all(w->window);
}

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

struct Window *create_window(GdkMonitor *monitor) {
	struct Window *w = calloc(1, sizeof(struct Window));
	if(w == NULL) g_error("Failed to allocate Window instance");
	w->monitor = monitor;
	g_array_append_val(gtklock->windows, w);

	w->window = gtk_application_window_new(gtklock->app);
	GtkStyleContext *context = gtk_widget_get_style_context(w->window);
	gtk_style_context_add_class(context, "not-focused");
	g_signal_connect(w->window, "destroy", G_CALLBACK(window_destroy_notify), NULL);

	/*
		This code uses a deprecated function and assumes one GDK screen...
		However there isn't really a good way to do this in GTK3 currently.
		Related issue: https://gitlab.gnome.org/GNOME/gtk/-/issues/4982
	*/
	char *name = NULL;
	GdkDisplay *display = gtk_widget_get_display(w->window);
	GdkScreen *screen = gtk_widget_get_screen(w->window);
	for(int i = 0; i < gdk_display_get_n_monitors(display); i++) {
		GdkMonitor *monitor = gdk_display_get_monitor(display, i);
		if(monitor != w->monitor) continue;
		name = gdk_screen_get_monitor_plug_name(screen, i);
	}

	if(name) gtk_widget_set_name(w->window, name);
	gtk_window_set_title(GTK_WINDOW(w->window), "Lockscreen");
	gtk_window_set_decorated(GTK_WINDOW(w->window), FALSE);
	gtk_widget_realize(w->window);
	if(gtklock->use_layer_shell) window_setup_layer_shell(w);

	return w;
}

