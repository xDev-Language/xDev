/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library: xDev TUI
 * File: dbedit.prg
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "inkey.ch"
#include "dbedit.ch"
#include "tbrowse.ch"

/* ---------------------------------------------------------------------------------------------
   DBEdit() on TBrowse, for the records of the current work area.

   Like READ it has its own Inkey() loop. Two limits, both because xDev compiles
   no text at run time:
   - the user function is a code block {| nMode, nCol | ... } instead of a function name,
   - an element of acColumns is a field name or a code block, not any expression in a string.
   --------------------------------------------------------------------------------------------- */

// Browse the records of the current work area in a window
// DBEdit( [nTop], [nLeft], [nBottom], [nRight], [acColumns], [bUserFunc], ... ) --> NIL
FUNCTION DBEdit( nTop, nLeft, nBottom, nRight, acColumns, bUserFunc, xPictures, xHeaders, ;
                 xHeadSeps, xColSeps, xFootSeps, xFootings )

   LOCAL lWholeScreen := nBottom == NIL .AND. nRight == NIL
   LOCAL oBrowse
   LOCAL oCol
   LOCAL xColumn
   LOCAL nColumn
   LOCAL nKey
   LOCAL nResult

   IF ! Used()
      RETURN NIL
   ENDIF

   nTop := IIF( nTop == NIL, 0, nTop )
   nLeft := IIF( nLeft == NIL, 0, nLeft )
   nBottom := IIF( nBottom == NIL, MaxRow(), nBottom )
   nRight := IIF( nRight == NIL, MaxCol(), nRight )
   oBrowse := TBrowse():New( nTop, nLeft, nBottom, nRight )
   oBrowse:goTopBlock    := {|| dbGoTop() }
   oBrowse:goBottomBlock := {|| dbGoBottom() }
   oBrowse:skipBlock     := {| n | Skipper( n ) }

   IF acColumns == NIL
      acColumns := {}
      FOR nColumn := 1 TO FCount()
         AAdd( acColumns, FieldName( nColumn ) )
      NEXT
   ENDIF

   FOR nColumn := 1 TO Len( acColumns )
      xColumn := acColumns[ nColumn ]
      IF ValType( xColumn ) == "B"
         // A block has no name: the heading is the number of the column
         oCol := TBColumn():New( "Column " + LTrim( Str( nColumn ) ), xColumn )
      ELSE
         oCol := TBColumn():New( xColumn, FieldBlock( xColumn ) )
      ENDIF

      oCol:picture := Item( xPictures, nColumn, "" )
      oCol:heading := Item( xHeaders, nColumn, oCol:heading )
      oCol:headSep := Item( xHeadSeps, nColumn, "═" )
      oCol:colSep  := Item( xColSeps, nColumn, "│" )
      oCol:footSep := Item( xFootSeps, nColumn, "" )
      oCol:footing := Item( xFootings, nColumn, "" )
      oBrowse:AddColumn( oCol )
   NEXT

   DO WHILE .T.
      oBrowse:ForceStable()

      // No key pending: one call of the user function, then wait for a key
      nKey := Inkey()
      IF nKey == 0
         IF bUserFunc != NIL
            nResult := Eval( bUserFunc, Mode( oBrowse ), oBrowse:colPos )
            IF nResult == DE_ABORT
               EXIT
            ELSEIF nResult == DE_REFRESH
               oBrowse:RefreshAll()
               LOOP
            ENDIF
         ENDIF

         nKey := Inkey( 0, INKEY_ALLEVENTS )
      ENDIF

      DO CASE
      CASE nKey == K_RESIZE
         // The whole screen follows the window, a given rectangle stays
         IF lWholeScreen
            oBrowse:nBottom := MaxRow()
            oBrowse:nRight  := MaxCol()
         ENDIF

         oBrowse:Configure()
      CASE bUserFunc == NIL .AND. ( nKey == K_ESC .OR. nKey == K_ENTER )
         EXIT
      CASE nKey != K_ESC .AND. nKey != K_ENTER .AND. oBrowse:ApplyKey( nKey ) != TBR_EXCEPTION
         // A movement key of TBrowse
      CASE bUserFunc != NIL
         nResult := Eval( bUserFunc, DE_EXCEPT, oBrowse:colPos )
         IF nResult == DE_ABORT
            EXIT
         ELSEIF nResult == DE_REFRESH
            oBrowse:RefreshAll()
         ENDIF
      ENDCASE
   ENDDO

   RETURN NIL

/* ---------------------------------------------------------------------------------------------
   Functions of this file
   --------------------------------------------------------------------------------------------- */

// Mode of an idle call of the user function
STATIC FUNCTION Mode( oBrowse )

   DO CASE
   CASE Eof() .AND. ! s_HasVisibleRecords()
      RETURN DE_EMPTY
   CASE oBrowse:hitTop
      RETURN DE_HITTOP
   CASE oBrowse:hitBottom
      RETURN DE_HITBOTTOM
   ENDCASE

   RETURN DE_IDLE

// .T. when the work area shows a record: a filter or SET DELETED ON can hide all of them, so
// LastRec() is not enough; the current record stays where it was
STATIC FUNCTION s_HasVisibleRecords()

   LOCAL nRecord := RecNo()
   LOCAL lVisible

   dbGoTop()
   lVisible := ! Eof()
   dbGoto( nRecord )

   RETURN lVisible

// Element nColumn of an array argument, a single string for all columns, or xDefault
STATIC FUNCTION Item( xValue, nColumn, xDefault )

   IF ValType( xValue ) == "A"
      RETURN IIF( nColumn <= Len( xValue ) .AND. xValue[ nColumn ] != NIL, xValue[ nColumn ], xDefault )
   ENDIF

   RETURN IIF( xValue == NIL, xDefault, xValue )

// Skip block of the browse: moves at most nRequest records and returns how many it moved
STATIC FUNCTION Skipper( nRequest )

   LOCAL nDone := 0

   IF LastRec() == 0 .OR. nRequest == 0
      dbSkip( 0 )
      RETURN 0
   ENDIF

   DO WHILE nDone < nRequest
      dbSkip( 1 )
      IF Eof()
         dbSkip( -1 )
         EXIT
      ENDIF

      nDone++
   ENDDO

   DO WHILE nDone > nRequest
      dbSkip( -1 )
      IF Bof()
         EXIT
      ENDIF

      nDone--
   ENDDO

   RETURN nDone
