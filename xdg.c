#include "glib.h"

static char *resolve_xdg_style_path_home(void) {
	const char *dir = g_get_user_config_dir();
	char *path = g_build_path("/", dir, "gtklock", "style.css", NULL);
	if(g_file_test(path, G_FILE_TEST_IS_REGULAR) == TRUE) return path;
	return NULL;
};

static char *resolve_xdg_style_path_etc(void) {
	const char * const *dir = g_get_system_config_dirs();
	for(int i = 0; dir[i] != NULL; ++i) {
		char *path = g_build_path("/", dir[i], "gtklock", "style.css", NULL);
		if(g_file_test(path, G_FILE_TEST_IS_REGULAR) == TRUE) return path;
	}
	return NULL;
};

char *resolve_xdg_style_path(void) {
	char *style_path = NULL;
	// prefer user directory
	style_path = resolve_xdg_style_path_home();
	if(style_path) return style_path;
	// then etc directory
	style_path = resolve_xdg_style_path_etc();
	if(style_path) return style_path;
	return NULL;
}

