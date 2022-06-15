// gtklock
// Copyright (c) 2022 Jovan Lanik

// Configuration

#include "config.h"

void config_load(char *path, GOptionEntry entries[]) {
	GKeyFile *keyfile = g_key_file_new();
	g_key_file_load_from_file(keyfile, path, G_KEY_FILE_NONE, NULL);
	for(int i = 0; entries[i].long_name != NULL; ++i) {
		GError *error = NULL;
		switch(entries[i].arg) {
			case G_OPTION_ARG_NONE:
				*(gboolean *)entries[i].arg_data =
					g_key_file_get_boolean(keyfile, "main", entries[i].long_name, NULL);
				break;
			case G_OPTION_ARG_STRING:
				*(gchar **)entries[i].arg_data =
					g_key_file_get_string(keyfile, "main", entries[i].long_name, NULL);
				break;
			case G_OPTION_ARG_FILENAME:
				*(gchar **)entries[i].arg_data =
					g_key_file_get_string(keyfile, "main", entries[i].long_name, &error);
				if(error == NULL && !g_file_test(entries[i].arg_data, G_FILE_TEST_EXISTS))
					g_free(entries[i].arg_data);
				break;
			default:
				g_error("Unknown entry argument type");
		}
	}
}

