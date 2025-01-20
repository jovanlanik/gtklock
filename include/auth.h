// gtklock
// Copyright (c) 2022 Jovan Lanik

// PAM Authentication

#pragma once

#include <glib.h>
#include <security/pam_appl.h>

enum pwcheck {
	PW_WAIT,
	PW_FAILURE,
	PW_SUCCESS,
	PW_ERROR,
	PW_MESSAGE,
};

struct conv_data {
	gboolean error;
	const char *pw;
	gpointer ctx;
};

int start_authentication(
	int (*convers)(int, const struct pam_message **, struct pam_response **, void *),
	struct conv_data data
);
