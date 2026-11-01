/*
 *
 */

#include "inkey.ch"

#if OS_LINUX
   REQUEST GTXWC
#else
   REQUEST GTWVT
#endif

PROCEDURE Main()

   LOCAL lQuit := .F.
   LOCAL nKey

   SetColor( "N/W" )
   Scroll()

   DO WHILE ! lQuit

      nKey := Inkey( 1, INKEY_ALL + INKEY_EXT )

      SWITCH nKey

         CASE K_ESC
            lQuit := .T.
            EXIT

         CASE K_CLOSE
            lQuit := .T.
            EXIT

      ENDSWITCH

   ENDDO

RETURN
