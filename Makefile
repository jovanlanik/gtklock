# gtklock
# Copyright (c) 2022 Jovan Lanik

# Makefile

NAME := gtklock
MAJOR_VERSION := 2
MINOR_VERSION := 0
MICRO_VERSION := 0

PREFIX = /usr/local
SYSCONFDIR = /etc
ifeq ($(shell uname), FreeBSD)
	SYSCONFDIR = $(PREFIX)/etc
endif
INSTALL = install

LIBS := pam wayland-client gtk+-wayland-3.0 gtk-layer-shell-0 gmodule-export-2.0
ifeq ($(shell uname), FreeBSD)
	LIBS := $(shell echo $(LIBS) | cut -d' ' -f 2-)
endif
CFLAGS += -std=c11 -Iinclude -DPREFIX=$(PREFIX) $(shell pkg-config --cflags $(LIBS))
CFLAGS += -DMAJOR_VERSION=$(MAJOR_VERSION) -DMINOR_VERSION=$(MINOR_VERSION) -DMICRO_VERSION=$(MICRO_VERSION)
LDLIBS += -Wl,--export-dynamic $(shell pkg-config --libs $(LIBS))
ifeq ($(shell uname), FreeBSD)
	LDLIBS += -lpam
endif

OBJ = wlr-input-inhibitor-unstable-v1-client-protocol.o
OBJ += $(patsubst %.c, %.o, $(wildcard src/*.c))
OBJ += $(patsubst res/%.gresource.xml, %.gresource.o, $(wildcard res/*.gresource.xml))

TRASH = $(OBJ) $(NAME) $(NAME).1
TRASH += $(wildcard *.gresource.c) $(wildcard *.gresource.h)
TRASH += $(wildcard *-client-protocol.c) $(wildcard include/*-client-protocol.h)

VPATH = src
.PHONY: all clean install install-bin install-data uninstall

all: $(NAME) $(NAME).1

clean:
	@rm $(TRASH) | true

install-bin:
	$(INSTALL) -d $(DESTDIR)$(PREFIX)/bin
	$(INSTALL) $(NAME) $(DESTDIR)$(PREFIX)/bin/$(NAME)

install-data:
	$(INSTALL) -d $(DESTDIR)$(SYSCONFDIR)/pam.d
	$(INSTALL) -m644 pam/$(NAME) $(DESTDIR)$(SYSCONFDIR)/pam.d/$(NAME)
	$(INSTALL) -d $(DESTDIR)$(PREFIX)/share/man/man1
	$(INSTALL) -m644 $(NAME).1 $(DESTDIR)$(PREFIX)/share/man/man1/$(NAME).1

install: install-bin install-data

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(NAME)
	rm -f $(DESTDIR)$(SYSCONFDIR)/pam.d/$(NAME)
	rm -r $(DESTDIR)$(PREFIX)/share/man/man1/$(NAME).1

$(NAME): $(OBJ)
	$(LINK.c) $^ $(LDLIBS) -o $@

%.gresource.c: res/%.gresource.xml
	glib-compile-resources --generate-source $< --target=$@ --sourcedir=res

%.gresource.h: res/%.gresource.xml
	glib-compile-resources --generate-header $< --target=$@ --sourcedir=res

%-client-protocol.c: wayland/%.xml
	wayland-scanner private-code $< $@

include/%-client-protocol.h: wayland/%.xml
	wayland-scanner client-header $< $@

src/input-inhibitor.c: include/wlr-input-inhibitor-unstable-v1-client-protocol.h 

%.1: man/%.1.scd
	scdoc < $< > $@
