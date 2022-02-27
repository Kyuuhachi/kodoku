#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <dlfcn.h>
#include <sys/auxv.h>
#include <assert.h>

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

void ko_set_home(char **envp, char *home) {
	if(home == NULL) return;
	assert(strncmp(home, "HOME=", 5) == 0);
	for(int i = 0; envp[i]; i++)
		if(strncmp(envp[i], "HOME=", 5) == 0) {
			envp[i] = home;
			return;
		}
}

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
		char *suf = getenv("KODOKU_MISC");
		if(suf == NULL) suf = "misc";
		ko_home_misc = ko_build_home(dir, suf);

		char *exe = (char*)getauxval(AT_EXECFN);
		char buf[PATH_MAX+1];
		if((exe != NULL && realpath(exe, buf) != NULL) || realpath("/proc/self/exe", buf) != NULL) {
			char *last = strrchr(buf, '/');
			if(last != NULL) {
				ko_home_self = ko_build_home(dir, last+1);
				ko_set_home(environ, ko_home_self);
			}
		}
	}
}

__attribute__((destructor)) void ko_deinit() {
	free(ko_home_self);
	free(ko_home_misc);
}

int execve(const char *file, char *const argv[], char *const envp[]) {
	static __typeof(execve) *o_execve;
	if(!o_execve) o_execve = dlsym(RTLD_NEXT, "execve");

	if(envp == NULL)
		return o_execve(file, argv, envp);

	int i;
	for(i = 0; envp[i] != 0; i++) {}
	char *new_envp[i+1];
	memcpy(new_envp, envp, sizeof(new_envp));
	ko_set_home(new_envp, ko_home_misc);
	return o_execve(file, argv, new_envp);
}
