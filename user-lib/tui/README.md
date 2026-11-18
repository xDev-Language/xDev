# xDev TUI

Text user interface for the [xDev](https://github.com/xDev-Language) language, written in xDev itself: the GET system and `TBrowse`, `DBEdit()`, a text editor, a function key bar and a sidebar menu. It works in a text screen of any GT driver.

## Key features

- `Get` and `ReadModal()` as READ: pictures, `VALID` and `WHEN` blocks, the insert mode, `ReadVar()`, `Updated()` and `ReadKill()`. Text in UTF-8.
- A Get also works without READ: `Get():New()`, `:SetFocus()` and `GetApplyKey()` in the loop of the program.
- `TBrowse` and `TBColumn` with frozen columns, headings, footings, separators, colors and the mouse.
- `DBEdit()` on `TBrowse` for the records of the current work area.
- `TEdit`: a text editor in a screen rectangle with a status line.
- `TKeyBar`: captions of F1 - F10 on the last screen row, with the mouse.
- `TSidebar`: a menu at a side of the screen with groups and submenus.
- The constants in `tui.ch`, `button.ch`, `getexit.ch`, `tbrowse.ch` and `dbedit.ch`.

## Platforms

- [x] Windows
- [x] Linux
- [ ] macOS

## Building

```
make
make install
```

`make` builds `libxtui.a` from the `.prg` files with xDev. `make install` installs the library and its `.ch` files into the xDev directory. Libraries in `lib/xdev` are linked automatically.

## Example

```xBase
#include "inkey.ch"
#include "tui.ch"

PROCEDURE Main()

   LOCAL GetList := {}
   LOCAL cName   := UPadR( "Zażółć gęślą jaźń", 20 )
   LOCAL nAmount := 1234.5
   LOCAL dDate   := Date()

   Scroll()
   DispOutAt( 2, 2, "Name" )
   DispOutAt( 3, 2, "Amount" )
   DispOutAt( 4, 2, "Date" )
   GetAdd( GetList, 2, 12, @cName, , {|| ! Empty( cName ) } )
   GetAdd( GetList, 3, 12, @nAmount, "999,999.99" )
   GetAdd( GetList, 4, 12, @dDate )

   IF ReadModal( GetList )
      DispOutAt( 6, 2, "Saved: " + AllTrim( cName ) )
   ENDIF

   Inkey( 0 )

RETURN
```

## License

GPL-3.0. See [LICENSE](LICENSE).
