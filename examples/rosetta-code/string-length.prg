/*
 * $DOC$
 * $NAME$
 * String length
 *
 * $ONELINER$
 * Demonstrates the String length task.
 *
 * $DESCRIPTION$
 * Implements the String length task in xDev.
 * Len() counts bytes, ULen() counts UTF-8 characters.
 *
 * $EXAMPLES$
 * examples/rosetta-code/string-length.prg
 *
 * $OUTPUT$
 * Hello: 5 bytes, 5 characters
 * møøse: 7 bytes, 5 characters
 * 𝔘𝔫𝔦𝔠𝔬𝔡𝔢: 28 bytes, 7 characters
 * Łódź: 7 bytes, 4 characters
 *
 * $DOCEND$
 */

PROCEDURE Main()

   LOCAL cText
   FOR EACH cText IN { "Hello", "møøse", "𝔘𝔫𝔦𝔠𝔬𝔡𝔢", "Łódź" }
      QOut( cText + ": " + LTrim( Str( Len( cText ) ) ) + " bytes, " + LTrim( Str( ULen( cText ) ) ) + " characters" )
   NEXT

   RETURN
