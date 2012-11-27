C=gcc
SRC=openvaccine.c
CFLAGS=-W -Wall -Wextra -std=c99
	
all:
	$(CC) $(CFLAGS) $(DFLAGS) -o openvaccine $(SRC)
	
install:
	install openvaccine $(DESTDIR)/usr/bin
	
clean:
	rm -f openvaccine
	
uninstall:
	rm -f $(DESTDIR)/usr/bin/openvaccine
