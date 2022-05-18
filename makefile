# gtklock
# Copyright (c) 2022 Jovan Lanik

# Makefile

NAME := gtklock
PREFIX ?= /usr/local

LIBS := pam gtk+-3.0 gtk-layer-shell-0 wayland-client
CFLAGS += -std=c11 $(shell pkg-config --cflags $(LIBS))
LDFLAGS += $(shell pkg-config --libs $(LIBS))

SRC = $(wildcard *.c) 
OBJ = wlr-input-inhibitor-unstable-v1-client-protocol.o $(SRC:%.c=%.o)

TRASH = $(OBJ) $(NAME) $(wildcard *-client-protocol.h) $(wildcard *-client-protocol.c)

.PHONY: all clean install uninstall

all: $(NAME)
clean:
	@rm $(TRASH) | true
install:
	install $(NAME) $(DESTDIR)$(PREFIX)/bin/$(NAME)
	install -m 644 $(NAME).pam $(DESTDIR)/etc/pam.d/$(NAME)
uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(NAME)
	rm -f $(DESTDIR)/etc/pam.d/$(NAME)
$(NAME): wlr-input-inhibitor-unstable-v1-client-protocol.h $(OBJ)
	$(CC) $(LDFLAGS) $(OBJ) -o $@
%-client-protocol.c: %.xml
	wayland-scanner private-code $< $@
%-client-protocol.h: %.xml
	wayland-scanner client-header $< $@
