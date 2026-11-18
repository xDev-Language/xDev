/*
 * $DOC$
 * $NAME$
 * Letter positions
 *
 * $ONELINER$
 * Tests PosAlpha(), PosUpper() and PosLower() with UTF-8 text.
 *
 * $DESCRIPTION$
 * Polish letters are letters, upper case or lower case, as for IsAlpha(), IsUpper() and IsLower(). nSkipChars and the result count characters, not bytes, so the position suits USubStr(). Text without Polish letters gives the same results as before.
 *
 * $EXAMPLES$
 *
 * $OUTPUT$
 * PosAlpha() finds a Polish letter  OK
 * PosAlpha() lNoAlpha               OK
 * PosAlpha() lNoAlpha: none         OK
 * PosAlpha() counts characters      OK
 * PosUpper() finds Ł                OK
 * PosUpper() nSkipChars             OK
 * PosLower() finds ł                OK
 * PosLower() lNoLower               OK
 * position suits USubStr()          OK
 * ASCII text as before              OK
 *
 * $DOCEND$
 */

PROCEDURE Main()

   s_Check( "PosAlpha() finds a Polish letter  ", PosAlpha( "12ąb" ), 3 )
   s_Check( "PosAlpha() lNoAlpha               ", PosAlpha( "ą1b", .T. ), 2 )
   s_Check( "PosAlpha() lNoAlpha: none         ", PosAlpha( "ąb", .T. ), 0 )
   s_Check( "PosAlpha() counts characters      ", PosAlpha( "ąę1", .T. ), 3 )
   s_Check( "PosUpper() finds Ł                ", PosUpper( "ąęŁx" ), 3 )
   s_Check( "PosUpper() nSkipChars             ", PosUpper( "ŁąŻ", , 1 ), 3 )
   s_Check( "PosLower() finds ł                ", PosLower( "ĄĘłX" ), 3 )
   s_Check( "PosLower() lNoLower               ", PosLower( "ŁŻ", .T. ), 1 )
   s_Check( "position suits USubStr()          ", USubStr( "ĄĘłX", PosLower( "ĄĘłX" ), 1 ), "ł" )
   s_Check( "ASCII text as before              ", PosAlpha( "12ab" ), 3 )

RETURN

STATIC PROCEDURE s_Check( cTitle, xResult, xExpected )

   QOut( cTitle + IIF( ValType( xResult ) == ValType( xExpected ) .AND. xResult == xExpected, "OK", "ERROR" ) )

RETURN
