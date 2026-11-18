# xDev GUI

Windows user interface library for the [xDev](https://github.com/xDev-Language) language, built on the Win32 API and the common controls. A program gets windows, menus and controls as objects: `gui_Window()`, `gui_Menu()`, `gui_Button()`, `gui_Browse()` and others. Window events are methods overridden in a subclass.

## Key features

- Windows, dialogs, panels, menus with shortcuts and pictures, toolbars and status bars.
- Controls: label, edit field, memo, button, check box, radio button, group box, list box, combo box, progress bar, date picker, slider, tab control, list view and tree view.
- `gui_Get()` and `gui_Say()` with pictures, as `@ ... GET` and `@ ... SAY` in a text screen.
- `gui_Browse()` on a DBF table with the columns of `TBColumn`.
- Unicode text in UTF-8, scaling for the DPI of the monitor, fonts and colors of windows and controls.
- Timers, message boxes and file dialogs.

## Platforms

- [x] Windows
- [ ] Linux
- [ ] macOS

## Building

```
make
make install
```

`make` builds `libxgui.a`. PNG pictures are unpacked with zlib from `libxzlib.a`. `make install` installs the library into the xDev directory. Libraries in `lib/xdev` are linked automatically. A program with a window is built with the `--windows` option.

## Example

```xBase
PROCEDURE Main()

   LOCAL oWindow := gui_Window():New( "xDev GUI", 640, 400 )
   LOCAL oMenu := gui_Menu():New()
   LOCAL oFile

   gui_Label():New( oWindow, 40, 120, 560, 60, "Zażółć gęślą jaźń" )

   oFile := oMenu:AddMenu( "&File" )
   oFile:Add( "E&xit", {|| oWindow:Close() }, "Alt+F4" )
   oWindow:menu := oMenu

   gui_Application():Run( oWindow )

RETURN
```

## License

GPL-3.0. See [LICENSE](LICENSE).
