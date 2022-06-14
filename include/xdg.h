#pragma once

/*
 * resolve_xdg_style_path will attempt to discover a style.css using the XDG
 * standard directories 
 *
 * If a style path is discovered the returned pointer is non-nil and points to 
 * a string containing the path to style.css
 *
 * If not path is found a nil is returned.
 */
char *resolve_xdg_style_path();

