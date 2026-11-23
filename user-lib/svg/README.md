# xDev SVG

A library for creating SVG images from programs in the [xDev](https://github.com/xDev-Language) language, written in xDev itself. A program opens a file with `svgInit()`, draws into it, and closes it with `svgClose()`.

## Key features

- Drawing in custom coordinates: `svgInit( cFilename, nWidth, nHeight, [nViewSize] )` with `nViewSize` uses coordinates from 0 to `nViewSize`, scaled to the image. For example, an icon drawn at 24 x 24 can be written at 512 x 512.
- A pen for outlines and lines: `svgPen( nSvg, nStrokeWidth, cColor, [lRound] )` sets the width, the color and round line ends and corners for everything drawn after it, until the next `svgPen()`. Omit the last `cColor` argument of an outline, or pass `NIL`, to use the pen color. A color passed there applies only to that object and does not change the pen. A new file starts with a pen 1 wide, black, with flat ends. Filled shapes and text take their color as a parameter and do not use the pen.
- Lines: `svgLine()`.
- Rectangles and circles, filled or as an outline: `svgFilledRect()`, `svgRect()`, `svgFilledCircle()`, `svgCircle()`. Rectangles can have rounded corners.
- Ellipses, filled or as an outline: `svgFilledEllipse()`, `svgEllipse()`.
- Polygons through any points, filled or as an outline: `svgFilledPolygon()`, `svgPolygon()`. Both can have rounded corners.
- Arcs and slices of a circle between two angles: `svgArc()`, `svgFilledPie()`, `svgPie()`. Angles in degrees as in SVG: 0 points right, 90 down.
- Arcs and curves, filled or as an outline, with `svgFilledPath()` and `svgPath()`, from SVG path data: `M x y` move, `L x y` line, `A rx ry rotation large sweep x y` arc, `Q` and `C` curves, `Z` close.
- An outline lies on the edge of the shape, half of its width inside and half outside, as in SVG.
- `"currentColor"` as a color takes the color of the surrounding text, e.g. for icons on a web page.
- Text with a font, size, weight, color and alignment: `svgText()`.
- Comments in the file: `svgComment()`.
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

   LOCAL nSvg := svgInit( "example.svg", 400, 200 )

   IF nSvg == 0
      ? "The file cannot be created"
      RETURN
   ENDIF

   svgFilledRect( nSvg, 0, 0, 400, 200, "#F2F2F2" )
   svgPen( nSvg, 4, "#264D73" )
   svgCircle( nSvg, 100, 100, 60 )
   svgPen( nSvg, 2, "black", .T. )
   svgLine( nSvg, 200, 40, 360, 160 )
   svgText( nSvg, 200, 190, "Zażółć gęślą jaźń", "Segoe UI", 16, FONT_WEIGHT_BOLD, "#1A3550", SVG_ALIGN_CENTER )
   svgClose( nSvg )

RETURN
```

## License

xDev jest na licencji GNU GPL v3, patrz plik LICENSE.
