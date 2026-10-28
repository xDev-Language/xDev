/*
 *
 */

#include "hpdf.ch"
#include "qrcode.ch"
#include "std.ch"


#if OS_LINUX
   #define FONT_FILE "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
#else
   #define FONT_FILE "C:\Windows\Fonts\arial.ttf"
#endif

STATIC s_cReport := ""

PROCEDURE Main()

   LOCAL pPdf  := HPDF_New()
   LOCAL aRows := { { "1", "Usługa programistyczna – biblioteka PDF", "1", "1 200,00", "1 200,00" }, ;
                    { "2", "Wdrożenie KSeF i szkolenie użytkowników", "2", "850,00", "1 700,00" }, ;
                    { "3", "Zażółć gęślą jaźń", "3", "10,00", "30,00" } }
   LOCAL aQr
   LOCAL pPage
   LOCAL pFont
   LOCAL cFont
   LOCAL cData
   LOCAL nY
   LOCAL nRow
   LOCAL nCol
   LOCAL nLen
   LOCAL nWidth := 0
   LOCAL aCols  := { 50, 80, 330, 380, 460, 545 }

   s_Line( "Error after HPDF_New: " + LTrim( Str( HPDF_GetError( pPdf ) ) ) )

   HPDF_SetCompressionMode( pPdf, HPDF_COMP_ALL )
   HPDF_SetInfoAttr( pPdf, HPDF_INFO_TITLE, "Faktura VAT 1/2026" )
   HPDF_SetInfoAttr( pPdf, HPDF_INFO_AUTHOR, "xDev" )
   HPDF_UseUTFEncodings( pPdf )
   HPDF_SetCurrentEncoder( pPdf, "UTF-8" )

   cFont := HPDF_LoadTTFontFromFile( pPdf, FONT_FILE, HPDF_TRUE )
   s_Line( "Font: " + IIF( cFont == NIL, "NIL, error " + LTrim( Str( HPDF_GetError( pPdf ) ) ), cFont ) )
   pFont := HPDF_GetFont( pPdf, cFont, "UTF-8" )

   pPage := HPDF_AddPage( pPdf )
   HPDF_Page_SetSize( pPage, HPDF_PAGE_SIZE_A4, HPDF_PAGE_PORTRAIT )
   s_Line( "Page: " + LTrim( Str( HPDF_Page_GetWidth( pPage ) ) ) + " x " + LTrim( Str( HPDF_Page_GetHeight( pPage ) ) ) )

   HPDF_Page_BeginText( pPage )
   HPDF_Page_SetFontAndSize( pPage, pFont, 18 )
   HPDF_Page_TextOut( pPage, 50, 780, "Faktura VAT nr 1/2026" )
   HPDF_Page_SetFontAndSize( pPage, pFont, 10 )
   HPDF_Page_TextOut( pPage, 50, 755, "Sprzedawca: Firma Łódzka Sp. z o.o., NIP 123-456-78-90" )
   HPDF_Page_TextOut( pPage, 50, 740, "Nabywca: Przedsiębiorstwo Świętokrzyskie S.A., NIP 987-654-32-10" )
   HPDF_Page_TextOut( pPage, 50, 725, "Data wystawienia: 2026-10-25" )
   HPDF_Page_EndText( pPage )

   nY := 690
   HPDF_Page_SetLineWidth( pPage, 0.5 )
   HPDF_Page_SetRGBFill( pPage, 0.9, 0.9, 0.9 )
   HPDF_Page_Rectangle( pPage, aCols[ 1 ], nY - 4, aCols[ 6 ] - aCols[ 1 ], 18 )
   HPDF_Page_FillStroke( pPage )
   HPDF_Page_SetRGBFill( pPage, 0, 0, 0 )

   HPDF_Page_BeginText( pPage )
   HPDF_Page_SetFontAndSize( pPage, pFont, 9 )
   s_Cells( pPage, aCols, nY, { "Lp.", "Nazwa towaru lub usługi", "Ilość", "Cena netto", "Wartość" } )
   FOR nRow := 1 TO Len( aRows )
      s_Cells( pPage, aCols, nY - nRow * 18, aRows[ nRow ] )
   NEXT
   HPDF_Page_EndText( pPage )

   FOR nRow := 1 TO Len( aRows )
      HPDF_Page_Rectangle( pPage, aCols[ 1 ], nY - nRow * 18 - 4, aCols[ 6 ] - aCols[ 1 ], 18 )
   NEXT
   FOR nCol := 2 TO 5
      HPDF_Page_MoveTo( pPage, aCols[ nCol ], nY + 14 )
      HPDF_Page_LineTo( pPage, aCols[ nCol ], nY - Len( aRows ) * 18 - 4 )
   NEXT
   HPDF_Page_Stroke( pPage )

   HPDF_Page_BeginText( pPage )
   HPDF_Page_SetFontAndSize( pPage, pFont, 11 )
   HPDF_Page_TextOut( pPage, 380, nY - 4 * 18 - 10, "Razem: 2 930,00 PLN" )
   HPDF_Page_SetFontAndSize( pPage, pFont, 9 )
   HPDF_Page_TextRect( pPage, 50, 560, 330, 480, "Uwagi: płatność przelewem w terminie 14 dni na rachunek " + ;
                       "PL 61 1090 1014 0000 0712 1981 2874. Faktura wystawiona w Krajowym Systemie e-Faktur.", ;
                       HPDF_TALIGN_JUSTIFY, @nLen )
   HPDF_Page_EndText( pPage )
   s_Line( "TextRect bytes written: " + LTrim( Str( nLen ) ) )

   aQr := QrEncode( "https://ksef-test.mf.gov.pl/client-app/invoice/1234567890/25-10-2026/UtQp9Gpc51y-u3xApZjIjgkpZ01js-J8KflSPW8WzIE" )
   s_Line( "QR: " + LTrim( Str( Len( aQr ) ) ) + " x " + LTrim( Str( Len( aQr[ 1 ] ) ) ) + " modules" )
   s_Qr( pPage, aQr, 400, 470, 1.6 )
   HPDF_Page_BeginText( pPage )
   HPDF_Page_SetFontAndSize( pPage, pFont, 7 )
   HPDF_Page_TextOut( pPage, 400, 455, "KSeF: 1234567890-20261025-ABCDEF-12" )
   HPDF_Page_EndText( pPage )

   HPDF_Page_SetFontAndSize( pPage, pFont, 10 )
   s_Line( "MeasureText bytes: " + LTrim( Str( HPDF_Page_MeasureText( pPage, "Zażółć gęślą jaźń", 200, .F., @nWidth ) ) ) + ;
           ", width " + LTrim( Str( nWidth ) ) )
   s_Line( "TextWidth: " + LTrim( Str( HPDF_Page_TextWidth( pPage, "Zażółć" ) ) ) )
   s_Line( "Line width: " + LTrim( Str( HPDF_Page_GetLineWidth( pPage ) ) ) )
   s_Line( "RGB fill: " + s_Array( HPDF_Page_GetRGBFill( pPage ) ) )

   s_Line( "Save: " + LTrim( Str( HPDF_SaveToFile( pPdf, "invoice.pdf" ) ) ) )
   s_Line( "GetContents: " + LTrim( Str( HPDF_GetContents( pPdf, @cData ) ) ) + ", " + LTrim( Str( Len( cData ) ) ) + ;
           " bytes, starts with " + Left( cData, 8 ) )
   HPDF_Free( pPdf )

   s_Line( "" )
   s_Line( "QR of '123' (version 1, numeric):" )
   AEval( QrEncode( "123", QR_ECC_LOW ), {| cRow | s_Line( StrTran( StrTran( cRow, "1", "##" ), "0", "  " ) ) } )

   MemoWrit( "pdf.txt", s_cReport, .F. )
   ? "Results saved to pdf.txt, invoice in invoice.pdf"

   RETURN

STATIC PROCEDURE s_Cells( pPage, aCols, nY, aText )

   LOCAL nCol

   FOR nCol := 1 TO Len( aText )
      HPDF_Page_TextOut( pPage, aCols[ nCol ] + 4, nY, aText[ nCol ] )
   NEXT

   RETURN

STATIC PROCEDURE s_Qr( pPage, aQr, nX, nY, nModule )

   LOCAL nRow
   LOCAL nCol

   FOR nRow := 1 TO Len( aQr )
      FOR nCol := 1 TO Len( aQr[ nRow ] )
         IF SubStr( aQr[ nRow ], nCol, 1 ) == "1"
            HPDF_Page_Rectangle( pPage, nX + ( nCol - 1 ) * nModule, nY + ( Len( aQr ) - nRow ) * nModule, nModule, nModule )
         ENDIF
      NEXT
   NEXT
   HPDF_Page_Fill( pPage )

   RETURN

STATIC FUNCTION s_Array( aValues )

   LOCAL cText := ""

   AEval( aValues, {| n | cText += LTrim( Str( n ) ) + " " } )

   RETURN "{ " + cText + "}"

STATIC PROCEDURE s_Line( cText )

   s_cReport += cText + Eol()

   RETURN
