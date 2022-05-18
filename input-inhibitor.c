/*

MIT License

Copyright (c) 2020 William Wold

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkwayland.h>

#include "wlr-input-inhibitor-unstable-v1-client-protocol.h"

static struct wl_display *wl_display = NULL;
static struct wl_registry *wl_registry_global = NULL;
static struct zwlr_input_inhibit_manager_v1 *input_inhibit_manager_global = NULL;

static gboolean has_initialized = FALSE;

static void wl_registry_handle_global(
	void *_data,
	struct wl_registry *registry,
	uint32_t id,
	const char *interface,
	uint32_t version
) {
	// pull out needed globals
	if(strcmp(interface, zwlr_input_inhibit_manager_v1_interface.name) == 0) {
		input_inhibit_manager_global = wl_registry_bind(
			registry,
			id,
			&zwlr_input_inhibit_manager_v1_interface,
			1
		);
	}
}

static void wl_registry_handle_global_remove(void *_data, struct wl_registry *_registry, uint32_t _id) { }

static const struct wl_registry_listener wl_registry_listener = {
	.global = wl_registry_handle_global,
	.global_remove = wl_registry_handle_global_remove,
};

static void gtk_wayland_init_if_needed(void) {
	if(has_initialized) return;

	GdkDisplay *gdk_display = gdk_display_get_default();
	g_return_if_fail(gdk_display);
	g_return_if_fail(GDK_IS_WAYLAND_DISPLAY(gdk_display));

	wl_display = gdk_wayland_display_get_wl_display(gdk_display);
	wl_registry_global = wl_display_get_registry(wl_display);
	wl_registry_add_listener(wl_registry_global, &wl_registry_listener, NULL);
	wl_display_roundtrip(wl_display);

	if(!input_inhibit_manager_global)
		g_warning("Your Wayland compositor does not support wlr-input-inhibitor");

	has_initialized = TRUE;
}

void input_inhibitor_get(void) {
	gtk_wayland_init_if_needed();
	if(!input_inhibit_manager_global) return;
	
	zwlr_input_inhibit_manager_v1_get_inhibitor(input_inhibit_manager_global);
	if(wl_display_roundtrip(wl_display) == -1 && input_inhibit_manager_global)
		g_warning("Failed to inhibit input. Is another lockscreen already running?");
}

void input_inhibitor_destroy(void) {
	zwlr_input_inhibit_manager_v1_destroy(input_inhibit_manager_global);
}

