// gtklock
// Copyright (c) 2022 Jovan Lanik

// PAM Authentication

#define _POSIX_C_SOURCE 200809L

#include <unistd.h>
#include <pwd.h>
#include <sys/wait.h>
#include <security/pam_appl.h>

#include "auth.h"

static int conversation(
	int num_msg,
	const struct pam_message **msg,
	struct pam_response **resp,
	void *appdata_ptr
) {
	const char *password = appdata_ptr;
	*resp = calloc(num_msg, sizeof(struct pam_response));
	if(*resp == NULL) {
		g_warning("Failed allocation");
		return PAM_ABORT;
	}

	for(int i = 0; i < num_msg; ++i) {
		resp[i]->resp_retcode = 0;
		switch(msg[i]->msg_style) {
			case PAM_PROMPT_ECHO_OFF:
			case PAM_PROMPT_ECHO_ON:
				resp[i]->resp = strdup(password);
				if(resp[i]->resp == NULL) {
					g_warning("Failed allocation");
					return PAM_ABORT;
				}
				break;
			case PAM_ERROR_MSG:
			case PAM_TEXT_INFO:
				break;
		}
	}
	return PAM_SUCCESS;
}

static void auth_child(const char *s) {
	struct passwd pwd;
	struct passwd *result = NULL;
	size_t len = sysconf(_SC_GETPW_R_SIZE_MAX);
	char *buf = malloc(len);

	getpwuid_r(getuid(), &pwd, buf, len, &result);
	if(result == NULL) g_error("getpwuid() failed");

	char *username = pwd.pw_name;
	int pam_status;
	struct pam_handle *handle;
	struct pam_conv conv = { conversation, (void *)s };
	pam_status = pam_start("gtklock", username, &conv, &handle);
	if(pam_status != PAM_SUCCESS) g_error("pam_start() failed");

	int ret = pam_status = pam_authenticate((pam_handle_t *)handle, 0);
	pam_status = pam_setcred((pam_handle_t *)handle, PAM_REFRESH_CRED);
	if(pam_end(handle, pam_status) != PAM_SUCCESS) g_warning("pam_end() failed");
	if(ret != PAM_SUCCESS) exit(EXIT_FAILURE);
	exit(EXIT_SUCCESS);
}

gboolean auth_pwcheck(const char *s) {
	int status;
	pid_t pid = fork();
	if(pid == 0) {
		auth_child(s);
		g_error("auth_child failure");
	}
	waitpid(pid, &status, 0);
	if(WIFEXITED(status)) {
		if(WEXITSTATUS(status) == EXIT_SUCCESS) return TRUE;
	} else g_warning("auth_child failure");
	return FALSE;
}

