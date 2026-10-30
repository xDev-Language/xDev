/*
 *
 */

PROCEDURE Main()

   LOCAL aCena  := { 4.50, 3.20, 8.99 }
   LOCAL aNazwa := { "chleb", "mleko", "masło" }
   LOCAL hStan  := { "chleb" => 10, "mleko" => 0, "masło" => 4 }
   LOCAL cKod   := "ab-12-cd"
   LOCAL cNazwa, nCena, nIlosc, c

   QOut( "FOR EACH w xDev" )
   QOut( "" )

   // dwie tablice naraz, :__enumIndex to numer pozycji
   FOR EACH cNazwa, nCena IN aNazwa, aCena
      QOut( Str( cNazwa:__enumIndex, 1 ) + ". " + PadR( cNazwa, 8 ) + Str( nCena, 6, 2 ) )
   NEXT

   // zmienna to element tablicy: przypisanie zmienia tablicę
   FOR EACH nCena IN aCena
      nCena := Round( nCena * 1.23, 2 )
   NEXT
   QOut( "Z VAT: " + Str( aCena[ 1 ], 6, 2 ) + Str( aCena[ 2 ], 6, 2 ) + Str( aCena[ 3 ], 6, 2 ) )
   QOut( "" )

   // haszmapa: :__enumKey to klucz, LOOP pomija pozycję
   FOR EACH nIlosc IN hStan
      IF nIlosc == 0
         QOut( nIlosc:__enumKey + ": brak" )
         LOOP
      ENDIF
      QOut( nIlosc:__enumKey + ": " + LTrim( Str( nIlosc ) ) + IIF( nIlosc:__enumIsLast, " (ostatni)", "" ) )
   NEXT
   QOut( "" )

   // tekst przez @ zmienia się na miejscu, DESCEND idzie od końca
   FOR EACH c IN @cKod
      IF c == "-"
         c := "/"
      ELSE
         c := Upper( c )
      ENDIF
   NEXT
   QOut( "Kod: " + cKod )
   QQOut( "  od końca: " )
   FOR EACH c IN cKod DESCEND
      QQOut( c )
   NEXT

RETURN
