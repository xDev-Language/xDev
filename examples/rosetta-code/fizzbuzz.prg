/*
 * FizzBuzz
 */

PROCEDURE Main()

   LOCAL i AS INT

   FOR i := 1 TO 100
      DO CASE
      CASE i % 15 == 0
         QOut( "FizzBuzz" )
      CASE i % 3 == 0
         QOut( "Fizz" )
      CASE i % 5 == 0
         QOut( "Buzz" )
      OTHERWISE
         QOut( LTrim( Str( i ) ) )
      ENDCASE
   NEXT

   RETURN
