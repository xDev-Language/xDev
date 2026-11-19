/*
 * $DOC$
 * $NAME$
 * Open URL test
 *
 * $ONELINER$
 * Open a UTF-8 Wikipedia link through the operating system.
 *
 * $DESCRIPTION$
 * The test validates the URL with several string functions and then asks the
 * operating system to open it with the default browser. The checks are printed
 * to the ordinary console.
 *
 * $EXAMPLES$
 *
 * $OUTPUT$
 * URL is a character string          OK
 * URL is valid UTF-8                 OK
 * URL has the HTTPS prefix           OK
 * URL contains the Wikipedia domain OK
 * URL length is positive             OK
 * Wikipedia link launch requested    OK
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

   LOCAL lStdoutCon := GtInfo( GTI_STDOUTCON, .F. )

   Check( 'URL is a character string          ', ValType( cUrl ), "C" )
   Check( 'URL is valid UTF-8                 ', StrIsUTF8( cUrl ), .T. )
   Check( 'URL has the HTTPS prefix           ', Left( cUrl, 8 ), "https://" )
   Check( 'URL contains the Wikipedia domain ', At( "wikipedia.org", cUrl ) > 0, .T. )
   Check( 'URL length is positive             ', Len( cUrl ) > 0, .T. )

   Check( 'Wikipedia link launch requested    ', GtInfo( GTI_OPENURL, cUrl ), .T. )

   GtInfo( GTI_STDOUTCON, lStdoutCon )

RETURN

STATIC PROCEDURE Check( cTitle, xResult, xExpected )

   QOut( cTitle + IIF( ValType( xResult ) == ValType( xExpected ) .AND. xResult == xExpected, "OK", "ERROR" ) )

RETURN
