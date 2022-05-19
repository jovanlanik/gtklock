# gtklock
GTK-based lockscreen for Wayland.

![screenshot](https://user-images.githubusercontent.com/21199271/169087480-af61f915-7ab9-40a7-bf73-85fb2ca8586b.png)
## About
gtklock is based on [gtkgreet](https://git.sr.ht/~kennylevinsen/gtkgreet).
It uses the wlr-layer-shell and wlr-input-inhibitor Wayland protocols
and works and sway and other wlroots-based compositors.

Available on these repositories:

[![Packaging status](https://repology.org/badge/vertical-allrepos/gtklock.svg)](https://repology.org/project/gtklock/versions)
## Usage
- Lock screen: `$ gtklock`
- Lock screen and daemonize: `$ gtklock -d`
- Example style with background: `$ gtklock -s ./assets/example-style.css`
- Show help options: `$ gtklock -h`
## Building from source
```
$ make
$ make install
```
### Dependencies
- gtk+3.0
- gtk-layer-shell
- wayland-client
- PAM
