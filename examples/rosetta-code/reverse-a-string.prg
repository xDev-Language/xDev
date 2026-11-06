/*
 * Reverse a string
 */

PROCEDURE Main()

   QOut( Reverse( "asdf" ) )
   QOut( Reverse( "Zażółć gęślą jaźń" ) )

   RETURN

// ULen() and USubStr() count UTF-8 characters, not bytes
STATIC FUNCTION Reverse( cText AS STRING ) AS STRING

   LOCAL cResult AS STRING := ""
   LOCAL i       AS INT

   FOR i := ULen( cText ) TO 1 STEP -1
      cResult += USubStr( cText, i, 1 )
   NEXT

   RETURN cResult
