/*
 * $DOC$
 * $NAME$
 * JSON
 *
 * $ONELINER$
 * Demonstrates the JSON task.
 *
 * $DESCRIPTION$
 * Implements the JSON task in xDev.
 * A JSON object becomes a hash, an array an array.
 * And back: the order of the keys is kept.
 *
 * $EXAMPLES$
 * examples/rosetta-code/json.prg
 *
 * $OUTPUT$
 * foo  = 1
 * bar  = 10 apples
 * ok   = .T.
 * none = NIL
 * {"name":"Jan Kowalski","age":42,"languages":["xDev","C"],"address":{"city":"Kraków"}}
 * {
 *   "name": "Jan Kowalski",
 *   "age": 42,
 *   "languages": [
 *     "xDev",
 *     "C"
 *   ],
 *   "address": {
 *     "city": "Kraków"
 *   }
 * }
 *
 * $DOCEND$
 */

PROCEDURE Main()

   LOCAL hData
   LOCAL hPerson
   hData := json_Decode( '{ "foo": 1, "bar": [10, "apples"], "ok": true, "none": null }' )
   QOut( "foo  =", hData[ "foo" ] )
   QOut( "bar  =", hData[ "bar" ][ 1 ], hData[ "bar" ][ 2 ] )
   QOut( "ok   =", hData[ "ok" ] )
   QOut( "none =", hData[ "none" ] )
   hPerson := { "name" => "Jan Kowalski", "age" => 42, "languages" => { "xDev", "C" }, "address" => { "city" => "Kraków" } }
   QOut( json_Encode( hPerson ) )
   QOut( json_Encode( hPerson, .T. ) )

   RETURN
