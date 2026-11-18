/*
 * $DOC$
 * $NAME$
 * Smiley interpreter
 *
 * $ONELINER$
 * Demonstrates evaluating a small Smiley-language program.
 *
 * $DESCRIPTION$
 * Runs a sample program in the Smiley interpreter and reports the value left in its accumulator.
 * On Windows, test in Windows PowerShell using a terminal and font that support emoji.
 * Emoji display.
 *
 * $EXAMPLES$
 * examples/smiley.prg
 *
 * $OUTPUT$
 * The program "+ + * 😃 - /" calculates the value 7.5
 *
 * $DOCEND$
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
