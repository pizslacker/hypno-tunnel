hypno-tunnel: hypno-tunnel.c
	gcc -Wall -Wextra -O3 -o hypno-tunnel hypno-tunnel.c -lSDL2 -lSDL2_mixer -lm
	strip hypno-tunnel

clean:
	rm -f hypno-tunnel
