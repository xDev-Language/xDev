/*
 * Remove duplicate elements
 */

PROCEDURE Main()

   LOCAL aList  := { 1, "a", 2, 1, "b", "a", 3, 2, 1 }
   LOCAL hSeen  := { => }
   LOCAL xValue

   // a hash keeps the order of insertion, so the first occurrence wins
   FOR EACH xValue IN aList
      hSeen[ xValue ] := .T.
   NEXT
   AEval( HGetKeys( hSeen ), {| x | QQOut( x, "" ) } )

   RETURN
