// gcc -shared -fPIC -Wall -Wextra -ldl -Wl,--no-undefined kodoku.c -o kodoku.so
#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <dlfcn.h>

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

// Much easier to piggyback off musl than implementing these manually
#define __strchrnul strchrnul
#define weak __attribute__((__weak__))
#define hidden __attribute__((__visibility__("hidden")))
#define weak_alias(old, new) \
	extern __typeof(old) new __attribute__((__weak__, __alias__(#old)))

#include "musl/src/process/execl.c"
#include "musl/src/process/execle.c"
#include "musl/src/process/execlp.c"
#include "musl/src/process/execv.c"
#include "musl/src/process/execvp.c"


static char *home_misc = NULL;
static __typeof(&execve) o_execve;

int set_home() {
	char *homedir = getenv("KODOKU_HOME");
	if(homedir == NULL) return 0;

	char buf[1024];
	ssize_t r = readlink("/proc/self/exe", buf, sizeof(buf));
	if(r < 0) return r;
	if(r >= sizeof(buf)) return ENAMETOOLONG;
	buf[r] = 0;
	char *last = strrchr(buf, '/');
	if(last == NULL) return 0; // could possibly happen on fexecve(); in that case we just keep the old home

	char home_str[strlen(homedir)+1+strlen(last+1)+1];
	strcpy(home_str, homedir);
	strcat(home_str, "/");
	strcat(home_str, last+1);
	return setenv("HOME", home_str, 1);
}

__attribute__((constructor)) void init() {
	set_home();
	char *_home_misc = getenv("KODOKU_HOME_MISC");
	if(_home_misc != NULL) home_misc = strdup(_home_misc);
	o_execve = dlsym(RTLD_NEXT, "execve");
}

__attribute__((destructor)) void deinit() {
	free(home_misc);
}

int execve(const char *file, char *const argv[], char *const envp[]) {
	if(envp == NULL || home_misc == NULL)
		return o_execve(file, argv, envp);

	char home_str[5+strlen(home_misc)+1];
	strcpy(home_str, "HOME=");
	strcat(home_str, home_misc);

	int envc;
	for(envc = 0; envp[envc] != NULL; envc++) {}
	char *new_envp[envc+1];
	for(int i = 0; i < envc; i++)
		new_envp[i] = !strncmp(envp[i], "HOME=", 5) ? home_str : envp[i];
	new_envp[envc] = NULL;

	return o_execve(file, argv, new_envp);
}
