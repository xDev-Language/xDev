/*
 * Bitwise operations
 */

PROCEDURE Main()

   s_Bitwise( 255, 2 )
   QOut( "" )
   s_Bitwise( -255, 2 )

   RETURN

// integers are 64-bit; BitShift() shifts right filling with 0, so the arithmetic shift is a division
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
