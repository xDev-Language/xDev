# xDev GUI

Windows user interface library for the [xDev](https://github.com/xDev-Language) language, built on the Win32 API and common controls. It provides windows, menus, and controls as objects: `guiWindow()`, `guiMenu()`, `guiButton()`, `guiBrowse()`, and others. Window events are handled by methods overridden in a subclass.

## Key features

- Windows, dialogs, panels, menus with shortcuts and pictures, toolbars and status bars.
- Controls: label, edit field, memo, button, check box, radio button, group box, list box, combo box, progress bar, date picker, slider, tab control, list view and tree view.
- `guiGet()` and `guiSay()` with pictures, as `@ ... GET` and `@ ... SAY` in a text screen.
- `guiBrowse()` on a DBF table with the columns of `TBColumn`.
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

After a library change, rebuild only the library, including `libxgui.dll` for `--dynamic` and `--shared`:

```
cd ~/xDev-src/user-lib/gui
make dynamic
```

## Example

```xBase
PROCEDURE Main()

   LOCAL oWindow := guiWindow():New( "xDev GUI", 640, 400 )
   LOCAL oMenu := guiMenu():New()
   LOCAL oFile

   guiLabel():New( oWindow, 40, 120, 560, 60, "Zażółć gęślą jaźń" )

   oFile := oMenu:AddMenu( "&File" )
   oFile:Add( "E&xit", {|| oWindow:Close() }, "Alt+F4" )
   oWindow:menu := oMenu

   guiApplication():Run( oWindow )

RETURN
```

## License

xDev jest na licencji GNU GPL v3, patrz plik LICENSE.
