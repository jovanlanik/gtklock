# gtklock
# Copyright (c) 2022 Jovan Lanik

# Version

VERSION := v1.3.5
git := $(shell command -v git)
ifdef git
	tag := $(shell git describe --always --tags)
	ifeq '$(.SHELLSTATUS)' '0'
		ifneq '$(tag)' '$(VERSION)'
			sed := $(shell sed -i 's/^VERSION := $(VERSION)/VERSION := $(tag)/g' '$(lastword $(MAKEFILE_LIST))')
			VERSION := $(tag)
		endif
	endif
endif

undefine git
undefine tag
undefine sed
