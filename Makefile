.PHONY: clean
.DEFAULT_GOAL := kodoku.so

clean:
	@rm kodoku.so || true

kodoku.so: kodoku.c kodoku-lib.c
	@gcc -shared -fPIC -Wall -Wextra -ldl -Wl,--no-undefined -fvisibility=hidden -Imusl-shim -o kodoku.so \
		$^ musl/src/process/exec{l,le,lp,v,vp}.c
