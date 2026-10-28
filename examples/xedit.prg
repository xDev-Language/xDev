/*
 *
 */

#include "gtinfo.ch"
#include "inkey.ch"
#include "std.ch"
#include "tui.ch"

#if OS_LINUX
   REQUEST GTXWC
#else
   REQUEST GTWVT
#endif

PROCEDURE Main( cFileName )

   LOCAL oEdit
   LOCAL lContinue := .T.
   LOCAL nKey
   LOCAL nHeight := 20

   SetColorExt( COLOR_TEXT, 0x1F1F1F )
   Scroll()

   IF cFileName == NIL
      cFileName := "Untitled-1.prg"
   ENDIF

   oEdit := TEdit():New( 0, 0, MaxRow(), MaxCol(), cFileName )

   IF ! oEdit:Load() .AND. ;
      Alert( "Cannot find the file " + '"' + cFileName + '";;Do you want to create a new file?', { "Yes", "No" } ) != 1
      QUIT
   ENDIF

   GtInfo( GTI_RESIZEBLOCK, {|| oEdit:bottom := MaxRow(), oEdit:right := MaxCol(), ;
                                oEdit:MoveTo( oEdit:line, oEdit:column ), oEdit:Display() } )

   oEdit:Display()

   DO WHILE lContinue

      nKey := Inkey( 0, BitOr( INKEY_ALLEVENTS, INKEY_EXT ) )

      SWITCH KeyStd( nKey )

      CASE K_ESC
      CASE K_CLOSE
         lContinue := .F.
         EXIT

      CASE K_RESIZE
         oEdit:bottom := MaxRow()
         oEdit:right  := MaxCol()
         oEdit:MoveTo( oEdit:line, oEdit:column )
         oEdit:Display()
         EXIT

      CASE K_F1
         SET CURSOR OFF
         xEdit_Help()
         oEdit:Display()
         EXIT

      CASE K_RBUTTONUP
         SET CURSOR OFF
         s_ContextMenu( MRow(), MCol() )
         oEdit:Display()
         EXIT

      CASE K_SH_F10
         SET CURSOR OFF
         s_ContextMenu( Row(), Col() + 1 )
         oEdit:Display()
         EXIT

      CASE K_MWFORWARD
         IF BitAnd( KeyMod( nKey ), KF_CTRL ) != 0
            GtInfo( GTI_FONTWIDTH, Int( nHeight-- / 2 ) )
            GtInfo( GTI_FONTSIZE, nHeight )
         ELSE
            oEdit:HandleKey( nKey )
         ENDIF
         EXIT

      CASE K_MWBACKWARD
         IF BitAnd( KeyMod( nKey ), KF_CTRL ) != 0
            GtInfo( GTI_FONTWIDTH, Int( nHeight++ / 2 ) )
            GtInfo( GTI_FONTSIZE, nHeight )
         ELSE
            oEdit:HandleKey( nKey )
         ENDIF
         EXIT

      OTHERWISE
         oEdit:HandleKey( nKey )

      ENDSWITCH

   ENDDO

   IF oEdit:changed
      oEdit:Save()
   ENDIF

   RETURN

PROCEDURE xEdit_Help()

   LOCAL cScreen := SaveScreen( 0, 0, MaxRow(), MaxCol() )
   LOCAL lContinue := .T.
   LOCAL aEditHelp := { ;
      "Esc                or Ctrl+[           Save the file and exit from xEdit.", ;
      "Mouse Wheel Forward                    Moves the cursor one line up.", ;
      "Ctrl + Mouse Wheel Forward             Zoom--", ;
      "Mouse Wheel Backward                   Moves the cursor one line down.", ;
      "Ctrl + Mouse Wheel Backward            Zoom++", ;
      "Uparrow            or Ctrl+E           Moves the cursor one line up.", ;
      "Dnarrow            or Ctrl+X           Moves the cursor one line down.", ;
      "Leftarrow          or Ctrl+S           Moves the cursor one character to the left.", ;
      "Rightarrow         or Ctrl+D           Moves the cursor one character to the right.", ;
      "Home               or Ctrl+A           Moves the cursor to the beginning of the line.", ;
      "End                or Ctrl+F           Moves the cursor to the end of the line.", ;
      "PgUp               or Ctrl+R           Moves the cursor one page up.", ;
      "PgDn               or Ctrl+C           Moves the cursor one page down.", ;
      "Ctrl+PgUp                              Moves the cursor to the beginning of the file.", ;
      "Ctrl+PgDn                              Moves the cursor to the end of the file.", ;
      "Enter              or Ctrl+M           Begin next line.", ;
      "Ins                or Ctrl+V           Toggles the insert mode on or off.", ;
      "Delete             or Ctrl+G           Deletes the character to the right of the cursor.", ;
      "Backspace          or Ctrl+H           Deletes the character to the left of the cursor.", ;
      "Tab                or Ctrl+I           Insert spaces.", ;
      "F1                 or Ctrl+Backslash   Display help screen.", ;
      "Mouse Right Button or Shift-F10        Context menu", ;
      "", ;
      "                                       v0.0.0" }
   LOCAL nRow := 1
   LOCAL nColLength := 0
   LOCAL i

   FOR i := 1 TO Len( aEditHelp )
      nColLength := Max( nColLength, Len( aEditHelp[ i ] ) )
   NEXT

   DO WHILE lContinue

      DispBegin()

      DispBoxExt( 0, 0, MaxRow(), MaxCol(), Space( 9 ), COLOR_TEXT, COLOR_PAPER )

      FOR i := 1 TO Len( aEditHelp )
         DispOutAtExt( i, 4, PadR( aEditHelp[ i ], nColLength ), ;
            IIF( i == nRow, COLOR_PAPER, COLOR_TEXT ), IIF( i == nRow, COLOR_BLUE, COLOR_PAPER ) )
      NEXT

      DispEnd()

      SWITCH KeyStd( Inkey( 0, BitOr( INKEY_ALLEVENTS, INKEY_EXT ) ) )

      CASE K_ESC
      CASE K_CLOSE
         lContinue := .F.
         EXIT

      CASE K_UP
         IF nRow > 1
            nRow--
         ENDIF
         EXIT

      CASE K_DOWN
         IF nRow < Len( aEditHelp )
            nRow++
         ENDIF
         EXIT

      ENDSWITCH

   ENDDO

   RestScreen( 0, 0, MaxRow(), MaxCol(), cScreen )

   RETURN

STATIC FUNCTION s_ContextMenu( nRowPos, nColPos )

   LOCAL aEditContextMenu := { ;
      "Cut", ;
      "Copy", ;
      "Paste", ;
      "", ;
      "Command Palette..." }
   LOCAL nRow := 1
   LOCAL nColLength := 0
   LOCAL nKey
   LOCAL lInside
   LOCAL i

   FOR i := 1 TO Len( aEditContextMenu )
      nColLength := Max( nColLength, Len( aEditContextMenu[ i ] ) )
   NEXT

   DO WHILE .T.

      DispBegin()

      DispBoxExt( nRowPos, nColPos - 1, nRowPos + 6, nColPos + nColLength, 1, COLOR_BLUE, COLOR_PAPER )
      DispOutAtExt( nRowPos + 4, nColPos - 1, "├" + Replicate( "─", nColLength ) + "┤", COLOR_BLUE, COLOR_PAPER )

      FOR i := 1 TO Len( aEditContextMenu )
         IF ! Empty( aEditContextMenu[ i ] )
            DispOutAtExt( nRowPos + i, nColPos, PadR( aEditContextMenu[ i ], nColLength ), ;
               IIF( i == nRow, COLOR_PAPER, COLOR_TEXT ), IIF( i == nRow, COLOR_BLUE, COLOR_PAPER ) )
         ENDIF
      NEXT

      DispEnd()

      nKey    := KeyStd( Inkey( 0, BitOr( INKEY_ALLEVENTS, INKEY_EXT ) ) )
      lInside := nRowPos < MRow() .AND. nColPos <= MCol() .AND. nRowPos + 6 > MRow() .AND. nColPos + nColLength > MCol()

      SWITCH nKey

      CASE K_ESC
      CASE K_CLOSE
      CASE K_RBUTTONDOWN
         RETURN NIL

      CASE K_MOUSEMOVE
         IF lInside
            nRow := MRow() - nRowPos
         ENDIF
         EXIT

      CASE K_LBUTTONDOWN
         RETURN IIF( lInside, nRow, NIL )

      CASE K_UP
      CASE K_MWFORWARD
         nRow := IIF( nRow > 1, nRow - 1, Len( aEditContextMenu ) )
         EXIT

      CASE K_DOWN
      CASE K_MWBACKWARD
         nRow := IIF( nRow < Len( aEditContextMenu ), nRow + 1, 1 )
         EXIT

      CASE K_ENTER
         RETURN nRow

      ENDSWITCH

   ENDDO

   RETURN NIL
