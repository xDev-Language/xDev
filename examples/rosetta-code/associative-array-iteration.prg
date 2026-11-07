/*
 * $DOC$
 * $NAME$
 * Associative array/Iteration
 *
 * $ONELINER$
 * Demonstrates the Associative array/Iteration task.
 *
 * $DESCRIPTION$
 * Implements the Associative array/Iteration task in xDev.
 * Key and value; the insertion order is kept.
 * The same with a code block.
 * Keys only and values only.
 * Assigning a new key adds it to the hash map.
 *
 * $EXAMPLES$
 * examples/rosetta-code/associative-array-iteration.prg
 *
 * $OUTPUT$
 * Poland  -> Warsaw
 * France  -> Paris
 * Japan   -> Tokyo
 * Peru    -> Lima
 * Poland: Warsaw
 * France: Paris
 * Japan: Tokyo
 * Peru: Lima
 * Poland France Japan Peru
 * Warsaw Paris Tokyo Lima
 *
 * $DOCEND$
 */

PROCEDURE Main()

   LOCAL hCapital := { "Poland" => "Warsaw", "France" => "Paris", "Japan" => "Tokyo" }
   LOCAL cCity, cCountry

   hCapital[ "Peru" ] := "Lima"
   FOR EACH cCity IN hCapital
      QOut( PadR( cCity:__enumKey, 8 ) + "-> " + cCity )
   NEXT
   HEval( hCapital, {| cKey, cValue | QOut( cKey + ": " + cValue ) } )
   QOut( "" )
   FOR EACH cCountry IN HGetKeys( hCapital )
      QQOut( cCountry + " " )
   NEXT
   QOut( "" )
   FOR EACH cCity IN HGetValues( hCapital )
      QQOut( cCity + " " )
   NEXT

   RETURN
