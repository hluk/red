# install prefix
prefix = /usr/local
# required packages for pkg-config
PKGS = glib-2.0 gdk-pixbuf-2.0 libnotify
# binary filename
EXE = red
# default flags
CFLAGS = -O2 -std=c99
CFLAGS += -O2 -std=c99 -Wextra -Wall -pedantic $(shell $(PKGCONFIG) --cflags $(PKGS))
LDFLAGS += $(shell $(PKGCONFIG) --libs $(PKGS))
# tools
CC = gcc
PKGCONFIG = pkg-config

all: $(EXE)

$(EXE): main.c
	$(CC) $< -o $@ $(CFLAGS) $(LDFLAGS)

clean:
	$(RM) $(EXE)

install:
	echo sudo cp $(EXE) $(prefix)/bin/

