/*
 * $DOC$
 * $NAME$
 * DBF to CSV converter
 *
 * $ONELINER$
 * Converts a DBF table and its memo data to a spreadsheet-ready CSV file.
 *
 * $DESCRIPTION$
 * Reads xBase 7 and classic DBF files with DBT, FPT, or SMT memo fields. Writes UTF-8 CSV using semicolons, skips deleted records, and returns 0 on success or 1 on error. The output path is optional.
 *
 * $EXAMPLES$
 * examples/dbf7tocsv.prg
 *
 * $OUTPUT$
 * Zapisano 3 rekordy do faktury.csv
 *
 * $DOCEND$
 */

#include "set.ch"

FUNCTION Main( cTable, cCsv )

   LOCAL bError := ErrorBlock( {| oError | Break( oError ) } )
   LOCAL oError
   LOCAL nCount
   LOCAL nResult := 0

   IF Empty( cTable )
      OutStd( "Użycie: dbf7tocsv plik.dbf [plik.csv]" + Chr( 10 ) )
      RETURN 1
   ENDIF
   IF ! s_HasExtension( cTable )
      cTable := cTable + ".dbf"
   ENDIF
   IF Empty( cCsv )
      cCsv := s_StripExtension( cTable ) + ".csv"
   ENDIF
   IF ! File( cTable )
      OutStd( "Nie ma pliku: " + cTable + Chr( 10 ) )
      RETURN 1
   ENDIF

   BEGIN SEQUENCE
      Set( _SET_DELETED, .T. )
      // The DBF driver detects the memo type from the header; read-only, CP852 for unmarked files
      dbUseArea( .T., "DBF", cTable, NIL, .T., .T., "CP852" )
      nCount := DbExportCsv( cCsv )
      dbCloseArea()
      OutStd( "Zapisano " + LTrim( Str( nCount ) ) + " " + s_Records( nCount ) + " do " + cCsv + Chr( 10 ) )
   RECOVER USING oError
      ErrorBlock( bError )
      OutStd( "Błąd: " + s_ErrorText( oError ) + Chr( 10 ) )
      nResult := 1
   END SEQUENCE

   RETURN nResult

STATIC FUNCTION s_ErrorText( oError )

   LOCAL cText

   IF ValType( oError ) != "O"
      RETURN "nieznany"
   ENDIF
   cText := oError:description
   IF ! Empty( oError:operation )
      cText := cText + ": " + oError:operation
   ENDIF

   RETURN cText

STATIC FUNCTION s_NameStart( cFile )
   RETURN Max( Max( RAt( "/", cFile ), RAt( "\", cFile ) ), RAt( ":", cFile ) ) + 1

STATIC FUNCTION s_HasExtension( cFile )
   RETURN "." $ SubStr( cFile, s_NameStart( cFile ) )

STATIC FUNCTION s_StripExtension( cFile )

   LOCAL nDot := RAt( ".", cFile )

   IF nDot >= s_NameStart( cFile )
      RETURN Left( cFile, nDot - 1 )
   ENDIF

   RETURN cFile

STATIC FUNCTION s_Records( nCount )

   IF nCount == 1
      RETURN "rekord"
   ELSEIF nCount % 10 >= 2 .AND. nCount % 10 <= 4 .AND. ( nCount % 100 < 12 .OR. nCount % 100 > 14 )
      RETURN "rekordy"
   ENDIF

   RETURN "rekordów"
