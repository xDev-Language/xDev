/*
 * MD5
 */

PROCEDURE Main()

   LOCAL cText

   // the RFC 1321 test suite, then the task's own string
   FOR EACH cText IN { "", "a", "abc", "message digest", "abcdefghijklmnopqrstuvwxyz", ;
                       "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789", ;
                       Replicate( "1234567890", 8 ), ;
                       "The quick brown fox jumped over the lazy dog's back" }
      QOut( Lower( StrToHex( HashData( "MD5", cText ) ) ) + "  " + Chr( 34 ) + cText + Chr( 34 ) )
   NEXT

   RETURN
