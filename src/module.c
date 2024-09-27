// gtklock
// Copyright (c) 2022 Jovan Lanik, Zephyr Lykos

// Module support

#include "util.h"
#include "module.h"

#ifndef LIBDIR

#warning LIBDIR not defined.

#ifndef PREFIX
#warning PREFIX not defined.
#define LIBDIR /usr/local/lib
#else
#warning PREFIX is soft-deprecated. Define LIBDIR instead.
#define LIBDIR PREFIX/lib
#endif

#endif

#ifndef MAJOR_VERSION
#warning MAJOR_VERSION not defined.
#define MAJOR_VERSION 0
#endif
#ifndef MINOR_VERSION
#warning MINOR_VERSION not defined.
#define MINOR_VERSION 0
#endif
#ifndef MICRO_VERSION
#warning MICRO_VERSION not defined.
#define MICRO_VERSION 0
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
			path = g_build_path("/", STR(LIBDIR) "/gtklock", name, NULL);
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

	guint *major = NULL;
	guint *minor = NULL;
	gboolean has_major = g_module_symbol(module, "module_major_version", (gpointer *)&major);
	gboolean has_minor = g_module_symbol(module, "module_minor_version", (gpointer *)&minor);
	if(has_major && has_minor) {
		if(*major != MAJOR_VERSION)
			report_error_and_exit("%s: module has mismatched major version (%u), is incompatible", name, *major);
		else if(*minor != MINOR_VERSION)
			g_warning("%s: module has mismatched minor version (%u), may be incompatible", name, *minor);
	}
	else {
		const gchar *module_version = NULL;
		if(g_module_symbol(module, "module_version", (gpointer *)&module_version))
			report_error_and_exit("%s: module has legacy version info (%s), is incompatible", name, module_version);
		else
			report_error_and_exit("%s: module has no version info, is incompatible", name);
	}

	return module;
}

void module_on_activation(struct GtkLock *gtklock) {
	for(guint idx = 0; idx < gtklock->modules->len; idx++) {
		void (*fn)(struct GtkLock *, int) = NULL;
		GModule *module = g_array_index(gtklock->modules, GModule *, idx);
		if(g_module_symbol(module, "on_activation", (gpointer *)&fn)) fn(gtklock, idx);
	}
}

void module_on_locked(struct GtkLock *gtklock) {
	for(guint idx = 0; idx < gtklock->modules->len; idx++) {
		void (*fn)(struct GtkLock *) = NULL;
		GModule *module = g_array_index(gtklock->modules, GModule *, idx);
		if(g_module_symbol(module, "on_locked", (gpointer *)&fn)) fn(gtklock);
	}
}

void module_on_output_change(struct GtkLock *gtklock) {
	for(guint idx = 0; idx < gtklock->modules->len; idx++) {
		void (*fn)(struct GtkLock *) = NULL;
		GModule *module = g_array_index(gtklock->modules, GModule *, idx);
		if(g_module_symbol(module, "on_output_change", (gpointer *)&fn)) fn(gtklock);
	}
}

void module_on_focus_change(struct GtkLock *gtklock, struct Window *win, struct Window *old) {
	for(guint idx = 0; idx < gtklock->modules->len; idx++) {
		void (*fn)(struct GtkLock *, struct Window *, struct Window *) = NULL;
		GModule *module = g_array_index(gtklock->modules, GModule *, idx);
		if(g_module_symbol(module, "on_focus_change", (gpointer *)&fn)) fn(gtklock, win, old);
	}
}

void module_on_idle_hide(struct GtkLock *gtklock) {
	for(guint idx = 0; idx < gtklock->modules->len; idx++) {
		void (*fn)(struct GtkLock *) = NULL;
		GModule *module = g_array_index(gtklock->modules, GModule *, idx);
		if(g_module_symbol(module, "on_idle_hide", (gpointer *)&fn)) fn(gtklock);
	}
}

void module_on_idle_show(struct GtkLock *gtklock) {
	for(guint idx = 0; idx < gtklock->modules->len; idx++) {
		void (*fn)(struct GtkLock *) = NULL;
		GModule *module = g_array_index(gtklock->modules, GModule *, idx);
		if(g_module_symbol(module, "on_idle_show", (gpointer *)&fn)) fn(gtklock);
	}
}

void module_on_window_create(struct GtkLock *gtklock, struct Window *win) {
	for(guint idx = 0; idx < gtklock->modules->len; idx++) {
		void (*fn)(struct GtkLock *, struct Window *) = NULL;
		GModule *module = g_array_index(gtklock->modules, GModule *, idx);
		if(g_module_symbol(module, "on_window_create", (gpointer *)&fn)) fn(gtklock, win);
	}
}

void module_on_window_destroy(struct GtkLock *gtklock, struct Window *win) {
	for(guint idx = 0; idx < gtklock->modules->len; idx++) {
		void (*fn)(struct GtkLock *, struct Window *) = NULL;
		GModule *module = g_array_index(gtklock->modules, GModule *, idx);
		if(g_module_symbol(module, "on_window_destroy", (gpointer *)&fn)) fn(gtklock, win);
	}
}

