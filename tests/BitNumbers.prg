/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev Tests
 * File......: BitNumbers.prg
 *
 */

/*
 * $DOC$
 * $NAME$
 * Bit and byte functions
 *
 * $ONELINER$
 * Tests the functions that work on the bits and bytes of numbers and hex strings.
 *
 * $DESCRIPTION$
 * NumAnd(), NumOr(), NumXor(), NumNot(), SetBit(), ClearBit(), IsBit(), NumRol(), IntNeg(), IntPos(), NumHigh(), NumLow(), NumMirr() and NumMirrX() take a number or the same number as a string of hex digits. An invalid argument raises no error: it gives -1, 0 for IntNeg() and IntPos(), .F. for IsBit().
 *
 * $EXAMPLES$
 *
 * $OUTPUT$
 * NumAnd() of two numbers          OK
 * NumAnd() of three values         OK
 * NumOr() sets a bit               OK
 * NumOr() with a hex string        OK
 * NumXor() of two numbers          OK
 * NumXor() twice gives the number  OK
 * NumNot() is 16 bits              OK
 * NumNot() of "FFFF"               OK
 * SetBit() of three bits           OK
 * SetBit() of a hex string         OK
 * ClearBit() of three bits         OK
 * ClearBit() of a hex string       OK
 * IsBit() bit 1 by default         OK
 * IsBit() bit 2 not set            OK
 * IsBit() of a hex string          OK
 * IsBit() of a negative number     OK
 * IsBit() agrees with SetBit()     OK
 * NumRol() by 3                    OK
 * NumRol() by 14, 2 to the right   OK
 * NumRol() by 33 is by 1           OK
 * NumRol() of the low byte         OK
 * IntNeg() of 32768                OK
 * IntNeg() of "FFFF"               OK
 * IntNeg() of 32 bits              OK
 * IntPos() of -1                   OK
 * IntPos() keeps 16 bits           OK
 * IntPos() of 32 bits              OK
 * IntPos() of IntNeg() is the same OK
 * NumHigh() of a word              OK
 * NumLow() of a word               OK
 * NumHigh() skips leading spaces   OK
 * NumMirr() of 16 bits             OK
 * NumMirr() of the low byte        OK
 * NumMirrX() of 32 bits            OK
 * invalid: NumAnd() with "GG"      OK
 * invalid: NumOr() with .T.        OK
 * invalid: NumNot() without value  OK
 * invalid: SetBit() bit 33         OK
 * invalid: ClearBit() bit 0        OK
 * invalid: IsBit() of .T.          OK
 * invalid: IsBit() bit 33          OK
 * invalid: NumRol() of 70000       OK
 * invalid: IntNeg() of 90000       OK
 * invalid: IntNeg() of -1          OK
 * invalid: IntPos() of "GG"        OK
 * invalid: NumHigh() of 65536      OK
 * invalid: NumLow() of ""          OK
 * invalid: NumMirr() of -1         OK
 * invalid: NumMirrX() width 33     OK
 *
 * $DOCEND$
 */

PROCEDURE Main()

   s_Logic()
   s_Bits()
   s_Rotate()
   s_Signed()
   s_Bytes()
   s_Invalid()

RETURN

STATIC PROCEDURE s_Logic()

   Check( 'NumAnd() of two numbers          ', NumAnd( 29, 7 ), 5 )
   Check( 'NumAnd() of three values         ', NumAnd( "FF", "0F", 6 ), 6 )
   Check( 'NumOr() sets a bit               ', NumOr( 3, 16 ), 19 )
   Check( 'NumOr() with a hex string        ', NumOr( 1, 2, "4" ), 7 )
   Check( 'NumXor() of two numbers          ', NumXor( 3, 5 ), 6 )
   Check( 'NumXor() twice gives the number  ', NumXor( NumXor( 1234, 9 ), 9 ), 1234 )
   Check( 'NumNot() is 16 bits              ', NumNot( 5 ), 65530 )
   Check( 'NumNot() of "FFFF"               ', NumNot( "FFFF" ), 0 )

RETURN

STATIC PROCEDURE s_Bits()

   Check( 'SetBit() of three bits           ', SetBit( 0, 1, 2, 5 ), 19 )
   Check( 'SetBit() of a hex string         ', SetBit( "F0", 1 ), 241 )
   Check( 'ClearBit() of three bits         ', ClearBit( 255, 3, 4, 6 ), 211 )
   Check( 'ClearBit() of a hex string       ', ClearBit( "FF", 8 ), 127 )
   Check( 'IsBit() bit 1 by default         ', IsBit( 5 ), .T. )
   Check( 'IsBit() bit 2 not set            ', IsBit( 5, 2 ), .F. )
   Check( 'IsBit() of a hex string          ', IsBit( "100", 9 ), .T. )
   Check( 'IsBit() of a negative number     ', IsBit( -1, 32 ), .T. )
   Check( 'IsBit() agrees with SetBit()     ', IsBit( SetBit( 0, 17 ), 17 ), .T. )

RETURN

STATIC PROCEDURE s_Rotate()

   Check( 'NumRol() by 3                    ', NumRol( 1, 3 ), 8 )
   Check( 'NumRol() by 14, 2 to the right   ', NumRol( 4, 14 ), 1 )
   Check( 'NumRol() by 33 is by 1           ', NumRol( 1, 33 ), 2 )
   Check( 'NumRol() of the low byte         ', NumRol( 60000, 2, .T. ), 60033 )

RETURN

STATIC PROCEDURE s_Signed()

   Check( 'IntNeg() of 32768                ', IntNeg( 32768 ), -32768 )
   Check( 'IntNeg() of "FFFF"               ', IntNeg( "FFFF" ), -1 )
   Check( 'IntNeg() of 32 bits              ', IntNeg( "D0000000", .T. ), -805306368 )
   Check( 'IntPos() of -1                   ', IntPos( -1 ), 65535 )
   Check( 'IntPos() keeps 16 bits           ', IntPos( 90000 ), 24464 )
   Check( 'IntPos() of 32 bits              ', IntPos( -60000, .T. ), 4294907296 )
   Check( 'IntPos() of IntNeg() is the same ', IntPos( IntNeg( 40000 ) ), 40000 )

RETURN

STATIC PROCEDURE s_Bytes()

   Check( 'NumHigh() of a word              ', NumHigh( 43981 ), 171 )
   Check( 'NumLow() of a word               ', NumLow( "ABCD" ), 205 )
   Check( 'NumHigh() skips leading spaces   ', NumHigh( "  ABCD" ), 171 )
   Check( 'NumMirr() of 16 bits             ', NumMirr( 1 ), 32768 )
   Check( 'NumMirr() of the low byte        ', NumMirr( 4209, .T. ), 4238 )
   Check( 'NumMirrX() of 32 bits            ', NumMirrX( 32, 1 ), 2147483648 )

RETURN

STATIC PROCEDURE s_Invalid()

   Check( 'invalid: NumAnd() with "GG"      ', NumAnd( 29, "GG" ), -1 )
   Check( 'invalid: NumOr() with .T.        ', NumOr( 3, .T. ), -1 )
   Check( 'invalid: NumNot() without value  ', NumNot(), -1 )
   Check( 'invalid: SetBit() bit 33         ', SetBit( 0, 33 ), -1 )
   Check( 'invalid: ClearBit() bit 0        ', ClearBit( 255, 0 ), -1 )
   Check( 'invalid: IsBit() of .T.          ', IsBit( .T. ), .F. )
   Check( 'invalid: IsBit() bit 33          ', IsBit( 5, 33 ), .F. )
   Check( 'invalid: NumRol() of 70000       ', NumRol( 70000, 1 ), -1 )
   Check( 'invalid: IntNeg() of 90000       ', IntNeg( 90000 ), 0 )
   Check( 'invalid: IntNeg() of -1          ', IntNeg( -1 ), 0 )
   Check( 'invalid: IntPos() of "GG"        ', IntPos( "GG" ), 0 )
   Check( 'invalid: NumHigh() of 65536      ', NumHigh( 65536 ), -1 )
   Check( 'invalid: NumLow() of ""          ', NumLow( "" ), -1 )
   Check( 'invalid: NumMirr() of -1         ', NumMirr( -1 ), -1 )
   Check( 'invalid: NumMirrX() width 33     ', NumMirrX( 33, 1 ), -1 )

RETURN

STATIC PROCEDURE Check( cTitle, xResult, xExpected )

   QOut( cTitle + IIF( ValType( xResult ) == ValType( xExpected ) .AND. xResult == xExpected, "OK", "ERROR" ) )

RETURN


