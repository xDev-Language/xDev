/*
 * $DOC$
 * $NAME$
 * Terminal sidebar menu
 *
 * $ONELINER$
 * Demonstrates a keyboard- and mouse-driven terminal sidebar.
 *
 * $DESCRIPTION$
 * Displays sample business menu items, handles navigation and terminal resizing, and exits on Esc or window close.
 *
 * $EXAMPLES$
 * examples/sidebar.prg
 *
 * $OUTPUT$
 *
 * $DOCEND$
 */

#include "inkey.ch"
#include "tui.ch"

#if OS_LINUX
   REQUEST GTXWC
#else
   REQUEST GTWVT
#endif

PROCEDURE Main()

   LOCAL lQuit := .F.
   LOCAL nKey
   LOCAL oSidebar

   SetColorExt( COLOR_TEXT, COLOR_PAPER )
   Scroll()

   oSidebar := TSidebar():New( 0, 0, 23, "NIP:", { ;
      "START", ;
      "LISTA SPRZEDAŻY", ;
      "LISTA ZAKUPU", ;
      "LISTA KASA FISKALNA", ;
      "ROZLICZENIA", ;
      "LISTA JPK", ;
      "EWIDENCJE", ;
      "BAZA KONTRAHENTÓW" } )

   oSidebar:Display()

   DO WHILE ! lQuit

      nKey := Inkey( 1, INKEY_ALLEVENTS )

      SWITCH nKey

         CASE K_ESC
            lQuit := .T.
            EXIT

         CASE K_CLOSE
            lQuit := .T.
            EXIT

         CASE K_RESIZE
            DispBegin()
            Scroll()
            oSidebar:Display()
            DispEnd()
            EXIT

         OTHERWISE
            IF oSidebar:HandleKey( nKey )
               DispOutAtExt( 1, 25, PadR( oSidebar:items[ oSidebar:active ], 40 ), COLOR_TEXT, COLOR_PAPER )
            ENDIF

      ENDSWITCH

   ENDDO

RETURN
