# xDev QRCODE

QR code library for the [xDev](https://github.com/xDev-Language) language. `QrEncode()` turns text into QR code modules represented as rows of `"1"` (dark) and `"0"` (light), allowing a program to draw the code on the screen, in an image, or in a PDF.

## Key features

- QR code model 2 (ISO/IEC 18004), versions 1 to 40.
- Numeric, alphanumeric or byte mode, chosen for the text; text in UTF-8.
- Error correction levels `QR_ECC_LOW`, `QR_ECC_MEDIUM`, `QR_ECC_QUARTILE` and `QR_ECC_HIGH` from `qrcode.ch`.
- The smallest version that can hold the text, or at least the version requested by the program.
- The mask with the lowest penalty, as the standard says.
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

`make` builds `libxqrcode.a`. `make install` installs the library into the xDev directory. Libraries in `lib/xdev` are linked automatically.

## Example

```xBase
#include "qrcode.ch"

PROCEDURE Main()

   LOCAL aRows := QrEncode( "https://github.com/xDev-Language", QR_ECC_MEDIUM )
   LOCAL cRow

   IF aRows == NIL
      ? "The text does not fit into a QR code"
      RETURN
   ENDIF

   // The program is responsible for the quiet zone of four light modules around the code
   FOR EACH cRow IN aRows
      ? "    " + StrTran( StrTran( cRow, "1", "██" ), "0", "  " )
   NEXT

RETURN
```

## License

xDev jest na licencji GNU GPL v3, patrz plik LICENSE.
