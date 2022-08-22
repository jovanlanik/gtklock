// gtklock
// Copyright (c) 2022 Jovan Lanik

// Utility functions

#include "util.h"

void report_error_and_exit(gchar const *format, ...) {
	va_list args;
	va_start(args, format);
	g_logv(NULL, G_LOG_LEVEL_CRITICAL, format, args);
	va_end(args);
	exit(EXIT_FAILURE);
}

