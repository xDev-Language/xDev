/*
 * xbase 7 table: field names longer than 10 characters, DBT memo and Polish characters.
 * The program creates faktury.dbf and faktury.dbt, writes a few records and prints them again
 * after reopening the file. The files can be checked in another program that reads xbase 7 tables
 * (CP852 encoding).
 */

PROCEDURE Main()

   LOCAL aData := { ;
      { "FV/1/2026", "Kowalski i Wspólnicy sp. j.",  "Warszawa", 1250.50, .T., 14, "Dostawa częściowa, reszta w grudniu." }, ;
      { "FV/2/2026", "Zakład Usług Wiśniewski",      "Gdańsk",    8400.00, .F., 30, "" }, ;
      { "FV/3/2026", "Łódzka Spółdzielnia Pracy",    "Łódź",        99.99, .T.,  7, Replicate( "Długa uwaga do faktury. ", 20 ) } }
   LOCAL nRow

   dbCreate( "faktury", { ;
      { "NUMER_FAKTURY",                "C", 15, 0 }, ;
      { "NAZWA_ODBIORCY",               "C", 40, 0 }, ;
      { "MIEJSCOWOSC_ODBIORCY",         "C", 20, 0 }, ;
      { "DATA_WYSTAWIENIA",             "D",  8, 0 }, ;
      { "KWOTA_NETTO_RAZEM_ZA_FAKTURE", "N", 12, 2 }, ;
      { "ZAPLACONO",                    "L",  1, 0 }, ;
      { "TERMIN_PLATNOSCI_W_DNIACH",    "N",  3, 0 }, ;
      { "UWAGI_DO_FAKTURY",             "M", 10, 0 } } )

   dbUseArea( .T., NIL, "faktury" )
   FOR nRow := 1 TO Len( aData )
      dbAppend()
      faktury->NUMER_FAKTURY                := aData[ nRow ][ 1 ]
      faktury->NAZWA_ODBIORCY               := aData[ nRow ][ 2 ]
      faktury->MIEJSCOWOSC_ODBIORCY         := aData[ nRow ][ 3 ]
      faktury->DATA_WYSTAWIENIA             := Date() - nRow
      faktury->KWOTA_NETTO_RAZEM_ZA_FAKTURE := aData[ nRow ][ 4 ]
      faktury->ZAPLACONO                    := aData[ nRow ][ 5 ]
      faktury->TERMIN_PLATNOSCI_W_DNIACH    := aData[ nRow ][ 6 ]
      faktury->UWAGI_DO_FAKTURY             := aData[ nRow ][ 7 ]
   NEXT
   dbCloseArea()

   // Reopen: structure and data are read from the file
   dbUseArea( .T., NIL, "faktury" )
   QOut( "Pola tabeli faktury.dbf:" )
   AEval( dbStruct(), {| aField | QOut( "  " + PadR( aField[ 1 ], 32 ), aField[ 2 ], aField[ 3 ], aField[ 4 ] ) } )
   QOut( "" )
   dbGoTop()
   DO WHILE ! Eof()
      QOut( RTrim( faktury->NUMER_FAKTURY ), RTrim( faktury->NAZWA_ODBIORCY ), ;
            RTrim( faktury->MIEJSCOWOSC_ODBIORCY ), faktury->DATA_WYSTAWIENIA, ;
            faktury->KWOTA_NETTO_RAZEM_ZA_FAKTURE, IIF( faktury->ZAPLACONO, "zapłacono", "do zapłaty" ), ;
            faktury->TERMIN_PLATNOSCI_W_DNIACH, "dni" )
      QOut( "  Uwagi:", IIF( Empty( faktury->UWAGI_DO_FAKTURY ), "(brak)", Left( faktury->UWAGI_DO_FAKTURY, 60 ) ) )
      dbSkip()
   ENDDO
   dbCloseArea()

   RETURN
