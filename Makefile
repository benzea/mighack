mighack.so: mighack.c
	cc -fPIC -shared -Wall  `pkg-config --libs --cflags gtk+-2.0` mighack.c -o mighack.so
