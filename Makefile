all:
	gcc -Wall -Werror wish.c -fsanitize=address -o wish

debug:
	gcc -Wall -Werror wish.c -fsanitize=address  -DDEBUG -o wish -g
clean:
	rm wish
	rm -rf tests-out/
