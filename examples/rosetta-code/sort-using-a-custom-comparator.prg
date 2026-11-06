/*
 * Sort using a custom comparator
 */

PROCEDURE Main()

   LOCAL aWords := { "Here", "are", "some", "sample", "strings", "to", "be", "sorted" }

   // longest first, then alphabetically without regard to case
   ASort( aWords, , , {| a, b | IIF( Len( a ) == Len( b ), Upper( a ) < Upper( b ), Len( a ) > Len( b ) ) } )
   AEval( aWords, {| c | QQOut( c + " " ) } )

   RETURN
