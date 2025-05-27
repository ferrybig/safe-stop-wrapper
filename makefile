CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c99 -D_POSIX_C_SOURCE=200809L
LDFLAGS =
TARGET = safe-stop-on-signal
SRCFILES = safe-stop-on-signal.c

.PHONY: all clean install uninstall debug static

all: $(TARGET)

$(TARGET)-debug: $(SRCFILES)
	$(CC) $(CFLAGS) -g $(LDFLAGS) -o $@ $^
$(TARGET): $(SRCFILES)
	$(CC) $(CFLAGS) -Oz -fno-ident -fno-asynchronous-unwind-tables $(LDFLAGS) -o $@ $^
	strip -s $@
	upx $@

clean:
	rm -f $(TARGET) $(TARGET)-debug $(TARGET)-static *.o

install: $(TARGET)
	install -D $(TARGET) $(DESTDIR)/usr/local/bin/$(TARGET)

uninstall:
	rm -f $(DESTDIR)/usr/local/bin/$(TARGET)

debug: $(TARGET)-debug

static: $(TARGET)-static
