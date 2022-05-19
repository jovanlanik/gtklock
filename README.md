# gtklock
GTK-based lockscreen for Wayland.

![screenshot](https://user-images.githubusercontent.com/21199271/169087480-af61f915-7ab9-40a7-bf73-85fb2ca8586b.png)
## About
gtklock is based on gtkgreet written by Kenny Levinsen.
It uses the wlr-layer-shell and wlr-input-inhibitor Wayland protocols.
## Usage
- Lock screen: `$ gtklock`
- Lock screen and daemonize: `$ gtklock -d`
- Example style with background: `$ gtklock -s ./example-style.css`
- Show help options: `$ gtklock -h`
## Building and Installing
```
$ make
$ make install
```
## Dependencies
- gtk+3.0
- gtk-layer-shell
- wayland-client
- PAM
