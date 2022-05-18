// gtklock
// Copyright (c) 2022 Jovan Lanik

// PAM Authentication

#define _POSIX_C_SOURCE 200809L

#include <pwd.h>
#include <security/pam_appl.h>

#include "window.h"
#include "gtklock.h"
#include "input-inhibitor.h"

static char *password;
static pam_handle_t *handle = NULL;

static int conversation(
	int num_msg,
	const struct pam_message **msg,
	struct pam_response **resp,
	void *appdata_ptr
) {
	*resp = calloc(num_msg, sizeof(struct pam_response));
	if(*resp == NULL) {
		g_warning("Failed allocation");
		return PAM_ABORT;
	}

	for(int i = 0; i < num_msg; ++i) {
		if(msg[i]->msg_style != PAM_PROMPT_ECHO_OFF
			&& msg[i]->msg_style != PAM_PROMPT_ECHO_ON) continue;
		resp[i]->resp_retcode = 0;
		resp[i]->resp = password;
	}
	return PAM_SUCCESS;
}

void auth_start(void) {
	struct passwd *pw = getpwuid(getuid());
	if(pw == NULL) g_error("getpwuid() failed");
	char *username = pw->pw_name;

	const struct pam_conv conv = { conversation, NULL };
	int ret = pam_start("gtklock", username, &conv, &handle);
	if(ret != PAM_SUCCESS) g_error("pam_start() failed");
}

void auth_check(GtkWidget *widget, gpointer data) {
	struct Window *ctx = data;
	password = strdup(gtk_entry_get_text((GtkEntry*)ctx->input_field));

	int ret = pam_authenticate(handle, 0);
	// TODO: error message
	if(ret != PAM_SUCCESS) {
		g_print("Authentication failure!\n");
		return;
	}

	input_inhibitor_destroy();
	ret = pam_setcred(handle, PAM_REFRESH_CRED);
	if(pam_end(handle, ret) != PAM_SUCCESS) g_warning("pam_end() failed");
	exit(0);
}

