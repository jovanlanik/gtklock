// gtklock
// Copyright (c) 2022 Jovan Lanik

// XDG funtions

#include "glib.h"

static char *get_path(const char *basename, const char *(*get_dir)(void), const char * const *(*get_dirs)(void)) {
	const char *dir = get_dir();
	char *path = g_build_path("/", dir, "gtklock", basename, NULL);
	if(g_file_test(path, G_FILE_TEST_IS_REGULAR) == TRUE) return path;

	const char * const *dirs = get_dirs();
	for(int i = 0; dirs[i] != NULL; ++i) {
		path = g_build_path("/", dirs[i], "gtklock", basename, NULL);
		if(g_file_test(path, G_FILE_TEST_IS_REGULAR) == TRUE) return path;
	}
	return NULL;
}

char *xdg_get_config_path(const char *basename) { return get_path(basename, g_get_user_config_dir, g_get_system_config_dirs); }
char *xdg_get_data_path(const char *basename) { return get_path(basename, g_get_user_data_dir, g_get_system_data_dirs); }

