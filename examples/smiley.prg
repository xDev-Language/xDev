/*
 *
 */

PROCEDURE Main()

   LOCAL cProgram     := "+ + * 😃 - /"
   LOCAL nAccumulator := 0
   LOCAL cToken
   LOCAL i AS int

   FOR i := 1 TO ULen( cProgram )
      cToken := USubStr( cProgram, i, 1 )
      DO CASE
      CASE cToken == "+"
         nAccumulator += 1
      CASE cToken == "-"
         nAccumulator -= 1
      CASE cToken == "*"
         nAccumulator *= 2
      CASE cToken == "/"
         nAccumulator /= 2
      CASE cToken == "😃"
         nAccumulator *= nAccumulator
      ENDCASE
   NEXT

   QOut( 'The program "' + cProgram + '" calculates the value ' + LTrim( Str( nAccumulator ) ) )

   RETURN
