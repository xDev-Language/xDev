/*
 * $DOC$
 * $NAME$
 * JSON and hash maps
 *
 * $ONELINER$
 * Demonstrates parsing, building, and serializing JSON data.
 *
 * $DESCRIPTION$
 * Pokazuje odczyt zagnieżdżonych obiektów JSON jako haszmap, budowę zapytania z haszmapy, zachowanie kolejności kluczy, obsługę Unicode i emoji oraz błąd parsowania.
 *
 * $EXAMPLES$
 * examples/json.prg
 *
 * $OUTPUT$
 * JSON w xDev
 *
 * referenceNumber: 20261030-AU-1A2B3C
 * token:           eyJhbGciOi
 * code:            200 (N)
 * ok:              tak
 * kwota:            1234.56
 * uwagi:           U
 *
 * {"challenge":"20261030-CR-77","contextIdentifier":{"type":"Nip","value":"1234567890"},"encryptedToken":"c2VrcmV0","daty":["2026-11-07",null],"liczby":[1,-2,3.5,0.3333333333333333]}
 *
 * {
 *   "firma": "Zakład \"Pod Lipą\"",
 *   "linie": "a\nb",
 *   "lista": []
 * }
 *
 * Unicode: Zażółć gęślą jaźń 😀
 * Tam i z powrotem: OK
 * Błędny JSON: U, expected a key in quotes at position 11
 *
 * $DOCEND$
 */

PROCEDURE Main()

   LOCAL cOdp   := '{ "referenceNumber": "20261030-AU-1A2B3C", "authenticationToken": { "token": "eyJhbGciOi", ' + ;
                   '"validUntil": "2026-10-30T22:30:00+01:00" }, "status": { "code": 200, "ok": true, "kwota": 1234.56, "uwagi": null } }'
   LOCAL hOdp
   LOCAL hZapytanie
   LOCAL xBlad
   LOCAL cTekst

   QOut( "JSON w xDev" )
   QOut( "" )

   // odczyt odpowiedzi: obiekt to haszmapa, zagnieżdżone obiekty to haszmapy w haszmapie
   hOdp := json_Decode( cOdp )
   QOut( "referenceNumber: " + hOdp[ "referenceNumber" ] )
   QOut( "token:           " + hOdp[ "authenticationToken" ][ "token" ] )
   QOut( "code:            " + LTrim( Str( hOdp[ "status" ][ "code" ] ) ) + " (" + ValType( hOdp[ "status" ][ "code" ] ) + ")" )
   QOut( "ok:              " + IIF( hOdp[ "status" ][ "ok" ], "tak", "nie" ) )
   QOut( "kwota:           " + Str( hOdp[ "status" ][ "kwota" ], 8, 2 ) )
   QOut( "uwagi:           " + ValType( hOdp[ "status" ][ "uwagi" ] ) )
   QOut( "" )

   // budowa zapytania: haszmapa zamienia się w obiekt, kolejność kluczy zostaje
   hZapytanie := { "challenge" => "20261030-CR-77", ;
                   "contextIdentifier" => { "type" => "Nip", "value" => "1234567890" }, ;
                   "encryptedToken" => "c2VrcmV0", ;
                   "daty" => { Date(), CToD( "" ) }, ;
                   "liczby" => { 1, -2, 3.5, 1/3 } }
   QOut( json_Encode( hZapytanie ) )
   QOut( "" )
   QOut( json_Encode( { "firma" => 'Zakład "Pod Lipą"', "linie" => "a" + Chr( 10 ) + "b", "lista" => {} }, .T. ) )
   QOut( "" )

   // znaki \uXXXX i para zastępcza (emoji) zamieniają się w UTF-8
   QOut( "Unicode: " + json_Decode( '"Zażółć gęślą jaźń 😀"' ) )

   // tam i z powrotem
   cTekst := json_Encode( hOdp )
   QOut( "Tam i z powrotem: " + IIF( json_Encode( json_Decode( cTekst ) ) == cTekst, "OK", "różnica" ) )

   // błąd
   xBlad := json_Decode( '{ "a": 1, }' )
   QOut( "Błędny JSON: " + ValType( xBlad ) + ", " + json_Error() )

RETURN
