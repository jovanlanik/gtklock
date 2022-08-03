// gtklock
// Copyright (c) 2022 Jovan Lanik

// Module support

#include "module.h"

#ifndef PREFIX
#warning PREFIX not defined.
#define PREFIX /usr/local
#endif
#define _STR(x) #x
#define STR(x) _STR(x)

GModule *module_load(const char *name) {
	if(g_module_supported() == FALSE) return NULL;
	
	char *path = g_path_get_basename(name);
	if(g_strcmp0(name, path) != 0) path = g_strdup(name);
	else {
		if(g_file_test(name, G_FILE_TEST_IS_REGULAR)) path = g_strdup(name);
		else {
			g_free(path);
			path = g_build_path("/", STR(PREFIX)"/lib/gtklock", name, NULL);
		}
	}

	GError *err = NULL;
	GModule *module = g_module_open_full(path, 0, &err);
	g_free(path);
	if(module == NULL) {
		g_warning("Module loading failed: %s", err->message);
		g_error_free(err);
		return NULL;
	}
	return module;
}

void module_on_activation(struct GtkLock *gtklock) {
	for(int idx = 0; idx < gtklock->modules->len; idx++) {
		void (*fn)(struct GtkLock *, int) = NULL;
		GModule *module = g_array_index(gtklock->modules, GModule *, idx);
		if(g_module_symbol(module, "on_activation", (gpointer *)&fn)) fn(gtklock, idx);
	}
}

void module_on_output_change(struct GtkLock *gtklock) {
	for(int idx = 0; idx < gtklock->modules->len; idx++) {
		void (*fn)(struct GtkLock *) = NULL;
		GModule *module = g_array_index(gtklock->modules, GModule *, idx);
		if(g_module_symbol(module, "on_output_change", (gpointer *)&fn)) fn(gtklock);
	}
}

void module_on_focus_change(struct GtkLock *gtklock, struct Window *win, struct Window *old) {
	for(int idx = 0; idx < gtklock->modules->len; idx++) {
		void (*fn)(struct GtkLock *, struct Window *, struct Window *) = NULL;
		GModule *module = g_array_index(gtklock->modules, GModule *, idx);
		if(g_module_symbol(module, "on_focus_change", (gpointer *)&fn)) fn(gtklock, win, old);
	}
}

void module_on_window_empty(struct GtkLock *gtklock, struct Window *ctx) {
	for(int idx = 0; idx < gtklock->modules->len; idx++) {
		void (*fn)(struct GtkLock *, struct Window *) = NULL;
		GModule *module = g_array_index(gtklock->modules, GModule *, idx);
		if(g_module_symbol(module, "on_window_empty", (gpointer *)&fn)) fn(gtklock, ctx);
	}
}

void module_on_body_empty(struct GtkLock *gtklock, struct Window *ctx) {
	for(int idx = 0; idx < gtklock->modules->len; idx++) {
		void (*fn)(struct GtkLock *, struct Window *) = NULL;
		GModule *module = g_array_index(gtklock->modules, GModule *, idx);
		if(g_module_symbol(module, "on_body_empty", (gpointer *)&fn)) fn(gtklock, ctx);
	}
}

void module_on_idle_hide(struct GtkLock *gtklock) {
	for(int idx = 0; idx < gtklock->modules->len; idx++) {
		void (*fn)(struct GtkLock *) = NULL;
		GModule *module = g_array_index(gtklock->modules, GModule *, idx);
		if(g_module_symbol(module, "on_idle_hide", (gpointer *)&fn)) fn(gtklock);
	}
}

void module_on_idle_show(struct GtkLock *gtklock) {
	for(int idx = 0; idx < gtklock->modules->len; idx++) {
		void (*fn)(struct GtkLock *) = NULL;
		GModule *module = g_array_index(gtklock->modules, GModule *, idx);
		if(g_module_symbol(module, "on_idle_show", (gpointer *)&fn)) fn(gtklock);
	}
}

