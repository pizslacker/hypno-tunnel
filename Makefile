iws: iws.c
	gcc -Wall -Wextra -O3 -o iws iws.c -lSDL2 -lSDL2_mixer -lm
	strip iws

clean:
	rm -f iws
