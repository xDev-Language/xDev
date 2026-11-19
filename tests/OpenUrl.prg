/*
 * $DOC$
 * $NAME$
 * Open URL test
 *
 * $ONELINER$
 * Open a UTF-8 Wikipedia link through the active GT driver.
 *
 * $DESCRIPTION$
 * The test validates the URL with several string functions and then asks the
 * active GT driver to open it with the system browser. The final line is OK
 * only when GtInfo( GTI_OPENURL ) reports that the link was opened.
 *
 * $EXAMPLES$
 *
 * $OUTPUT$
 * URL is a character string          OK
 * URL is valid UTF-8                 OK
 * URL has the HTTPS prefix           OK
 * URL contains the Wikipedia domain OK
 * URL length is positive             OK
 * Wikipedia link opened              OK
 *
 * $DOCEND$
 */

#include "gtinfo.ch"

#if OS_LINUX
   REQUEST GTXWC
#else
   REQUEST GTWVT
#endif

PROCEDURE Main()

   LOCAL cUrl := "https://pl.wikipedia.org/wiki/Znaki_diakrytyczne"
   LOCAL lOpened

   Check( 'URL is a character string          ', ValType( cUrl ), "C" )
   Check( 'URL is valid UTF-8                 ', StrIsUTF8( cUrl ), .T. )
   Check( 'URL has the HTTPS prefix           ', Left( cUrl, 8 ), "https://" )
   Check( 'URL contains the Wikipedia domain ', At( "wikipedia.org", cUrl ) > 0, .T. )
   Check( 'URL length is positive             ', Len( cUrl ) > 0, .T. )

   lOpened := GtInfo( GTI_OPENURL, cUrl )
   Check( 'Wikipedia link opened              ', lOpened, .T. )

RETURN

STATIC PROCEDURE Check( cTitle, xResult, xExpected )

   QOut( cTitle + IIF( ValType( xResult ) == ValType( xExpected ) .AND. xResult == xExpected, "OK", "ERROR" ) )

RETURN
