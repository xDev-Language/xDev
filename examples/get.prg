/*
 * $DOC$
 * $NAME$
 * GET input screen
 *
 * $ONELINER$
 * Demonstrates terminal GET fields, validation, pictures, and resize handling.
 *
 * $DESCRIPTION$
 * Displays editable UTF-8 text, phone, code, number, date, logical, and note fields. Shows VALID, RANGE, WHEN, and picture clauses. Esc finishes input; any key closes the result screen.
 *
 * $EXAMPLES$
 * examples/get.prg
 *
 * $OUTPUT$
 *
 * $DOCEND$
 */

#include "inkey.ch"
#include "setcurs.ch"
#include "gtinfo.ch"
#include "tui.ch"

#if OS_LINUX
   REQUEST GTXWC
#else
   REQUEST GTWVT
#endif

PROCEDURE Main()

   LOCAL GetList := {}
   LOCAL cName   := UPadR( "Zażółć gęślą", 20 )
   LOCAL cCity   := UPadR( "Łódź", 20 )
   LOCAL cPhone  := "601234567"
   LOCAL cCode   := UPadR( "ab12", 6 )
   LOCAL nAmount := 1234.5
   LOCAL nCount  := 5
   LOCAL dDate   := Date()
   LOCAL lActive := .T.
   LOCAL cNote   := UPadR( "", 20 )
   LOCAL lSaved

   SetColorExt( COLOR_TEXT, COLOR_PAPER )
   SetCursor( SC_NORMAL )
   GtInfo( GTI_RESIZEBLOCK, {|| s_DrawScreen() } )
   s_DrawScreen()

   GetAdd( GetList,  3, 22, @cName )
   GetAdd( GetList,  4, 22, @cCity, , {|| ! Empty( cCity ) } )
   GetAdd( GetList,  5, 22, @cPhone, "@R 999-999-999" )
   GetAdd( GetList,  6, 22, @cCode, "@K!" )
   GetAdd( GetList,  7, 22, @nAmount, "999,999.99" )
   GetAdd( GetList,  8, 22, @nCount, , {| oGet | RangeCheck( oGet, , 1, 99 ) } )
   GetAdd( GetList,  9, 22, @dDate )
   GetAdd( GetList, 10, 22, @lActive, "Y" )
   GetAdd( GetList, 11, 22, @cNote, , , {|| lActive } )

   lSaved := ReadModal( GetList )
   GetList := {}

   GtInfo( GTI_RESIZEBLOCK, NIL )
   DispBoxExt( 13, 2, 22, 60, Space( 9 ), COLOR_TEXT, COLOR_PAPER )
   DispOutAtExt( 13, 2, "Updated(): " + IIF( lSaved, ".T.", ".F." ) + "   LastKey(): " + LTrim( Str( LastKey() ) ), COLOR_BLUE, COLOR_PAPER )
   DispOutAtExt( 14, 2, "Name:   [" + cName + "]", COLOR_TEXT, COLOR_PAPER )
   DispOutAtExt( 15, 2, "City:   [" + cCity + "]", COLOR_TEXT, COLOR_PAPER )
   DispOutAtExt( 16, 2, "Phone:  [" + cPhone + "]", COLOR_TEXT, COLOR_PAPER )
   DispOutAtExt( 17, 2, "Code:   [" + cCode + "]", COLOR_TEXT, COLOR_PAPER )
   DispOutAtExt( 18, 2, "Amount: " + Str( nAmount ) + "   Count: " + Str( nCount ), COLOR_TEXT, COLOR_PAPER )
   DispOutAtExt( 19, 2, "Date:   " + DToC( dDate ) + "   Active: " + IIF( lActive, ".T.", ".F." ), COLOR_TEXT, COLOR_PAPER )
   DispOutAtExt( 20, 2, "Note:   [" + cNote + "]", COLOR_TEXT, COLOR_PAPER )
   DispOutAtExt( 22, 2, "Any key ends the program", COLOR_BLUE, COLOR_PAPER )
   Inkey( 0 )

RETURN

// The screen of the program around the Gets; also the block of GTI_RESIZEBLOCK
STATIC PROCEDURE s_DrawScreen()

   DispBoxExt( 0, 0, MaxRow(), MaxCol(), Space( 9 ), COLOR_TEXT, COLOR_PAPER )
   DispOutAtExt( 1, 2, "xDev Get - Enter / arrows / mouse, Esc ends READ", COLOR_BLUE, COLOR_PAPER )
   DispOutAtExt(  3, 2, "Name (UTF-8):", COLOR_TEXT, COLOR_PAPER )
   DispOutAtExt(  4, 2, "City (VALID):", COLOR_TEXT, COLOR_PAPER )
   DispOutAtExt(  5, 2, "Phone (@R):", COLOR_TEXT, COLOR_PAPER )
   DispOutAtExt(  6, 2, "Code (@K!):", COLOR_TEXT, COLOR_PAPER )
   DispOutAtExt(  7, 2, "Amount:", COLOR_TEXT, COLOR_PAPER )
   DispOutAtExt(  8, 2, "Count (RANGE):", COLOR_TEXT, COLOR_PAPER )
   DispOutAtExt(  9, 2, "Date:", COLOR_TEXT, COLOR_PAPER )
   DispOutAtExt( 10, 2, "Active (L):", COLOR_TEXT, COLOR_PAPER )
   DispOutAtExt( 11, 2, "Note (WHEN):", COLOR_TEXT, COLOR_PAPER )

RETURN
