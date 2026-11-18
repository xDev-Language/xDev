/*
 * $DOC$
 * $NAME$
 * Memo lines
 *
 * $ONELINER$
 * Tests MemoLine(), MLCount(), MLPos(), MLCToPos() and MPosToLC() with UTF-8 text.
 *
 * $DESCRIPTION$
 * The line length and the columns count characters, not bytes, and the positions are character positions that suit USubStr(). A line never ends inside a character, also when a word is longer than the line. Text without Polish letters gives the same lines as before.
 *
 * $EXAMPLES$
 *
 * $OUTPUT$
 * MLCount() counts characters     OK
 * MemoLine() line 1               OK
 * MemoLine() line 2               OK
 * MemoLine() width in characters  OK
 * MLPos() counts characters       OK
 * MLPos() suits USubStr()         OK
 * MPosToLC(): line                OK
 * MPosToLC(): column              OK
 * MLCToPos() is its inverse       OK
 * long word: no broken character  OK
 * long word: the rest             OK
 * long word: MLCount()            OK
 * ASCII text as before            OK
 *
 * $DOCEND$
 */

PROCEDURE Main()

   LOCAL cText := "Zażółć gęślą jaźń"
   LOCAL cLong := "ąąąąąąąą"
   LOCAL aPlace

   s_Check( "MLCount() counts characters     ", MLCount( cText, 10 ), 2 )
   s_Check( "MemoLine() line 1               ", MemoLine( cText, 10, 1 ), "Zażółć    " )
   s_Check( "MemoLine() line 2               ", MemoLine( cText, 10, 2 ), "gęślą jaźń" )
   s_Check( "MemoLine() width in characters  ", ULen( MemoLine( cText, 10, 1 ) ), 10 )
   s_Check( "MLPos() counts characters       ", MLPos( cText, 10, 2 ), 8 )
   s_Check( "MLPos() suits USubStr()         ", USubStr( cText, MLPos( cText, 10, 2 ) ), "gęślą jaźń" )
   aPlace := MPosToLC( cText, 10, 9 )
   s_Check( "MPosToLC(): line                ", aPlace[ 1 ], 2 )
   s_Check( "MPosToLC(): column              ", aPlace[ 2 ], 1 )
   s_Check( "MLCToPos() is its inverse       ", MLCToPos( cText, 10, 2, 1 ), 9 )
   s_Check( "long word: no broken character  ", MemoLine( cLong, 5, 1 ), "ąąąąą" )
   s_Check( "long word: the rest             ", MemoLine( cLong, 5, 2 ), "ąąą  " )
   s_Check( "long word: MLCount()            ", MLCount( cLong, 5 ), 2 )
   s_Check( "ASCII text as before            ", MemoLine( "abc def", 5, 1 ), "abc  " )

RETURN

STATIC PROCEDURE s_Check( cTitle, xResult, xExpected )

   QOut( cTitle + IIF( ValType( xResult ) == ValType( xExpected ) .AND. xResult == xExpected, "OK", "ERROR" ) )

RETURN
