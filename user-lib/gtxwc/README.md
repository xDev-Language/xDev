# xDev GTXWC

Terminal window driver (GT) for the [xDev](https://github.com/xDev-Language) language on Linux, built directly on Xlib (X11). It provides a text screen in its own window, where `DispOutAt()`, `Inkey()`, `SetColor()`, `DispBox()`, and the other screen functions work.

## Key features

- UTF-8 text on the screen, from the keyboard, and on the clipboard.
- A resizable and maximizable window, fullscreen mode with Alt+Enter, and scaling for the screen DPI.
- The X11 Unicode font "fixed" 9x18 (package `xfonts-base`).
- Mouse, a blinking cursor, window title and icon, images, sound, message box and file dialog.
- Settings and queries through `GtInfo( GTI_* )`.

## Platforms

- [ ] Windows
- [x] Linux (X11)
- [x] Linux (Wayland, through XWayland)
- [ ] macOS

## Building

```
make
make install
```

`make` builds `libxgtxwc.a`. Xlib comes from the system (`pkg-config x11`, package `libx11-dev`). `make install` installs the library into the xDev directory. Libraries in `lib/xdev` are linked automatically.

The file dialog uses `zenity`, sound uses `paplay` or `aplay`, links open with `xdg-open`, and the system theme is read with `gdbus` when these programs are installed.

## Example

```xBase
REQUEST GTXWC

PROCEDURE Main()

   SetColor( "W+/B" )
   Scroll()
   DispOutAt( 10, 20, "Zażółć gęślą jaźń – press any key" )
   Inkey( 0 )

RETURN
```

## License

xDev jest na licencji GNU GPL v3, patrz plik LICENSE.
