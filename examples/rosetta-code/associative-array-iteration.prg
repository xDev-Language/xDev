/*
 * Associative array/Iteration
 */

PROCEDURE Main()

   LOCAL hCapital := { "Poland" => "Warsaw", "France" => "Paris", "Japan" => "Tokyo" }
   LOCAL cCity, cCountry

   hCapital[ "Peru" ] := "Lima"          // a new key is added by assignment

   // key and value; the insertion order is kept
   FOR EACH cCity IN hCapital
      QOut( PadR( cCity:__enumKey, 8 ) + "-> " + cCity )
   NEXT

   // the same with a code block
   HEval( hCapital, {| cKey, cValue | QOut( cKey + ": " + cValue ) } )

   // keys only and values only
   QOut( "" )
   FOR EACH cCountry IN HGetKeys( hCapital )
      QQOut( cCountry + " " )
   NEXT
   QOut( "" )
   FOR EACH cCity IN HGetValues( hCapital )
      QQOut( cCity + " " )
   NEXT

   RETURN
