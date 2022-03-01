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

bool strequal(const char *l, const char *r) {
	if((l == NULL) != (r == NULL)) return false;
	return !strcmp(l, r);
}

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
			if(strequal(info.dli_fname, "/usr/lib/libglib-2.0.so.0")
					&& strequal(info.dli_sname, "g_get_user_config_dir"))
				return make_home(kodoku_home, "gtk");
			if(i == 1 && strequal(info.dli_fname, "/usr/lib/libfontconfig.so.1"))
				return make_home(kodoku_home, "fontconfig");
		}
	}

	return o_getenv(name);
}
