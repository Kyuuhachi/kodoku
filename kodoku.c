#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <dlfcn.h>
#include <sys/auxv.h>
#include <assert.h>
#include <stdbool.h>

#define DLLEXPORT __attribute__((__visibility__("default")))
DLLEXPORT int execl(const char *pathname, const char *arg, ... /*, (char *) NULL */);
DLLEXPORT int execlp(const char *file, const char *arg, ... /*, (char *) NULL */);
DLLEXPORT int execle(const char *pathname, const char *arg, ... /*, (char *) NULL, char *const envp[] */);
DLLEXPORT int execv(const char *pathname, char *const argv[]);
DLLEXPORT int execvp(const char *file, char *const argv[]);
DLLEXPORT int execvpe(const char *file, char *const argv[], char *const envp[]);
DLLEXPORT int execve(const char *file, char *const argv[], char *const envp[]);
// Let's skip fexecve, it's rare
#undef DLLEXPORT

static char *ko_home_self = NULL;
static char *ko_home_misc = NULL;

char *ko_build_home(char *dir, char *suf) {
	char *str = malloc(5 + strlen(dir) + 1 + strlen(suf) + 1);
	strcpy(str, "HOME=");
	strcat(str, dir);
	strcat(str, "/");
	strcat(str, suf);
	return str;
}

__attribute__((constructor)) void ko_init() {
	char *dir = getenv("KODOKU_HOME");
	if(dir) {
		char *exe = (char*)getauxval(AT_EXECFN);
		char buf[PATH_MAX+1];
		if((exe != NULL && realpath(exe, buf) != NULL)
		|| realpath("/proc/self/exe", buf) != NULL) {
			bool is_lib = false;
			is_lib |= strncmp(buf, "/usr/lib/", 9) == 0;
			is_lib |= strncmp(buf, "/usr/lib32/", 11) == 0;
			is_lib |= strncmp(buf, "/usr/lib64/", 11) == 0;
			is_lib &= strncmp(buf, "/usr/lib/firefox/", 16) != 0;
			if(!is_lib) {
				char *last = strrchr(buf, '/');
				assert(last != NULL);
				setenv("KODOKU_NAME", last+1, 1);
			}
		}

		char *misc = getenv("KODOKU_MISC");
		if(misc == NULL) misc = "misc";
		if(!getenv("KODOKU_NAME")) setenv("KODOKU_NAME", misc, 1);
		char *name = getenv("KODOKU_NAME");

		ko_home_self = ko_build_home(dir, name);
		ko_home_misc = ko_build_home(dir, misc);
		putenv(ko_home_self);
	}
}

__attribute__((destructor)) void ko_deinit() {
	free(ko_home_self);
	free(ko_home_misc);
}

int o_execve(const char *file, char *const argv[], char *const envp[]) {
	static __typeof(o_execve) *super;
	if(!super) super = dlsym(RTLD_NEXT, "execve");
	return super(file, argv, envp);
}

int execve(const char *file, char *const argv[], char *const envp[]) {
	if(envp == NULL) envp = environ;
	int i;
	for(i = 0; envp[i] != 0; i++) {}
	char *new_envp[i+1];
	memcpy(new_envp, envp, sizeof(new_envp));
	for(int i = 0; new_envp[i]; i++)
		if(strncmp(new_envp[i], "HOME=", 5) == 0) {
			new_envp[i] = ko_home_misc;
			break;
		}
	return o_execve(file, argv, new_envp);
}
