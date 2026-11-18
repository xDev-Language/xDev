# xDev GTWVT

Terminal window driver (GT) for Windows for the [xDev](https://github.com/xDev-Language) language. A program gets a text screen in its own window: `DispOutAt()`, `Inkey()`, `SetColor()`, `DispBox()` and the other screen functions work in it.

## Key features

- Unicode text in UTF-8 on screen, from the keyboard and on the clipboard.
- Window that can be resized and maximized, full screen with Alt+Enter, scaling for the DPI of the monitor.
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

GPL-3.0. See [LICENSE](LICENSE).
