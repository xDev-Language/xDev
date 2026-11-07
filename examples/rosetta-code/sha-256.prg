/*
 * $DOC$
 * $NAME$
 * SHA-256
 *
 * $ONELINER$
 * Demonstrates the SHA-256 task.
 *
 * $DESCRIPTION$
 * Implements the SHA-256 task in xDev.
 * HashData() comes from the xopenssl library shipped with xDev.
 *
 * $EXAMPLES$
 * examples/rosetta-code/sha-256.prg
 *
 * $OUTPUT$
 * 764faf5c61ac315f1497f9dfa542713965b785e5cc2f707d6468d7d1124cdfcf
 *
 * $DOCEND$
 */

PROCEDURE Main()
   QOut( Lower( StrToHex( HashData( "SHA256", "Rosetta code" ) ) ) )

   RETURN
