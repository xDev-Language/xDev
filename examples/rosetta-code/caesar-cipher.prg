/*
 * $DOC$
 * $NAME$
 * Caesar cipher
 *
 * $ONELINER$
 * Demonstrates the Caesar cipher task.
 *
 * $DESCRIPTION$
 * Implements the Caesar cipher task in xDev.
 *
 * $EXAMPLES$
 * examples/rosetta-code/caesar-cipher.prg
 *
 * $OUTPUT$
 * The quick brown fox Jumped over the lazy Dog
 * Esp bftnv mczhy qzi Ufxapo zgpc esp wlkj Ozr
 * The quick brown fox Jumped over the lazy Dog
 *
 * $DOCEND$
 */

PROCEDURE Main()

   LOCAL cPlain   := "The quick brown fox Jumped over the lazy Dog"
   LOCAL cEncoded := Caesar( cPlain, 11 )

   QOut( cPlain )
   QOut( cEncoded )
   QOut( Caesar( cEncoded, -11 ) )

   RETURN

STATIC FUNCTION Caesar( cText AS STRING, nKey AS INT ) AS STRING

   LOCAL cResult AS STRING := ""
   LOCAL nBase   AS INT
   LOCAL c

   FOR EACH c IN cText
      DO CASE
      CASE c >= "A" .AND. c <= "Z"
         nBase := Asc( "A" )
      CASE c >= "a" .AND. c <= "z"
         nBase := Asc( "a" )
      OTHERWISE
         cResult += c
         LOOP
      ENDCASE
      cResult += Chr( nBase + ( Asc( c ) - nBase + nKey + 26 ) % 26 )
   NEXT

   RETURN cResult
