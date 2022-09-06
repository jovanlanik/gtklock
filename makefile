# gtklock
# Copyright (c) 2022 Jovan Lanik

# Makefile

NAME := gtklock

PREFIX ?= /usr/local
INSTALL ?= install

include util/version.mk

LIBS := pam wayland-client gtk+-wayland-3.0 gtk-layer-shell-0 gmodule-no-export-2.0
CFLAGS += -std=c11 -DVERSION=$(VERSION) -DPREFIX=$(PREFIX) -Iinclude $(shell pkg-config --cflags $(LIBS))
LDLIBS += -Wl,--export-dynamic $(shell pkg-config --libs $(LIBS))

SRC = $(wildcard src/*.c) 
OBJ = wlr-input-inhibitor-unstable-v1-client-protocol.o $(SRC:src/%.c=%.o)

TRASH = $(OBJ) $(NAME) $(NAME).1 $(wildcard *-client-protocol.c) $(wildcard include/*-client-protocol.h)

VPATH = src
.PHONY: all clean install install-bin install-data uninstall

all: $(NAME) $(NAME).1

clean:
	@rm $(TRASH) | true

install-bin:
	$(INSTALL) -d $(DESTDIR)$(PREFIX)/bin
	$(INSTALL) $(NAME) $(DESTDIR)$(PREFIX)/bin/$(NAME)

install-data:
	$(INSTALL) -d $(DESTDIR)/etc/pam.d
	$(INSTALL) -m644 pam/$(NAME) $(DESTDIR)/etc/pam.d/$(NAME)
	$(INSTALL) -d $(DESTDIR)$(PREFIX)/share/man/man1
	$(INSTALL) -m644 $(NAME).1 $(DESTDIR)$(PREFIX)/share/man/man1/$(NAME).1

install: install-bin install-data

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(NAME)
	rm -f $(DESTDIR)/etc/pam.d/$(NAME)
	rm -r $(DESTDIR)$(PREFIX)/share/man/man1/$(NAME).1

$(NAME): $(OBJ)

%-client-protocol.c: wayland/%.xml
	wayland-scanner private-code $< $@

include/%-client-protocol.h: wayland/%.xml
	wayland-scanner client-header $< $@

src/input-inhibitor.c: include/wlr-input-inhibitor-unstable-v1-client-protocol.h 

%.1: man/%.1.scd
	scdoc < $< > $@
