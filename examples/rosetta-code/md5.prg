/*
 * $DOC$
 * $NAME$
 * MD5
 *
 * $ONELINER$
 * Demonstrates the MD5 task.
 *
 * $DESCRIPTION$
 * Implements the MD5 task in xDev.
 * The RFC 1321 test suite, then the task's own string.
 *
 * $EXAMPLES$
 * examples/rosetta-code/md5.prg
 *
 * $OUTPUT$
 * d41d8cd98f00b204e9800998ecf8427e  ""
 * 0cc175b9c0f1b6a831c399e269772661  "a"
 * 900150983cd24fb0d6963f7d28e17f72  "abc"
 * f96b697d7cb7938d525a2f31aaf161d0  "message digest"
 * c3fcd3d76192e4007dfb496cca67e13b  "abcdefghijklmnopqrstuvwxyz"
 * d174ab98d277d9f5a5611c2c9f419d9f  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"
 * 57edf4a22be3c955ac49da2e2107b67a  "12345678901234567890123456789012345678901234567890123456789012345678901234567890"
 * e38ca1d920c4b8b8d3946b2c72f01680  "The quick brown fox jumped over the lazy dog's back"
 *
 * $DOCEND$
 */

PROCEDURE Main()

   LOCAL cText
   FOR EACH cText IN { "", "a", "abc", "message digest", "abcdefghijklmnopqrstuvwxyz", ;
                       "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789", ;
                       Replicate( "1234567890", 8 ), ;
                       "The quick brown fox jumped over the lazy dog's back" }
      QOut( Lower( StrToHex( HashData( "MD5", cText ) ) ) + "  " + Chr( 34 ) + cText + Chr( 34 ) )
   NEXT

   RETURN
