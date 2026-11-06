/*
 * Environment variables
 */

PROCEDURE Main()

   QOut( GetEnv( "PATH" ) )
   QOut( GetEnv( "NO_SUCH_VARIABLE", "(not set)" ) )

   RETURN
