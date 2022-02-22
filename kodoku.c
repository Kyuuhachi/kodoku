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

// Much easier to piggyback off musl than implementing the exec variants manually
#define __strchrnul strchrnul
#define weak_alias(old, new) \
	extern __typeof(old) new __attribute__((__weak__, __alias__(#old)))

#include "musl/src/process/execl.c"
#include "musl/src/process/execle.c"
#include "musl/src/process/execlp.c"
#include "musl/src/process/execv.c"
#include "musl/src/process/execvp.c"


static __typeof(&execve) o_execve;

struct ko_llist {
	char *val;
	struct ko_llist *next;
};

static struct ko_llist *ko_env_exe = NULL;
static struct ko_llist *ko_env_misc = NULL;

struct ko_llist *ko_llist_cons(struct ko_llist *list, char *val) {
	struct ko_llist *head = malloc(sizeof (struct ko_llist));
	head->val = val;
	head->next = list;
	return head;
}

void ko_llist_free(struct ko_llist *list) {
	while(list) {
		struct ko_llist *next = list->next;
		free(list->val);
		free(list);
		list = next;
	}
}

size_t ko_envp_size(char *const *envp, struct ko_llist *list) {
	int envc;
	for(envc = 0; envp[envc] != NULL; envc++) {}
	for(; list; envc++, list = list->next) {}
	return envc + 1;
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

void ko_envp_fill(char **dst, char *const *src, struct ko_llist *list) {
	int i;
	for(i = 0; src[i]; i++)
		dst[i] = src[i];
	dst[i] = NULL;
	for(; list; list = list->next)
		ko_envp_insert(dst, list->val);
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

	{
		char *suf = getenv("KODOKU_STATIC");
		if(suf == NULL) suf = "misc";
		char *str = ko_build_var(name, dir, suf);
		ko_env_misc = ko_llist_cons(ko_env_misc, str);
	}

	{
		char *exe = (char*)getauxval(AT_EXECFN);
		char buf[PATH_MAX+1];
		if(exe == NULL || realpath(exe, buf) == NULL) {
			if(realpath("/proc/self/exe", buf) == NULL) goto end;
		}
		char *last = strrchr(buf, '/');
		if(last == NULL) goto end;
		char *str = ko_build_var(name, dir, last+1);
		ko_env_exe = ko_llist_cons(ko_env_exe, str);
	} end:;
}

__attribute__((constructor)) void ko_init() {
	o_execve = dlsym(RTLD_NEXT, "execve");
	ko_add_var("HOME");
	ko_add_var("TMPDIR");

	char **new_environ = calloc(sizeof(char*), ko_envp_size(environ, ko_env_exe));
	ko_envp_fill(new_environ, environ, ko_env_exe);
	environ = new_environ;
}

__attribute__((destructor)) void ko_deinit() {
	ko_llist_free(ko_env_exe);
	ko_llist_free(ko_env_misc);
	free(environ);
}

int execve(const char *file, char *const argv[], char *const envp[]) {
	if(envp == NULL)
		return o_execve(file, argv, envp);

	char *new_envp[ko_envp_size(envp, ko_env_misc)];
	ko_envp_fill(new_envp, envp, ko_env_misc);
	return o_execve(file, argv, new_envp);
}
