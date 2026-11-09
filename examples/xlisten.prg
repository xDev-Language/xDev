/*
 * $DOC$
 * $NAME$
 * xListen filterbank viewer
 *
 * $ONELINER$
 * Visualizes spectra and waterfalls from radio-telescope filterbank files.
 *
 * $DESCRIPTION$
 * Reads .fil data. Arrow keys move between spectra, Home and End select the first and last spectrum, W toggles the waterfall, I shows the header, and Esc exits. A 64 MB Voyager 1 sample is available at http://blpd0.ssl.berkeley.edu/Voyager_data/Voyager1.single_coarse.fine_res.fil.
 *
 * $EXAMPLES$
 * examples/xlisten.prg
 *
 * $OUTPUT$
 *
 * $DOCEND$
 */

#include "fileio.ch"
#include "inkey.ch"
#include "setcurs.ch"
#include "gtinfo.ch"
#include "gfxdef.ch"

#define MAX_KEYWORD  80

// Width of one coarse channel of Breakthrough Listen in MHz (187.5 MHz / 64)
#define COARSE_WIDTH 2.9296875

#define COLOR_BACK   0x101418
#define COLOR_GRID   0x303A44
#define COLOR_LABEL  0xA0A8B0
#define COLOR_TITLE  0xFFFFFF
#define COLOR_PLOT   0x40E070
#define COLOR_PEAK   0xFFD040

#define HELP_TEXT    "Arrows: spectrum   Home/End   W: waterfall   I: header   Esc: exit"

REQUEST GTGLFW

// Waterfall rows in dB, computed once for a given number of bins
STATIC s_aRows AS ARRAY := {}
STATIC s_nBins AS INT   := 0

PROCEDURE Main( cFileName )

   LOCAL oFil       AS OBJECT
   LOCAL nSpectrum  AS INT   := 0
   LOCAL lWaterfall AS LOGIC := .F.
   LOCAL nKey       AS INT

   IF cFileName == NIL
      QOut( "Usage: xlisten <file.fil>" )
      Inkey( 0, INKEY_KEYBOARD )
      RETURN
   ENDIF

   oFil := Filterbank():New( cFileName )

   IF ! oFil:Open()
      QOut( "xListen: " + oFil:error )
      oFil:Close()
      Inkey( 0, INKEY_KEYBOARD )
      RETURN
   ENDIF

   IF oFil:Spectra() == 0
      QOut( "xListen: the file has no spectra, it has " + Num( oFil:fileSize - oFil:headerSize ) + ;
            " bytes of data and one spectrum needs " + Num( oFil:SpectrumSize() ) )
      oFil:Close()
      Inkey( 0, INKEY_KEYBOARD )
      RETURN
   ENDIF

   SetMode( 40, 120 )
   SetCursor( SC_NONE )
   GtInfo( GTI_WINTITLE, "xListen - " + cFileName )

   Draw( oFil, nSpectrum, lWaterfall )

   DO WHILE .T.

      nKey := Inkey( 0, INKEY_KEYBOARD + INKEY_GTEVENT )

      DO CASE
      CASE nKey == K_ESC .OR. nKey == K_CLOSE
         EXIT
      CASE ( nKey == K_RIGHT .OR. nKey == K_DOWN ) .AND. nSpectrum < oFil:Spectra() - 1
         nSpectrum++
         Draw( oFil, nSpectrum, lWaterfall )
      CASE ( nKey == K_LEFT .OR. nKey == K_UP ) .AND. nSpectrum > 0
         nSpectrum--
         Draw( oFil, nSpectrum, lWaterfall )
      CASE nKey == K_HOME
         nSpectrum := 0
         Draw( oFil, nSpectrum, lWaterfall )
      CASE nKey == K_END
         nSpectrum := oFil:Spectra() - 1
         Draw( oFil, nSpectrum, lWaterfall )
      CASE nKey == K_RESIZE
         Draw( oFil, nSpectrum, lWaterfall )
      CASE nKey == Asc( "i" ) .OR. nKey == Asc( "I" )
         ShowHeader( oFil, cFileName )
         Draw( oFil, nSpectrum, lWaterfall )
      CASE nKey == Asc( "w" ) .OR. nKey == Asc( "W" )
         lWaterfall := ! lWaterfall
         Draw( oFil, nSpectrum, lWaterfall )
      ENDCASE

   ENDDO

   oFil:Close()

   RETURN

/* ---------------------------------------------------------------------------------------------
   Display
   --------------------------------------------------------------------------------------------- */

// Show the header of the file as text and wait for a key
STATIC PROCEDURE ShowHeader( oFil AS OBJECT, cFileName AS STRING )

   Cls()
   QOut( "xListen - " + cFileName )
   QOut( "" )
   QOut( "Source         " + oFil:sourceName )
   QOut( "Telescope      " + Num( oFil:telescopeId ) )
   QOut( "Machine        " + Num( oFil:machineId ) )
   QOut( "Channels       " + Num( oFil:channels ) )
   QOut( "Bits           " + Num( oFil:bits ) )
   QOut( "IFs            " + Num( oFil:ifs ) )
   QOut( "Frequency      " + Num( oFil:fch1 ) + " .. " + Num( oFil:LastFrequency() ) + " MHz" )
   QOut( "Channel width  " + Num( Abs( oFil:foff ) * 1000000 ) + " Hz" )
   QOut( "Sample time    " + Num( oFil:tsamp ) + " s" )
   QOut( "Start (MJD)    " + Num( oFil:tstart ) )
   QOut( "Spectra        " + Num( oFil:Spectra() ) )
   QOut( "Duration       " + Num( oFil:Spectra() * oFil:tsamp ) + " s" )
   QOut( "Header size    " + Num( oFil:headerSize ) + " bytes" )
   QOut( "File size      " + Num( oFil:fileSize ) + " bytes" )

   Inkey( 0, INKEY_KEYBOARD )

   Cls()
   RETURN

// Draw the spectrum or the waterfall
STATIC PROCEDURE Draw( oFil AS OBJECT, nSpectrum AS INT, lWaterfall AS LOGIC )

   IF lWaterfall
      DrawWaterfall( oFil, nSpectrum )
   ELSE
      DrawSpectrum( oFil, nSpectrum )
   ENDIF

   RETURN

// Draw one spectrum as power in dB against frequency, low frequencies on the left
STATIC PROCEDURE DrawSpectrum( oFil AS OBJECT, nSpectrum AS INT )

   LOCAL nCellWidth  AS INT := GtInfo( GTI_FONTWIDTH )
   LOCAL nCellHeight AS INT := GtInfo( GTI_FONTSIZE )
   LOCAL nWidth      AS INT := GtInfo( GTI_SCREENWIDTH )
   LOCAL nHeight     AS INT := GtInfo( GTI_SCREENHEIGHT )
   LOCAL nLeft       AS INT := 10 * nCellWidth
   LOCAL nTop        AS INT := 4 * nCellHeight
   LOCAL nRight      AS INT := nWidth - 3 * nCellWidth
   LOCAL nBottom     AS INT := nHeight - 3 * nCellHeight
   LOCAL nLowFreq    AS FLOAT := Min( oFil:fch1, oFil:LastFrequency() )
   LOCAL nHighFreq   AS FLOAT := Max( oFil:fch1, oFil:LastFrequency() )
   LOCAL aBins       AS ARRAY
   LOCAL nLow        AS FLOAT
   LOCAL nHigh       AS FLOAT
   LOCAL nIndex      AS INT
   LOCAL nX          AS INT
   LOCAL nY          AS INT
   LOCAL nLastY      AS INT
   LOCAL cLabel      AS STRING

   GfxPrimitive( GFX_ACQUIRESCREEN )
   GfxPrimitive( GFX_FILLEDRECT, 0, 0, nHeight - 1, nWidth - 1, COLOR_BACK )

   IF nRight - nLeft < 50 .OR. nBottom - nTop < 50
      GfxPrimitive( GFX_RELEASESCREEN )
      RETURN
   ENDIF

   aBins := oFil:Bins( nSpectrum, nRight - nLeft + 1 )

   nLow := aBins[ 1 ]
   nHigh := aBins[ 1 ]
   FOR nIndex := 2 TO Len( aBins )
      nLow := Min( nLow, aBins[ nIndex ] )
      nHigh := Max( nHigh, aBins[ nIndex ] )
   NEXT
   IF nHigh - nLow < 1
      nHigh := nLow + 1
   ENDIF

   // Grid and power labels
   FOR nIndex := 0 TO 4
      nY := nTop + Int( nIndex * ( nBottom - nTop ) / 4 )
      GfxPrimitive( GFX_LINE, nY, nLeft, nY, nRight, COLOR_GRID )
      GfxText( nY - Int( nCellHeight / 2 ), nCellWidth, Str( nHigh - nIndex * ( nHigh - nLow ) / 4, 7, 1 ), ;
               COLOR_LABEL, 0, 0 )
   NEXT

   // Grid and frequency labels
   FOR nIndex := 0 TO 4
      nX := nLeft + Int( nIndex * ( nRight - nLeft ) / 4 )
      GfxPrimitive( GFX_LINE, nTop, nX, nBottom, nX, COLOR_GRID )
      cLabel := LTrim( Str( nLowFreq + nIndex * ( nHighFreq - nLowFreq ) / 4, 12, 4 ) )
      nX := Max( Min( nX - Int( Len( cLabel ) * nCellWidth / 2 ), nWidth - Len( cLabel ) * nCellWidth ), 0 )
      GfxText( nBottom + Int( nCellHeight / 2 ), nX, cLabel, COLOR_LABEL, 0, 0 )
   NEXT

   // Spectrum
   FOR nIndex := 1 TO Len( aBins )
      nX := nLeft + nIndex - 1
      nY := nBottom - Int( ( aBins[ nIndex ] - nLow ) / ( nHigh - nLow ) * ( nBottom - nTop ) )
      IF nIndex > 1
         GfxPrimitive( GFX_LINE, nLastY, nX - 1, nY, nX, COLOR_PLOT )
      ENDIF
      nLastY := nY
   NEXT

   // Titles
   GfxText( 0, nCellWidth, "xListen - " + oFil:sourceName + "   spectrum " + Num( nSpectrum + 1 ) + "/" + ;
            Num( oFil:Spectra() ) + "   t = " + LTrim( Str( nSpectrum * oFil:tsamp, 10, 1 ) ) + " s", ;
            COLOR_TITLE, 0, 0 )
   GfxText( nCellHeight, nCellWidth, "Peak " + LTrim( Str( oFil:Frequency( oFil:peakChannel ), 12, 6 ) ) + ;
            " MHz   " + LTrim( Str( 10 * Log10( Max( oFil:peakPower, 1e-30 ) ), 7, 1 ) ) + " dB", ;
            COLOR_PEAK, 0, 0 )
   GfxText( 2 * nCellHeight, nCellWidth, "dB", COLOR_LABEL, 0, 0 )
   GfxText( nBottom + Int( nCellHeight * 3 / 2 ), nRight - 2 * nCellWidth, "MHz", COLOR_LABEL, 0, 0 )
   GfxText( nHeight - nCellHeight, nCellWidth, HELP_TEXT, COLOR_LABEL, 0, 0 )

   GfxPrimitive( GFX_RELEASESCREEN )

   RETURN

// Draw all spectra as a waterfall: frequency across, time down, power as colour
STATIC PROCEDURE DrawWaterfall( oFil AS OBJECT, nSpectrum AS INT )

   LOCAL nCellWidth  AS INT := GtInfo( GTI_FONTWIDTH )
   LOCAL nCellHeight AS INT := GtInfo( GTI_FONTSIZE )
   LOCAL nWidth      AS INT := GtInfo( GTI_SCREENWIDTH )
   LOCAL nHeight     AS INT := GtInfo( GTI_SCREENHEIGHT )
   LOCAL nLeft       AS INT := 10 * nCellWidth
   LOCAL nTop        AS INT := 4 * nCellHeight
   LOCAL nRight      AS INT := nWidth - 3 * nCellWidth
   LOCAL nBottom     AS INT := nHeight - 3 * nCellHeight
   LOCAL nCount      AS INT := oFil:Spectra()
   LOCAL nLowFreq    AS FLOAT := Min( oFil:fch1, oFil:LastFrequency() )
   LOCAL nHighFreq   AS FLOAT := Max( oFil:fch1, oFil:LastFrequency() )
   LOCAL aPalette    AS ARRAY := Palette()
   LOCAL aRow        AS ARRAY
   LOCAL nBins       AS INT
   LOCAL nLow        AS FLOAT
   LOCAL nHigh       AS FLOAT
   LOCAL nRow        AS INT
   LOCAL nIndex      AS INT
   LOCAL nX          AS INT
   LOCAL nY          AS INT
   LOCAL nRowTop     AS INT
   LOCAL nRowBottom  AS INT
   LOCAL nColor      AS INT
   LOCAL cLabel      AS STRING

   IF nRight - nLeft < 50 .OR. nBottom - nTop < 50
      GfxPrimitive( GFX_ACQUIRESCREEN )
      GfxPrimitive( GFX_FILLEDRECT, 0, 0, nHeight - 1, nWidth - 1, COLOR_BACK )
      GfxPrimitive( GFX_RELEASESCREEN )
      RETURN
   ENDIF

   nBins := nRight - nLeft + 1

   IF s_nBins != nBins
      GfxPrimitive( GFX_ACQUIRESCREEN )
      GfxPrimitive( GFX_FILLEDRECT, 0, 0, nHeight - 1, nWidth - 1, COLOR_BACK )
      GfxText( 0, nCellWidth, "Reading " + Num( nCount ) + " spectra...", COLOR_TITLE, 0, 0 )
      GfxPrimitive( GFX_RELEASESCREEN )
      s_aRows := Array( nCount )
      FOR nRow := 1 TO nCount
         s_aRows[ nRow ] := oFil:Bins( nRow - 1, nBins )
      NEXT
      s_nBins := nBins
   ENDIF

   nLow := s_aRows[ 1 ][ 1 ]
   nHigh := nLow
   FOR nRow := 1 TO nCount
      aRow := s_aRows[ nRow ]
      FOR nIndex := 1 TO nBins
         nLow := Min( nLow, aRow[ nIndex ] )
         nHigh := Max( nHigh, aRow[ nIndex ] )
      NEXT
   NEXT
   IF nHigh - nLow < 1
      nHigh := nLow + 1
   ENDIF

   GfxPrimitive( GFX_ACQUIRESCREEN )
   GfxPrimitive( GFX_FILLEDRECT, 0, 0, nHeight - 1, nWidth - 1, COLOR_BACK )

   // Rows of colour, the first spectrum on the top
   FOR nRow := 1 TO nCount
      aRow := s_aRows[ nRow ]
      nRowTop := nTop + Int( ( nRow - 1 ) * ( nBottom - nTop + 1 ) / nCount )
      nRowBottom := Max( nTop + Int( nRow * ( nBottom - nTop + 1 ) / nCount ) - 1, nRowTop )
      FOR nIndex := 1 TO nBins
         nColor := aPalette[ Max( Min( Int( ( aRow[ nIndex ] - nLow ) / ( nHigh - nLow ) * 255 ), 255 ), 0 ) + 1 ]
         nX := nLeft + nIndex - 1
         GfxPrimitive( GFX_FILLEDRECT, nRowTop, nX, nRowBottom, nX, nColor )
      NEXT
   NEXT

   // Marker of the current spectrum
   nRowTop := nTop + Int( nSpectrum * ( nBottom - nTop + 1 ) / nCount )
   nRowBottom := Max( nTop + Int( ( nSpectrum + 1 ) * ( nBottom - nTop + 1 ) / nCount ) - 1, nRowTop )
   GfxPrimitive( GFX_FILLEDRECT, nRowTop, nLeft - 6, nRowBottom, nLeft - 3, COLOR_PEAK )

   // Time labels
   FOR nIndex := 0 TO 4
      nY := nTop + Int( nIndex * ( nBottom - nTop ) / 4 )
      GfxText( nY - Int( nCellHeight / 2 ), nCellWidth, Str( nIndex * nCount * oFil:tsamp / 4, 7, 1 ), ;
               COLOR_LABEL, 0, 0 )
   NEXT

   // Frequency labels
   FOR nIndex := 0 TO 4
      nX := nLeft + Int( nIndex * ( nRight - nLeft ) / 4 )
      GfxPrimitive( GFX_LINE, nBottom + 1, nX, nBottom + 4, nX, COLOR_LABEL )
      cLabel := LTrim( Str( nLowFreq + nIndex * ( nHighFreq - nLowFreq ) / 4, 12, 4 ) )
      nX := Max( Min( nX - Int( Len( cLabel ) * nCellWidth / 2 ), nWidth - Len( cLabel ) * nCellWidth ), 0 )
      GfxText( nBottom + Int( nCellHeight / 2 ), nX, cLabel, COLOR_LABEL, 0, 0 )
   NEXT

   // Titles
   GfxText( 0, nCellWidth, "xListen - " + oFil:sourceName + "   waterfall of " + Num( nCount ) + ;
            " spectra   " + LTrim( Str( nCount * oFil:tsamp, 10, 1 ) ) + " s", COLOR_TITLE, 0, 0 )
   GfxText( nCellHeight, nCellWidth, "Power " + LTrim( Str( nLow, 7, 1 ) ) + " .. " + ;
            LTrim( Str( nHigh, 7, 1 ) ) + " dB", COLOR_PEAK, 0, 0 )
   GfxText( 2 * nCellHeight, nCellWidth, "s", COLOR_LABEL, 0, 0 )
   GfxText( nBottom + Int( nCellHeight * 3 / 2 ), nRight - 2 * nCellWidth, "MHz", COLOR_LABEL, 0, 0 )
   GfxText( nHeight - nCellHeight, nCellWidth, HELP_TEXT, COLOR_LABEL, 0, 0 )

   GfxPrimitive( GFX_RELEASESCREEN )

   RETURN

// Return 256 colours from black through blue and yellow to white
STATIC FUNCTION Palette() AS ARRAY

   LOCAL aPalette AS ARRAY := Array( 256 )
   LOCAL nIndex   AS INT

   FOR nIndex := 0 TO 255
      DO CASE
      CASE nIndex < 96
         aPalette[ nIndex + 1 ] := Blend( 0x000000, 0x1838B0, nIndex / 96 )
      CASE nIndex < 192
         aPalette[ nIndex + 1 ] := Blend( 0x1838B0, 0xF0C020, ( nIndex - 96 ) / 96 )
      OTHERWISE
         aPalette[ nIndex + 1 ] := Blend( 0xF0C020, 0xFFFFFF, ( nIndex - 192 ) / 63 )
      ENDCASE
   NEXT

   RETURN aPalette

// Mix two 0xRRGGBB colours; nPart 0 gives nFrom, 1 gives nTo
STATIC FUNCTION Blend( nFrom AS INT, nTo AS INT, nPart AS FLOAT ) AS INT

   LOCAL nRed   AS INT := Mix( BitAnd( BitShift( nFrom, -16 ), 0xFF ), BitAnd( BitShift( nTo, -16 ), 0xFF ), nPart )
   LOCAL nGreen AS INT := Mix( BitAnd( BitShift( nFrom, -8 ), 0xFF ), BitAnd( BitShift( nTo, -8 ), 0xFF ), nPart )
   LOCAL nBlue  AS INT := Mix( BitAnd( nFrom, 0xFF ), BitAnd( nTo, 0xFF ), nPart )

   RETURN BitOr( BitShift( nRed, 16 ), BitOr( BitShift( nGreen, 8 ), nBlue ) )

// Mix two colour components
STATIC FUNCTION Mix( nFrom AS INT, nTo AS INT, nPart AS FLOAT ) AS INT

   RETURN Int( nFrom + ( nTo - nFrom ) * nPart + 0.5 )

/* ---------------------------------------------------------------------------------------------
   Filterbank
   --------------------------------------------------------------------------------------------- */

CREATE CLASS Filterbank

   // Name and handle of the file
   DATA fileName    AS STRING INIT ""
   DATA handle      AS INT    INIT -1

   // Header fields of the SIGPROC format
   DATA sourceName  AS STRING INIT ""
   DATA telescopeId AS INT    INIT 0
   DATA machineId   AS INT    INIT 0
   DATA channels    AS INT    INIT 0
   DATA bits        AS INT    INIT 0
   DATA ifs         AS INT    INIT 1
   DATA fch1        AS FLOAT  INIT 0
   DATA foff        AS FLOAT  INIT 0
   DATA tstart      AS FLOAT  INIT 0
   DATA tsamp       AS FLOAT  INIT 0

   // Sizes in bytes
   DATA headerSize  AS INT    INIT 0
   DATA fileSize    AS INT    INIT 0

   // Reason of the last failure of Open()
   DATA error       AS STRING INIT ""

   // Strongest channel found by the last Bins()
   DATA peakChannel AS INT    INIT 0
   DATA peakPower   AS FLOAT  INIT 0

   METHOD New( cFileName )
   METHOD Open()
   METHOD Close()
   METHOD SpectrumSize()
   METHOD Spectra()
   METHOD Frequency( nChannel )
   METHOD LastFrequency()
   METHOD ReadSpectrum( nSpectrum )
   METHOD Bins( nSpectrum, nBins )

ENDCLASS

// Create a reader of the file cFileName
// Filterbank():New( cFileName ) --> self
METHOD New( cFileName ) CLASS Filterbank

   ::fileName := cFileName

   RETURN Self

// Open the file and read its header; return .F. and set :error when it fails
// oFil:Open() --> lSuccess
METHOD Open() CLASS Filterbank

   LOCAL cKeyword AS STRING
   LOCAL cType    AS STRING
   LOCAL cText    AS STRING
   LOCAL nValue   AS INT
   LOCAL nReal    AS FLOAT

   ::handle := FOpen( ::fileName, FO_READ )
   IF ::handle < 0
      ::error := "cannot open the file"
      RETURN .F.
   ENDIF

   ::fileSize := FSeek( ::handle, 0, FS_END )
   FSeek( ::handle, 0, FS_SET )

   IF ! ReadString( ::handle ) == "HEADER_START"
      ::error := "not a filterbank file, HEADER_START is missing"
      RETURN .F.
   ENDIF

   DO WHILE .T.

      cKeyword := ReadString( ::handle )
      IF cKeyword == "HEADER_END"
         EXIT
      ENDIF

      cType := KeywordType( cKeyword )

      DO CASE
      CASE cType == "S"
         cText := ReadString( ::handle )
         IF cKeyword == "source_name"
            ::sourceName := cText
         ENDIF

      CASE cType == "I"
         nValue := Bin2L( ReadBytes( ::handle, 4 ) )
         DO CASE
         CASE cKeyword == "telescope_id"
            ::telescopeId := nValue
         CASE cKeyword == "machine_id"
            ::machineId := nValue
         CASE cKeyword == "nchans"
            ::channels := nValue
         CASE cKeyword == "nbits"
            ::bits := nValue
         CASE cKeyword == "nifs"
            ::ifs := nValue
         ENDCASE

      CASE cType == "D"
         nReal := CToF( ReadBytes( ::handle, 8 ) )
         DO CASE
         CASE cKeyword == "fch1"
            ::fch1 := nReal
         CASE cKeyword == "foff"
            ::foff := nReal
         CASE cKeyword == "tstart"
            ::tstart := nReal
         CASE cKeyword == "tsamp"
            ::tsamp := nReal
         ENDCASE

      CASE cType == "B"
         ReadBytes( ::handle, 1 )

      CASE cKeyword == ""
         ::error := "the header is damaged or cut off"
         RETURN .F.

      OTHERWISE
         ::error := "unknown header keyword: " + cKeyword
         RETURN .F.

      ENDCASE

   ENDDO

   ::headerSize := FSeek( ::handle, 0, FS_RELATIVE )

   IF ::channels <= 0 .OR. ::bits <= 0
      ::error := "the header has no nchans or nbits"
      RETURN .F.
   ENDIF

   IF ::bits != 8 .AND. ::bits != 32
      ::error := "only 8 and 32 bit data are supported, the file has " + Num( ::bits )
      RETURN .F.
   ENDIF

   RETURN .T.

// Close the file
// oFil:Close() --> self
METHOD Close() CLASS Filterbank

   IF ::handle >= 0
      FClose( ::handle )
      ::handle := -1
   ENDIF

   RETURN Self

// Return the number of bytes of one spectrum (all channels and IFs of one time sample)
// oFil:SpectrumSize() --> nBytes
METHOD SpectrumSize() CLASS Filterbank

   RETURN Int( ::channels * ::ifs * ::bits / 8 )

// Return the number of spectra (time samples) in the file
// oFil:Spectra() --> nCount
METHOD Spectra() CLASS Filterbank

   RETURN Int( ( ::fileSize - ::headerSize ) / Max( ::SpectrumSize(), 1 ) )

// Return the frequency of a channel in MHz; channels count from 0 in the order of the file
// oFil:Frequency( nChannel ) --> nMHz
METHOD Frequency( nChannel ) CLASS Filterbank

   RETURN ::fch1 + nChannel * ::foff

// Return the frequency of the last channel in MHz
// oFil:LastFrequency() --> nMHz
METHOD LastFrequency() CLASS Filterbank

   RETURN ::Frequency( ::channels - 1 )

// Read the first IF of a spectrum as raw bytes; spectra count from 0
// oFil:ReadSpectrum( nSpectrum ) --> cData
METHOD ReadSpectrum( nSpectrum ) CLASS Filterbank

   FSeek( ::handle, ::headerSize + nSpectrum * ::SpectrumSize(), FS_SET )

   RETURN ReadBytes( ::handle, Int( ::channels * ::bits / 8 ) )

// Reduce a spectrum to nBins values in dB, low frequencies first; each value is the strongest
// channel of its bin, so a narrow signal stays visible. Sets :peakChannel and :peakPower.
// oFil:Bins( nSpectrum, nBins ) --> aBins
METHOD Bins( nSpectrum, nBins ) CLASS Filterbank

   LOCAL cData    AS STRING := ::ReadSpectrum( nSpectrum )
   LOCAL aBins    AS ARRAY  := Array( nBins )
   LOCAL nBin     AS INT
   LOCAL nFirst   AS INT
   LOCAL nLast    AS INT
   LOCAL nIndex   AS INT
   LOCAL nChannel AS INT
   LOCAL nRead    AS INT
   LOCAL nPower   AS FLOAT
   LOCAL nMax     AS FLOAT
   LOCAL nCoarse  AS INT := Round( ::channels * Abs( ::foff ) / COARSE_WIDTH, 0 )
   LOCAL nFine    AS INT := 0

   // The middle channel of every coarse channel holds a spike of the receiver (DC), not a signal
   IF nCoarse >= 1 .AND. ::channels % nCoarse == 0
      nFine := Int( ::channels / nCoarse )
   ENDIF

   ::peakChannel := 0
   ::peakPower := -1

   FOR nBin := 1 TO nBins

      nFirst := Int( ( nBin - 1 ) * ::channels / nBins )
      nLast := Max( Int( nBin * ::channels / nBins ) - 1, nFirst )
      nMax := -1

      FOR nIndex := nFirst TO nLast
         nChannel := IIF( ::foff < 0, ::channels - 1 - nIndex, nIndex )
         nRead := nChannel
         IF nFine > 1 .AND. nChannel % nFine == Int( nFine / 2 )
            nRead := nChannel - 1
         ENDIF
         IF ::bits == 32
            nPower := Bin2F( SubStr( cData, nRead * 4 + 1, 4 ) )
         ELSE
            nPower := Asc( SubStr( cData, nRead + 1, 1 ) )
         ENDIF
         IF nPower > nMax
            nMax := nPower
         ENDIF
         IF nPower > ::peakPower
            ::peakPower := nPower
            ::peakChannel := nChannel
         ENDIF
      NEXT

      aBins[ nBin ] := 10 * Log10( Max( nMax, 1e-30 ) )

   NEXT

   RETURN aBins

/* ---------------------------------------------------------------------------------------------
   Reading of the file
   --------------------------------------------------------------------------------------------- */

// Return the type of the value of a header keyword: S string, I integer, D double, B byte, "" unknown
STATIC FUNCTION KeywordType( cKeyword AS STRING ) AS STRING

   LOCAL cName AS STRING := "|" + cKeyword + "|"

   DO CASE
   CASE cName $ "|source_name|rawdatafile|"
      RETURN "S"
   CASE cName $ "|telescope_id|machine_id|data_type|nchans|nbits|nifs|nbeams|ibeam|barycentric|pulsarcentric|nsamples|"
      RETURN "I"
   CASE cName $ "|fch1|foff|tstart|tsamp|az_start|za_start|src_raj|src_dej|refdm|period|"
      RETURN "D"
   CASE cName $ "|signed|"
      RETURN "B"
   ENDCASE

   RETURN ""

// Read a header string: 4-byte length, then the characters; "" when the length is not valid
STATIC FUNCTION ReadString( nHandle AS INT ) AS STRING

   LOCAL nLength AS INT := Bin2L( ReadBytes( nHandle, 4 ) )

   IF nLength <= 0 .OR. nLength > MAX_KEYWORD
      RETURN ""
   ENDIF

   RETURN ReadBytes( nHandle, nLength )

// Read nCount bytes; fewer at the end of the file
STATIC FUNCTION ReadBytes( nHandle AS INT, nCount AS INT ) AS STRING

   LOCAL cBuffer AS STRING := Space( nCount )
   LOCAL nRead   AS INT    := FRead( nHandle, @cBuffer, nCount )

   RETURN Left( cBuffer, nRead )

// Return a number as text without leading spaces
STATIC FUNCTION Num( nValue AS FLOAT ) AS STRING

   RETURN LTrim( Str( nValue ) )
