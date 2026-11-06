/*
 * 99 bottles of beer
 */

PROCEDURE Main()

   LOCAL n AS INT

   FOR n := 99 TO 1 STEP -1
      QOut( Bottles( n ) + " of beer on the wall, " + Bottles( n ) + " of beer." )
      QOut( "Take one down, pass it around, " + Bottles( n - 1 ) + " of beer on the wall." )
      QOut( "" )
   NEXT

   RETURN

STATIC FUNCTION Bottles( n AS INT ) AS STRING

   DO CASE
   CASE n == 0
      RETURN "no more bottles"
   CASE n == 1
      RETURN "1 bottle"
   ENDCASE

   RETURN LTrim( Str( n ) ) + " bottles"
