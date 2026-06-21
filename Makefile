TARGET = flipfetch
CC = gcc
CFLAGS = -Wall -O2 -std=c99
PREFIX = /usr/local

.PHONY: all clean install uninstall

all: $(TARGET)

$(TARGET): flipfetch.c
	$(CC) $(CFLAGS) -o $(TARGET) flipfetch.c

install: $(TARGET)
	mkdir -p $(PREFIX)/bin
	cp $(TARGET) $(PREFIX)/bin/$(TARGET)
	cp flogo.txt $(PREFIX)/bin/flogo.txt
	chmod 755 $(PREFIX)/bin/$(TARGET)
	@echo "installed to $(PREFIX)/bin/"

uninstall:
	rm -f $(PREFIX)/bin/$(TARGET)
	rm -f $(PREFIX)/bin/flogo.txt
	@echo "removed"

clean:
	rm -f $(TARGET)
