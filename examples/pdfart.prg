/*
 *
 */

#include "hpdf.ch"
#include "std.ch"

#if OS_LINUX
   #define FONT_FILE  "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
#else
   #define FONT_FILE  "C:\Windows\Fonts\arialbd.ttf"
#endif

#define PAGE_WIDTH    595
#define PAGE_HEIGHT   842

#define BALL_X        1
#define BALL_Y        2
#define BALL_DX       3
#define BALL_DY       4
#define BALL_COLOR    5

STATIC s_pPdf
STATIC s_pFont

PROCEDURE Main()

   s_pPdf := HPDF_New()
   HPDF_SetCompressionMode( s_pPdf, HPDF_COMP_ALL )
   HPDF_SetInfoAttr( s_pPdf, HPDF_INFO_TITLE, "Grafika w PDF z xDev" )
   HPDF_UseUTFEncodings( s_pPdf )
   HPDF_SetCurrentEncoder( s_pPdf, "UTF-8" )
   s_pFont := HPDF_GetFont( s_pPdf, HPDF_LoadTTFontFromFile( s_pPdf, FONT_FILE, HPDF_TRUE ), "UTF-8" )

   s_BouncingCircles()
   s_Spirograph()
   s_Flower()
   s_Mixing()
   s_Screensaver()

   IF HPDF_SaveToFile( s_pPdf, "pdfart.pdf" ) == HPDF_OK
      ? "pdfart.pdf saved"
   ELSE
      ? "pdfart.pdf not saved, libharu error " + LTrim( Str( HPDF_GetError( s_pPdf ) ) )
   ENDIF

   HPDF_Free( s_pPdf )

   RETURN

/* ---------------------------------------------------------------------------------------------
   Pages
   --------------------------------------------------------------------------------------------- */

// Circles start in the middle and leave a trace; each bounce gives a new colour
STATIC PROCEDURE s_BouncingCircles()

   LOCAL pPage   := s_NewPage( "Odbijające się kółka", { 0.05, 0.05, 0.1 }, { 1, 1, 1 } )
   LOCAL aBalls  := {}
   LOCAL nRadius := 14
   LOCAL nTop    := PAGE_HEIGHT - 70
   LOCAL nStep
   LOCAL nBall
   LOCAL aBall
   LOCAL nAngle

   FOR nBall := 1 TO 7
      nAngle := nBall * 2 * Pi() / 7 + 0.3
      AAdd( aBalls, { PAGE_WIDTH / 2, nTop / 2, Cos( nAngle ) * 3.2, Sin( nAngle ) * 3.2, s_Hue( nBall / 7 ) } )
   NEXT

   HPDF_Page_SetExtGState( pPage, s_Alpha( 0.45 ) )
   HPDF_Page_SetLineWidth( pPage, 0.8 )

   FOR nStep := 1 TO 1000
      FOR nBall := 1 TO Len( aBalls )
         aBall := aBalls[ nBall ]

         IF aBall[ BALL_X ] + aBall[ BALL_DX ] < 30 + nRadius .OR. aBall[ BALL_X ] + aBall[ BALL_DX ] > PAGE_WIDTH - 30 - nRadius
            aBall[ BALL_DX ]    := -aBall[ BALL_DX ]
            aBall[ BALL_COLOR ] := s_Hue( Random( 0, 1000 ) / 1000 )
         ENDIF
         IF aBall[ BALL_Y ] + aBall[ BALL_DY ] < 30 + nRadius .OR. aBall[ BALL_Y ] + aBall[ BALL_DY ] > nTop - nRadius
            aBall[ BALL_DY ]    := -aBall[ BALL_DY ]
            aBall[ BALL_COLOR ] := s_Hue( Random( 0, 1000 ) / 1000 )
         ENDIF

         aBall[ BALL_X ] += aBall[ BALL_DX ]
         aBall[ BALL_Y ] += aBall[ BALL_DY ]

         IF nStep % 3 == 0
            s_Stroke( pPage, aBall[ BALL_COLOR ] )
            HPDF_Page_Circle( pPage, aBall[ BALL_X ], aBall[ BALL_Y ], nRadius )
            HPDF_Page_Stroke( pPage )
         ENDIF
      NEXT
   NEXT

   RETURN

// Hypotrochoid: a point at distance nD from the centre of a circle of radius nSmall rolling inside a
// circle of radius nBig
STATIC PROCEDURE s_Spirograph()

   LOCAL pPage  := s_NewPage( "Spirograf", { 0, 0, 0 }, { 1, 1, 1 } )
   LOCAL nBig   := 10
   LOCAL nSmall := 7
   LOCAL nD     := 5
   LOCAL nScale := 230 / ( nBig - nSmall + nD )
   LOCAL nX0    := PAGE_WIDTH / 2
   LOCAL nY0    := PAGE_HEIGHT / 2 - 20
   LOCAL nEnd   := 14 * Pi()
   LOCAL nT     := 0
   LOCAL nX
   LOCAL nY
   LOCAL nLastX := nX0 + ( nBig - nSmall + nD ) * nScale
   LOCAL nLastY := nY0

   HPDF_Page_SetLineWidth( pPage, 1.2 )
   HPDF_Page_SetLineCap( pPage, HPDF_ROUND_END )

   DO WHILE nT < nEnd
      nT += 0.01
      nX := nX0 + ( ( nBig - nSmall ) * Cos( nT ) + nD * Cos( ( nBig - nSmall ) / nSmall * nT ) ) * nScale
      nY := nY0 + ( ( nBig - nSmall ) * Sin( nT ) - nD * Sin( ( nBig - nSmall ) / nSmall * nT ) ) * nScale

      s_Stroke( pPage, s_Hue( nT / nEnd ) )
      HPDF_Page_MoveTo( pPage, nLastX, nLastY )
      HPDF_Page_LineTo( pPage, nX, nY )
      HPDF_Page_Stroke( pPage )

      nLastX := nX
      nLastY := nY
   ENDDO

   RETURN

// Petals are two Bezier curves; every petal and every word is the same drawing in a rotated
// coordinate system
STATIC PROCEDURE s_Flower()

   LOCAL pPage   := s_NewPage( "Kwiat z krzywych Béziera", { 0.04, 0.08, 0.06 }, { 1, 1, 1 } )
   LOCAL nX0     := PAGE_WIDTH / 2
   LOCAL nY0     := PAGE_HEIGHT / 2 - 20
   LOCAL nPetals := 36
   LOCAL nWords  := 20
   LOCAL nAngle
   LOCAL nIndex

   HPDF_Page_SetLineWidth( pPage, 0.6 )
   HPDF_Page_SetExtGState( pPage, s_Alpha( 0.35 ) )

   FOR nIndex := 1 TO nPetals
      nAngle := nIndex * 2 * Pi() / nPetals

      HPDF_Page_GSave( pPage )
      HPDF_Page_Concat( pPage, Cos( nAngle ), Sin( nAngle ), -Sin( nAngle ), Cos( nAngle ), nX0, nY0 )
      s_Fill( pPage, s_Hue( nIndex / nPetals ) )
      s_Stroke( pPage, { 1, 1, 1 } )
      HPDF_Page_MoveTo( pPage, 0, 0 )
      HPDF_Page_CurveTo( pPage, 50, 45, 150, 45, 205, 0 )
      HPDF_Page_CurveTo( pPage, 150, -45, 50, -45, 0, 0 )
      HPDF_Page_FillStroke( pPage )
      HPDF_Page_GRestore( pPage )
   NEXT

   HPDF_Page_SetExtGState( pPage, s_Alpha( 1 ) )
   s_Fill( pPage, { 1, 1, 1 } )
   HPDF_Page_Circle( pPage, nX0, nY0, 12 )
   HPDF_Page_Fill( pPage )

   HPDF_Page_BeginText( pPage )
   HPDF_Page_SetFontAndSize( pPage, s_pFont, 16 )
   HPDF_Page_SetTextRenderingMode( pPage, HPDF_STROKE )
   HPDF_Page_SetLineWidth( pPage, 0.7 )
   FOR nIndex := 1 TO nWords
      nAngle := nIndex * 2 * Pi() / nWords
      s_Stroke( pPage, s_Hue( nIndex / nWords ) )
      HPDF_Page_SetTextMatrix( pPage, Cos( nAngle - Pi() / 2 ), Sin( nAngle - Pi() / 2 ), -Sin( nAngle - Pi() / 2 ), ;
                               Cos( nAngle - Pi() / 2 ), nX0 + Cos( nAngle ) * 240, nY0 + Sin( nAngle ) * 240 )
      HPDF_Page_ShowText( pPage, "xDev" )
   NEXT
   HPDF_Page_EndText( pPage )

   RETURN

// Multiply works like transparent inks on paper: cyan and yellow give green, all three give black
STATIC PROCEDURE s_Mixing()

   LOCAL pPage   := s_NewPage( "Mieszanie kolorów", { 1, 1, 1 }, { 0, 0, 0 } )
   LOCAL pGState := HPDF_CreateExtGState( s_pPdf )
   LOCAL aInks   := { { 0, 0.75, 1 }, { 1, 0.2, 0.75 }, { 1, 0.9, 0 } }
   LOCAL nX0     := PAGE_WIDTH / 2
   LOCAL nY0     := PAGE_HEIGHT - 300
   LOCAL nInk
   LOCAL nRing
   LOCAL nAngle

   HPDF_ExtGState_SetBlendMode( pGState, HPDF_BM_MULTIPLY )
   HPDF_Page_GSave( pPage )
   HPDF_Page_SetExtGState( pPage, pGState )
   FOR nInk := 1 TO Len( aInks )
      nAngle := Pi() / 2 + ( nInk - 1 ) * 2 * Pi() / 3
      s_Fill( pPage, aInks[ nInk ] )
      HPDF_Page_Circle( pPage, nX0 + Cos( nAngle ) * 70, nY0 + Sin( nAngle ) * 70, 120 )
      HPDF_Page_Fill( pPage )
   NEXT
   HPDF_Page_GRestore( pPage )

   HPDF_Page_SetLineCap( pPage, HPDF_ROUND_END )
   FOR nRing := 1 TO 12
      s_Stroke( pPage, s_Hue( nRing / 12 ) )
      HPDF_Page_SetLineWidth( pPage, 2 + nRing / 3 )
      HPDF_Page_SetDash( pPage, { 0.1, 6 + nRing * 2 }, 2, nRing * 3 )
      HPDF_Page_Circle( pPage, nX0, 190, 12 * nRing )
      HPDF_Page_Stroke( pPage )
   NEXT

   RETURN

// The circle of the screensaver, large: a ring of 44 circles, each in another colour of the rainbow
STATIC PROCEDURE s_Screensaver()

   LOCAL pPage   := s_NewPage( "Tęczowa rozeta", { 0.17, 0.18, 0.2 }, { 1, 1, 1 } )
   LOCAL nRadius := 120
   LOCAL nX0     := PAGE_WIDTH / 2
   LOCAL nY0     := PAGE_HEIGHT / 2 - 20
   LOCAL nIndex

   HPDF_Page_SetExtGState( pPage, s_Alpha( 0.85 ) )
   HPDF_Page_SetLineWidth( pPage, 1.2 )

   FOR nIndex := 1 TO 44
      s_Stroke( pPage, s_Hue( nIndex / 44 ) )
      HPDF_Page_Circle( pPage, nX0 + Sin( nIndex ) * nRadius, nY0 + Cos( nIndex ) * nRadius, nRadius )
      HPDF_Page_Stroke( pPage )
   NEXT

   RETURN

/* ---------------------------------------------------------------------------------------------
   Helpers
   --------------------------------------------------------------------------------------------- */

// New A4 page with the background colour and the title
STATIC FUNCTION s_NewPage( cTitle, aBackground, aTitle )

   LOCAL pPage := HPDF_AddPage( s_pPdf )

   HPDF_Page_SetSize( pPage, HPDF_PAGE_SIZE_A4, HPDF_PAGE_PORTRAIT )

   s_Fill( pPage, aBackground )
   HPDF_Page_Rectangle( pPage, 0, 0, PAGE_WIDTH, PAGE_HEIGHT )
   HPDF_Page_Fill( pPage )

   s_Fill( pPage, aTitle )
   HPDF_Page_BeginText( pPage )
   HPDF_Page_SetFontAndSize( pPage, s_pFont, 22 )
   HPDF_Page_TextOut( pPage, ( PAGE_WIDTH - HPDF_Page_TextWidth( pPage, cTitle ) ) / 2, PAGE_HEIGHT - 50, cTitle )
   HPDF_Page_EndText( pPage )

   RETURN pPage

// Graphics state with the same transparency of lines and fills (1 = opaque)
STATIC FUNCTION s_Alpha( nAlpha )

   LOCAL pGState := HPDF_CreateExtGState( s_pPdf )

   HPDF_ExtGState_SetAlphaFill( pGState, nAlpha )
   HPDF_ExtGState_SetAlphaStroke( pGState, nAlpha )

   RETURN pGState

STATIC PROCEDURE s_Fill( pPage, aColor )

   HPDF_Page_SetRGBFill( pPage, aColor[ 1 ], aColor[ 2 ], aColor[ 3 ] )

   RETURN

STATIC PROCEDURE s_Stroke( pPage, aColor )

   HPDF_Page_SetRGBStroke( pPage, aColor[ 1 ], aColor[ 2 ], aColor[ 3 ] )

   RETURN

// Fully saturated colour of the hue 0..1 (red, yellow, green, cyan, blue, magenta, red)
STATIC FUNCTION s_Hue( nHue )

   LOCAL nSector := ( nHue - Int( nHue ) ) * 6
   LOCAL nPart   := nSector - Int( nSector )

   DO CASE
   CASE nSector < 1
      RETURN { 1, nPart, 0 }
   CASE nSector < 2
      RETURN { 1 - nPart, 1, 0 }
   CASE nSector < 3
      RETURN { 0, 1, nPart }
   CASE nSector < 4
      RETURN { 0, 1 - nPart, 1 }
   CASE nSector < 5
      RETURN { nPart, 0, 1 }
   ENDCASE

   RETURN { 1, 0, 1 - nPart }
