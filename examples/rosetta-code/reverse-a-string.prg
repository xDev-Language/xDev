/*
 * $DOC$
 * $NAME$
 * Reverse a string
 *
 * $ONELINER$
 * Demonstrates the Reverse a string task.
 *
 * $DESCRIPTION$
 * Implements the Reverse a string task in xDev.
 * ULen() and USubStr() count UTF-8 characters, not bytes.
 *
 * $EXAMPLES$
 * examples/rosetta-code/reverse-a-string.prg
 *
 * $OUTPUT$
 * fdsa
 * ńźaj ąlśęg ćłóżaZ
 *
 * $DOCEND$
 */

PROCEDURE Main()

   QOut( Reverse( "asdf" ) )
   QOut( Reverse( "Zażółć gęślą jaźń" ) )

   RETURN
STATIC FUNCTION Reverse( cText AS STRING ) AS STRING

   LOCAL cResult AS STRING := ""
   LOCAL i       AS INT

   FOR i := ULen( cText ) TO 1 STEP -1
      cResult += USubStr( cText, i, 1 )
   NEXT

   RETURN cResult
