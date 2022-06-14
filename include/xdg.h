#pragma once

/* resolve_xdg_style_path will attempt to discover a style.css using the XDG
   standard directories 

   if a style path is discovered the returned pointer is non-nil and points to 
   a string containing the path to style.css

   if not path is found a nil is returned.
*/
char* resolve_xdg_style_path();
