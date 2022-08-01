// gtklock
// Copyright (c) 2022 Jovan Lanik

// Configuration

#include "config.h"

void config_load(const char *path, const char *group, GOptionEntry entries[]) {
	GKeyFile *keyfile = g_key_file_new();
	g_key_file_load_from_file(keyfile, path, G_KEY_FILE_NONE, NULL);
	for(int i = 0; entries[i].long_name != NULL; ++i) {
		if(!g_key_file_has_key(keyfile, group, entries[i].long_name, NULL)) continue;
		switch(entries[i].arg) {
			case G_OPTION_ARG_NONE:
				*(gboolean *)entries[i].arg_data =
					g_key_file_get_boolean(keyfile, group, entries[i].long_name, NULL);
				break;
			case G_OPTION_ARG_INT:
				*(gboolean *)entries[i].arg_data =
					g_key_file_get_integer(keyfile, group, entries[i].long_name, NULL);
				break;
			case G_OPTION_ARG_STRING:
			case G_OPTION_ARG_FILENAME:
				*(gchar **)entries[i].arg_data =
					g_key_file_get_string(keyfile, group, entries[i].long_name, NULL);
				break;
			case G_OPTION_ARG_STRING_ARRAY:
			case G_OPTION_ARG_FILENAME_ARRAY:
				*(gchar ***)entries[i].arg_data =
					g_key_file_get_string_list(keyfile, group, entries[i].long_name, NULL, NULL);
				break;
			default:
				g_error("Unknown entry argument type");
		}
	}
	g_key_file_unref(keyfile);
}

