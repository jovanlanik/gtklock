// gtklock
// Copyright (c) 2022 Jovan Lanik

// Module support

#pragma once

#include <gmodule.h>

#include "gtklock.h"

GModule *module_load(const char *name);
void module_on_activation(struct GtkLock *gtklock);
void module_on_locked(struct GtkLock *gtklock);
void module_on_output_change(struct GtkLock *gtklock);
void module_on_focus_change(struct GtkLock *gtklock, struct Window *win, struct Window *old);
void module_on_idle_hide(struct GtkLock *gtklock);
void module_on_idle_show(struct GtkLock *gtklock);
void module_on_window_create(struct GtkLock *gtklock, struct Window *win);
void module_on_window_destroy(struct GtkLock *gtklock, struct Window *win);

