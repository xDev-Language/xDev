/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev Tests
 * File......: Pad.prg
 *
 */

/*
 * $DOC$
 * $NAME$
 * Padding
 *
 * $ONELINER$
 * Tests the padding and spacing functions with UTF-8 text and UTF-8 fill characters.
 *
 * $DESCRIPTION$
 * Pad(), PadR(), PadL(), PadC(), Center(), PadLeft(), PadRight(), JustLeft(), JustRight(), TabExpand(), TabPack(), CharSpread(), NToC() and Expand() count lengths and columns in characters, not bytes, and take any UTF-8 character as the fill character. Pad() pads on the right like PadR(). Text without Polish letters gives the same results as before.
 *
 * $EXAMPLES$
 *
 * $OUTPUT$
 * PadR() counts characters         OK
 * PadL() counts characters         OK
 * PadC() counts characters         OK
 * PadR() cuts whole characters     OK
 * PadR() with a UTF-8 fill         OK
 * PadR() fill: characters          OK
 * Center() counts characters       OK
 * Center() on both sides           OK
 * Center() replaces a UTF-8 pad    OK
 * PadLeft() keeps the last chars   OK
 * PadLeft() with a UTF-8 fill      OK
 * PadRight() with a UTF-8 fill     OK
 * JustLeft() with a UTF-8 fill     OK
 * JustRight() with a UTF-8 fill    OK
 * TabExpand() counts characters    OK
 * TabExpand() with a UTF-8 fill    OK
 * TabPack() with a UTF-8 fill      OK
 * CharSpread() counts characters   OK
 * NToC() with a UTF-8 pad          OK
 * Expand() keeps characters whole  OK
 * Expand() with a UTF-8 fill       OK
 * ASCII text as before             OK
 * Pad() pads on the right          OK
 *
 * $DOCEND$
 */

PROCEDURE Main()

   LOCAL cTab := Chr( 9 )

   s_Check( "PadR() counts characters         ", PadR( "Zażółć", 10 ), "Zażółć    " )
   s_Check( "PadL() counts characters         ", PadL( "Zażółć", 10 ), "    Zażółć" )
   s_Check( "PadC() counts characters         ", PadC( "Zażółć", 10 ), "  Zażółć  " )
   s_Check( "PadR() cuts whole characters     ", PadR( "Zażółć gęślą", 6 ), "Zażółć" )
   s_Check( "PadR() with a UTF-8 fill         ", PadR( "ab", 5, "─" ), "ab───" )
   s_Check( "PadR() fill: characters          ", ULen( PadR( "ą", 20, "─" ) ), 20 )
   s_Check( "Center() counts characters       ", Center( "Łódź", 10 ), "   Łódź" )
   s_Check( "Center() on both sides           ", Center( "Łódź", 10, "*", .T. ), "***Łódź***" )
   s_Check( "Center() replaces a UTF-8 pad    ", Center( "··Łódź··", 10, "·", .T. ), "···Łódź···" )
   s_Check( "PadLeft() keeps the last chars   ", PadLeft( "Zażółć", 4 ), "żółć" )
   s_Check( "PadLeft() with a UTF-8 fill      ", PadLeft( "ąę", 4, "ż" ), "żżąę" )
   s_Check( "PadRight() with a UTF-8 fill     ", PadRight( "ąę", 4, "ż" ), "ąężż" )
   s_Check( "JustLeft() with a UTF-8 fill     ", JustLeft( "··ąb", "·" ), "ąb··" )
   s_Check( "JustRight() with a UTF-8 fill    ", JustRight( "ąb··", "·" ), "··ąb" )
   s_Check( "TabExpand() counts characters    ", TabExpand( "ą" + cTab + "b", 4 ), "ą   b" )
   s_Check( "TabExpand() with a UTF-8 fill    ", TabExpand( "ą" + cTab + "b", 4, "·" ), "ą···b" )
   s_Check( "TabPack() with a UTF-8 fill      ", TabPack( "ą···b", 4, "·" ), "ą" + cTab + "b" )
   s_Check( "CharSpread() counts characters   ", CharSpread( "ą b", 5 ), "ą   b" )
   s_Check( "NToC() with a UTF-8 pad          ", NToC( 255, 16, 6, "·" ), "····FF" )
   s_Check( "Expand() keeps characters whole  ", Expand( "ąę" ), "ą ę" )
   s_Check( "Expand() with a UTF-8 fill       ", Expand( "ąę", "·" ), "ą·ę" )
   s_Check( "ASCII text as before             ", PadR( "abc", 5 ), "abc  " )
   s_Check( "Pad() pads on the right          ", Pad( "Zażółć", 10, "·" ), "Zażółć····" )

RETURN

STATIC PROCEDURE s_Check( cTitle, xResult, xExpected )

   QOut( cTitle + IIF( ValType( xResult ) == ValType( xExpected ) .AND. xResult == xExpected, "OK", "ERROR" ) )

RETURN


