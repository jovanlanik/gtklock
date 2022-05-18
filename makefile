# gtklock
# Copyright (c) 2022 Jovan Lanik

# Makefile

NAME := gtklock
PREFIX ?= /usr/local

INSTALL ?= install

LIBS := pam gtk+-3.0 gtk-layer-shell-0 wayland-client
CFLAGS += -std=c11 $(shell pkg-config --cflags $(LIBS))
LDLIBS += $(shell pkg-config --libs $(LIBS))

SRC = $(wildcard *.c) 
OBJ = wlr-input-inhibitor-unstable-v1-client-protocol.o $(SRC:%.c=%.o)

TRASH = $(OBJ) $(NAME) $(wildcard *-client-protocol.h) $(wildcard *-client-protocol.c)

.PHONY: all clean install uninstall

all: $(NAME)

clean:
	@rm $(TRASH) | true

install:
	$(INSTALL) -d $(DESTDIR)$(PREFIX)/bin
	$(INSTALL) -d $(DESTDIR)/etc/pam.d
	$(INSTALL) $(NAME) $(DESTDIR)$(PREFIX)/bin/$(NAME)
	$(INSTALL) -m644 $(NAME).pam $(DESTDIR)/etc/pam.d/$(NAME)

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(NAME)
	rm -f $(DESTDIR)/etc/pam.d/$(NAME)

$(NAME): wlr-input-inhibitor-unstable-v1-client-protocol.h $(OBJ)

%-client-protocol.c: %.xml
	wayland-scanner private-code $< $@

%-client-protocol.h: %.xml
	wayland-scanner client-header $< $@
