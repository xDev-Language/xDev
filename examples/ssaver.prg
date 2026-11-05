/*
 *
 */

#include "gfxdef.ch"
#include "gtinfo.ch"
#include "inkey.ch"
#include "setcurs.ch"

#if OS_LINUX
   REQUEST GTXWC
#else
   REQUEST GTWVT
#endif

#define CIRCLE_ROW    1
#define CIRCLE_COL    2
#define CIRCLE_DROW   3
#define CIRCLE_DCOL   4
#define CIRCLE_HUE    5

#define RADIUS        100
#define RING          44
#define MAX_CIRCLES   3

PROCEDURE Main()

   LOCAL nOldCursor := SetCursor( SC_NONE )
   LOCAL nHeight    := 0
   LOCAL nWidth     := 0
   LOCAL nAdded     := Seconds()
   LOCAL aCircles
   LOCAL aCircle
   LOCAL nCircle

   GtInfo( GTI_MAXIMIZED, .T. )
   Scroll()

   aCircles := { s_NewCircle( -1 ) }

   DO WHILE Inkey( 0.01, INKEY_KEYBOARD + INKEY_LDOWN + INKEY_RDOWN ) == 0

      IF nHeight != GtInfo( GTI_SCREENHEIGHT ) .OR. nWidth != GtInfo( GTI_SCREENWIDTH )
         nHeight := GtInfo( GTI_SCREENHEIGHT )
         nWidth  := GtInfo( GTI_SCREENWIDTH )
      ENDIF

      FOR nCircle := 1 TO Len( aCircles )
         aCircle := aCircles[ nCircle ]

         IF aCircle[ CIRCLE_ROW ] <= RADIUS * 2 .OR. aCircle[ CIRCLE_ROW ] >= nHeight - RADIUS * 2 - 1
            aCircle[ CIRCLE_DROW ] := -aCircle[ CIRCLE_DROW ]
            aCircle[ CIRCLE_HUE ]  := Random( 0, 1000 ) / 1000
         ENDIF
         IF aCircle[ CIRCLE_COL ] <= RADIUS * 2 .OR. aCircle[ CIRCLE_COL ] >= nWidth - RADIUS * 2 - 1
            aCircle[ CIRCLE_DCOL ] := -aCircle[ CIRCLE_DCOL ]
            aCircle[ CIRCLE_HUE ]  := Random( 0, 1000 ) / 1000
         ENDIF

         aCircle[ CIRCLE_ROW ] += aCircle[ CIRCLE_DROW ]
         aCircle[ CIRCLE_COL ] += aCircle[ CIRCLE_DCOL ]
      NEXT

      s_View( aCircles, nHeight, nWidth )

      IF Len( aCircles ) < MAX_CIRCLES .AND. Seconds() - nAdded > 15
         AAdd( aCircles, s_NewCircle( Len( aCircles ) + 1 ) )
         nAdded := Seconds()
      ENDIF

   ENDDO

   SetCursor( nOldCursor )

   RETURN

// A circle in the middle of the window, moving one pixel down and nSpeed pixels sideways per frame
STATIC FUNCTION s_NewCircle( nSpeed )

   RETURN { Int( GtInfo( GTI_SCREENHEIGHT ) / 2 ), Int( GtInfo( GTI_SCREENWIDTH ) / 2 ), 1, nSpeed, ;
            Random( 0, 1000 ) / 1000 }

// One frame: the background and every circle as a ring of rainbow circles
STATIC PROCEDURE s_View( aCircles, nHeight, nWidth )

   LOCAL aCircle
   LOCAL nCircle
   LOCAL nIndex

   GfxPrimitive( GFX_ACQUIRESCREEN )
   GfxPrimitive( GFX_FILLEDRECT, 0, 0, nHeight - 1, nWidth - 1, 0x000000 )

   FOR nCircle := 1 TO Len( aCircles )
      aCircle := aCircles[ nCircle ]

      FOR nIndex := 1 TO RING
         GfxPrimitive( GFX_CIRCLE, Int( Cos( nIndex ) * RADIUS + aCircle[ CIRCLE_ROW ] ), ;
                       Int( Sin( nIndex ) * RADIUS + aCircle[ CIRCLE_COL ] ), RADIUS, ;
                       s_Rainbow( aCircle[ CIRCLE_HUE ] + nIndex / RING ) )
      NEXT
   NEXT

   GfxPrimitive( GFX_RELEASESCREEN )

   RETURN

// Colour 0xRRGGBB of the hue (0..1 is red, yellow, green, cyan, blue, magenta and red again)
STATIC FUNCTION s_Rainbow( nHue )

   LOCAL nSector := ( nHue - Int( nHue ) ) * 6
   LOCAL nUp     := Int( ( nSector - Int( nSector ) ) * 255 )
   LOCAL nDown   := 255 - nUp

   DO CASE
   CASE nSector < 1
      RETURN 0xFF0000 + nUp * 0x100
   CASE nSector < 2
      RETURN nDown * 0x10000 + 0x00FF00
   CASE nSector < 3
      RETURN 0x00FF00 + nUp
   CASE nSector < 4
      RETURN nDown * 0x100 + 0x0000FF
   CASE nSector < 5
      RETURN nUp * 0x10000 + 0x0000FF
   ENDCASE

   RETURN 0xFF0000 + nDown
