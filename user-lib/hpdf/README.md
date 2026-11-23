# xDev HPDF

PDF library for the [xDev](https://github.com/xDev-Language) language, built on [libharu](https://github.com/libharu/libharu) 2.4.6. The functions retain the libharu names: `HPDF_New()`, `HPDF_AddPage()`, `HPDF_Page_TextOut()`, and others.

## Key features

- Text in UTF-8 with TrueType fonts; the ToUnicode map of libharu is corrected, so text copied from the PDF stays correct.
- Pages, text, lines, shapes, PNG and JPEG images, annotations, outlines and compression.
- Results returned by libharu through pointers are exposed through arguments passed by reference (`@`).
- Errors are read with `HPDF_GetError()` and `HPDF_GetErrorDetail()`.
- The sources of libharu are included, so the library builds without installing libharu.

## Building

```
make
make install
```

`make` builds `libxhpdf.a` with libharu and libpng inside. libpng comes from the system (`pkg-config`), zlib from `libxzlib.a`. `make install` installs the library into the xDev directory. Libraries in `lib/xdev` are linked automatically.

## Example

```xBase
#include "hpdf.ch"

PROCEDURE Main()

   LOCAL pPdf  := HPDF_New()
   LOCAL pPage, pFont

   HPDF_UseUTFEncodings( pPdf )
   HPDF_SetCurrentEncoder( pPdf, "UTF-8" )
   pFont := HPDF_GetFont( pPdf, HPDF_LoadTTFontFromFile( pPdf, "C:\Windows\Fonts\arial.ttf", HPDF_TRUE ), "UTF-8" )

   pPage := HPDF_AddPage( pPdf )
   HPDF_Page_SetSize( pPage, HPDF_PAGE_SIZE_A4, HPDF_PAGE_PORTRAIT )
   HPDF_Page_BeginText( pPage )
   HPDF_Page_SetFontAndSize( pPage, pFont, 18 )
   HPDF_Page_TextOut( pPage, 50, 780, "Faktura VAT nr 1/2026 – Zażółć gęślą jaźń" )
   HPDF_Page_EndText( pPage )

   HPDF_SaveToFile( pPdf, "faktura.pdf" )
   HPDF_Free( pPdf )

RETURN
```

## License

xDev jest na licencji GNU GPL v3, patrz plik LICENSE.
