/*
 * JSON
 */

PROCEDURE Main()

   LOCAL hData
   LOCAL hPerson

   // a JSON object becomes a hash, an array an array
   hData := json_Decode( '{ "foo": 1, "bar": [10, "apples"], "ok": true, "none": null }' )
   QOut( "foo  =", hData[ "foo" ] )
   QOut( "bar  =", hData[ "bar" ][ 1 ], hData[ "bar" ][ 2 ] )
   QOut( "ok   =", hData[ "ok" ] )
   QOut( "none =", hData[ "none" ] )

   // and back: the order of the keys is kept
   hPerson := { "name" => "Jan Kowalski", "age" => 42, "languages" => { "xDev", "C" }, "address" => { "city" => "Kraków" } }
   QOut( json_Encode( hPerson ) )
   QOut( json_Encode( hPerson, .T. ) )

   RETURN
