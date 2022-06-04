# gtklock
GTK-based lockscreen for Wayland.

![screenshot](https://user-images.githubusercontent.com/21199271/169707623-2ac5f02b-b6ed-461a-b9a3-5d96440843a2.png)
## About
gtklock is a lockscreen based on [gtkgreet](https://git.sr.ht/~kennylevinsen/gtkgreet).
It uses the wlr-layer-shell and wlr-input-inhibitor Wayland protocols.
Works on sway and other wlroots-based compositors.

gtklock also has basic module support, check out the [example-module](https://github.com/jovanlanik/gtklock-example-module).

Available on these repositories:

[![Packaging status](https://repology.org/badge/vertical-allrepos/gtklock.svg)](https://repology.org/project/gtklock/versions)
## Usage
- Lock screen: `$ gtklock`
- Lock screen and daemonize: `$ gtklock -d`
- Example style with background: `$ gtklock -s ./assets/example-style.css`
- Lock and load a module: `$ gtklock -m /path/to/module.so`
- Show help options: `$ gtklock -h`
## Building from source
```
$ make
# make install
```
### Dependencies
- GNU Make (build-time)
- pkg-config (build-time)
- PAM
- wayland-client
- gtk+3.0
- gtk-layer-shell
