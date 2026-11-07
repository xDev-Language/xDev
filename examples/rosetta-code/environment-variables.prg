/*
 * $DOC$
 * $NAME$
 * Environment variables
 *
 * $ONELINER$
 * Demonstrates the Environment variables task.
 *
 * $DESCRIPTION$
 * Implements the Environment variables task in xDev.
 *
 * $EXAMPLES$
 * examples/rosetta-code/environment-variables.prg
 *
 * $OUTPUT$
 * C:\Windows\System32
 * (not set)
 *
 * $DOCEND$
 */

PROCEDURE Main()

   QOut( GetEnv( "PATH" ) )
   QOut( GetEnv( "NO_SUCH_VARIABLE", "(not set)" ) )

   RETURN
