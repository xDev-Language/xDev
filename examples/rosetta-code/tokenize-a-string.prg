/*
 * $DOC$
 * $NAME$
 * Tokenize a string
 *
 * $ONELINER$
 * Demonstrates the Tokenize a string task.
 *
 * $DESCRIPTION$
 * Implements the Tokenize a string task in xDev.
 *
 * $EXAMPLES$
 * examples/rosetta-code/tokenize-a-string.prg
 *
 * $OUTPUT$
 * Hello.How.Are.You.Today
 *
 * $DOCEND$
 */

PROCEDURE Main()

   LOCAL aWords := ATokens( "Hello,How,Are,You,Today", "," )
   LOCAL cResult := ""
   LOCAL cWord

   FOR EACH cWord IN aWords
      cResult += cWord + IIF( cWord:__enumIsLast, "", "." )
   NEXT
   QOut( cResult )

   RETURN
