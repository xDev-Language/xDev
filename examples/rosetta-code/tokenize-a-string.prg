/*
 * Tokenize a string
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
