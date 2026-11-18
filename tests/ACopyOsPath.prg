/*
 * $DOC$
 * $NAME$
 * ACopy() and path characters
 *
 * $ONELINER$
 * Tests ACopy() with omitted and NIL arguments, and the path characters of the operating system.
 *
 * $DESCRIPTION$
 * ACopy() takes NIL in nStart, nCount and nTargetPos as the default, the same as an omitted argument. OsPathSeparator(), PS(), OsPathDelimiters(), OsPathListSeparator() and OsDriveSeparator() return the characters of the system the program runs on; the titles are the same on Windows and Linux, only the expected values differ.
 *
 * $EXAMPLES$
 *
 * $OUTPUT$
 * ACopy() of all elements          OK
 * ACopy() returns the target       OK
 * ACopy() from nStart              OK
 * ACopy() of nCount elements       OK
 * ACopy() to nTargetPos            OK
 * ACopy() stops at the target end  OK
 * ACopy() stops at the source end  OK
 * ACopy() with NIL nStart          OK
 * ACopy() with NIL nCount          OK
 * ACopy() with NIL nTargetPos      OK
 * ACopy() with all NIL             OK
 * ACopy() keeps a subarray         OK
 * OsPathSeparator()                OK
 * PS() is OsPathSeparator()        OK
 * OsPathDelimiters()               OK
 * separator is a delimiter         OK
 * OsPathListSeparator()            OK
 * list separator not a delimiter   OK
 * OsDriveSeparator()               OK
 * root of the system               OK
 *
 * $DOCEND$
 */

PROCEDURE Main()

   s_ACopy()
   s_ACopyNil()
   s_Path()

RETURN

STATIC PROCEDURE s_ACopy()

   LOCAL aSource := { 1, 2, 3, 4, 5 }
   LOCAL aTarget

   aTarget := Array( 5 )
   ACopy( aSource, aTarget )
   Check( 'ACopy() of all elements          ', s_Join( aTarget ), "12345" )

   aTarget := { 0, 0, 0, 0, 0 }
   Check( 'ACopy() returns the target       ', ACopy( aSource, aTarget ) == aTarget, .T. )

   aTarget := { 0, 0, 0, 0, 0 }
   ACopy( aSource, aTarget, 3 )
   Check( 'ACopy() from nStart              ', s_Join( aTarget ), "34500" )

   aTarget := { 0, 0, 0, 0, 0 }
   ACopy( aSource, aTarget, 1, 2 )
   Check( 'ACopy() of nCount elements       ', s_Join( aTarget ), "12000" )

   aTarget := { 0, 0, 0, 0, 0 }
   ACopy( aSource, aTarget, 1, 2, 4 )
   Check( 'ACopy() to nTargetPos            ', s_Join( aTarget ), "00012" )

   aTarget := { 0, 0, 0 }
   ACopy( aSource, aTarget )
   Check( 'ACopy() stops at the target end  ', s_Join( aTarget ), "123" )

   aTarget := { 0, 0, 0, 0, 0 }
   ACopy( aSource, aTarget, 4, 10 )
   Check( 'ACopy() stops at the source end  ', s_Join( aTarget ), "45000" )

RETURN

STATIC PROCEDURE s_ACopyNil()

   LOCAL aSource := { 1, 2, 3, 4, 5 }
   LOCAL aSub    := { 9 }
   LOCAL aTarget

   aTarget := { 0, 0, 0, 0, 0 }
   ACopy( aSource, aTarget, NIL, 2 )
   Check( 'ACopy() with NIL nStart          ', s_Join( aTarget ), "12000" )

   aTarget := { 0, 0, 0, 0, 0 }
   ACopy( aSource, aTarget, 2, NIL )
   Check( 'ACopy() with NIL nCount          ', s_Join( aTarget ), "23450" )

   aTarget := { 0, 0, 0, 0, 0 }
   ACopy( aSource, aTarget, 1, 2, NIL )
   Check( 'ACopy() with NIL nTargetPos      ', s_Join( aTarget ), "12000" )

   aTarget := { 0, 0, 0, 0, 0 }
   ACopy( aSource, aTarget, NIL, NIL, 4 )
   Check( 'ACopy() with all NIL             ', s_Join( aTarget ), "00012" )

   /* a subarray is copied as a reference, not as a copy */
   aTarget := { 0 }
   ACopy( { aSub }, aTarget )
   aSub[ 1 ] := 7
   Check( 'ACopy() keeps a subarray         ', aTarget[ 1 ][ 1 ], 7 )

RETURN

STATIC PROCEDURE s_Path()

#if OS_WINDOWS
   Check( 'OsPathSeparator()                ', OsPathSeparator(), "\" )
   Check( 'PS() is OsPathSeparator()        ', PS(), "\" )
   Check( 'OsPathDelimiters()               ', OsPathDelimiters(), "\/:" )
   Check( 'separator is a delimiter         ', "\" $ OsPathDelimiters(), .T. )
   Check( 'OsPathListSeparator()            ', OsPathListSeparator(), ";" )
   Check( 'list separator not a delimiter   ', ";" $ OsPathDelimiters(), .F. )
   Check( 'OsDriveSeparator()               ', OsDriveSeparator(), ":" )
   Check( 'root of the system               ', "C" + OsDriveSeparator() + OsPathSeparator(), "C:\" )
#else
   Check( 'OsPathSeparator()                ', OsPathSeparator(), "/" )
   Check( 'PS() is OsPathSeparator()        ', PS(), "/" )
   Check( 'OsPathDelimiters()               ', OsPathDelimiters(), "/" )
   Check( 'separator is a delimiter         ', "/" $ OsPathDelimiters(), .T. )
   Check( 'OsPathListSeparator()            ', OsPathListSeparator(), ":" )
   Check( 'list separator not a delimiter   ', ":" $ OsPathDelimiters(), .F. )
   Check( 'OsDriveSeparator()               ', OsDriveSeparator(), "" )
   Check( 'root of the system               ', OsDriveSeparator() + OsPathSeparator(), "/" )
#endif

RETURN

STATIC FUNCTION s_Join( aArray )

   LOCAL cResult := ""
   LOCAL xItem

   FOR EACH xItem IN aArray
      cResult += Str( xItem, 1 )
   NEXT

RETURN cResult

STATIC PROCEDURE Check( cTitle, xResult, xExpected )

   QOut( cTitle + IIF( ValType( xResult ) == ValType( xExpected ) .AND. xResult == xExpected, "OK", "ERROR" ) )

RETURN
