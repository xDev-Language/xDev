/*
 *
 */

#include "hash.ch"

PROCEDURE Main()

   LOCAL hCena  := { "chleb" => 4.50, "mleko" => 3.20, "masło" => 8.99 }
   LOCAL hKopia
   LOCAL hRazem
   LOCAL nSuma  := 0
   LOCAL nPoz

   QOut( "Haszmapy w xDev" )
   QOut( "" )

   // nowy klucz dodaje się przez przypisanie, kolejność dodawania zostaje
   hCena[ "ser" ] := 12.40
   QOut( "Pozycji: " + LTrim( Str( Len( hCena ) ) ) + ", typ: " + ValType( hCena ) )

   HEval( hCena, {| cNazwa, nCena, nPos | QOut( Str( nPos, 2 ) + ". " + PadR( cNazwa, 8 ) + Str( nCena, 8, 2 ) ), nSuma += nCena } )
   QOut( "   Razem:  " + Str( nSuma, 8, 2 ) )
   QOut( "" )

   // szukanie wartości i klucza
   nPoz := HScan( hCena, 3.20 )
   QOut( "Cena 3.20 ma: " + HGetKeyAt( hCena, nPoz ) )
   nPoz := HScan( hCena, {| cNazwa, nCena | nCena > 10 } )
   QOut( "Pierwsze droższe niż 10: " + HGetKeyAt( hCena, nPoz ) )
   QOut( "Jest mleko? " + IIF( HHasKey( hCena, "mleko" ), "tak", "nie" ) )

   // bez rozróżniania wielkości liter
   HSetCaseMatch( hCena, .F. )
   QOut( "CHLEB: " + Str( hCena[ "CHLEB" ], 5, 2 ) )
   QOut( "" )

   // kopia jest osobna, przypisanie to ta sama haszmapa
   hKopia := HClone( hCena )
   hKopia[ "chleb" ] := 5.00
   QOut( "Chleb w oryginale: " + Str( hCena[ "chleb" ], 5, 2 ) + ", w kopii: " + Str( hKopia[ "chleb" ], 5, 2 ) )

   // łączenie: dodaje nowe klucze i zmienia te, które już są
   hRazem := HMerge( HClone( hCena ), { "jajka" => 9.90, "chleb" => 4.80 }, HASH_UNION )
   QOut( "Po HMerge: " + LTrim( Str( Len( hRazem ) ) ) + " pozycji, chleb " + Str( hRazem[ "chleb" ], 5, 2 ) )

   // usuwanie
   HDel( hRazem, "masło" )
   QOut( "Po HDel masła: " + LTrim( Str( Len( hRazem ) ) ) + " pozycji" )

   // klucze i wartości jako tablice
   QOut( "Klucze: " )
   AEval( HGetKeys( hRazem ), {| cKlucz | QQOut( cKlucz + " " ) } )

RETURN
