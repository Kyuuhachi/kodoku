// gcc -shared -fPIC -Wall -Wextra kodoku.cpp -o kodoku.so
#include <cstdio>
#include <unistd.h>
#include <dlfcn.h>

extern char **environ;

extern "C" {

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
}

int execve(const char *file, char *const argv[], char *const envp[]) {
	for(int i = 0; envp[i] != NULL; i++) {
		if(strncmp(envp[i], "HOME=", 5) == 0) {
			fprintf(stderr, "%s\n", envp[i]);
			fflush(stderr);
		}
	}
	auto execve_o = (__typeof(&execve))dlsym(RTLD_NEXT, "execve");
	return execve_o(file, argv, envp);
}
