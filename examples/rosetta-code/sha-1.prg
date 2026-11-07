/*
 * $DOC$
 * $NAME$
 * SHA-1
 *
 * $ONELINER$
 * Demonstrates the SHA-1 task.
 *
 * $DESCRIPTION$
 * Implements the SHA-1 task in xDev.
 *
 * $EXAMPLES$
 * examples/rosetta-code/sha-1.prg
 *
 * $OUTPUT$
 * 48c98f7e5a6e736d790ab740dfc3f51a61abe2b5
 *
 * $DOCEND$
 */

PROCEDURE Main()

   QOut( Lower( StrToHex( HashData( "SHA1", "Rosetta Code" ) ) ) )

   RETURN
