/*
 * $DOC$
 * $NAME$
 * Palindrome detection
 *
 * $ONELINER$
 * Demonstrates the Palindrome detection task.
 *
 * $DESCRIPTION$
 * Implements the Palindrome detection task in xDev.
 * Letters only, case ignored.
 *
 * $EXAMPLES$
 * examples/rosetta-code/palindrome-detection.prg
 *
 * $OUTPUT$
 * "racecar"                           is a palindrome
 * "rotor"                             is a palindrome
 * "xDev"                              is not a palindrome
 * "A man, a plan, a canal: Panama!"   is a palindrome
 *
 * $DOCEND$
 */

PROCEDURE Main()

   LOCAL cText

   FOR EACH cText IN { "racecar", "rotor", "xDev", "A man, a plan, a canal: Panama!" }
      QOut( PadR( Chr( 34 ) + cText + Chr( 34 ), 36 ) + IIF( IsPalindrome( cText ), "is", "is not" ) + " a palindrome" )
   NEXT

   RETURN
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
