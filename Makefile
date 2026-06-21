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
	chmod 755 $(PREFIX)/bin/$(TARGET)
	@echo "installed in $(PREFIX)/bin/$(TARGET)"

uninstall:
	rm -f $(PREFIX)/bin/$(TARGET)
	@echo "deleted from $(PREFIX)/bin/$(TARGET)"

clean:
	rm -f $(TARGET)