libsortdir.so: libsortdir.c
	gcc -Wall -shared -ldl -o libsortdir.so libsortdir.c

clean:
	rm -f libsortdir.so

