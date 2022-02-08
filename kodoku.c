// gcc -shared -fPIC -Wall -Wextra kodoku.c -o kodoku.so
#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <dlfcn.h>
#include <stdbool.h>

extern char **environ;

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

int execve(const char *file, char *const argv[], char *const envp[]) {
	char *home_path = getenv("KODOKU_HOME_MISC");
	if(home_path == NULL)
		return ((__typeof(&execve))dlsym(RTLD_NEXT, "execve"))(file, argv, envp);

	char home_str[5+strlen(home_path)+1];
	strcpy(home_str, "HOME=");
	strcpy(home_str+5, home_path);

	int envc;
	for(envc = 0; envp[envc] != NULL; envc++) {}
	char *new_envp[envc+1];
	for(int i = 0; i < envc; i++)
		new_envp[i] = !strncmp(envp[i], "HOME=", 5) ? home_str : envp[i];
	new_envp[envc] = NULL;

	return ((__typeof(&execve))dlsym(RTLD_NEXT, "execve"))(file, argv, new_envp);
}
