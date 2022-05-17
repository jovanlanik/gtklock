# gtklock
# Copyright (c) 2022 Jovan Lanik

# Makefile

NAME := gtklock

LIBS := gtk+-3.0 gtk-layer-shell-0 wayland-client
CFLAGS += -std=c11 $(shell pkg-config --cflags $(LIBS))
LDFLAGS += $(shell pkg-config --libs $(LIBS))

ifndef NDEBUG
    NDEBUG := 0
endif

ifeq '$(NDEBUG)' '1'
    CFLAGS += -DNDEBUG -O2
    LDFLAGS += -Wl,-S
endif

SRC = $(wildcard *.c) 
OBJ = wlr-input-inhibitor-unstable-v1-client-protocol.o $(SRC:%.c=%.o)

TRASH = $(OBJ) $(NAME) $(wildcard *-client-protocol.h) $(wildcard *-client-protocol.c)

.PHONY: all clean

all: $(NAME)
clean:
	@rm $(TRASH) | true
$(NAME): wlr-input-inhibitor-unstable-v1-client-protocol.h $(OBJ)
	$(CC) $(LDFLAGS) $(OBJ) -o $@
%-client-protocol.c: %.xml
	wayland-scanner private-code $< $@
%-client-protocol.h: %.xml
	wayland-scanner client-header $< $@
