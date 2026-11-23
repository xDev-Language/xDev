/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev TUI
 * File......: getsys.prg
 *
 */

#include "inkey.ch"
#include "set.ch"
#include "button.ch"
#include "getexit.ch"

/* ---------------------------------------------------------------------------------------------
   READ. ReadModal() has its own Inkey() loop, the only exception to the rule
   that a TUI control takes its keys from the program. A Get also
   works without READ: Get():New(), :SetFocus() and GetApplyKey() in the loop of the program.
   --------------------------------------------------------------------------------------------- */

// State of the current READ; ReadModal() saves it and restores it, so a READ can run inside another;
// only s_lUpdated stays, Updated() tells whether the last READ changed a variable
STATIC s_oActive
STATIC s_cReadVar := ""
STATIC s_lUpdated := .F.
STATIC s_lKill    := .F.
STATIC s_nJump    := 0     // Get chosen with the mouse, 0 without one

// Edit the Gets of GetList from nGet on until a key ends the READ; .T. when a variable changed
// ReadModal( GetList, [nGet] ) --> lUpdated
FUNCTION ReadModal( GetList, nGet )

   LOCAL aSaved := { s_oActive, s_cReadVar, s_lKill, s_nJump }
   LOCAL nPos := IIF( nGet == NIL, 1, nGet )
   LOCAL nNext
   LOCAL nDir := 1
   LOCAL oGet

   s_lUpdated := .F.
   s_lKill    := .F.
   s_nJump    := 0

   DO WHILE nPos >= 1 .AND. nPos <= Len( GetList ) .AND. ! s_lKill
      oGet       := GetList[ nPos ]
      s_oActive  := oGet
      s_cReadVar := Upper( oGet:name )
      IF oGet:reader == NIL
         GetReader( oGet, GetList )
      ELSE
         Eval( oGet:reader, oGet, GetList )
      ENDIF

      nNext := NextGet( GetList, nPos, oGet:exitState, nDir )
      IF nNext > 0 .AND. nNext != nPos
         nDir := IIF( nNext < nPos, -1, 1 )
      ENDIF

      nPos := nNext
   ENDDO

   s_oActive  := aSaved[ 1 ]
   s_cReadVar := aSaved[ 2 ]
   s_lKill    := aSaved[ 3 ]
   s_nJump    := aSaved[ 4 ]

   RETURN s_lUpdated

// The standard editing of one Get: WHEN, keys until an exit, VALID
FUNCTION GetReader( oGet, GetList )

   IF GetPreValidate( oGet )
      oGet:SetFocus()
      DO WHILE oGet:exitState == GE_NOEXIT
         IF oGet:typeOut
            oGet:exitState := GE_ENTER
         ENDIF

         DO WHILE oGet:exitState == GE_NOEXIT
            GetApplyKey( oGet, Inkey( 0, INKEY_ALLEVENTS ), GetList )
         ENDDO

         IF ! GetPostValidate( oGet )
            oGet:exitState := GE_NOEXIT
         ENDIF
      ENDDO

      oGet:KillFocus()
   ENDIF

   RETURN NIL

// One key for a Get with the input focus; GetList is needed only for the mouse and K_RESIZE
FUNCTION GetApplyKey( oGet, nKey, GetList )

   LOCAL bKey := SetKey( nKey )
   LOCAL cChar
   LOCAL nPos

   IF bKey != NIL
      GetDoSetKey( bKey, oGet )
      RETURN NIL
   ENDIF

   DO CASE
   CASE nKey == K_UP .OR. nKey == K_SH_TAB
      oGet:exitState := GE_UP
   CASE nKey == K_DOWN .OR. nKey == K_TAB
      oGet:exitState := GE_DOWN
   CASE nKey == K_ENTER
      oGet:exitState := GE_ENTER
   CASE nKey == K_ESC .OR. nKey == K_CLOSE
      // xDev extension: closing the window during READ works like Esc
      IF Set( _SET_ESCAPE )
         oGet:Undo()
         oGet:exitState := GE_ESCAPE
      ENDIF
   CASE nKey == K_PGUP .OR. nKey == K_PGDN .OR. nKey == K_CTRL_W
      oGet:exitState := GE_WRITE
   CASE nKey == K_CTRL_HOME
      oGet:exitState := GE_TOP
   CASE nKey == K_INS
      ReadInsert( ! ReadInsert() )
   CASE nKey == K_CTRL_U
      oGet:Undo()
   CASE nKey == K_HOME
      oGet:Home()
   CASE nKey == K_END
      oGet:End()
   CASE nKey == K_LEFT
      oGet:Left()
   CASE nKey == K_RIGHT
      oGet:Right()
   CASE nKey == K_CTRL_LEFT
      oGet:WordLeft()
   CASE nKey == K_CTRL_RIGHT
      oGet:WordRight()
   CASE nKey == K_BS
      oGet:BackSpace()
   CASE nKey == K_DEL
      oGet:Delete()
   CASE nKey == K_CTRL_T
      oGet:DelWordRight()
   CASE nKey == K_CTRL_Y
      oGet:DelEnd()
   CASE nKey == K_CTRL_BS
      oGet:DelWordLeft()
   CASE nKey == K_LBUTTONDOWN
      IF oGet:HitTest( MRow(), MCol() ) == HTCLIENT
         oGet:pos := Max( 1, oGet:offset + MCol() - oGet:col + 1 )
         oGet:Display()
      ELSEIF GetList != NIL
         FOR nPos := 1 TO Len( GetList )
            IF GetList[ nPos ]:HitTest( MRow(), MCol() ) == HTCLIENT
               s_nJump := nPos
               oGet:exitState := GE_ENTER
               EXIT
            ENDIF
         NEXT
      ENDIF
   CASE nKey == K_RESIZE
      // The program redraws its own screen in GtInfo( GTI_RESIZEBLOCK ), READ only its Gets
      IF GetList != NIL
         AEval( GetList, {| o | o:Display() } )
      ENDIF

      oGet:Display()
   OTHERWISE
      cChar := KeyChar( nKey )
      IF ! cChar == ""
         IF oGet:type == "N" .AND. ( cChar == "." .OR. cChar == "," )
            oGet:ToDecPos()
         ELSEIF ReadInsert()
            oGet:Insert( cChar )
         ELSE
            oGet:OverStrike( cChar )
         ENDIF

         IF oGet:typeOut .AND. ! Set( _SET_CONFIRM )
            oGet:exitState := GE_ENTER
         ENDIF
      ENDIF
   ENDCASE

   RETURN NIL

// WHEN: .F. leaves the Get out
FUNCTION GetPreValidate( oGet )

   LOCAL lWhen := .T.

   oGet:exitState := GE_NOEXIT
   IF oGet:preBlock != NIL
      lWhen := Eval( oGet:preBlock, oGet )
      oGet:Display()
      IF ! lWhen
         oGet:exitState := GE_WHEN
      ENDIF
   ENDIF

   RETURN lWhen

// VALID: the value is assigned first, .F. keeps the Get in editing
FUNCTION GetPostValidate( oGet )

   LOCAL lValid := .T.

   IF oGet:exitState == GE_ESCAPE
      RETURN .T.
   ENDIF

   IF oGet:badDate
      oGet:Home()
      RETURN .F.
   ENDIF

   IF oGet:changed
      oGet:Assign()
      s_lUpdated := .T.
   ENDIF

   oGet:Reset()
   IF oGet:postBlock != NIL
      lValid := Eval( oGet:postBlock, oGet )
      oGet:UpdateBuffer()
   ENDIF

   RETURN lValid

// Create a Get for a variable at a screen position, add it to GetList and display it
// GetAdd( GetList, nRow, nCol, @xVar, [cPicture], [bValid], [bWhen], [cCaption], [cMessage] ) --> oGet
FUNCTION GetAdd( GetList, nRow, nCol, xVar, cPicture, bValid, bWhen, cCaption, cMessage )

   LOCAL oGet := Get():New( nRow, nCol, {| x | IIF( PCount() == 0, xVar, xVar := x ) }, "", cPicture )

   oGet:postBlock := bValid
   oGet:preBlock  := bWhen
   IF cMessage != NIL
      oGet:message := cMessage
   ENDIF

   IF cCaption != NIL
      DispOutAt( nRow, nCol - ULen( cCaption ) - 1, cCaption )
   ENDIF

   AAdd( GetList, oGet )
   oGet:Display()

   RETURN oGet

// A SET KEY block during editing gets the procedure, the line and the variable;
// ProcName( n ) of a caller needs a runtime call stack, so the procedure is "" and the line 0
FUNCTION GetDoSetKey( bKey, oGet )

   IF oGet:changed
      oGet:Assign()
      s_lUpdated := .T.
   ENDIF

   Eval( bKey, "", 0, ReadVar() )
   oGet:UpdateBuffer()
   IF s_lKill
      oGet:exitState := GE_ESCAPE
   ENDIF

   RETURN NIL

// The Get being edited; with oGet it becomes the active one
// GetActive( [oGet] ) --> oPrevious
FUNCTION GetActive( oGet )

   LOCAL oOld := s_oActive

   IF PCount() > 0
      s_oActive := oGet
   ENDIF

   RETURN oOld

// The name of the variable of the Get being edited
// ReadVar( ) --> cName
FUNCTION ReadVar()

   RETURN s_cReadVar

// .T. when the last READ changed a variable
// Updated( ) --> lUpdated
FUNCTION Updated()

   RETURN s_lUpdated

// .T.: Up on the first Get and Down on the last one end the READ
FUNCTION ReadExit( lToggle )

   RETURN IIF( lToggle == NIL, Set( _SET_EXIT ), Set( _SET_EXIT, lToggle ) )

// The insert mode of READ, SET INSERT; lToggle sets it
// ReadInsert( [lToggle] ) --> lPrevious
FUNCTION ReadInsert( lToggle )

   RETURN IIF( lToggle == NIL, Set( _SET_INSERT ), Set( _SET_INSERT, lToggle ) )

// .T. ends the READ at the next key; returns the previous setting
// ReadKill( [lKillRead] ) --> lPrevious
FUNCTION ReadKill( lKillRead )

   LOCAL lOld := s_lKill

   IF lKillRead != NIL
      s_lKill := lKillRead
   ENDIF

   RETURN lOld

// VALID of a Get that accepts values from xLow to xHigh
FUNCTION RangeCheck( oGet, xDummy, xLow, xHigh )

   LOCAL xValue

   IF ! oGet:changed
      RETURN .T.
   ENDIF

   xValue := oGet:VarGet()

   RETURN xValue >= xLow .AND. xValue <= xHigh

/* ---------------------------------------------------------------------------------------------
   Functions of this file
   --------------------------------------------------------------------------------------------- */

// The Get that READ edits after nPos, 0 when READ ends; nDir is the last move, -1 up or 1 down
STATIC FUNCTION NextGet( GetList, nPos, nExit, nDir )

   IF s_nJump > 0
      nPos    := s_nJump
      s_nJump := 0
      RETURN nPos
   ENDIF

   DO CASE
   CASE nExit == GE_UP
      IF nPos > 1
         RETURN nPos - 1
      ENDIF

      RETURN IIF( ReadExit(), 0, 1 )
   CASE nExit == GE_DOWN
      IF nPos < Len( GetList )
         RETURN nPos + 1
      ENDIF

      RETURN IIF( ReadExit(), 0, nPos )
   CASE nExit == GE_TOP
      RETURN 1
   CASE nExit == GE_BOTTOM
      RETURN Len( GetList )
   CASE nExit == GE_ENTER
      RETURN IIF( nPos == Len( GetList ), 0, nPos + 1 )
   CASE nExit == GE_WHEN
      // A Get left out by WHEN: further in the same direction
      nPos += nDir
      IF nPos < 1
         RETURN 2
      ENDIF

      RETURN IIF( nPos > Len( GetList ), 0, nPos )
   ENDCASE

   // GE_WRITE and GE_ESCAPE end the READ
   RETURN 0
