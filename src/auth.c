// gtklock
// Copyright (c) 2022 Jovan Lanik, Zephyr Lykos

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

int start_authentication(
		int (*convers)(int, const struct pam_message **, struct pam_response **, void *),
		struct conv_data data
	) {
	struct passwd *pwd = getpwuid(getuid());

	if(pwd == NULL) {
		perror("getpwnam");
		return PW_FAILURE;
	}
	errno = 0;

	char *username = pwd->pw_name;
	int pam_status;

	struct pam_handle *handle;
	struct pam_conv conv = { convers, (void *)&data };

	pam_status = pam_start("gtklock", username, &conv, &handle);
	if(pam_status != PAM_SUCCESS) {
		perror("pam_start() failed");
		return PW_FAILURE;
	}

	int auth_status = pam_authenticate((pam_handle_t *)handle, 0);
	pam_status = pam_setcred((pam_handle_t *)handle, PAM_REFRESH_CRED);
	if(pam_end(handle, pam_status) != PAM_SUCCESS) {
		perror("pam_end() failed");
	}

	if(auth_status == PAM_SUCCESS)
		return PW_SUCCESS;
	else
		return PW_FAILURE;
}
