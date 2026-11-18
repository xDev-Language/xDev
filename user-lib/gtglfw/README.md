# xDev GTGLFW

Terminal window driver (GT) for Windows and Linux for the [xDev](https://github.com/xDev-Language) language, built on [GLFW](https://www.glfw.org) and OpenGL. A program gets a text screen in its own window: `DispOutAt()`, `Inkey()`, `SetColor()`, `DispBox()` and the other screen functions work in it.

## Key features

- Unicode text in UTF-8 on screen, from the keyboard and on the clipboard.
- Window that can be resized and maximized, full screen with Alt+Enter, scaling for the DPI of the monitor.
- Built-in 9x18 bitmap font, so no font files are needed.
- Mouse, a blinking cursor, window title and icon, images, message box and file dialog.
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

`make` builds `libxgtglfw.a` with the static GLFW library inside. GLFW comes from the system (`pkg-config glfw3`). `make install` installs the library into the xDev directory. Libraries in `lib/xdev` are linked automatically.

## Example

```xBase
REQUEST GTGLFW

PROCEDURE Main()

   SetColor( "W+/B" )
   Scroll()
   DispOutAt( 10, 20, "Zażółć gęślą jaźń – press any key" )
   Inkey( 0 )

RETURN
```

## License

GPL-3.0. See [LICENSE](LICENSE). GLFW has its own zlib/libpng license.
