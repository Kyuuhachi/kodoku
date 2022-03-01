#define _GNU_SOURCE
#include <stdio.h>
#include <stdbool.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <assert.h>
#include <string.h>

#define DLLEXPORT __attribute__((__visibility__("default")))

#define MAX_STACK_DEPTH 32
#define BUF_MAX_LENGTH 256
#define BUF_COUNT 32

char BUF[BUF_COUNT][BUF_MAX_LENGTH];
size_t buf_idx = 0;
char *make_home(char *prefix, char *suffix) {
	assert(strlen(prefix) + 1 + strlen(suffix) < BUF_MAX_LENGTH);
	size_t bufi = __atomic_fetch_add(&buf_idx, 1, __ATOMIC_RELAXED) % BUF_COUNT;
	strcpy(BUF[bufi], prefix);
	strcat(BUF[bufi], "/");
	strcat(BUF[bufi], suffix);
	return BUF[bufi];
}

bool isfname(Dl_info *dl, const char *name) {
	char *start = strstr(dl->dli_fname, name);
	if(!start) return false;
	size_t len = strlen(name);
	if(start[len] != 0 && start[len] != '.') return false;
	return start == dl->dli_fname || start[-1] == '/';
}

bool issname(Dl_info *dl, const char *name) {
	if(dl->dli_sname == NULL) return false;
	return strcmp(dl->dli_sname, name) == 0;
}

DLLEXPORT char *getenv(const char *name) {
	static __typeof(getenv) *o_getenv;
	if(!o_getenv) o_getenv = dlsym(RTLD_NEXT, "getenv");

	if(!strcmp(name, "HOME")) {
		char *kodoku_home = o_getenv("KODOKU_HOME");
		if(!kodoku_home) return o_getenv(name);

		void *stack[MAX_STACK_DEPTH];
		int naddr = backtrace(stack, MAX_STACK_DEPTH);

		char *debug = o_getenv("KODOKU_LIB_DEBUG");
		if(debug && *debug) {
			Dl_info info;
			dladdr(name, &info);
			for(int i = 1; i < naddr; i++) {
				dladdr(stack[i], &info);
				fprintf(stderr, i == 1
						? "[KODOKU] getenv(\"HOME\") %s(%s+0x%lx)\n"
						: "[KODOKU]                %s(%s+0x%lx)\n",
					info.dli_fname, info.dli_sname, stack[i]-info.dli_saddr);
			}
		}

		for(int i = 1; i < naddr; i++) {
			Dl_info info;
			dladdr(stack[i], &info);
			if(isfname(&info, "libglib-2.0.so") && issname(&info, "g_get_user_config_dir"))
				return make_home(kodoku_home, "gtk");
			if(i == 1 && isfname(&info, "libfontconfig.so"))
				return make_home(kodoku_home, "fontconfig");
		}
	}

	return o_getenv(name);
}
