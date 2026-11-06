/*
 * Temperature conversion
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
