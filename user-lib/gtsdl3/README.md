# xDev GTSDL3

Terminal window driver (GT) for the [xDev](https://github.com/xDev-Language) language, built on [SDL3](https://www.libsdl.org). A program gets a text screen in its own window: `DispOutAt()`, `Inkey()`, `SetColor()`, `DispBox()` and the other screen functions work in it.

## Key features

- Unicode text in UTF-8 on screen, from the keyboard and on the clipboard.
- Window that can be resized and maximized, full screen with Alt+Enter, scaling for the DPI of the monitor.
- Built-in 9x18 bitmap font, so no font files are needed.
- Mouse, a blinking cursor, window title and icon, images, sound, message box and file dialog.
- Settings and queries through `GtInfo( GTI_* )`.

## Platforms

- [x] Windows
- [x] Linux (X11)
- [x] Linux (Wayland)
- [ ] macOS

## Building

```
make
make install
```

`make` builds `libxgtsdl3.a` with the static SDL3 library inside. SDL3 comes from the system (`pkg-config sdl3`). `make install` installs the library into the xDev directory. Libraries in `lib/xdev` are linked automatically.

## Example

```xBase
REQUEST GTSDL3

PROCEDURE Main()

   SetColor( "W+/B" )
   Scroll()
   DispOutAt( 10, 20, "Zażółć gęślą jaźń – press any key" )
   Inkey( 0 )

RETURN
```

## License

GPL-3.0. See [LICENSE](LICENSE). SDL3 has its own zlib license.
