/*
 * $DOC$
 * $NAME$
 * Bitwise operations
 *
 * $ONELINER$
 * Demonstrates the Bitwise operations task.
 *
 * $DESCRIPTION$
 * Implements the Bitwise operations task in xDev.
 * Integers are 64-bit; BitShift() shifts right filling with 0, so the arithmetic shift is a division.
 *
 * $EXAMPLES$
 * examples/rosetta-code/bitwise-operations.prg
 *
 * $OUTPUT$
 * a and b:           2
 * a or b:            255
 * a xor b:           253
 * not a:             -256
 * a << b:            1020
 * a >> b:            63
 * a >> b arithmetic: 63
 * a rotl b:          1020
 * a rotr b:          -4611686018427387841
 *
 * a and b:           0
 * a or b:            -253
 * a xor b:           -253
 * not a:             254
 * a << b:            -1020
 * a >> b:            4611686018427387840
 * a >> b arithmetic: -64
 * a rotl b:          -1017
 * a rotr b:          9223372036854775744
 *
 * $DOCEND$
 */

PROCEDURE Main()

   s_Bitwise( 255, 2 )
   QOut( "" )
   s_Bitwise( -255, 2 )

   RETURN
STATIC PROCEDURE s_Bitwise( a AS INT, b AS INT )

   QOut( "a and b:          ", BitAnd( a, b ) )
   QOut( "a or b:           ", BitOr( a, b ) )
   QOut( "a xor b:          ", BitXor( a, b ) )
   QOut( "not a:            ", BitNot( a ) )
   QOut( "a << b:           ", BitShift( a, b ) )
   QOut( "a >> b:           ", BitShift( a, -b ) )
   QOut( "a >> b arithmetic:", Floor( a / 2 ^ b ) )
   QOut( "a rotl b:         ", BitRotate( a, b ) )
   QOut( "a rotr b:         ", BitRotate( a, -b ) )

   RETURN
