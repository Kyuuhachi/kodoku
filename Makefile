kodoku.so: kodoku.c
	gcc -shared -fPIC -Wall -Wextra -ldl -Wl,--no-undefined -fvisibility=hidden kodoku.c -o kodoku.so
