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

struct conv_data {
	const char *pw;
	int *err;
	int *out;
};

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
	struct conv_data *data = appdata_ptr;
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
				resp[i]->resp = strdup(data->pw);
				if(resp[i]->resp == NULL) {
					g_warning("Failed allocation");
					return PAM_ABORT;
				}
				break;
			case PAM_ERROR_MSG:
				send_msg(msg[i]->msg, data->err[1]);
				break;
			case PAM_TEXT_INFO:
				send_msg(msg[i]->msg, data->out[1]);
				break;
		}
	}
	return PAM_SUCCESS;
}

static void auth_child(const char *s, int *err, int *out) {
	struct passwd *pwd = NULL;

	errno = 0;
	pwd = getpwuid(getuid());
	if(pwd == NULL) {
		perror("getpwnam");
		exit(EXIT_FAILURE);
	}

	char *username = pwd->pw_name;
	int pam_status;
	struct pam_handle *handle;
	struct conv_data data = { .pw = s, .err = err, .out = out };
	struct pam_conv conv = { conversation, (void *)&data };
	pam_status = pam_start("gtklock", username, &conv, &handle);
	if(pam_status != PAM_SUCCESS) {
		fprintf(stderr, "pam_start() failed");
		exit(EXIT_FAILURE);
	}

	int ret = pam_authenticate((pam_handle_t *)handle, 0);
	pam_status = ret;
	pam_status = pam_setcred((pam_handle_t *)handle, PAM_REFRESH_CRED);
	if(pam_end(handle, pam_status) != PAM_SUCCESS)
		fprintf(stderr, "pam_end() failed");
	if(ret == PAM_SUCCESS) exit(EXIT_SUCCESS);
	exit(EXIT_FAILURE);
}

enum pwcheck auth_pw_check(const char *s) {
	static pipe_t err_pipe;
	static pipe_t out_pipe;
	static pid_t pid = -2;

	if(pid < 0) {
		if(pipe(err_pipe) != 0) {
			g_warning("err pipe failure");
			return PW_WAIT;
		}
		if(pipe(out_pipe) != 0) {
			close(err_pipe[PIPE_PARENT]);
			close(err_pipe[PIPE_CHILD]);
			g_warning("out pipe failure");
			return PW_WAIT;
		}
		pid = fork();
		if(pid == -1) {
			close(err_pipe[PIPE_PARENT]);
			close(err_pipe[PIPE_CHILD]);
			close(out_pipe[PIPE_PARENT]);
			close(out_pipe[PIPE_CHILD]);
			g_warning("fork failure");
			return PW_WAIT;
		}
		else if(pid == 0) {
			close(err_pipe[PIPE_PARENT]);
			close(out_pipe[PIPE_PARENT]);
			freopen("/dev/null", "r", stdin);
			auth_child(s, err_pipe, out_pipe);
		}
		close(err_pipe[PIPE_CHILD]);
		close(out_pipe[PIPE_CHILD]);
		fcntl(err_pipe[PIPE_PARENT], F_SETFL, O_NONBLOCK);
		fcntl(out_pipe[PIPE_PARENT], F_SETFL, O_NONBLOCK);
	}

	if(error_string) free(error_string);
	if(message_string) free(message_string);

	size_t len;
	ssize_t nread;
	nread = read(err_pipe[PIPE_PARENT], &len, sizeof(size_t));
	if(nread > 0 && len <= PAM_MAX_MSG_SIZE) {
		error_string = malloc(PAM_MAX_MSG_SIZE);
		nread = read(err_pipe[PIPE_PARENT], error_string, len);
		error_string[nread] = '\0';
		return PW_ERROR;
	}
	nread = read(out_pipe[PIPE_PARENT], &len, sizeof(size_t));
	if(nread > 0 && len <= PAM_MAX_MSG_SIZE) {
		message_string = malloc(PAM_MAX_MSG_SIZE);
		nread = read(out_pipe[PIPE_PARENT], message_string, len);
		message_string[nread] = '\0';
		return PW_MESSAGE;
	}

	int status;
	if(waitpid(pid, &status, WNOHANG) != 0 && WIFEXITED(status)) {
		pid = -2;
		if(WEXITSTATUS(status) == EXIT_SUCCESS) return PW_SUCCESS;
		else return PW_FAILURE;
	}
	return PW_WAIT;
}

