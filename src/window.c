// gtklock
// Copyright (c) 2022 Kenny Levinsen, Jovan Lanik, Erik Reider, Melih Darcan, Bhaskar Khoraja

// Window functions

#include <time.h>

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <gtk4-session-lock.h>

#include "util.h"
#include "window.h"
#include "gtklock.h"
#include "auth.h"
#include "module.h"

extern struct GtkLock *gtklock;

struct Window *window_by_widget(GtkWidget *window) {
	for(guint idx = 0; idx < gtklock->windows->len; idx++) {
		struct Window *ctx = g_array_index(gtklock->windows, struct Window *, idx);
		if(ctx->window == window) return ctx;
	}
	return NULL;
}

struct Window *window_by_monitor(GdkMonitor *monitor) {
	for(guint idx = 0; idx < gtklock->windows->len; idx++) {
		struct Window *ctx = g_array_index(gtklock->windows, struct Window *, idx);
		if(ctx->monitor == monitor) return ctx;
	}
	return NULL;
}

struct Window *window_last_active(void) {
	GtkWindow *window = gtk_application_get_active_window(gtklock->app);
	if(window) return window_by_widget(GTK_WIDGET(window));
	return NULL;
}

void window_update_clock(struct Window *ctx) {
	gtk_label_set_text(GTK_LABEL(ctx->clock_label), gtklock->time);
}

void window_update_date(struct Window *ctx) {
	gtk_label_set_text(GTK_LABEL(ctx->date_label), gtklock->date);
}

static void window_setup_messages(struct Window *ctx);

static void window_close_message(GtkInfoBar *bar, gint response, gpointer data) {
	struct Window *ctx = window_by_widget(GTK_WIDGET(gtk_widget_get_root(GTK_WIDGET(bar))));
	gtk_widget_unrealize(GTK_WIDGET(bar));
	for(guint idx = 0; idx < gtklock->errors->len; idx++) {
		char *err = g_array_index(gtklock->errors, char *, idx);
		if(err == data) {
			g_array_remove_index(gtklock->errors, idx);
			g_free(err);
			window_setup_messages(ctx);
			return;
		}
	}
	for(guint idx = 0; idx < gtklock->messages->len; idx++) {
		char *msg = g_array_index(gtklock->messages, char *, idx);
		if(msg == data) {
			g_array_remove_index(gtklock->messages, idx);
			g_free(msg);
			window_setup_messages(ctx);
			return;
		}
	}
}

static GtkInfoBar *window_new_message(struct Window *ctx, char *msg) {
	GtkWidget *bar = gtk_info_bar_new();
	gtk_info_bar_set_show_close_button(GTK_INFO_BAR(bar), TRUE);
	g_signal_connect(bar, "response", G_CALLBACK(window_close_message), msg);
	gtk_box_append(GTK_BOX(ctx->message_box), bar);

	GtkWidget *label = gtk_label_new(msg);
	gtk_info_bar_add_child(GTK_INFO_BAR(bar), label);
	return GTK_INFO_BAR(bar);
}

static void destroy_callback(GtkWidget* widget, gpointer _data) {
	gtk_widget_unrealize(widget);
}

static void window_setup_messages(struct Window *ctx) {
    GtkWidget *child = gtk_widget_get_first_child(ctx->message_box);
    while (child != NULL) {
        destroy_callback(child, NULL);  // Call the callback for each child
        child = gtk_widget_get_next_sibling(child);  // Move to the next child
    }

	gtk_revealer_set_reveal_child(GTK_REVEALER(ctx->message_revealer), FALSE);
	gtk_widget_hide(ctx->message_revealer);

	for(guint idx = 0; idx < gtklock->errors->len; idx++) {
		char *err = g_array_index(gtklock->errors, char *, idx);
		GtkInfoBar *bar = window_new_message(ctx, err);
		gtk_info_bar_set_message_type(bar, GTK_MESSAGE_WARNING);

		gtk_revealer_set_reveal_child(GTK_REVEALER(ctx->message_revealer), TRUE);
		gtk_widget_show(ctx->message_revealer);
	}
	for(guint idx = 0; idx < gtklock->messages->len; idx++) {
		char *msg = g_array_index(gtklock->messages, char *, idx);
		GtkInfoBar *bar = window_new_message(ctx, msg);
		gtk_info_bar_set_message_type(bar, GTK_MESSAGE_INFO);

		gtk_revealer_set_reveal_child(GTK_REVEALER(ctx->message_revealer), TRUE);
		gtk_widget_show(ctx->message_revealer);
	}

}

static void window_set_busy(struct Window *ctx, gboolean busy) {
	if(busy) g_application_mark_busy(G_APPLICATION(gtklock->app));
	else g_application_unmark_busy(G_APPLICATION(gtklock->app));

	GdkCursor *cursor = gdk_cursor_new_from_name(busy ? "wait" : "default", NULL);
	gtk_widget_set_cursor(GTK_WIDGET(gtk_widget_get_root(ctx->window)), cursor);
	g_object_unref(cursor);

	gtk_widget_set_sensitive(ctx->unlock_button, !busy);
	gtk_widget_set_sensitive(ctx->input_field, !busy);
}

static gboolean window_pw_failure(gpointer data) {
	struct Window *ctx = data;
	window_set_busy(ctx, FALSE);
	GtkEntryBuffer* buffer = gtk_entry_get_buffer(GTK_ENTRY(ctx->input_field));
	gtk_entry_buffer_delete_text(buffer, 0, -1); // delete all text
	gtk_entry_grab_focus_without_selecting(GTK_ENTRY(ctx->input_field));
	gtk_label_set_text(GTK_LABEL(ctx->error_label), _("Login failed"));
	return G_SOURCE_REMOVE;
}

static gboolean window_pw_message(gpointer data) {
	window_setup_messages((struct Window *)data);
	return G_SOURCE_REMOVE;
}

static gpointer window_pw_wait(gpointer data) {
	struct Window *ctx = data;
	GtkEntryBuffer* buffer = gtk_entry_get_buffer(GTK_ENTRY(ctx->input_field));
	const char *password = gtk_entry_buffer_get_text(buffer);
	while(TRUE) {
		enum pwcheck ret = auth_pw_check(password); // lets me through on success or failure, unless account is locked``
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
					return NULL;
				}
				break;
			case PW_MESSAGE:
				{
					char *msg = auth_get_message();
					g_array_append_val(gtklock->messages, msg);
					g_main_context_invoke(NULL, window_pw_message, ctx);
					return NULL;
				}
				break;
			case PW_WAIT:
				break;
		}
	}

}

void window_pw_check(GtkWidget *widget, gpointer data) {
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

void window_pw_toggle_vis(GtkEntry* entry, GtkEntryIconPosition icon_pos) {
	if(icon_pos != GTK_ENTRY_ICON_SECONDARY) return;
	gboolean visibility = gtk_entry_get_visibility(entry);
	window_pw_set_vis(entry, !visibility);
}

static void window_destroy_notify(GtkWidget *widget, gpointer data) {
	struct Window *win = window_by_widget(widget);
	module_on_window_destroy(gtklock, win);
	gtk_widget_unrealize(widget);
	gtklock_remove_window(gtklock, win);
}

void window_swap_focus(struct Window *win, struct Window *old) {
	if(!gtklock->hidden) gtk_revealer_set_reveal_child(GTK_REVEALER(win->body_revealer), TRUE);

	GtkStyleContext *win_context = gtk_widget_get_style_context(win->window);
	gtk_style_context_add_class(win_context, "focused");

	if(old != NULL && old != win) {
		gtk_revealer_set_reveal_child(GTK_REVEALER(old->body_revealer), FALSE);

		GtkStyleContext *old_context = gtk_widget_get_style_context(old->window);
		gtk_style_context_remove_class(old_context, "focused");

		if(old->input_field != NULL && win->input_field != NULL) {
			// Get previous cursor position
			gint cursor_pos = 0;
			g_object_get(GTK_ENTRY(old->input_field), "cursor-position", &cursor_pos, NULL);

			// Move content
			GtkEntryBuffer* old_buffer = gtk_entry_get_buffer(GTK_ENTRY(old->input_field));
			GtkEntryBuffer* new_buffer = gtk_entry_get_buffer(GTK_ENTRY(win->input_field));
			gtk_entry_buffer_set_text(new_buffer, gtk_entry_buffer_get_text(old_buffer), -1);
			gtk_entry_buffer_delete_text(old_buffer, 0, -1); // delete all text

			// Update new cursor position
			g_signal_emit_by_name(GTK_ENTRY(win->input_field), "move-cursor", GTK_MOVEMENT_BUFFER_ENDS, -1, FALSE);
			g_signal_emit_by_name(GTK_ENTRY(win->input_field), "move-cursor", GTK_MOVEMENT_LOGICAL_POSITIONS, cursor_pos, FALSE);

			// Copy pw visibility
			window_pw_set_vis(GTK_ENTRY(win->input_field), gtk_entry_get_visibility(GTK_ENTRY(old->input_field)));
		}
	}

	gtk_entry_grab_focus_without_selecting(GTK_ENTRY(win->input_field));
}

void window_idle_hide(struct Window *ctx) {
	GtkStyleContext *context = gtk_widget_get_style_context(ctx->window);
	gtk_style_context_add_class(context, "hidden");
	gtk_revealer_set_reveal_child(GTK_REVEALER(ctx->body_revealer), FALSE);
	GdkCursor *cursor = gdk_cursor_new_from_name("none",NULL);
	gtk_widget_set_cursor(GTK_WIDGET(gtk_widget_get_root(ctx->window)), cursor);
	g_object_unref(cursor);
}

void window_idle_show(struct Window *ctx) {
	GtkStyleContext *context = gtk_widget_get_style_context(ctx->window);
	gtk_style_context_remove_class(context, "hidden");
	if(ctx == gtklock->focused_window) {
		gtk_revealer_set_reveal_child(GTK_REVEALER(ctx->body_revealer), TRUE);
		gtk_entry_grab_focus_without_selecting(GTK_ENTRY(ctx->input_field));
	}
	GdkCursor *cursor = gdk_cursor_new_from_name("default",NULL);
	gtk_widget_set_cursor(GTK_WIDGET(gtk_widget_get_root(ctx->window)), cursor);
	g_object_unref(cursor);
}

static gboolean window_idle_key(GtkWidget             *drawing_area,
                      guint                  keyval,
                      guint                  keycode,
                      GdkModifierType        state,
								GtkEventControllerKey *event_controller){
	gtklock_idle_show(gtklock);
	return FALSE;
}
//static gboolean window_idle_motion(GtkWidget *self, GdkMotionEvent *event, gpointer user_data) {
static void window_idle_motion	(
  GtkEventControllerMotion* self,
  gdouble x,
  gdouble y,
  gpointer user_data) {
	gtklock_idle_show(gtklock);
	//return FALSE;
}

static void window_caps_state_changed(GtkWidget             *drawing_area,
                      guint                  keyval,
                      guint                  keycode,
                      GdkModifierType        state,
								GtkEventControllerKey *event_controller) {
	struct Window *w = gtklock->focused_window;
	if(!w || !w->warning_label) return;
	GdkDisplay *display = gtk_widget_get_display(w->window);
	// now lets get the primary seat for the display
	GdkSeat *seat = gdk_display_get_default_seat(display);
	// now lets get the keyboard device for the seat finally
	GdkDevice *device = gdk_seat_get_keyboard(seat);
	if(gdk_device_get_caps_lock_state(device)) gtk_label_set_text(GTK_LABEL(w->warning_label), _("Caps Lock is on"));
	else gtk_label_set_text(GTK_LABEL(w->warning_label), "");
}

static gboolean entry_button_press(GtkWidget *widget, GdkButtonEvent *event, gpointer data) {
	if(gdk_button_event_get_button(GDK_EVENT(event)) != 1) return TRUE;
	return FALSE;
}

static gboolean window_enter_notify(GtkWidget *widget, gpointer data) {
	struct Window *win = window_by_widget(widget);
	gtk_entry_grab_focus_without_selecting(GTK_ENTRY(win->input_field));
	gtklock_focus_window(gtklock, win);
	return FALSE;
}

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

struct Window *create_window(GdkMonitor *monitor) {
	struct Window *w = g_malloc0(sizeof(struct Window) + gtklock->modules->len * sizeof(void *));
	if(!w) report_error_and_exit("Failed allocation");

	//g_array_append_val(gtklock->windows, w);
	g_array_append_vals(gtklock->windows, &w, 1);
	w->monitor = monitor;
	w->window = gtk_application_window_new(gtklock->app);

	g_signal_connect(w->window, "destroy", G_CALLBACK(window_destroy_notify), NULL);
	if(gtklock->follow_focus)
		g_signal_connect(w->window, "enter-notify-event", G_CALLBACK(window_enter_notify), NULL);
	if(gtklock->use_idle_hide || gtklock->hidden) {
		//gtk_widget_add_events(w->window, GDK_POINTER_MOTION_MASK);
		GtkEventController *motion_controller = gtk_event_controller_motion_new();
		GtkEventController *event_controller = gtk_event_controller_key_new ();
		gtk_event_controller_set_propagation_phase(event_controller, GTK_PHASE_CAPTURE);
		g_signal_connect(event_controller, "key-pressed", G_CALLBACK(window_idle_key), NULL);
		gtk_widget_add_controller (GTK_WIDGET (w->window), event_controller);
		gtk_widget_add_controller (GTK_WIDGET (w->window), motion_controller);
		gtk_widget_set_focusable(w->window, TRUE);
		//g_signal_connect(w->window, "key-pressed", G_CALLBACK(window_idle_key), NULL);
		g_signal_connect(motion_controller, "motion", G_CALLBACK(window_idle_motion), NULL);
		g_signal_connect(event_controller, "key-released", G_CALLBACK(window_idle_key), NULL);
	}

	GdkDisplay *display = gtk_widget_get_display(w->window);

	/*
		This code uses a deprecated function and assumes one GDK screen...
		However there isn't really a good way to do this in GTK3 currently.
		Related issue: https://gitlab.gnome.org/GNOME/gtk/-/issues/4982
	*/
	char *name = NULL;
	GListModel* monitors = gdk_display_get_monitors(display);
	for(int i = 0; i < g_list_model_get_n_items(monitors); i++) {
		GdkMonitor *monitor = g_list_model_get_item (monitors, i);
		if(monitor != w->monitor) continue;
		name = gdk_monitor_get_model(monitor);
	}

	GtkEventController *event_controller2 = gtk_event_controller_key_new ();
	//GdkKeymapKey *keymapkey = gdk_display_get_keymap(display);
	//g_signal_connect_object(event_controller2, "key-pressed", G_CALLBACK(window_caps_state_changed), w->window, G_CONNECT_SWAPPED);
	//g_signal_connect(keymapkey, "state-changed", G_CALLBACK(window_caps_state_changed), NULL);

	if(name) {
		gtk_widget_set_name(w->window, name);
		//g_free(name); // THIS BREAKS EVERYTHING!
	}
	gtk_window_set_title(GTK_WINDOW(w->window), "Lockscreen");
	gtk_window_set_decorated(GTK_WINDOW(w->window), FALSE);

	gtk_session_lock_instance_assign_window_to_monitor(gtklock->lock, GTK_WINDOW(w->window), monitor);
	//gtk_widget_realize(w->window);
	//gtk_session_lock_lock_new_surface(gtklock->lock, GTK_WINDOW(w->window), monitor);

	w->overlay = gtk_overlay_new();
	gtk_window_set_child(GTK_WINDOW(w->window), w->overlay);
	
	GtkBuilder *builder = gtk_builder_new();
	//gtk_builder_set_current_object(builder, (void*)w);
	if(gtklock->layout_path) gtk_builder_add_from_file(builder, gtklock->layout_path, NULL);
	else gtk_builder_add_from_resource(builder, "/gtklock/gtklock.ui", NULL);

	w->window_box = GTK_WIDGET(gtk_builder_get_object(builder, "window-box"));
	//gtk_container_add(GTK_CONTAINER(w->overlay), w->window_box);
	gtk_overlay_set_child(GTK_OVERLAY(w->overlay), w->window_box);

	w->body_revealer = GTK_WIDGET(gtk_builder_get_object(builder, "body-revealer"));
	w->body_grid = GTK_WIDGET(gtk_builder_get_object(builder, "body-grid"));
	w->input_label = GTK_WIDGET(gtk_builder_get_object(builder, "input-label"));

	w->input_field = GTK_WIDGET(gtk_builder_get_object(builder, "input-field"));
	//g_signal_connect(w->input_field, "button-press-event", G_CALLBACK(entry_button_press), NULL);
	g_signal_connect(w->input_field, "activate", G_CALLBACK(window_pw_check), (gpointer)(w));
	g_signal_connect(w->input_field, "icon-release", G_CALLBACK(window_pw_toggle_vis), NULL);

	w->message_revealer = GTK_WIDGET(gtk_builder_get_object(builder, "message-revealer"));
	w->message_scrolled_window = GTK_WIDGET(gtk_builder_get_object(builder, "message-scrolled-window"));
	w->message_box = GTK_WIDGET(gtk_builder_get_object(builder, "message-box"));
	w->unlock_button = GTK_WIDGET(gtk_builder_get_object(builder, "unlock-button"));
	g_signal_connect(w->unlock_button, "clicked", G_CALLBACK(window_pw_check), (gpointer)w);
	w->error_label = GTK_WIDGET(gtk_builder_get_object(builder, "error-label"));
	w->warning_label = GTK_WIDGET(gtk_builder_get_object(builder, "warning-label"));
	
	w->info_box = GTK_WIDGET(gtk_builder_get_object(builder, "info-box"));
	w->time_box = GTK_WIDGET(gtk_builder_get_object(builder, "time-box"));

	w->clock_label = GTK_WIDGET(gtk_builder_get_object(builder, "clock-label"));
	window_update_clock(w);

	w->date_label = GTK_WIDGET(gtk_builder_get_object(builder, "date-label"));
	window_update_date(w);

	// we don't need to do this, the locker does it for us

	//gtk_window_present(GTK_WINDOW(w->window));

	if(gtklock->hidden) window_idle_hide(w);
	module_on_window_create(gtklock, w);

	g_object_unref(builder);

	return w;
}

