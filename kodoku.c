// gcc -shared -fPIC -Wall -Wextra -ldl -Wl,--no-undefined -fvisibility=hidden kodoku.c -o kodoku.so
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


static __typeof(&execve) o_execve;

int set_home() {
	char *homedir = getenv("KODOKU_HOME");
	if(homedir == NULL) return 0;

	char *exe = getenv("_KODOKU_TMP_EXE");
	unsetenv("_KODOKU_TMP_EXE");
	if(exe == NULL) {
		char buf[PATH_MAX+1];
		ssize_t r = readlink("/proc/self/exe", buf, sizeof(buf));
		if(r < 0) return r;
		if((size_t)r >= sizeof(buf)) return ENAMETOOLONG;
		buf[r] = 0;
		exe = buf;
	}
	char *last = strrchr(exe, '/');
	if(last == NULL) return 0; // could possibly happen on fexecve(); in that case we just keep the old home

	char home_str[strlen(homedir)+strlen(last)+1];
	strcpy(home_str, homedir);
	strcat(home_str, last);
	return setenv("HOME", home_str, 1);
}

__attribute__((constructor)) void init() {
	set_home();
	o_execve = dlsym(RTLD_NEXT, "execve");
}

int execve(const char *file, char *const argv[], char *const envp[]) {
	if(file == NULL || envp == NULL)
		return o_execve(file, argv, envp);

	char path[PATH_MAX+1];
	if(realpath(file, path) == NULL) // Realpath() can call getcwd(), which is not signal-safe?
		return o_execve(file, argv, envp);

	char exe_str[strlen("_KODOKU_TMP_EXE=")+strlen(path)+1];
	strcpy(exe_str, "_KODOKU_TMP_EXE=");
	strcat(exe_str, file);

	int envc;
	for(envc = 0; envp[envc] != NULL; envc++) {}
	char *new_envp[envc+2];
	for(int i = 0; i < envc; i++) new_envp[i] = envp[i];
	new_envp[envc+0] = exe_str;
	new_envp[envc+1] = NULL;

	return o_execve(file, argv, new_envp);
}
