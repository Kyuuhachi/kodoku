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

struct ko_statics {
	char *key;
	char *env_static;
	struct ko_statics *next;
};

static struct ko_statics *ko_vars = NULL;

void ko_add_env(char *name, char *dir) {
	char *exe = (char*)getauxval(AT_EXECFN);
	char buf[PATH_MAX+1];
	if(exe == NULL || realpath(exe, buf) == NULL) {
		if(realpath("/proc/self/exe", buf) == NULL) return;
	}
	char *last = strrchr(buf, '/');
	if(last == NULL) return;

	char val[strlen(dir)+strlen(exe)+1];
	strcpy(val, dir);
	strcat(val, last); // it begins with a slash courtesy of strrchr
	setenv(name, val, 1);
}

void ko_add_static(char *name, char *dir) {
	char *suf = getenv("KODOKU_STATIC");
	if(suf == NULL) suf = "static";

	char *env_static = malloc(strlen(name) + 1 + strlen(dir) + 1 + strlen(suf) + 1);
	strcpy(env_static, name);
	strcat(env_static, "=");
	strcat(env_static, dir);
	strcat(env_static, "/");
	strcat(env_static, suf);

	struct ko_statics *prev = ko_vars;
	ko_vars = malloc(sizeof (struct ko_statics));
	ko_vars->key = name;
	ko_vars->env_static = env_static;
	ko_vars->next = prev;
}

void ko_addvar(char *name) {
	char varname[strlen("KODOKU_")+strlen(name)+1];
	strcpy(varname, "KODOKU_");
	strcat(varname, name);
	char *dir = getenv(varname);
	if(dir == NULL) return;

	ko_add_env(name, dir);
	ko_add_static(name, dir);
}

__attribute__((constructor)) void ko_init() {
	o_execve = dlsym(RTLD_NEXT, "execve");
	ko_addvar("HOME");
	ko_addvar("TMPDIR");
}

__attribute__((destructor)) void ko_deinit() {
	while(ko_vars) {
		struct ko_statics *next = ko_vars->next;
		free(ko_vars->env_static);
		free(ko_vars);
		ko_vars = next;
	}
}

int execve(const char *file, char *const argv[], char *const envp[]) {
	if(envp == NULL)
		return o_execve(file, argv, envp);

	int envc;
	for(envc = 0; envp[envc] != NULL; envc++) {}
	int newenvc = envc;
	for(struct ko_statics *var = ko_vars; var; var = var->next)
		newenvc++;

	char *new_envp[newenvc+1];
	for(int i = 0; i < envc; i++)
		new_envp[i] = envp[i];
	new_envp[envc] = NULL;

	for(struct ko_statics *var = ko_vars; var; var = var->next) {
		int i;
		for(i = 0; envp[i] != NULL; i++)
			if(!strncmp(new_envp[i], var->env_static, strlen(var->key)+1)) {
				new_envp[i] = var->env_static;
				goto l;
			}
		new_envp[i] = var->env_static;
		new_envp[i+1] = NULL;
		l:;
	}

	return o_execve(file, argv, new_envp);
}
