gifresize: gifresize.c
	$(CC) -o gifresize -O2 -I.. -I../ext/libnsgif/include gifresize.c ../cgif.c ../ext/libnsgif/src/libnsgif.c ../ext/libnsgif/src/lzw.c

.PHONY: clean

clean:
	rm -f gifresize
