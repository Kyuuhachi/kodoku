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

#define EXE_ENV "_KODOKU_EXE"

static __typeof(&execve) o_execve;

void set_var(char *var, char *invar, char *exe) {
	// exe should start with / or strange things might happen. This is
	// guaranteed by strrchr.
	char *dir = getenv(invar);
	if(dir == NULL) return;

	char val[strlen(dir)+strlen(exe)+1];
	strcpy(val, dir);
	strcat(val, exe);
	setenv(var, val, 1);
}

void set_vars() {
	char *exe = getenv(EXE_ENV);
	unsetenv(EXE_ENV);
	if(exe == NULL) {
		char buf[PATH_MAX+1];
		ssize_t r = readlink("/proc/self/exe", buf, sizeof(buf));
		if(r < 0) return;
		if((size_t)r >= sizeof(buf)) return;
		buf[r] = 0;
		exe = buf;
	}
	char *last = strrchr(exe, '/');
	if(last == NULL) return; // could possibly happen on fexecve(); in that case we just keep the old home

	set_var("HOME", "KODOKU_HOME", last);
	set_var("TMPDIR", "KODOKU_TMPDIR", last);
}

__attribute__((constructor)) void init() {
	set_vars();
	o_execve = dlsym(RTLD_NEXT, "execve");
}

int execve(const char *file, char *const argv[], char *const envp[]) {
	if(envp == NULL)
		return o_execve(file, argv, envp);

	char path[PATH_MAX+1];
	if(realpath(file, path) == NULL) // Realpath() can call getcwd(), which is not signal-safe?
		return o_execve(file, argv, envp);

	char exe_str[strlen(EXE_ENV"=")+strlen(path)+1];
	strcpy(exe_str, EXE_ENV"=");
	strcat(exe_str, path);

	int envc;
	for(envc = 0; envp[envc] != NULL; envc++) {}
	char *new_envp[envc+2];
	for(int i = 0; i < envc; i++) new_envp[i] = envp[i];
	new_envp[envc+0] = exe_str;
	new_envp[envc+1] = NULL;

	return o_execve(file, argv, new_envp);
}
