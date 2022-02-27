kodoku.so: kodoku.c
	gcc -shared -fPIC -Wall -Wextra -ldl -Wl,--no-undefined -fvisibility=hidden -Imusl-shim -o kodoku.so \
		kodoku.c musl/src/process/exec{l,le,lp,v,vp}.c
