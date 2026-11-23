# xDev WebGUI

Windows user interface library for the [xDev](https://github.com/xDev-Language) language, rendered in HTML by the
Windows WebView2 browser component. The program retains the logic while the page handles presentation. Windows,
menus, and controls are represented as objects such as `wgWindow()`, `wgMenu()`, `wgEdit()`, and `wgBrowse()`,
and their events run code blocks in the program.

The library is in progress: its classes and their arguments may still change.

## Key features

- Windows with a menu bar, menus with shortcuts, a sidebar with pages and a split with a line dragged by the mouse.
- Layout without pixels: a window, a column and a row place their controls; `:grow` gives a control the room left.
- Controls: label, button, edit field, check box, combo box and a table of an array.
- `wgGet()` with pictures, `VALID` and `WHEN`, as `@ ... GET` in a text screen.
- A custom HTML page opened with `Open()`, with messages sent to the program and responses returned through `Send()`.
- Unicode text in UTF-8.

## Platforms

- [x] Windows
- [ ] Linux
- [ ] macOS

## Requirements

- The WebView2 runtime, a part of Windows 10 and 11.
- `WebView2Loader.dll` of the MSYS2 package `mingw-w64-clang-x86_64-webview2-loader`, in `clang64/bin`.

## Building

`make` in the xDev sources builds `libxwebgui.a` with the other libraries. Libraries in `lib/xdev` are linked
automatically. A program with a window is built with the `--windows` option.

## Example

```xBase
PROCEDURE Main()

   LOCAL oWindow, oName, oButton, oGreeting

   oWindow := wgWindow():New( "xDev WebGUI", 480, 300 )

   // A field and a button, with the greeting beside them
   oName := wgEdit():New( oWindow, "xDev", "Your name" )
   oButton := wgButton():New( oWindow, "Greet" )
   oGreeting := wgLabel():New( oWindow, "" )
   oButton:onClick := {|| oGreeting:text := "Hello " + oName:value + "!" }

   oWindow:Run()

RETURN
```

More examples: `examples/webgui*.prg` in the xDev sources.

## License

xDev jest na licencji GNU GPL v3, patrz plik LICENSE.
