// gtklock
// Copyright (c) 2022 Jovan Lanik

// PAM Authentication

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <unistd.h>
#include <pwd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <security/pam_appl.h>

#include "auth.h"

static char *error_string = NULL;
static char *message_string = NULL;

char *auth_get_error(void) {
	char *s = error_string;
	error_string = NULL;
	return s;
}
char *auth_get_message(void) {
	char *s = message_string;
	message_string = NULL;
	return s;
}

static void send_msg(const char *msg, int fd) {
	size_t len = strlen(msg);
	write(fd, &len, sizeof(size_t));
	write(fd, msg, len);
}

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
				send_msg(msg[i]->msg, STDERR_FILENO);
				break;
			case PAM_TEXT_INFO:
				send_msg(msg[i]->msg, STDOUT_FILENO);
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

	int ret = pam_authenticate((pam_handle_t *)handle, 0);
	pam_status = ret;
	pam_status = pam_setcred((pam_handle_t *)handle, PAM_REFRESH_CRED);
	if(pam_end(handle, pam_status) != PAM_SUCCESS) g_warning("pam_end() failed");
	if(ret == PAM_SUCCESS) exit(EXIT_SUCCESS);
	exit(EXIT_FAILURE);
}

enum pwcheck auth_pw_check(const char *s) {
	static int err_pipe[2];
	static int out_pipe[2];
	static pid_t pid = -2;

	if(pid < 0) {
		if(pipe(err_pipe) != 0) {
			err_pipe[0] = open("/dev/null", O_RDONLY);
			err_pipe[1] = open("/dev/null", O_WRONLY);
		}
		if(pipe(out_pipe) != 0) {
			out_pipe[0] = open("/dev/null", O_RDONLY);
			out_pipe[1] = open("/dev/null", O_WRONLY);
		}
		pid = fork();
		if(pid == -1) {
			close(err_pipe[0]);
			close(err_pipe[1]);
			close(out_pipe[0]);
			close(out_pipe[1]);
			g_warning("fork failure");
			return PW_WAIT;
		}
		else if(pid == 0) {
			close(err_pipe[0]);
			close(out_pipe[0]);
			dup2(err_pipe[1], STDERR_FILENO);
			dup2(out_pipe[1], STDOUT_FILENO);
			freopen("/dev/null", "r", stdin);
			auth_child(s);
			exit(EXIT_FAILURE);
		}
		close(err_pipe[1]);
		close(out_pipe[1]);
		fcntl(err_pipe[0], F_SETFL, O_NONBLOCK);
		fcntl(out_pipe[0], F_SETFL, O_NONBLOCK);
	}

	if(error_string) free(error_string);
	if(message_string) free(message_string);

	size_t len;
	ssize_t nread;
	nread = read(out_pipe[0], &len, sizeof(size_t));
	if(nread > 0) {
		message_string = malloc(len+1);
		nread = read(out_pipe[0], message_string, len);
		message_string[nread] = '\0';
		return PW_MESSAGE;
	}
	nread = read(err_pipe[0], &len, sizeof(size_t));
	if(nread > 0) {
		error_string = malloc(len+1);
		nread = read(out_pipe[0], error_string, len);
		error_string[nread] = '\0';
		return PW_ERROR;
	}

	int status;
	if(waitpid(pid, &status, WNOHANG) != 0 && WIFEXITED(status)) {
		pid = -2;
		if(WEXITSTATUS(status) == EXIT_SUCCESS) return PW_SUCCESS;
		else return PW_FAILURE;
	}
	return PW_WAIT;
}

