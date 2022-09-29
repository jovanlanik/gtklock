// gtklock
// Copyright (c) 2022 Jovan Lanik

// PAM Authentication

#pragma once

#include <glib.h>

enum pwcheck {
	PW_WAIT,
	PW_FAILURE,
	PW_SUCCESS,
	PW_ERROR,
	PW_MESSAGE,
};

enum pipedir {
	PIPE_PARENT,
	PIPE_CHILD,
	PIPE_LAST,
};

typedef int pipe_t[PIPE_LAST];

char *auth_get_error(void);
char *auth_get_message(void);
enum pwcheck auth_pw_check(const char *s);

