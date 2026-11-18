   /*
 * $DOC$
 * $NAME$
 * xDev 7 RDD
 *
 * $ONELINER$
 * Tests the xDev 7 RDD: tables, UTF-8 text, memo, indexes, locks and shared access.
 *
 * $DESCRIPTION$
 * Creates dbf7test.dbf with a DBT memo and an NTX index, checks every field type, UTF-8
 * validation and truncation, numeric overflow, DELETED/PACK/ZAP, index maintenance and
 * reopening with compiled key blocks. The same table is then opened twice in shared mode:
 * a record lock held in one area must not stop reads in the other one. A flag in byte 14
 * of the header, left by an older version, must not block the table. The bulk part uses
 * dbf7bulk.dbf for COUNT, SUM, AVERAGE, COPY, APPEND FROM, SORT, JOIN, TOTAL, UPDATE and
 * the SDF, DELIMITED and CSV transfers. All files are removed at the end.
 *
 * $EXAMPLES$
 *
 * $OUTPUT$
 * create: .dbf and .dbt exist           OK
 * format is xDev 7                      OK
 * field count                           OK
 * 28-character field name               OK
 * append three records                  OK
 * UTF-8 text read back                  OK
 * C field cut on a UTF-8 boundary       OK
 * invalid UTF-8 is an error             OK
 * N with decimals                       OK
 * N without decimals is an integer      OK
 * N overflow is an error                OK
 * N overflow keeps the old value        OK
 * D field                               OK
 * empty D field                         OK
 * L field                               OK
 * long memo read back                   OK
 * empty memo                            OK
 * memo rewritten shorter                OK
 * memo rewritten longer                 OK
 * values after reopening                OK
 * DbCount()                             OK
 * DbSum()                               OK
 * Deleted()                             OK
 * SET DELETED skips the record          OK
 * DbRecall()                            OK
 * DbPack() removes the record           OK
 * memo after DbPack()                   OK
 * DbCopy() copies all records           OK
 * index: first key after DbGoTop()      OK
 * index: DbSeek() finds                 OK
 * index: DbSeek() misses                OK
 * index: OrdKeyCount()                  OK
 * index: changed key is found           OK
 * index: old key is gone                OK
 * index: OrdKeyNo() at the top          OK
 * index: OrdKeyNo() after DbSkip()      OK
 * index: OrdKeyNo() after DbSkip( -1 )  OK
 * index: OrdKeyCount() after append     OK
 * index: OrdKeyNo() after append        OK
 * index without key blocks is an error  OK
 * index reopened with key blocks        OK
 * shared: record lock in area A         OK
 * shared: same record busy in area B    OK
 * shared: B reads while A holds a lock  OK
 * shared: write without a lock is error OK
 * shared: B locks another record        OK
 * shared: A sees the change of B        OK
 * shared: B locks after A releases      OK
 * exclusive open sets NetErr()          OK
 * DbCloseAll() closes every area        OK
 * old byte 14 flag does not block       OK
 * byte 14 flag is cleared               OK
 * DbZap() empties the table             OK
 * bulk: DbCount() all records           OK
 * bulk: DbCount() with FOR              OK
 * bulk: DbCount() with NEXT             OK
 * bulk: DbCount() with RECORD           OK
 * bulk: DbCount() with REST             OK
 * bulk: DbCount() with WHILE            OK
 * bulk: DbSum() of integers             OK
 * bulk: DbSum() with FOR                OK
 * bulk: DbAverage()                     OK
 * bulk: DbAverage() empty scope is 0    OK
 * bulk: DbCopy() records, memo, DELETED OK
 * bulk: DbCopy() fields and FOR         OK
 * bulk: DbCopyStruct()                  OK
 * bulk: DbCopyXStruct()                 OK
 * bulk: DbCreateFrom()                  OK
 * bulk: DbAppendFrom() with FOR         OK
 * bulk: DbSort() ascending              OK
 * bulk: DbSort() /D                     OK
 * bulk: DbSort() /C                     OK
 * bulk: DbSort() two keys               OK
 * bulk: DbSort() with FOR               OK
 * bulk: DbJoin()                        OK
 * bulk: DbSDF() export                  OK
 * bulk: DbSDF() import                  OK
 * bulk: DbDelim() export                OK
 * bulk: DbDelim() import                OK
 * bulk: DbDelim() PIPE export           OK
 * bulk: DbExportCsv() count             OK
 * bulk: DbExportCsv() text              OK
 * bulk: DbImportCsv()                   OK
 * bulk: DbTotal()                       OK
 * bulk: DbUpdate() RANDOM               OK
 * bulk: DbUpdate() without lRandom      OK
 *
 * $DOCEND$
 */

#include "dbinfo.ch"
#include "fileio.ch"
#include "set.ch"

PROCEDURE Main()

   LOCAL cMemo := Replicate( "Długa uwaga do faktury. ", 20 )
   LOCAL lDeleted := Set( _SET_DELETED, .F. )
   LOCAL nHandle
   LOCAL cByte := Space( 1 )

   s_Remove()

   dbCreate( "dbf7test", { ;
      { "NUMER",                        "C", 15, 0 }, ;
      { "NAZWA",                        "C", 40, 0 }, ;
      { "KOD",                          "C",  5, 0 }, ;
      { "DATA",                         "D",  8, 0 }, ;
      { "KWOTA_NETTO_RAZEM_ZA_FAKTURE", "N", 12, 2 }, ;
      { "ZAPLACONO",                    "L",  1, 0 }, ;
      { "TERMIN",                       "N",  3, 0 }, ;
      { "UWAGI",                        "M", 10, 0 } } )

   s_Check( "create: .dbf and .dbt exist           ", FileExists( "dbf7test.dbf" ) .AND. FileExists( "dbf7test.dbt" ), .T. )

   // Exclusive use: types, text, numbers, memo
   dbUseArea( .T., NIL, "dbf7test", "T7", .F. )
   s_Check( "format is xDev 7                      ", dbInfo( DBI_DB_VERSION ), "xDev 7" )
   s_Check( "field count                           ", FCount(), 8 )
   s_Check( "28-character field name               ", FieldName( 5 ), "KWOTA_NETTO_RAZEM_ZA_FAKTURE" )

   s_Add( "FV/1/2026", "Kowalski i Wspólnicy sp. j.", Date() - 1, 1250.50, .T., 14, cMemo )
   s_Add( "FV/2/2026", "Zakład Usług Wiśniewski", CToD( "" ), 8400.00, .F., 30, "" )
   s_Add( "FV/3/2026", "Łódzka Spółdzielnia Pracy", Date() - 3, 99.99, .T., 7, "Krótka uwaga." )
   s_Check( "append three records                  ", LastRec(), 3 )

   dbGoTo( 1 )
   s_Check( "UTF-8 text read back                  ", RTrim( T7->NAZWA ), "Kowalski i Wspólnicy sp. j." )
   T7->KOD := "ąęść"
   s_Check( "C field cut on a UTF-8 boundary       ", RTrim( T7->KOD ), "ąę" )
   s_Check( "invalid UTF-8 is an error             ", ValType( s_Caught( {|| T7->NAZWA := "abc" + Chr( 195 ) } ) ), "O" )
   s_Check( "N with decimals                       ", T7->KWOTA_NETTO_RAZEM_ZA_FAKTURE, 1250.5 )
   s_Check( "N without decimals is an integer      ", T7->TERMIN, 14 )
   s_Check( "N overflow is an error                ", ValType( s_Caught( {|| T7->TERMIN := 12345 } ) ), "O" )
   s_Check( "N overflow keeps the old value        ", T7->TERMIN, 14 )
   s_Check( "D field                               ", T7->DATA, Date() - 1 )
   s_Check( "L field                               ", T7->ZAPLACONO, .T. )
   s_Check( "long memo read back                   ", T7->UWAGI, cMemo )

   dbGoTo( 2 )
   s_Check( "empty D field                         ", Empty( T7->DATA ), .T. )
   s_Check( "empty memo                            ", T7->UWAGI, "" )

   dbGoTo( 3 )
   T7->UWAGI := "Ok."
   s_Check( "memo rewritten shorter                ", T7->UWAGI, "Ok." )
   T7->UWAGI := cMemo + cMemo
   s_Check( "memo rewritten longer                 ", T7->UWAGI, cMemo + cMemo )
   T7->UWAGI := "Krótka uwaga."
   dbCloseArea()

   // Reopen: everything comes from the files
   dbUseArea( .T., NIL, "dbf7test", "T7", .F. )
   dbGoTo( 3 )
   s_Check( "values after reopening                ", RTrim( T7->NAZWA ) + "|" + T7->UWAGI + "|" + DToS( T7->DATA ), ;
            "Łódzka Spółdzielnia Pracy|Krótka uwaga.|" + DToS( Date() - 3 ) )
   s_Check( "DbCount()                             ", DbCount(), 3 )
   s_Check( "DbSum()                               ", Str( DbSum( {|| T7->KWOTA_NETTO_RAZEM_ZA_FAKTURE } ), 12, 2 ), "     9750.49" )

   // DELETED, PACK, COPY
   dbGoTo( 2 )
   dbDelete()
   s_Check( "Deleted()                             ", Deleted(), .T. )
   Set( _SET_DELETED, .T. )
   s_Check( "SET DELETED skips the record          ", DbCount(), 2 )
   Set( _SET_DELETED, .F. )
   dbGoTo( 2 )
   dbRecall()
   s_Check( "DbRecall()                            ", Deleted(), .F. )
   dbDelete()
   dbPack()
   s_Check( "DbPack() removes the record           ", LastRec(), 2 )
   dbGoTo( 1 )
   s_Check( "memo after DbPack()                   ", T7->UWAGI, cMemo )
   DbCopy( "dbf7copy" )
   dbUseArea( .T., NIL, "dbf7copy", "COPY7", .T., .T. )
   s_Check( "DbCopy() copies all records           ", LastRec(), 2 )
   dbCloseArea()
   dbSelectArea( "T7" )

   // Index: create, maintain, seek
   dbCreateIndex( "dbf7test", "NUMER", {|| T7->NUMER } )
   dbGoTop()
   s_Check( "index: first key after DbGoTop()      ", RTrim( T7->NUMER ), "FV/1/2026" )
   s_Check( "index: DbSeek() finds                 ", dbSeek( "FV/3/2026" ), .T. )
   s_Check( "index: DbSeek() misses                ", dbSeek( "FV/2/2026" ), .F. )
   s_Check( "index: OrdKeyCount()                  ", OrdKeyCount(), 2 )
   dbSeek( "FV/3/2026" )
   T7->NUMER := "FV/9/2026"
   dbCommit()
   s_Check( "index: changed key is found           ", dbSeek( "FV/9/2026" ), .T. )
   s_Check( "index: old key is gone                ", dbSeek( "FV/3/2026" ), .F. )
   dbGoTop()
   s_Check( "index: OrdKeyNo() at the top          ", OrdKeyNo(), 1 )
   dbSkip()
   s_Check( "index: OrdKeyNo() after DbSkip()      ", OrdKeyNo(), 2 )
   dbSkip( -1 )
   s_Check( "index: OrdKeyNo() after DbSkip( -1 )  ", OrdKeyNo(), 1 )
   dbAppend()
   T7->NUMER := "FV/5/2026"
   dbCommit()
   s_Check( "index: OrdKeyCount() after append     ", OrdKeyCount(), 3 )
   dbSeek( "FV/9/2026" )
   s_Check( "index: OrdKeyNo() after append        ", OrdKeyNo(), 3 )
   dbCloseArea()

   // An existing index needs the compiled key blocks
   dbUseArea( .T., NIL, "dbf7test", "T7", .F. )
   s_Check( "index without key blocks is an error  ", ValType( s_Caught( {|| dbSetIndex( "dbf7test" ) } ) ), "O" )
   dbSetIndex( "dbf7test", { { NIL, "NUMER", {|| T7->NUMER }, NIL, NIL } } )
   s_Check( "index reopened with key blocks        ", dbSeek( "FV/9/2026" ) .AND. RTrim( T7->NAZWA ) == "Łódzka Spółdzielnia Pracy", .T. )
   dbCloseArea()

   // Shared use: the same table in two areas, as on two workstations
   dbUseArea( .T., NIL, "dbf7test", "T7A", .T. )
   dbUseArea( .T., NIL, "dbf7test", "T7B", .T. )
   T7A->( dbGoTo( 1 ) )
   s_Check( "shared: record lock in area A         ", T7A->( RLock() ), .T. )
   T7B->( dbGoTo( 1 ) )
   s_Check( "shared: same record busy in area B    ", T7B->( RLock() ), .F. )
   s_Check( "shared: B reads while A holds a lock  ", ValType( s_Caught( {|| T7B->( dbGoTop() ), T7B->( dbSkip() ), T7B->NAZWA } ) ), "U" )
   T7B->( dbGoTo( 2 ) )
   s_Check( "shared: write without a lock is error ", ValType( s_Caught( {|| T7B->KOD := "B" } ) ), "O" )
   s_Check( "shared: B locks another record        ", T7B->( RLock() ), .T. )
   T7B->KOD := "B2"
   T7B->( dbCommit() )
   T7B->( dbUnlock() )
   T7A->( dbGoTo( 2 ) )
   s_Check( "shared: A sees the change of B        ", RTrim( T7A->KOD ), "B2" )
   T7A->( dbUnlock() )
   T7B->( dbGoTo( 1 ) )
   s_Check( "shared: B locks after A releases      ", T7B->( RLock() ), .T. )
   T7B->( dbUnlock() )
   dbUseArea( .T., NIL, "dbf7test", "T7C", .F. )
   s_Check( "exclusive open sets NetErr()          ", NetErr(), .T. )
   dbCloseAll()
   s_Check( "DbCloseAll() closes every area        ", Select( "T7A" ) + Select( "T7B" ), 0 )

   // A transaction flag in byte 14, left by an older version, must not block the table
   nHandle := FOpen( "dbf7test.dbf", FO_READWRITE )
   FSeek( nHandle, 14, FS_SET )
   FWrite( nHandle, Chr( 1 ) )
   FClose( nHandle )
   s_Check( "old byte 14 flag does not block       ", ValType( s_Caught( {|| dbUseArea( .T., NIL, "dbf7test", "T7", .F. ) } ) ), "U" )
   dbAppend()
   T7->NUMER := "FV/10/2026"
   dbCloseArea()
   nHandle := FOpen( "dbf7test.dbf", FO_READ )
   FSeek( nHandle, 14, FS_SET )
   FRead( nHandle, @cByte, 1 )
   FClose( nHandle )
   s_Check( "byte 14 flag is cleared               ", Asc( cByte ), 0 )

   dbUseArea( .T., NIL, "dbf7test", "T7", .F. )
   dbZap()
   s_Check( "DbZap() empties the table             ", LastRec() == 0 .AND. Eof(), .T. )
   dbCloseArea()

   s_Bulk()

   Set( _SET_DELETED, lDeleted )
   s_Remove()

RETURN

// Bulk operations: COUNT, SUM, AVERAGE, COPY, APPEND FROM, SORT, JOIN, TOTAL, UPDATE, SDF, DELIMITED, CSV
STATIC PROCEDURE s_Bulk()

   LOCAL cCrLf := Chr( 13 ) + Chr( 10 )
   LOCAL bText := {|| RTrim( R7->NAZWA ) + ":" + AllTrim( Str( R7->ILOSC ) ) + ":" + DToS( R7->DATA ) + ":" + IIF( R7->OK, "T", "F" ) }
   LOCAL xValue
   LOCAL oError

   s_BulkRemove()
   dbCreate( "dbf7bulk", { { "NAZWA", "C", 10, 0 }, { "GRUPA", "C", 3, 0 }, { "ILOSC", "N", 5, 0 }, ;
      { "KWOTA", "N", 8, 2 }, { "DATA", "D", 8, 0 }, { "OK", "L", 1, 0 }, { "OPIS", "M", 10, 0 } } )
   dbCreate( "dbf7join", { { "GRUPA", "C", 3, 0 }, { "OPIS2", "C", 10, 0 } } )
   dbCreate( "dbf7upd", { { "NAZWA", "C", 10, 0 }, { "ILOSC", "N", 5, 0 } } )

   dbUseArea( .T., NIL, "dbf7join", "J7", .F. )
   s_Fill( { { "A", "grupa A" }, { "B", "grupa B" } } )
   dbUseArea( .T., NIL, "dbf7upd", "U7", .F. )
   s_Fill( { { "Brak", 1 }, { "Gamma", 100 }, { "alfa", 50 } } )
   dbUseArea( .T., NIL, "dbf7bulk", "B7", .F. )
   s_Fill( { { "Beta",  "A", 2, 10.50, SToD( "20260102" ), .T., "memo B" }, ;
             { "alfa",  "B", 5,  4.00, SToD( "20260101" ), .F., "" }, ;
             { "Gamma", "A", 1,  1.25, SToD( "20260103" ), .T., "memo G" }, ;
             { "Łoś",   "B", 3,  2.00, SToD( "20260104" ), .F., "" } } )
   dbGoTo( 4 )
   dbDelete()

   // COUNT, SUM, AVERAGE and the scope arguments
   s_Check( PadR( "bulk: DbCount() all records", 38 ), DbCount(), 4 )
   s_Check( PadR( "bulk: DbCount() with FOR", 38 ), DbCount( {|| AllTrim( B7->GRUPA ) == "A" } ), 2 )
   dbGoTo( 2 )
   s_Check( PadR( "bulk: DbCount() with NEXT", 38 ), DbCount( NIL, NIL, 2 ), 2 )
   s_Check( PadR( "bulk: DbCount() with RECORD", 38 ), DbCount( NIL, NIL, NIL, 3 ), 1 )
   dbGoTo( 3 )
   s_Check( PadR( "bulk: DbCount() with REST", 38 ), DbCount( NIL, NIL, NIL, NIL, .T. ), 2 )
   dbGoTop()
   s_Check( PadR( "bulk: DbCount() with WHILE", 38 ), DbCount( NIL, {|| B7->ILOSC < 5 } ), 1 )
   s_Check( PadR( "bulk: DbSum() of integers", 38 ), DbSum( {|| B7->ILOSC } ), 11 )
   s_Check( PadR( "bulk: DbSum() with FOR", 38 ), Str( DbSum( {|| B7->KWOTA }, {|| AllTrim( B7->GRUPA ) == "B" } ), 8, 2 ), "    6.00" )
   s_Check( PadR( "bulk: DbAverage()", 38 ), Str( DbAverage( {|| B7->ILOSC } ), 6, 2 ), "  2.75" )
   xValue := NIL
   s_Caught( {|| xValue := DbAverage( {|| B7->ILOSC }, {|| .F. } ) } )
   s_Check( PadR( "bulk: DbAverage() empty scope is 0", 38 ), xValue, 0 )

   // COPY, COPY STRUCTURE, COPY STRUCTURE EXTENDED, CREATE FROM
   DbCopy( "dbf7b2" )
   s_Check( PadR( "bulk: DbCopy() records, memo, DELETED", 38 ), s_Rows( "dbf7b2", {|| RTrim( R7->NAZWA ) + ":" + R7->OPIS } ), ;
            "Beta:memo B,alfa:,Gamma:memo G,Łoś:*," )
   DbCopy( "dbf7b2a", { "NAZWA", "ILOSC" }, {|| B7->OK } )
   s_Check( PadR( "bulk: DbCopy() fields and FOR", 38 ), s_Table( "dbf7b2a", {|| FCount() } ) == 2 .AND. ;
            s_Rows( "dbf7b2a", {|| RTrim( R7->NAZWA ) + ":" + AllTrim( Str( R7->ILOSC ) ) } ) == "Beta:2,Gamma:1,", .T. )
   DbCopyStruct( "dbf7b3", { "NAZWA", "KWOTA" } )
   s_Check( PadR( "bulk: DbCopyStruct()", 38 ), s_Table( "dbf7b3", {|| Str( FCount(), 1 ) + Str( LastRec(), 1 ) + FieldName( 2 ) } ), "20KWOTA" )
   DbCopyXStruct( "dbf7b4" )
   s_Check( PadR( "bulk: DbCopyXStruct()", 38 ), s_Rows( "dbf7b4", {|| RTrim( R7->FIELD_NAME ) + R7->FIELD_TYPE + AllTrim( Str( R7->FIELD_LEN ) ) } ), ;
            "NAZWAC10,GRUPAC3,ILOSCN5,KWOTAN8,DATAD8,OKL1,OPISM10," )
   dbSelectArea( 0 )
   DbCreateFrom( "dbf7b5", "dbf7b4" )
   s_Check( PadR( "bulk: DbCreateFrom()", 38 ), FCount() == 7 .AND. FieldName( 7 ) == "OPIS" .AND. dbStruct()[ 4 ][ 4 ] == 2, .T. )
   dbCloseArea()
   dbSelectArea( "B7" )

   // APPEND FROM into a table with two fields
   DbCopyStruct( "dbf7b6", { "NAZWA", "ILOSC" } )
   dbUseArea( .T., NIL, "dbf7b6", "A7", .F. )
   DbAppendFrom( "dbf7b2", { "NAZWA", "ILOSC" }, {|| FieldGet( 3 ) > 1 } )
   dbCloseArea()
   dbSelectArea( "B7" )
   s_Check( PadR( "bulk: DbAppendFrom() with FOR", 38 ), s_Rows( "dbf7b6", {|| RTrim( R7->NAZWA ) + ":" + AllTrim( Str( R7->ILOSC ) ) } ), ;
            "Beta:2,alfa:5,Łoś:3*," )

   // SORT: bytes of UTF-8, /D descending, /C case insensitive, several keys, FOR
   DbSort( "dbf7b7", { "NAZWA" } )
   s_Check( PadR( "bulk: DbSort() ascending", 38 ), s_Rows( "dbf7b7", {|| RTrim( R7->NAZWA ) } ), "Beta,Gamma,alfa,Łoś*," )
   DbSort( "dbf7b7", { "NAZWA/D" } )
   s_Check( PadR( "bulk: DbSort() /D", 38 ), s_Rows( "dbf7b7", {|| RTrim( R7->NAZWA ) } ), "Łoś*,alfa,Gamma,Beta," )
   DbSort( "dbf7b7", { "NAZWA/C" } )
   s_Check( PadR( "bulk: DbSort() /C", 38 ), s_Rows( "dbf7b7", {|| RTrim( R7->NAZWA ) } ), "alfa,Beta,Gamma,Łoś*," )
   DbSort( "dbf7b7", { "GRUPA/D", "ILOSC/D" } )
   s_Check( PadR( "bulk: DbSort() two keys", 38 ), s_Rows( "dbf7b7", {|| RTrim( R7->NAZWA ) } ), "alfa,Łoś*,Beta,Gamma," )
   DbSort( "dbf7b7", { "NAZWA" }, {|| B7->OK } )
   s_Check( PadR( "bulk: DbSort() with FOR", 38 ), s_Rows( "dbf7b7", {|| RTrim( R7->NAZWA ) } ), "Beta,Gamma," )

   // JOIN with the group table
   DbJoin( "J7", "dbf7b9", { "NAZWA", "J7->OPIS2" }, {|| B7->GRUPA == J7->GRUPA } )
   s_Check( PadR( "bulk: DbJoin()", 38 ), s_Rows( "dbf7b9", {|| RTrim( R7->NAZWA ) + ":" + RTrim( R7->OPIS2 ) } ), ;
            "Beta:grupa A,alfa:grupa B,Gamma:grupa A,Łoś:grupa B," )

   // SDF, DELIMITED and CSV: export, file text, import
   DbSDF( .T., "dbf7sdf", { "NAZWA", "ILOSC", "DATA", "OK" } )
   s_Check( PadR( "bulk: DbSDF() export", 38 ), Left( MemoRead( "dbf7sdf.txt" ), 26 ), "Beta" + Space( 10 ) + "220260102T" + cCrLf )
   DbCopyStruct( "dbf7b10", { "NAZWA", "ILOSC", "DATA", "OK" } )
   s_Import( "dbf7b10", {|| DbSDF( .F., "dbf7sdf", { "NAZWA", "ILOSC", "DATA", "OK" } ) } )
   s_Check( PadR( "bulk: DbSDF() import", 38 ), s_Rows( "dbf7b10", bText ), ;
            "Beta:2:20260102:T,alfa:5:20260101:F,Gamma:1:20260103:T,Łoś:3:20260104:F," )
   DbDelim( .T., "dbf7delim", NIL, { "NAZWA", "ILOSC", "DATA", "OK" } )
   s_Check( PadR( "bulk: DbDelim() export", 38 ), Left( MemoRead( "dbf7delim.txt" ), 21 ), '"Beta",2,20260102,T' + cCrLf )
   DbCopyStruct( "dbf7b11", { "NAZWA", "ILOSC", "DATA", "OK" } )
   s_Import( "dbf7b11", {|| DbDelim( .F., "dbf7delim", NIL, { "NAZWA", "ILOSC", "DATA", "OK" } ) } )
   s_Check( PadR( "bulk: DbDelim() import", 38 ), s_Rows( "dbf7b11", bText ), ;
            "Beta:2:20260102:T,alfa:5:20260101:F,Gamma:1:20260103:T,Łoś:3:20260104:F," )
   DbDelim( .T., "dbf7pipe", "PIPE", { "NAZWA", "ILOSC", "DATA", "OK" } )
   s_Check( PadR( "bulk: DbDelim() PIPE export", 38 ), Left( MemoRead( "dbf7pipe.txt" ), 19 ), "Beta|2|20260102|T" + cCrLf )
   s_Check( PadR( "bulk: DbExportCsv() count", 38 ), DbExportCsv( "dbf7csv", { "NAZWA", "KWOTA", "DATA", "OK" } ), 4 )
   xValue := Chr( 239 ) + Chr( 187 ) + Chr( 191 ) + "NAZWA;KWOTA;DATA;OK" + cCrLf + '"Beta";10,50;2026-01-02;T' + cCrLf
   s_Check( PadR( "bulk: DbExportCsv() text", 38 ), Left( MemoRead( "dbf7csv.csv" ), Len( xValue ) ), xValue )
   DbCopyStruct( "dbf7b12", { "NAZWA", "KWOTA", "DATA", "OK" } )
   s_Import( "dbf7b12", {|| xValue := DbImportCsv( "dbf7csv" ) } )
   s_Check( PadR( "bulk: DbImportCsv()", 38 ), s_Rows( "dbf7b12", {|| RTrim( R7->NAZWA ) + ":" + AllTrim( Str( R7->KWOTA, 8, 2 ) ) } ) == ;
            "Beta:10.50,alfa:4.00,Gamma:1.25,Łoś:2.00," .AND. xValue == 4, .T. )

   // TOTAL: groups come from an index on GRUPA
   dbCreateIndex( "dbf7bulk", "GRUPA", {|| B7->GRUPA } )
   DbTotal( "dbf7b8", {|| B7->GRUPA }, { "ILOSC", "KWOTA" } )
   s_Check( PadR( "bulk: DbTotal()", 38 ), s_Rows( "dbf7b8", {|| RTrim( R7->NAZWA ) + ":" + AllTrim( Str( R7->ILOSC ) ) + ":" + AllTrim( Str( R7->KWOTA, 8, 2 ) ) } ), ;
            "Beta:3:11.75,alfa:8:6.00," )

   // UPDATE from dbf7upd: RANDOM with an index, then the sorted merge without lRandom
   dbCreateIndex( "dbf7bulk", "NAZWA", {|| B7->NAZWA } )
   DbUpdate( "U7", {|| FieldGet( 1 ) }, .T., {|| B7->ILOSC := B7->ILOSC + U7->ILOSC } )
   s_Check( PadR( "bulk: DbUpdate() RANDOM", 38 ), s_Amounts(), "101/55" )
   oError := s_Caught( {|| DbUpdate( "U7", {|| FieldGet( 1 ) }, NIL, {|| B7->ILOSC := B7->ILOSC + U7->ILOSC } ) } )
   s_Check( PadR( "bulk: DbUpdate() without lRandom", 38 ), IIF( ValType( oError ) == "U", s_Amounts(), "" ), "201/105" )
   dbClearIndex()

   dbCloseAll()
   s_BulkRemove()

RETURN

// Append rows of field values to the current work area
STATIC PROCEDURE s_Fill( aRows )

   LOCAL aRow
   LOCAL i

   FOR EACH aRow IN aRows
      dbAppend()
      FOR i := 1 TO Len( aRow )
         FieldPut( i, aRow[ i ] )
      NEXT
   NEXT

RETURN

// ILOSC of records 3 and 2 in B7 as "n3/n2"
STATIC FUNCTION s_Amounts()

   LOCAL cText

   dbGoTo( 3 )
   cText := AllTrim( Str( B7->ILOSC ) )
   dbGoTo( 2 )

RETURN cText + "/" + AllTrim( Str( B7->ILOSC ) )

// Open the empty table in a new area I7, run the import block in it and close it
STATIC PROCEDURE s_Import( cFile, bImport )

   dbUseArea( .T., NIL, cFile, "I7", .F. )
   Eval( bImport )
   dbCloseArea()
   dbSelectArea( "B7" )

RETURN

// Open a table read-only in a new area R7, evaluate the block and close the table
STATIC FUNCTION s_Table( cFile, bAction )

   LOCAL nOld := Select()
   LOCAL xResult

   dbUseArea( .T., NIL, cFile, "R7", .T., .T. )
   xResult := Eval( bAction )
   dbCloseArea()
   dbSelectArea( nOld )

RETURN xResult

// All records of a table as text: the row block, "*" for a deleted record and a comma
STATIC FUNCTION s_Rows( cFile, bRow )

RETURN s_Table( cFile, {|| s_Join( bRow ) } )

STATIC FUNCTION s_Join( bRow )

   LOCAL cRows := ""

   dbEval( {|| cRows := cRows + Eval( bRow ) + IIF( Deleted(), "*", "" ) + "," } )

RETURN cRows

// Remove the files of the bulk tests
STATIC PROCEDURE s_BulkRemove()

   LOCAL cName

   FOR EACH cName IN { "dbf7bulk", "dbf7join", "dbf7upd", "dbf7b2", "dbf7b2a", "dbf7b3", "dbf7b4", "dbf7b5", ;
                       "dbf7b6", "dbf7b7", "dbf7b8", "dbf7b9", "dbf7b10", "dbf7b11", "dbf7b12" }
      FErase( cName + ".dbf" )
      FErase( cName + ".dbt" )
      FErase( cName + ".ntx" )
   NEXT
   FErase( "dbf7sdf.txt" )
   FErase( "dbf7delim.txt" )
   FErase( "dbf7pipe.txt" )
   FErase( "dbf7csv.csv" )

RETURN

// Append one record to the current work area
STATIC PROCEDURE s_Add( cNumber, cName, dDate, nAmount, lPaid, nDays, cNotes )

   dbAppend()
   T7->NUMER                        := cNumber
   T7->NAZWA                        := cName
   T7->DATA                         := dDate
   T7->KWOTA_NETTO_RAZEM_ZA_FAKTURE := nAmount
   T7->ZAPLACONO                    := lPaid
   T7->TERMIN                       := nDays
   T7->UWAGI                        := cNotes

RETURN

// Remove the files of the test
STATIC PROCEDURE s_Remove()

   FErase( "dbf7test.dbf" )
   FErase( "dbf7test.dbt" )
   FErase( "dbf7test.ntx" )
   FErase( "dbf7copy.dbf" )
   FErase( "dbf7copy.dbt" )

RETURN

// Run the block and return the Error object of its runtime error, or NIL
STATIC FUNCTION s_Caught( bAction )

   LOCAL oError
   LOCAL bError := ErrorBlock( {| oCaught | Break( oCaught ) } )

   BEGIN SEQUENCE
      Eval( bAction )
   RECOVER USING oError
   END SEQUENCE
   ErrorBlock( bError )

RETURN oError

STATIC PROCEDURE s_Check( cTitle, xResult, xExpected )

   QOut( cTitle + IIF( ValType( xResult ) == ValType( xExpected ) .AND. xResult == xExpected, "OK", "ERROR" ) )

RETURN
