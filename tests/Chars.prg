/*
 * $DOC$
 * $NAME$
 * Character sets
 *
 * $ONELINER$
 * Tests the CT functions on characters and character sets with UTF-8 text.
 *
 * $DESCRIPTION$
 * CharRem(), CharOnly(), CharOne(), CharRepl(), RemAll(), RemLeft(), ReplAll(), ReplLeft(), CountLeft(), CountRight(), PosChar(), PosDel(), PosIns(), PosRepl(), PosRange(), RangeRem() and RangeRepl() treat a UTF-8 character as one character, in the lists, in the string and as the fill or replacement character; positions and counts are counted in characters. Removing "ą" must not break "ć", although both start with the same byte.
 *
 * $EXAMPLES$
 *
 * $OUTPUT$
 * CharRem() removes ą and ę       OK
 * CharRem() keeps ć whole         OK
 * CharOnly() keeps ą and ę        OK
 * CharOne() every character       OK
 * CharOne() listed characters     OK
 * CharRepl() Polish letters       OK
 * CharRepl() longer replacement   OK
 * RemAll() with a UTF-8 char      OK
 * RemLeft() with a UTF-8 char     OK
 * ReplAll() with UTF-8 chars      OK
 * ReplLeft() with UTF-8 chars     OK
 * CountLeft() counts characters   OK
 * CountRight() counts characters  OK
 * PosChar() at a position         OK
 * PosChar() at the end            OK
 * PosDel() at a position          OK
 * PosDel() at the end             OK
 * PosIns() at a position          OK
 * PosRepl() at a position         OK
 * PosRepl() at the end            OK
 * PosRange() counts characters    OK
 * RangeRem() Polish range         OK
 * RangeRepl() Polish range        OK
 * ASCII text as before            OK
 *
 * $DOCEND$
 */

PROCEDURE Main()

   s_Check( "CharRem() removes ą and ę       ", CharRem( "ąę", "Zażółć gęślą" ), "Zażółć gśl" )
   s_Check( "CharRem() keeps ć whole         ", CharRem( "ą", "ćą" ), "ć" )
   s_Check( "CharOnly() keeps ą and ę        ", CharOnly( "ąę", "Zażółć gęślą" ), "ęą" )
   s_Check( "CharOne() every character       ", CharOne( "ąąbb" ), "ąb" )
   s_Check( "CharOne() listed characters     ", CharOne( "ą", "ąąbb" ), "ąbb" )
   s_Check( "CharRepl() Polish letters       ", CharRepl( "ąę", "gęślą", "ae" ), "geśla" )
   s_Check( "CharRepl() longer replacement   ", CharRepl( "a", "abc", "ą" ), "ąbc" )
   s_Check( "RemAll() with a UTF-8 char      ", RemAll( "··ą··", "·" ), "ą" )
   s_Check( "RemLeft() with a UTF-8 char     ", RemLeft( "··ą··", "·" ), "ą··" )
   s_Check( "ReplAll() with UTF-8 chars      ", ReplAll( "··ą··", "-", "·" ), "--ą--" )
   s_Check( "ReplLeft() with UTF-8 chars     ", ReplLeft( "··ą··", "-", "·" ), "--ą··" )
   s_Check( "CountLeft() counts characters   ", CountLeft( "ąąb", "ą" ), 2 )
   s_Check( "CountRight() counts characters  ", CountRight( "bśś", "ś" ), 2 )
   s_Check( "PosChar() at a position         ", PosChar( "ąęć", "x", 2 ), "ąxć" )
   s_Check( "PosChar() at the end            ", PosChar( "abc", "ż" ), "abż" )
   s_Check( "PosDel() at a position          ", PosDel( "ąęć", 2, 1 ), "ąć" )
   s_Check( "PosDel() at the end             ", PosDel( "ąęć" ), "ąę" )
   s_Check( "PosIns() at a position          ", PosIns( "ąć", "ę", 2 ), "ąęć" )
   s_Check( "PosRepl() at a position         ", PosRepl( "ąęć", "xy", 2 ), "ąxy" )
   s_Check( "PosRepl() at the end            ", PosRepl( "ąę", "ćś" ), "ćś" )
   s_Check( "PosRange() counts characters    ", PosRange( "ą", "ż", "abcąd" ), 4 )
   s_Check( "RangeRem() Polish range         ", RangeRem( "ą", "ż", "aąbżc" ), "abc" )
   s_Check( "RangeRepl() Polish range        ", RangeRepl( "ą", "ż", "aąbżc", "_" ), "a_b_c" )
   s_Check( "ASCII text as before            ", CharRem( "a", "banana" ), "bnn" )

RETURN

STATIC PROCEDURE s_Check( cTitle, xResult, xExpected )

   QOut( cTitle + IIF( ValType( xResult ) == ValType( xExpected ) .AND. xResult == xExpected, "OK", "ERROR" ) )

RETURN
