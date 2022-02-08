// gcc -shared -fPIC -Wall -Wextra -ldl -Wl,--no-undefined -fvisibility=hidden kodoku.c -o kodoku.so
#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <dlfcn.h>
#include <sys/auxv.h>

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

struct ko_vars {
	char *env_exe;
	char *env_static;
	struct ko_vars *next;
};

static struct ko_vars *ko_vars = NULL;

size_t ko_envp_size(char *const *envp) {
	int envc;
	for(envc = 0; envp[envc] != NULL; envc++) {}
	for(struct ko_vars *var = ko_vars; var; var = var->next)
		envc++;
	return envc;
}

void ko_envp_copy(char **dst, char *const *src) {
	int i;
	for(i = 0; src[i]; i++)
		dst[i] = src[i];
	dst[i] = NULL;
}

void ko_envp_insert(char **envp, char *line) {
	int i;
	for(i = 0; envp[i] != NULL; i++)
		if(!strncmp(envp[i], line, strchrnul(line, '=')-line)) {
			envp[i] = line;
			return;
		}
	envp[i] = line;
	envp[i+1] = NULL;
}

char *ko_build_var(char *name, char *dir, char *suf) {
	char *str = malloc(strlen(name) + 1 + strlen(dir) + 1 + strlen(suf) + 1);
	strcpy(str, name);
	strcat(str, "=");
	strcat(str, dir);
	strcat(str, "/");
	strcat(str, suf);
	return str;
}

void ko_add_var(char *name) {
	char varname[strlen("KODOKU_")+strlen(name)+1];
	strcpy(varname, "KODOKU_");
	strcat(varname, name);
	char *dir = getenv(varname);
	if(dir == NULL) return;

	struct ko_vars *prev = ko_vars;
	ko_vars = malloc(sizeof (struct ko_vars));
	ko_vars->next = prev;

	{
		char *suf = getenv("KODOKU_STATIC");
		if(suf == NULL) suf = "static";
		ko_vars->env_static = ko_build_var(name, dir, suf);
	}

	{
		char *exe = (char*)getauxval(AT_EXECFN);
		char buf[PATH_MAX+1];
		if(exe == NULL || realpath(exe, buf) == NULL) {
			if(realpath("/proc/self/exe", buf) == NULL) goto end;
		}
		char *last = strrchr(buf, '/');
		if(last == NULL) goto end;
		ko_vars->env_exe = ko_build_var(name, dir, last+1);
	} end:;
}

__attribute__((constructor)) void ko_init() {
	o_execve = dlsym(RTLD_NEXT, "execve");
	ko_add_var("HOME");
	ko_add_var("TMPDIR");

	char **new_environ = calloc(sizeof(char*), ko_envp_size(environ)+1);
	ko_envp_copy(new_environ, environ);
	for(struct ko_vars *var = ko_vars; var; var = var->next)
		if(var->env_exe)
			ko_envp_insert(new_environ, var->env_exe);
	environ = new_environ;
}

__attribute__((destructor)) void ko_deinit() {
	while(ko_vars) {
		struct ko_vars *next = ko_vars->next;
		free(ko_vars->env_exe);
		free(ko_vars->env_static);
		free(ko_vars);
		ko_vars = next;
	}
	free(environ);
}

int execve(const char *file, char *const argv[], char *const envp[]) {
	if(envp == NULL)
		return o_execve(file, argv, envp);

	char *new_envp[ko_envp_size(envp)+1];
	ko_envp_copy(new_envp, envp);
	for(struct ko_vars *var = ko_vars; var; var = var->next)
		if(var->env_static)
			ko_envp_insert(new_envp, var->env_static);

	return o_execve(file, argv, new_envp);
}
