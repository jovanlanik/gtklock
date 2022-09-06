# gtklock
# Copyright (c) 2022 Jovan Lanik

# Version

VERSION := v1.3.5
command := $(shell command -v git)
ifdef command
	git := $(shell git rev-parse --is-inside-work-tree 2>&1 >/dev/null)
	ifeq '$(.SHELLSTATUS)' '0'
		tag := $(shell git describe --tags --exact-match 2>/dev/null)
		ifeq '$(.SHELLSTATUS)' '0'
			ifneq '$(tag)' '$(VERSION)'
				sed := $(shell sed -i 's/^VERSION := $(VERSION)/VERSION := $(tag)/g' '$(lastword $(MAKEFILE_LIST))')
			endif
		else
			tag := $(shell git describe --tags --always)
		endif
		VERSION := $(tag)
	endif
endif

undefine command
undefine git
undefine tag
undefine sed
