#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <assert.h>
#include <string.h>

#define DLLEXPORT __attribute__((__visibility__("default")))

DLLEXPORT char *getenv(const char *name) {
	static __typeof(getenv) *o_getenv;
	if(!o_getenv) o_getenv = dlsym(RTLD_NEXT, "getenv");

	if(!strcmp(name, "HOME")) {
		void *stack[2];
		int naddr = backtrace(stack, 2);
		if(naddr >= 2) {
			void *caller = stack[1];
			Dl_info info;
			dladdr(caller, &info);
			if(info.dli_fname) {
				char *debug = o_getenv("KODOKU_LIB_DEBUG");
				if(debug && *debug)
					fprintf(stderr, "[KODOKU] getenv(\"HOME\") from %s(%s+0x%lx)\n",
						info.dli_fname, info.dli_sname, caller-info.dli_saddr);

				if(!strcmp(info.dli_fname, "/usr/lib/libfontconfig.so.1"))
					return "/home/data/fontconfig";
			}
		}
	}

	return o_getenv(name);
}
