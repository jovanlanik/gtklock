// gtklock
// Copyright (c) 2022 Jovan Lanik

// PAM Authentication

#define _POSIX_C_SOURCE 200809L

#include <unistd.h>
#include <pwd.h>
#include <sys/wait.h>
#include <glib-unix.h>
#include <glib/gstdio.h>
#include <security/pam_appl.h>

#include "auth.h"

static GString *error_string = NULL;
static GString *message_string = NULL;

static char *get_and_free_string(GString **str) {
	if(str == NULL || *str == NULL) return NULL;
	char *buff = (*str)->str;
	g_string_free(*str, FALSE);
	*str = NULL;
	return buff;
}

char *auth_get_error(void) { return get_and_free_string(&error_string); }
char *auth_get_message(void) { return get_and_free_string(&message_string); }

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
				fprintf(stderr, "%s", msg[i]->msg);
				fputc(EOF, stderr);
				break;
			case PAM_TEXT_INFO:
				fprintf(stdout, "%s", msg[i]->msg);
				fputc(EOF, stdout);
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

enum pwcheck auth_pw_check(const char *s) {
	static int err_pipe[2];
	static int out_pipe[2];
	static pid_t pid = -2;

	if(pid < 0) {
		// TODO: GError
		// TODO: if pipes fail open /dev/null as stderr and stdout
		if(!g_unix_open_pipe(err_pipe, 0, NULL)) g_error("pipe failure");
		if(!g_unix_open_pipe(out_pipe, 0, NULL)) g_error("pipe failure");
		pid = fork();
		if(pid == 0) {
			close(err_pipe[0]);
			close(out_pipe[0]);
			dup2(err_pipe[1], STDERR_FILENO);
			dup2(out_pipe[1], STDOUT_FILENO);
			freopen("/dev/null", "r", stdin);
			auth_child(s);
			g_error("auth_child failure");
		}
		g_close(err_pipe[1], NULL);
		g_close(out_pipe[1], NULL);
	}

	if(error_string == NULL) error_string = g_string_new("");
	if(message_string == NULL) message_string = g_string_new("");

	// TODO: don't if pipes failed (or maybe read always fails safely?)
	char buff[128];
	int nread;
	gboolean has_read = FALSE;
	while((nread = read(err_pipe[0], buff, 127))) {
		buff[nread-1] = '\0';
		g_string_append(error_string, buff);
		has_read = TRUE;
	}
	if(has_read) return PW_ERROR;
	while((nread = read(out_pipe[0], buff, 127))) {
		buff[nread-1] = '\0';
		g_string_append(message_string, buff);
		has_read = TRUE;
	}
	if(has_read) return PW_MESSAGE;

	int status;
	waitpid(pid, &status, WNOHANG);
	if(WIFEXITED(status)) {
		pid = -1;
		if(WEXITSTATUS(status) == EXIT_SUCCESS) return PW_SUCCESS;
		else return PW_FAILURE;
	}
	return PW_WAIT;
}

