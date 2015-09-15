libsortdir.so: libsortdir.c
	gcc -fPIC -Wall -shared -ldl -o libsortdir.so libsortdir.c

clean:
	rm -f libsortdir.so

