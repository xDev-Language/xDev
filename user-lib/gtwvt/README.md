# xDev GTWVT

Terminal window driver (GT) for the [xDev](https://github.com/xDev-Language) language on Windows. It provides a text screen in its own window, where `DispOutAt()`, `Inkey()`, `SetColor()`, `DispBox()`, and the other screen functions work.

## Key features

- UTF-8 text on the screen, from the keyboard, and on the clipboard.
- A resizable and maximizable window, fullscreen mode with Alt+Enter, and scaling for the monitor DPI.
- Font, size, weight and quality chosen by the program; box characters fill the cells.
- Mouse, a blinking cursor, window title and icon, message box and file dialog.
- Settings and queries through `GtInfo( GTI_* )`.

## Platforms

- [x] Windows
- [ ] Linux (X11)
- [ ] Linux (Wayland)
- [ ] macOS

## Building

```
make
make install
```

`make` builds `libxgtwvt.a`. `make install` installs the library into the xDev directory. Libraries in `lib/xdev` are linked automatically.

## Example

```xBase
REQUEST GTWVT

PROCEDURE Main()

   SetColor( "W+/B" )
   Scroll()
   DispOutAt( 10, 20, "Zażółć gęślą jaźń – press any key" )
   Inkey( 0 )

RETURN
```

## License

xDev jest na licencji GNU GPL v3, patrz plik LICENSE.
