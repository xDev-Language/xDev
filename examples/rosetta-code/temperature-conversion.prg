/*
 * $DOC$
 * $NAME$
 * Temperature conversion
 *
 * $ONELINER$
 * Demonstrates the Temperature conversion task.
 *
 * $DESCRIPTION$
 * Implements the Temperature conversion task in xDev.
 *
 * $EXAMPLES$
 * examples/rosetta-code/temperature-conversion.prg
 *
 * $OUTPUT$
 * K    21.00
 * C  -252.15
 * F  -421.87
 * R    37.80
 *
 * $DOCEND$
 */

PROCEDURE Main()

   Convert( 21 )

   RETURN

STATIC PROCEDURE Convert( nKelvin AS FLOAT )

   QOut( "K  " + Str( nKelvin, 7, 2 ) )
   QOut( "C  " + Str( nKelvin - 273.15, 7, 2 ) )
   QOut( "F  " + Str( nKelvin * 1.8 - 459.67, 7, 2 ) )
   QOut( "R  " + Str( nKelvin * 1.8, 7, 2 ) )

   RETURN
