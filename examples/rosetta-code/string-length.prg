/*
 * String length
 */

PROCEDURE Main()

   LOCAL cText

   // Len() counts bytes, ULen() counts UTF-8 characters
   FOR EACH cText IN { "Hello", "møøse", "𝔘𝔫𝔦𝔠𝔬𝔡𝔢", "Łódź" }
      QOut( cText + ": " + LTrim( Str( Len( cText ) ) ) + " bytes, " + LTrim( Str( ULen( cText ) ) ) + " characters" )
   NEXT

   RETURN
