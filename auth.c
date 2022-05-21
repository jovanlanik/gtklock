// gtklock
// Copyright (c) 2022 Jovan Lanik

// PAM Authentication

#define _POSIX_C_SOURCE 200809L

#include <unistd.h>
#include <pwd.h>
#include <security/pam_appl.h>

#include "auth.h"

static int pam_status;
static char *password;
pam_handle_t *handle = NULL;

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
	pam_status = pam_start("gtklock", username, &conv, &handle);
	if(pam_status != PAM_SUCCESS) g_error("pam_start() failed");
}

void auth_end(void) {
	if(pam_end(handle, pam_status) != PAM_SUCCESS)
		g_warning("pam_end() failed");
}

gboolean auth_pwcheck(const char *s) {
	password = strdup(s);
	if(password == NULL) {
		g_warning("Failed allocation");
		return FALSE;
	}
	pam_status = pam_authenticate((pam_handle_t *)handle, 0);
	if(pam_status != PAM_SUCCESS) return FALSE;
	pam_status = pam_setcred((pam_handle_t *)handle, PAM_REFRESH_CRED);
	return TRUE;
}

