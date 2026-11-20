# xDev WebGUI

Windows user interface library for the [xDev](https://github.com/xDev-Language) language, drawn in HTML by the
WebView2 browser component of Windows. The program keeps the logic, the page only draws: a program gets windows,
menus and controls as objects, `wg_Window()`, `wg_Menu()`, `wg_Edit()`, `wg_Browse()` and others, and their events
run code blocks of the program.

The library is in progress: its classes and their arguments may still change.

## Key features

- Windows with a menu bar, menus with shortcuts, a sidebar with pages and a split with a line dragged by the mouse.
- Layout without pixels: a window, a column and a row place their controls; `:grow` gives a control the room left.
- Controls: label, button, edit field, check box, combo box and a table of an array.
- `wg_Get()` with pictures, `VALID` and `WHEN`, as `@ ... GET` in a text screen.
- An own HTML page with `Open()`, its messages to the program and the answers of the program with `Send()`.
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

   oWindow := wg_Window():New( "xDev WebGUI", 480, 300 )

   // A field and a button with the greeting beside it
   oName := wg_Edit():New( oWindow, "xDev", "Your name" )
   oButton := wg_Button():New( oWindow, "Greet" )
   oGreeting := wg_Label():New( oWindow, "" )
   oButton:onClick := {|| oGreeting:text := "Hello " + oName:value + "!" }

   oWindow:Run()

RETURN
```

More examples: `examples/webgui*.prg` in the xDev sources.

## License

GPL-3.0. See [LICENSE](LICENSE).
