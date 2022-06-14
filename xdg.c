#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <linux/limits.h>
#include <sys/stat.h>

#define HOME_STYLE_SUFFIX "/.config/gtklock/style.css"
#define ETC_STYLE_PATH "/etc/gtklock/style.css"

/*
 * resolve_xdg_style_path_home will perform a lookup of the current user's data
 * to discover their home directory. 
 *
 * Once the home discovery is discovered, a lookup of '~/.config/gtklock/style.css'
 * is performed. 
 *
 * If discovered the fully resolved path is returned, if not nil is returned.
 *
 * The caller should free the returned pointer when there is no longer use for it.
 */
char *resolve_xdg_style_path_home(void) {
	char *style_path = calloc(PATH_MAX, sizeof(char));

	uid_t euid = geteuid();
	struct passwd *pwd = getpwuid(euid);
	if(!pwd)
		goto null;
	strcpy(style_path, pwd->pw_dir);
	strcat(style_path, HOME_STYLE_SUFFIX);

	struct stat s;
	if(stat(style_path, &s) == -1)
		goto null;

	if(S_ISREG(s.st_mode) || S_ISLNK(s.st_mode)) {
		return style_path;
	}

null:
	free(style_path);
	return NULL;
};

/*
 * resolve_xdg_style_path_etc will perform a lookup of "style.css" in the canonical
 * "/etc/gtklock" directory.
 *
 * If discovered the fully resolved path is returned, if not nil is returned.
 *
 * The caller should free the returned pointer when there is no longer use for it.
 */
char *resolve_xdg_style_path_etc(void) {
	// we could pass back just the global ETC_STYLE_PATH but lets do this so
	// the usage of free is consistent with resolve_xdg_style_path_home.
	char *style_path = calloc(sizeof(ETC_STYLE_PATH), sizeof(char));

	strcpy(style_path, ETC_STYLE_PATH);

	struct stat s;
	if(stat(style_path, &s) == -1)
		goto null;

	if(S_ISREG(s.st_mode) || S_ISLNK(s.st_mode)) {
		return style_path;
	}

null:
	free(style_path);
	return NULL;
};

char *resolve_xdg_style_path(void) {
	char *style_path;
	// prefer user directory
	style_path = resolve_xdg_style_path_home();
	if(style_path) return style_path;
	// then etc directory
	style_path = resolve_xdg_style_path_home();
	if(style_path) return style_path;
	return NULL;
}

