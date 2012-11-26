C=gcc
SRC=src/openvaccine.c
CFLAGS=-W -Wall -Wextra -g
	
all:
	$(CC) $(CFLAGS) $(DFLAGS) -o openvaccine $(SRC)
	
install:
	install openvaccine $(DESTDIR)/usr/bin
	gzip -c -9 openvaccine.1 > $(DESTDIR)/usr/share/man/man1/openvacine.1.gz
	
clean:
	rm -f openvaccine
	
uninstall:
	rm -f $(DESTDIR)/usr/bin/openvaccine
	rm -f $(DESTDIR)/usr/share/man/man1/openvaccine.1.gz
