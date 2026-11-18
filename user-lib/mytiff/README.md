# xDev MyTiff

A small widget toolkit in the style of Motif for the [xDev](https://github.com/xDev-Language) language, written in xDev itself. Windows with the frame of the Motif window manager, labels and push buttons are drawn with the graphics of the GT driver. The names follow Motif with `Mt` for `Xm`: `MtCreateShell()`, `MtCreatePushButton()`, `MtAddCallback()`, `MtAppMainLoop()`.

## Key features

- Shells: windows with a title bar, a menu button, a maximize button and a frame to resize them; they move with the mouse and can be docked to the whole GT window.
- Labels and push buttons with Motif shadows, the focus highlight and Tab between them.
- Resources set and read as pairs, `{ { MtNlabelString, "Quit" }, ... }`, with `MtSetValues()` and `MtGetValues()`.
- Callbacks as code blocks: activate, arm, disarm and destroy.
- Colors derived from a background as in Motif, `MtGetColors()`.
- An event loop of its own (`MtAppMainLoop()`) or one event at a time (`MtAppProcessEvent()`).

## Platforms

- [x] Windows (GTWVT)
- [x] Linux (GTXWC)
- [ ] macOS

## Building

```
make
make install
```

`make` builds `libxmytiff.a` from the `.prg` files with xDev. `make install` installs the library into the xDev directory. Libraries in `lib/xdev` are linked automatically. A program needs a GT driver with graphics, e.g. `REQUEST GTWVT` or `REQUEST GTXWC`.

## Example

```xBase
#include "mytiff.ch"

REQUEST GTWVT

PROCEDURE Main()

   LOCAL app, shell, button

   app := MtAppInitialize( "Hello" )
   shell := MtCreateShell( app, "main", { { MtNtitle, "Zażółć gęślą jaźń" }, { MtNwidth, 360 }, { MtNheight, 180 } } )
   button := MtCreatePushButton( shell, "quit", { { MtNlabelString, "Quit" }, { MtNx, 20 }, { MtNy, 60 } } )

   MtAddCallback( button, MtNactivateCallback, {| w, aCallData | MtAppQuit( app ) } )
   MtRealizeWidget( shell )
   MtAppMainLoop( app )

RETURN
```

## License

GPL-3.0. See [LICENSE](LICENSE).
