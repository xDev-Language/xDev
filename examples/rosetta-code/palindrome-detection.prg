/*
 * Palindrome detection
 */

PROCEDURE Main()

   LOCAL cText

   FOR EACH cText IN { "racecar", "rotor", "xDev", "A man, a plan, a canal: Panama!" }
      QOut( PadR( Chr( 34 ) + cText + Chr( 34 ), 36 ) + IIF( IsPalindrome( cText ), "is", "is not" ) + " a palindrome" )
   NEXT

   RETURN

// letters only, case ignored
STATIC FUNCTION IsPalindrome( cText AS STRING ) AS LOGIC

   LOCAL cLetters AS STRING := ""
   LOCAL c

   FOR EACH c IN Lower( cText )
      IF IsAlpha( c )
         cLetters += c
      ENDIF
   NEXT

   RETURN cLetters == Reverse( cLetters )

STATIC FUNCTION Reverse( cText AS STRING ) AS STRING

   LOCAL cResult AS STRING := ""
   LOCAL c

   FOR EACH c IN cText DESCEND
      cResult += c
   NEXT

   RETURN cResult
