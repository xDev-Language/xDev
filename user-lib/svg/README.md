# xDev SVG

A library for writing SVG pictures from programs in the [xDev](https://github.com/xDev-Language) language, written in xDev itself. A program opens a file with `svg_Init()`, draws into it and ends it with `svg_Close()`.

## Key features

- Lines, rectangles and circles, filled or as an outline: `svg_Line()`, `svg_FilledRect()`, `svg_FilledCircle()`, `svg_Circle()`.
- Filled polygons through any points: `svg_FilledPolygon()`.
- Text with a font, size, weight, color and alignment: `svg_Text()`.
- Comments in the file: `svg_Comment()`.
- Colors as SVG writes them, e.g. `"#264D73"` or `"red"`; text in UTF-8, escaped for XML.
- The `FONT_WEIGHT_*` and `SVG_ALIGN_*` constants in `svg.ch`.
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

`make` builds `libxsvg.a` from `svg.prg` with xDev. `make install` installs the library into the xDev directory. Libraries in `lib/xdev` are linked automatically.

## Example

```xBase
#include "svg.ch"

PROCEDURE Main()

   LOCAL nSvg := svg_Init( "example.svg", 400, 200 )

   IF nSvg == 0
      ? "The file cannot be created"
      RETURN
   ENDIF

   svg_FilledRect( nSvg, 0, 0, 400, 200, "#F2F2F2" )
   svg_Circle( nSvg, 100, 100, 60, 4, "#264D73" )
   svg_Line( nSvg, 200, 40, 360, 160, 2, "black" )
   svg_Text( nSvg, 200, 190, "Zażółć gęślą jaźń", "Segoe UI", 16, FONT_WEIGHT_BOLD, "#1A3550", SVG_ALIGN_CENTER )
   svg_Close( nSvg )

RETURN
```

## License

GPL-3.0. See [LICENSE](LICENSE).
