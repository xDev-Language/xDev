/*
 * Exceptions
 */

PROCEDURE Main()

   LOCAL xError

   // Break() throws any value, RECOVER USING catches it
   BEGIN SEQUENCE
      QOut( "Before" )
      s_Risky( 2 )
      QOut( "Not reached" )
   RECOVER USING xError
      QOut( "Caught: " + xError )
   END SEQUENCE

   // a runtime error becomes an exception when the error block calls Break()
   ErrorBlock( {| oError | Break( oError ) } )
   BEGIN SEQUENCE
      QOut( 1 / 0 )
   RECOVER USING xError
      QOut( "Caught runtime error: " + xError:description )
   END SEQUENCE

   RETURN

STATIC PROCEDURE s_Risky( nLevel AS INT )

   IF nLevel == 0
      Break( "an exception from deep inside" )
   ENDIF
   s_Risky( nLevel - 1 )

   RETURN
