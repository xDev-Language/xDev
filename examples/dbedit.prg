/*
 *
 */

#include "inkey.ch"
#include "dbedit.ch"
#include "tui.ch"

#if OS_LINUX
   REQUEST GTXWC
#else
   REQUEST GTWVT
#endif

PROCEDURE Main()

   LOCAL aData := { ;
      { "Kowalski",    "Warszawa", 1250.50 }, ;
      { "Nowak",       "Kraków",   -320.00 }, ;
      { "Wiśniewski",  "Gdańsk",   8400.00 }, ;
      { "Wójcik",      "Wrocław",    99.99 }, ;
      { "Kamiński",    "Łódź",     4300.00 }, ;
      { "Lewandowski", "Szczecin",  720.10 } }
   LOCAL nRow

   dbCreate( "dbedit", { { "NAME", "C", 14, 0 }, { "CITY", "C", 12, 0 }, { "AMOUNT", "N", 10, 2 } } )
   dbUseArea( .T., NIL, "dbedit" )
   FOR nRow := 1 TO Len( aData )
      dbAppend()
      FieldPut( 1, aData[ nRow ][ 1 ] )
      FieldPut( 2, aData[ nRow ][ 2 ] )
      FieldPut( 3, aData[ nRow ][ 3 ] )
   NEXT
   dbGoTop()

   DispBoxExt( 0, 0, MaxRow(), MaxCol(), Space( 9 ), COLOR_TEXT, COLOR_PAPER )
   DispOutAtExt( 1, 2, "xDev DBEdit - arrows, PgUp/PgDn, mouse; F10 ends", COLOR_BLUE, COLOR_PAPER )

   DBEdit( 3, 2, 15, 60, { "NAME", "CITY", "AMOUNT", {|| FieldGet( 3 ) * 1.23 } }, ;
           {| nMode, nCol | s_UserFunc( nMode, nCol ) }, ;
           { "", "", "999,999.99", "999,999.99" }, ;
           { "Name", "City", "Amount", "Gross" } )

   dbCloseArea()
   FErase( "dbedit.dbf" )

RETURN

// Shows the record and the column; F10 ends DBEdit()
STATIC FUNCTION s_UserFunc( nMode, nCol )

   DispOutAtExt( 17, 2, PadR( "Record " + LTrim( Str( RecNo() ) ) + " of " + LTrim( Str( LastRec() ) ) + ;
                 "   column " + LTrim( Str( nCol ) ) + "   mode " + LTrim( Str( nMode ) ), 50 ), COLOR_TEXT, COLOR_PAPER )
   IF nMode == DE_EXCEPT .AND. LastKey() == K_F10
      RETURN DE_ABORT
   ENDIF

   RETURN DE_CONT
