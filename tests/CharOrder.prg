/*
 * $DOC$
 * $NAME$
 * Character order
 *
 * $ONELINER$
 * Tests the CT functions that reorder or count characters with UTF-8 text.
 *
 * $DESCRIPTION$
 * CharMirr(), CharSwap(), CharEven(), CharOdd(), CharMix(), CharSort(), CharList(), CharSList(), CharHist() and CharNoList() work on UTF-8 characters, not bytes. CharSort() and CharSList() sort by the code of the character, not by the Polish alphabet. CharHist() counts the characters with codes 0 to 255, and CharNoList() lists the characters with codes 0 to 255 that the string lacks.
 *
 * $EXAMPLES$
 *
 * $OUTPUT$
 * CharMirr() reverses characters  OK
 * CharMirr() lNoSpaces            OK
 * CharSwap() swaps characters     OK
 * CharSwap() odd length           OK
 * CharEven() counts characters    OK
 * CharOdd() counts characters     OK
 * CharMix() repeats the second    OK
 * CharMix() UTF-8 second string   OK
 * CharSort() by code              OK
 * CharSort() descending           OK
 * CharSort() elements of 2        OK
 * CharList() first appearance     OK
 * CharSList() sorted list         OK
 * CharHist() counts ó as 243      OK
 * CharHist() counts ASCII         OK
 * CharNoList() of 256 chars       OK
 * CharNoList() lacks the chars    OK
 * ASCII text as before            OK
 *
 * $DOCEND$
 */

PROCEDURE Main()

   s_Check( "CharMirr() reverses characters  ", CharMirr( "ąbć" ), "ćbą" )
   s_Check( "CharMirr() lNoSpaces            ", CharMirr( "ąb  ", .T. ), "bą  " )
   s_Check( "CharSwap() swaps characters     ", CharSwap( "ąbćd" ), "bądć" )
   s_Check( "CharSwap() odd length           ", CharSwap( "ąbć" ), "bąć" )
   s_Check( "CharEven() counts characters    ", CharEven( "ąbćd" ), "bd" )
   s_Check( "CharOdd() counts characters     ", CharOdd( "ąbćd" ), "ąć" )
   s_Check( "CharMix() repeats the second    ", CharMix( "ąę", "x" ), "ąxęx" )
   s_Check( "CharMix() UTF-8 second string   ", CharMix( "ab", "ąę" ), "aąbę" )
   s_Check( "CharSort() by code              ", CharSort( "żąć" ), "ąćż" )
   s_Check( "CharSort() descending           ", CharSort( "żąć", , , , , , .T. ), "żćą" )
   s_Check( "CharSort() elements of 2        ", CharSort( "bęaą", 2 ), "aąbę" )
   s_Check( "CharList() first appearance     ", CharList( "ąbąc" ), "ąbc" )
   s_Check( "CharSList() sorted list         ", CharSList( "żąćą" ), "ąćż" )
   s_Check( "CharHist() counts ó as 243      ", CharHist( "óóa" )[ 244 ], 2 )
   s_Check( "CharHist() counts ASCII         ", CharHist( "óóa" )[ 98 ], 1 )
   s_Check( "CharNoList() of 256 chars       ", ULen( CharNoList( "" ) ), 256 )
   s_Check( "CharNoList() lacks the chars    ", At( "ó", CharNoList( "ó" ) ), 0 )
   s_Check( "ASCII text as before            ", CharMirr( "abc" ), "cba" )

RETURN

STATIC PROCEDURE s_Check( cTitle, xResult, xExpected )

   QOut( cTitle + IIF( ValType( xResult ) == ValType( xExpected ) .AND. xResult == xExpected, "OK", "ERROR" ) )

RETURN
