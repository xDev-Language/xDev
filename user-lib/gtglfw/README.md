# xDev GTGLFW

Terminal window driver (GT) for the [xDev](https://github.com/xDev-Language) language on Windows and Linux, built on [GLFW](https://www.glfw.org) and OpenGL. It provides a text screen in its own window, where `DispOutAt()`, `Inkey()`, `SetColor()`, `DispBox()`, and the other screen functions work.

## Key features

- UTF-8 text on the screen, from the keyboard, and on the clipboard.
- A resizable and maximizable window, fullscreen mode with Alt+Enter, and scaling for the monitor DPI.
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

xDev jest na licencji GNU GPL v3, patrz plik LICENSE.
