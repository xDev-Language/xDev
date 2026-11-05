/*
 * tbrowse.prg
 */

#include "inkey.ch"
#include "setcurs.ch"
#include "tbrowse.ch"
#include "tui.ch"

#if OS_LINUX
   REQUEST GTXWC
#else
   REQUEST GTWVT
#endif

PROCEDURE Main()

   LOCAL aData := { ;
      { "Kowalski",    "Jan",       "Warszawa",      1250.50, .T., "601 234 567" }, ;
      { "Nowak",       "Anna",      "Kraków",        -320.00, .F., "602 345 678" }, ;
      { "Wiśniewski",  "Piotr",     "Gdańsk",        8400.00, .T., "603 456 789" }, ;
      { "Wójcik",      "Katarzyna", "Wrocław",         99.99, .T., "604 567 890" }, ;
      { "Kowalczyk",   "Tomasz",    "Poznań",         -15.75, .F., "605 678 901" }, ;
      { "Kamiński",    "Magdalena", "Łódź",          4300.00, .T., "606 789 012" }, ;
      { "Lewandowski", "Marek",     "Szczecin",       720.10, .F., "607 890 123" }, ;
      { "Zieliński",   "Ewa",       "Lublin",        1999.00, .T., "608 901 234" }, ;
      { "Szymański",   "Paweł",     "Bydgoszcz",     -880.40, .T., "609 012 345" }, ;
      { "Woźniak",     "Agnieszka", "Białystok",      310.00, .F., "690 123 456" }, ;
      { "Dąbrowski",   "Krzysztof", "Katowice",      5600.00, .T., "691 234 567" }, ;
      { "Kozłowski",   "Monika",    "Gdynia",          45.00, .T., "692 345 678" }, ;
      { "Jankowski",   "Michał",    "Częstochowa",   1200.00, .F., "693 456 789" }, ;
      { "Mazur",       "Joanna",    "Radom",         -150.00, .T., "694 567 890" }, ;
      { "Kwiatkowski", "Adam",      "Toruń",          990.90, .T., "695 678 901" }, ;
      { "Krawczyk",    "Barbara",   "Kielce",        3100.00, .F., "696 789 012" }, ;
      { "Piotrowski",  "Łukasz",    "Rzeszów",        640.00, .T., "697 890 123" }, ;
      { "Grabowski",   "Natalia",   "Olsztyn",      -2000.00, .T., "698 901 234" }, ;
      { "Nowakowski",  "Jakub",     "Opole",          150.25, .F., "699 012 345" }, ;
      { "Pawłowski",   "Zofia",     "Zielona Góra",   870.00, .T., "501 123 456" }, ;
      { "Michalski",   "Robert",    "Elbląg",        2500.00, .T., "502 234 567" }, ;
      { "Król",        "Alicja",    "Płock",          -60.00, .F., "503 345 678" } }
   LOCAL nRow  := 1
   LOCAL nSum  := 0
   LOCAL lQuit := .F.
   LOCAL oBrw
   LOCAL oCol
   LOCAL nKey
   LOCAL i

   FOR i := 1 TO Len( aData )
      nSum += aData[ i ][ 4 ]
   NEXT

   SetColorExt( COLOR_TEXT, COLOR_PAPER )
   SetCursor( SC_NONE )
   Scroll()

   oBrw := TBrowse():New( 1, 0, MaxRow() - 1, MaxCol() )
   oBrw:colorSpec := { ;
      { COLOR_TEXT,  COLOR_PAPER }, ;
      { COLOR_PAPER, COLOR_BLUE  }, ;
      { COLOR_BLUE,  COLOR_PAPER }, ;
      { COLOR_WINE,  COLOR_PAPER }, ;
      { COLOR_PAPER, COLOR_WINE  } }
   oBrw:border        := "┌─┐│┘─└│"
   oBrw:headSep       := "═╤═"
   oBrw:colSep        := " │ "
   oBrw:footSep       := "═╧═"
   oBrw:freeze        := 1
   oBrw:goTopBlock    := {|| nRow := 1 }
   oBrw:goBottomBlock := {|| nRow := Len( aData ) }
   oBrw:skipBlock     := {| n, nOld | nOld := nRow, nRow := Max( 1, Min( Len( aData ), nRow + n ) ), nRow - nOld }

   oCol := TBColumn():New( "Nazwisko", {|| aData[ nRow ][ 1 ] } )
   oCol:width    := 12
   oCol:footing  := "Rekordów:;" + LTrim( Str( Len( aData ) ) )
   oCol:defColor := { 1, 2, 3, 3 }
   oBrw:AddColumn( oCol )

   oCol := TBColumn():New( "Imię", {|| aData[ nRow ][ 2 ] } )
   oCol:width    := 12
   oCol:defColor := { 1, 2, 3, 3 }
   oBrw:AddColumn( oCol )

   oCol := TBColumn():New( "Miasto", {|| aData[ nRow ][ 3 ] } )
   oCol:width    := 14
   oCol:defColor := { 1, 2, 3, 3 }
   oBrw:AddColumn( oCol )

   oCol := TBColumn():New( "Kwota;PLN", {|| aData[ nRow ][ 4 ] } )
   oCol:picture    := "999,999.99"
   oCol:footing    := "Suma:;" + Transform( nSum, "999,999.99" )
   oCol:defColor   := { 1, 2, 3, 3 }
   oCol:colorBlock := {| n | IIF( n < 0, { 4, 5 }, { 1, 2 } ) }
   oBrw:AddColumn( oCol )

   oCol := TBColumn():New( "Aktywny", {|| aData[ nRow ][ 5 ] } )
   oCol:defColor := { 1, 2, 3, 3 }
   oBrw:AddColumn( oCol )

   oCol := TBColumn():New( "Telefon", {|| aData[ nRow ][ 6 ] } )
   oCol:defColor := { 1, 2, 3, 3 }
   oBrw:AddColumn( oCol )

   DO WHILE ! lQuit

      oBrw:ForceStable()

      DispOutAtExt( 0, 0, UPadR( " Strzałki, PgUp/PgDn, Ctrl+PgUp/Dn, Home/End, Ctrl+strzałki, mysz, Esc koniec", MaxCol() + 1 ), ;
                    COLOR_PAPER, COLOR_BLUE )
      DispOutAtExt( MaxRow(), 0, UPadR( " Wiersz: " + LTrim( Str( nRow ) ) + ;
                                      "  Kolumna: " + LTrim( Str( oBrw:colPos ) ) + ;
                                      IIF( oBrw:hitTop, "  [początek]", "" ) + ;
                                      IIF( oBrw:hitBottom, "  [koniec]", "" ), MaxCol() + 1 ), ;
                    COLOR_PAPER, COLOR_BLUE )

      nKey := Inkey( 0, INKEY_ALLEVENTS )

      SWITCH nKey

         CASE K_CLOSE
            lQuit := .T.
            EXIT

         CASE K_RESIZE
            Scroll()
            oBrw:nBottom := MaxRow() - 1
            oBrw:nRight  := MaxCol()
            oBrw:Invalidate()
            EXIT

         OTHERWISE
            lQuit := oBrw:ApplyKey( nKey ) == TBR_EXIT

      ENDSWITCH

   ENDDO

   SetCursor( SC_NORMAL )

RETURN
