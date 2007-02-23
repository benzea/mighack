mighack.so: mighack.c
	cc -g3 -fPIC -shared -Wall  `pkg-config --libs --cflags gtk+-2.0` mighack.c -o mighack.so

clean:
	rm -f mighack.so