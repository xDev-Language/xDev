/*
 * $DOC$
 * $NAME$
 * Compared positions
 *
 * $ONELINER$
 * Tests PosDiff(), PosEqual() and AtAdjust() with UTF-8 text.
 *
 * $DESCRIPTION$
 * The positions, nCompare, nSkipChars and the column of AtAdjust() count UTF-8 characters, not bytes, and the fill character of AtAdjust() may be any UTF-8 character. The other At*() functions count bytes, like At().
 *
 * $EXAMPLES$
 *
 * $OUTPUT$
 * PosDiff() counts characters   OK
 * PosDiff() equal strings       OK
 * PosDiff() nSkipChars          OK
 * PosEqual() nCompare           OK
 * PosEqual() nSkipChars         OK
 * AtAdjust() moves to a column  OK
 * AtAdjust() with a UTF-8 fill  OK
 * AtAdjust() removes the fill   OK
 * ASCII text as before          OK
 *
 * $DOCEND$
 */

PROCEDURE Main()

   s_Check( "PosDiff() counts characters   ", PosDiff( "ąbc", "ąbd" ), 3 )
   s_Check( "PosDiff() equal strings       ", PosDiff( "ąę", "ąę" ), 0 )
   s_Check( "PosDiff() nSkipChars          ", PosDiff( "ąęx", "ąęy", 1 ), 3 )
   s_Check( "PosEqual() nCompare           ", PosEqual( "xąę", "yąę", 2 ), 2 )
   s_Check( "PosEqual() nSkipChars         ", PosEqual( "ąęx", "ąęy", 1, 1 ), 2 )
   s_Check( "AtAdjust() moves to a column  ", AtAdjust( "ż", "ąż", 5 ), "ą   ż" )
   s_Check( "AtAdjust() with a UTF-8 fill  ", AtAdjust( "ż", "ąż", 4, , , "·" ), "ą··ż" )
   s_Check( "AtAdjust() removes the fill   ", AtAdjust( "ż", "ą···ż", 2, , , "·" ), "ąż" )
   s_Check( "ASCII text as before          ", PosDiff( "abc", "abd" ), 3 )

RETURN

STATIC PROCEDURE s_Check( cTitle, xResult, xExpected )

   QOut( cTitle + IIF( ValType( xResult ) == ValType( xExpected ) .AND. xResult == xExpected, "OK", "ERROR" ) )

RETURN
