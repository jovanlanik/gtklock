# gtklock
# Copyright (c) 2022 Jovan Lanik

# Version

VERSION := v1.3.8
command := $(shell command -v git)
ifdef command
	git := $(shell git rev-parse --is-inside-work-tree 2>&1 >/dev/null)
	ifeq '$(.SHELLSTATUS)' '0'
		VERSION := $(shell git describe --tags --always)
	endif
endif

undefine command
undefine git
