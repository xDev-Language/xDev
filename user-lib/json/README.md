# xDev JSON

JSON library for the [xDev](https://github.com/xDev-Language) language. `json_Encode()` turns a value into JSON text, `json_Decode()` turns JSON text into a value, and `json_Error()` reports what went wrong.

## Key features

- A hash becomes an object and an array remains an array; an object retains its key order when read back.
- A whole number within range is read as an integer and other numbers as doubles; numbers are written in their shortest exact form.
- A date is written as `"YYYY-MM-DD"`, NIL as `null`.
- Text in UTF-8; `\u` escapes and surrogate pairs are read, a byte order mark is skipped.
- A compact or a pretty form with new lines and indents.
- Excessive nesting or a cycle returns NIL and a message from `json_Error()` instead of causing a crash.
- No other libraries are needed.

## Platforms

- [x] Windows
- [x] Linux
- [ ] macOS

## Building

```
make
make install
```

`make` builds `libxjson.a`. `make install` installs the library into the xDev directory. Libraries in `lib/xdev` are linked automatically.

## Example

```xBase
PROCEDURE Main()

   LOCAL hData := { "name" => "Zażółć gęślą jaźń", "items" => { 1, 2.5, .T., NIL } }
   LOCAL cJson := json_Encode( hData, .T. )
   LOCAL xBack

   ? cJson

   xBack := json_Decode( cJson )
   IF xBack == NIL
      ? json_Error()
   ELSE
      ? xBack[ "name" ]
   ENDIF

RETURN
```

## License

xDev jest na licencji GNU GPL v3, patrz plik LICENSE.
