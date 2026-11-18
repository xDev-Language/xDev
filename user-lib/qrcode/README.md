# xDev QRCODE

QR code library for the [xDev](https://github.com/xDev-Language) language. `QrEncode()` turns a text into the modules of a QR code, as rows of `"1"` (dark) and `"0"` (light), so a program can draw it on the screen, in a picture or in a PDF.

## Key features

- QR code model 2 (ISO/IEC 18004), versions 1 to 40.
- Numeric, alphanumeric or byte mode, chosen for the text; text in UTF-8.
- Error correction levels `QR_ECC_LOW`, `QR_ECC_MEDIUM`, `QR_ECC_QUARTILE` and `QR_ECC_HIGH` from `qrcode.ch`.
- The smallest version that holds the text, or at least the version a program asks for.
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

   // The quiet zone of four light modules around the code is the program's own
   FOR EACH cRow IN aRows
      ? "    " + StrTran( StrTran( cRow, "1", "██" ), "0", "  " )
   NEXT

RETURN
```

## License

GPL-3.0. See [LICENSE](LICENSE).
