/*
 *
 */

#include "hpdf.ch"
#include "qrcode.ch"
#include "std.ch"

#if OS_LINUX
   #define FONT_REGULAR  "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
   #define FONT_BOLD     "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
#else
   #define FONT_REGULAR  "C:\Windows\Fonts\arial.ttf"
   #define FONT_BOLD     "C:\Windows\Fonts\arialbd.ttf"
#endif

#define INV_NUMBER    1
#define INV_PLACE     2
#define INV_ISSUED    3
#define INV_SOLD      4
#define INV_DUE       5
#define INV_ACCOUNT   6
#define INV_KSEF      7
#define INV_SELLER    8
#define INV_BUYER     9
#define INV_ITEMS     10

#define ITEM_NAME     1
#define ITEM_QTY      2
#define ITEM_UNIT     3
#define ITEM_PRICE    4
#define ITEM_VAT      5

#define PAGE_LEFT     50
#define PAGE_RIGHT    545
#define ROW_HEIGHT    24

STATIC s_pFont
STATIC s_pBold

PROCEDURE Main()

   LOCAL aInvoice := { ;
      "FV/2026/10/001", ;
      "Kraków", ;
      "25.10.2026", ;
      "24.10.2026", ;
      "08.11.2026", ;
      "PL61 1090 1014 0000 0712 1981 2874", ;
      "1234567890-20261025-0A1B2C3D4E5F-7A", ;
      { "Firma Łódzka Sp. z o.o.", "ul. Ciężkowicka 8b", "90-006 Łódź", "NIP 1234567890" }, ;
      { "Przedsiębiorstwo Świętokrzyskie S.A.", "ul. Sienkiewicza 63", "25-002 Kielce", "NIP 0987654321" }, ;
      { { "Licencja biblioteki PDF dla xDev", 1, "szt.", 1200.00, 23 }, ;
        { "Wdrożenie KSeF wraz ze szkoleniem użytkowników w siedzibie klienta", 2, "dzień", 850.00, 23 }, ;
        { "Podręcznik „Zażółć gęślą jaźń” – wydanie drukowane", 3, "szt.", 45.00, 5 }, ;
        { "Dojazd do klienta", 1, "usł.", 120.00, 8 } } }
   LOCAL cXml  := s_InvoiceXml( aInvoice )
   LOCAL pPdf  := HPDF_New()
   LOCAL pPage
   LOCAL nGross

   MemoWrit( "invoice.xml", cXml, .F. )

   HPDF_SetCompressionMode( pPdf, HPDF_COMP_ALL )
   HPDF_SetInfoAttr( pPdf, HPDF_INFO_TITLE, "Faktura " + aInvoice[ INV_NUMBER ] )
   HPDF_SetInfoAttr( pPdf, HPDF_INFO_CREATOR, "xDev" )
   HPDF_UseUTFEncodings( pPdf )
   HPDF_SetCurrentEncoder( pPdf, "UTF-8" )
   s_pFont := HPDF_GetFont( pPdf, HPDF_LoadTTFontFromFile( pPdf, FONT_REGULAR, HPDF_TRUE ), "UTF-8" )
   s_pBold := HPDF_GetFont( pPdf, HPDF_LoadTTFontFromFile( pPdf, FONT_BOLD, HPDF_TRUE ), "UTF-8" )

   pPage := HPDF_AddPage( pPdf )
   HPDF_Page_SetSize( pPage, HPDF_PAGE_SIZE_A4, HPDF_PAGE_PORTRAIT )

   s_Header( pPage, aInvoice )
   s_Parties( pPage, aInvoice )
   nGross := s_Items( pPage, aInvoice[ INV_ITEMS ], 640 )
   s_Verification( pPage, aInvoice, cXml )
   s_Footer( pPage )

   IF HPDF_SaveToFile( pPdf, "invoice.pdf" ) == HPDF_OK
      ? "invoice.pdf saved, amount due " + s_Money( nGross ) + " PLN"
   ELSE
      ? "invoice.pdf not saved, libharu error " + LTrim( Str( HPDF_GetError( pPdf ) ) )
   ENDIF

   HPDF_Free( pPdf )

   RETURN

/* ---------------------------------------------------------------------------------------------
   Parts of the page
   --------------------------------------------------------------------------------------------- */

STATIC PROCEDURE s_Header( pPage, aInvoice )

   s_Text( pPage, s_pBold, 22, PAGE_LEFT, 790, "Faktura VAT" )
   s_TextRight( pPage, s_pBold, 12, PAGE_RIGHT, 800, "Nr " + aInvoice[ INV_NUMBER ] )
   s_TextRight( pPage, s_pFont, 9, PAGE_RIGHT, 786, "Data wystawienia: " + aInvoice[ INV_ISSUED ] + ", " + aInvoice[ INV_PLACE ] )
   s_TextRight( pPage, s_pFont, 9, PAGE_RIGHT, 774, "Data sprzedaży: " + aInvoice[ INV_SOLD ] )

   HPDF_Page_SetLineWidth( pPage, 1 )
   s_Line( pPage, PAGE_LEFT, 762, PAGE_RIGHT, 762 )

   RETURN

STATIC PROCEDURE s_Parties( pPage, aInvoice )

   s_Party( pPage, PAGE_LEFT, "Sprzedawca", aInvoice[ INV_SELLER ] )
   s_Party( pPage, 305, "Nabywca", aInvoice[ INV_BUYER ] )

   RETURN

STATIC PROCEDURE s_Party( pPage, nX, cTitle, aLines )

   LOCAL nLine

   HPDF_Page_SetRGBFill( pPage, 0.95, 0.95, 0.95 )
   HPDF_Page_Rectangle( pPage, nX, 670, 240, 80 )
   HPDF_Page_Fill( pPage )
   HPDF_Page_SetRGBFill( pPage, 0, 0, 0 )

   s_Text( pPage, s_pBold, 9, nX + 8, 736, Upper( cTitle ) )
   FOR nLine := 1 TO Len( aLines )
      s_Text( pPage, IIF( nLine == 1, s_pBold, s_pFont ), 9, nX + 8, 736 - nLine * 13, aLines[ nLine ] )
   NEXT

   RETURN

// Table of items, summary by VAT rate and the amount due; returns the gross total
STATIC FUNCTION s_Items( pPage, aItems, nTop )

   LOCAL aCols   := { 50, 72, 245, 280, 312, 367, 397, 446, 495, 545 }
   LOCAL aRight  := { .F., .F., .T., .F., .T., .T., .T., .T., .T. }
   LOCAL aRates  := {}
   LOCAL nY      := nTop
   LOCAL nGross  := 0
   LOCAL nItem
   LOCAL nNet
   LOCAL nVat
   LOCAL nRate
   LOCAL aItem

   s_TableRow( pPage, aCols, aRight, nY, { "Lp.", "Nazwa towaru lub usługi", "Ilość", "J.m.", "Cena netto", ;
                                           "VAT", "Netto", "Kwota VAT", "Brutto" }, .T. )

   FOR nItem := 1 TO Len( aItems )
      aItem := aItems[ nItem ]
      nNet  := Round( aItem[ ITEM_QTY ] * aItem[ ITEM_PRICE ], 2 )
      nVat  := Round( nNet * aItem[ ITEM_VAT ] / 100, 2 )
      nY    -= ROW_HEIGHT

      s_TableRow( pPage, aCols, aRight, nY, { LTrim( Str( nItem ) ), aItem[ ITEM_NAME ], LTrim( Str( aItem[ ITEM_QTY ] ) ), ;
                                              aItem[ ITEM_UNIT ], s_Money( aItem[ ITEM_PRICE ] ), ;
                                              LTrim( Str( aItem[ ITEM_VAT ] ) ) + "%", s_Money( nNet ), s_Money( nVat ), ;
                                              s_Money( nNet + nVat ) }, .F. )

      IF ( nRate := AScan( aRates, {| a | a[ 1 ] == aItem[ ITEM_VAT ] } ) ) == 0
         AAdd( aRates, { aItem[ ITEM_VAT ], 0, 0 } )
         nRate := Len( aRates )
      ENDIF
      aRates[ nRate ][ 2 ] += nNet
      aRates[ nRate ][ 3 ] += nVat
      nGross += nNet + nVat
   NEXT

   ASort( aRates,,, {| a, b | a[ 1 ] > b[ 1 ] } )
   nY -= ROW_HEIGHT + 10
   s_TableRow( pPage, { 312, 367, 446, 495, 545 }, { .F., .T., .T., .T. }, nY, ;
               { "Stawka", "Netto", "Kwota VAT", "Brutto" }, .T. )
   FOR nRate := 1 TO Len( aRates )
      nY -= ROW_HEIGHT
      s_TableRow( pPage, { 312, 367, 446, 495, 545 }, { .F., .T., .T., .T. }, nY, ;
                  { LTrim( Str( aRates[ nRate ][ 1 ] ) ) + "%", s_Money( aRates[ nRate ][ 2 ] ), ;
                    s_Money( aRates[ nRate ][ 3 ] ), s_Money( aRates[ nRate ][ 2 ] + aRates[ nRate ][ 3 ] ) }, .F. )
   NEXT

   nY -= 40
   s_TextRight( pPage, s_pBold, 14, PAGE_RIGHT, nY, "Do zapłaty: " + s_Money( nGross ) + " PLN" )
   nY -= 16
   s_TextRight( pPage, s_pFont, 9, PAGE_RIGHT, nY, "Słownie: " + s_Words( nGross ) )

   RETURN nGross

// One row of a table; long texts wrap to a second line of the cell
STATIC PROCEDURE s_TableRow( pPage, aCols, aRight, nY, aCells, lHeader )

   LOCAL nCol
   LOCAL nLen := 0

   HPDF_Page_SetLineWidth( pPage, 0.5 )
   IF lHeader
      HPDF_Page_SetRGBFill( pPage, 0.85, 0.85, 0.85 )
      HPDF_Page_Rectangle( pPage, aCols[ 1 ], nY - ROW_HEIGHT, ATail( aCols ) - aCols[ 1 ], ROW_HEIGHT )
      HPDF_Page_FillStroke( pPage )
      HPDF_Page_SetRGBFill( pPage, 0, 0, 0 )
   ENDIF

   FOR nCol := 1 TO Len( aCols )
      s_Line( pPage, aCols[ nCol ], nY, aCols[ nCol ], nY - ROW_HEIGHT )
   NEXT
   s_Line( pPage, aCols[ 1 ], nY, ATail( aCols ), nY )
   s_Line( pPage, aCols[ 1 ], nY - ROW_HEIGHT, ATail( aCols ), nY - ROW_HEIGHT )

   HPDF_Page_BeginText( pPage )
   HPDF_Page_SetFontAndSize( pPage, IIF( lHeader, s_pBold, s_pFont ), 7.5 )
   FOR nCol := 1 TO Len( aCells )
      HPDF_Page_TextRect( pPage, aCols[ nCol ] + 3, nY - 3, aCols[ nCol + 1 ] - 3, nY - ROW_HEIGHT + 2, aCells[ nCol ], ;
                          IIF( aRight[ nCol ], HPDF_TALIGN_RIGHT, HPDF_TALIGN_LEFT ), @nLen )
   NEXT
   HPDF_Page_EndText( pPage )

   RETURN

// QR code KOD I: the link to the invoice in KSeF with the seller's NIP, the issue date and the
// SHA-256 of the invoice XML file in base64url
STATIC PROCEDURE s_Verification( pPage, aInvoice, cXml )

   LOCAL cHash := StrTran( StrTran( StrTran( Base64Encode( HashData( "SHA256", cXml ) ), "+", "-" ), "/", "_" ), "=", "" )
   LOCAL cUrl  := "https://ksef-test.mf.gov.pl/client-app/invoice/" + SubStr( ATail( aInvoice[ INV_SELLER ] ), 5 ) + ;
                  "/" + StrTran( aInvoice[ INV_ISSUED ], ".", "-" ) + "/" + cHash
   LOCAL aQr   := QrEncode( cUrl, QR_ECC_MEDIUM )
   LOCAL nSize := 1.6
   LOCAL nRow
   LOCAL nCol

   s_Text( pPage, s_pBold, 9, PAGE_LEFT, 250, "Płatność" )
   s_Text( pPage, s_pFont, 9, PAGE_LEFT, 236, "Przelew, termin płatności: " + aInvoice[ INV_DUE ] )
   s_Text( pPage, s_pFont, 9, PAGE_LEFT, 223, "Rachunek: " + aInvoice[ INV_ACCOUNT ] )

   FOR nRow := 1 TO Len( aQr )
      FOR nCol := 1 TO Len( aQr[ nRow ] )
         IF SubStr( aQr[ nRow ], nCol, 1 ) == "1"
            HPDF_Page_Rectangle( pPage, PAGE_LEFT + ( nCol - 1 ) * nSize, 100 + ( Len( aQr ) - nRow ) * nSize, nSize, nSize )
         ENDIF
      NEXT
   NEXT
   HPDF_Page_Fill( pPage )

   s_Text( pPage, s_pBold, 9, PAGE_LEFT + Len( aQr ) * nSize + 15, 100 + Len( aQr ) * nSize - 10, "KSeF: " + aInvoice[ INV_KSEF ] )
   s_Text( pPage, s_pFont, 8, PAGE_LEFT + Len( aQr ) * nSize + 15, 100 + Len( aQr ) * nSize - 24, ;
           "Zeskanuj kod, aby sprawdzić fakturę w Krajowym Systemie e-Faktur." )

   RETURN

STATIC PROCEDURE s_Footer( pPage )

   HPDF_Page_SetLineWidth( pPage, 0.5 )
   s_Line( pPage, PAGE_LEFT, 60, PAGE_RIGHT, 60 )
   HPDF_Page_SetRGBFill( pPage, 0.5, 0.5, 0.5 )
   s_Text( pPage, s_pFont, 7, PAGE_LEFT, 48, "Wygenerowano w xDev: hpdf, qrcode, xml, openssl" )
   HPDF_Page_SetRGBFill( pPage, 0, 0, 0 )

   RETURN

/* ---------------------------------------------------------------------------------------------
   Invoice data
   --------------------------------------------------------------------------------------------- */

STATIC FUNCTION s_InvoiceXml( aInvoice )

   LOCAL pDoc  := xml_New()
   LOCAL pRoot := xml_Add( pDoc, "Faktura" )
   LOCAL pItem
   LOCAL aItem
   LOCAL nItem
   LOCAL cXml

   xml_SetAttr( pRoot, "xmlns", "http://crd.gov.pl/wzor/2025/06/25/13775/" )
   xml_Add( pRoot, "P_1", aInvoice[ INV_ISSUED ] )
   xml_Add( pRoot, "P_2", aInvoice[ INV_NUMBER ] )
   xml_Add( pRoot, "Sprzedawca", aInvoice[ INV_SELLER ][ 1 ] )
   xml_Add( pRoot, "Nabywca", aInvoice[ INV_BUYER ][ 1 ] )
   FOR nItem := 1 TO Len( aInvoice[ INV_ITEMS ] )
      aItem := aInvoice[ INV_ITEMS ][ nItem ]
      pItem := xml_Add( pRoot, "FaWiersz" )
      xml_Add( pItem, "P_7", aItem[ ITEM_NAME ] )
      xml_Add( pItem, "P_8B", LTrim( Str( aItem[ ITEM_QTY ] ) ) )
      xml_Add( pItem, "P_9A", LTrim( Str( aItem[ ITEM_PRICE ], 15, 2 ) ) )
      xml_Add( pItem, "P_12", LTrim( Str( aItem[ ITEM_VAT ] ) ) )
   NEXT

   cXml := xml_ToString( pDoc )
   xml_Free( pDoc )

   RETURN cXml

/* ---------------------------------------------------------------------------------------------
   Drawing and formatting
   --------------------------------------------------------------------------------------------- */

STATIC PROCEDURE s_Text( pPage, pFont, nSize, nX, nY, cText )

   HPDF_Page_BeginText( pPage )
   HPDF_Page_SetFontAndSize( pPage, pFont, nSize )
   HPDF_Page_TextOut( pPage, nX, nY, cText )
   HPDF_Page_EndText( pPage )

   RETURN

STATIC PROCEDURE s_TextRight( pPage, pFont, nSize, nRight, nY, cText )

   HPDF_Page_BeginText( pPage )
   HPDF_Page_SetFontAndSize( pPage, pFont, nSize )
   HPDF_Page_TextOut( pPage, nRight - HPDF_Page_TextWidth( pPage, cText ), nY, cText )
   HPDF_Page_EndText( pPage )

   RETURN

STATIC PROCEDURE s_Line( pPage, nX1, nY1, nX2, nY2 )

   HPDF_Page_MoveTo( pPage, nX1, nY1 )
   HPDF_Page_LineTo( pPage, nX2, nY2 )
   HPDF_Page_Stroke( pPage )

   RETURN

// Polish money format: "1 234,50"
STATIC FUNCTION s_Money( nAmount )

   LOCAL cText    := LTrim( Str( Abs( Round( nAmount, 2 ) ), 20, 2 ) )
   LOCAL cInteger := Left( cText, Len( cText ) - 3 )
   LOCAL cResult  := ""

   DO WHILE Len( cInteger ) > 3
      cResult  := " " + Right( cInteger, 3 ) + cResult
      cInteger := Left( cInteger, Len( cInteger ) - 3 )
   ENDDO

   RETURN IIF( nAmount < 0, "-", "" ) + cInteger + cResult + "," + Right( cText, 2 )

// Amount in Polish words: "dwa tysiące dziewięćset trzydzieści złotych 00/100"
STATIC FUNCTION s_Words( nAmount )

   LOCAL nZloty  := Int( Round( nAmount, 2 ) )
   LOCAL nGrosz  := Round( ( Round( nAmount, 2 ) - nZloty ) * 100, 0 )
   LOCAL aGroups := { { "", "", "" }, { "tysiąc", "tysiące", "tysięcy" }, { "milion", "miliony", "milionów" } }
   LOCAL cWords  := ""
   LOCAL nGroup  := 1
   LOCAL nPart

   IF nZloty == 0
      cWords := "zero"
   ENDIF

   DO WHILE nZloty > 0 .AND. nGroup <= Len( aGroups )
      nPart := nZloty % 1000
      IF nPart > 0
         cWords := AllTrim( IIF( nPart == 1 .AND. nGroup > 1, "", s_Hundreds( nPart ) ) + " " + ;
                            s_Form( nPart, aGroups[ nGroup ] ) ) + " " + cWords
      ENDIF
      nZloty := Int( nZloty / 1000 )
      nGroup++
   ENDDO

   RETURN AllTrim( cWords ) + " " + s_Form( Int( Round( nAmount, 2 ) ), { "złoty", "złote", "złotych" } ) + " " + ;
          PadL( LTrim( Str( nGrosz ) ), 2, "0" ) + "/100"

STATIC FUNCTION s_Hundreds( nNumber )

   LOCAL aUnits    := { "jeden", "dwa", "trzy", "cztery", "pięć", "sześć", "siedem", "osiem", "dziewięć" }
   LOCAL aTeens    := { "dziesięć", "jedenaście", "dwanaście", "trzynaście", "czternaście", "piętnaście", ;
                        "szesnaście", "siedemnaście", "osiemnaście", "dziewiętnaście" }
   LOCAL aTens     := { "", "dwadzieścia", "trzydzieści", "czterdzieści", "pięćdziesiąt", "sześćdziesiąt", ;
                        "siedemdziesiąt", "osiemdziesiąt", "dziewięćdziesiąt" }
   LOCAL aHundreds := { "sto", "dwieście", "trzysta", "czterysta", "pięćset", "sześćset", "siedemset", ;
                        "osiemset", "dziewięćset" }
   LOCAL nHundred  := Int( nNumber / 100 )
   LOCAL nRest     := nNumber % 100
   LOCAL cWords    := IIF( nHundred > 0, aHundreds[ nHundred ] + " ", "" )

   IF nRest >= 10 .AND. nRest <= 19
      cWords += aTeens[ nRest - 9 ]
   ELSE
      IF nRest >= 20
         cWords += aTens[ Int( nRest / 10 ) ] + " "
      ENDIF
      IF nRest % 10 > 0
         cWords += aUnits[ nRest % 10 ]
      ENDIF
   ENDIF

   RETURN AllTrim( cWords )

// Polish plural: 1 złoty, 2-4 złote (but 12-14 złotych), otherwise złotych
STATIC FUNCTION s_Form( nNumber, aForms )

   LOCAL nLast := nNumber % 10
   LOCAL nTens := nNumber % 100

   DO CASE
   CASE nNumber == 1
      RETURN aForms[ 1 ]
   CASE nLast >= 2 .AND. nLast <= 4 .AND. ( nTens < 12 .OR. nTens > 14 )
      RETURN aForms[ 2 ]
   ENDCASE

   RETURN aForms[ 3 ]
