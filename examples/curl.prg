/*
 *
 */

#include "curl.ch"
#include "std.ch"

STATIC s_cReport   := ""
STATIC s_nProgress := 0
STATIC s_nReceived := 0

PROCEDURE Main()

   LOCAL pCurl := curl_easy_init()
   LOCAL pCopy
   LOCAL pMulti
   LOCAL pUrl
   LOCAL aHeaders
   LOCAL aMessage
   LOCAL nCode
   LOCAL nRunning
   LOCAL nIndex
   LOCAL nFile

   s_Line( "Version: " + curl_version() )
   s_Line( "SSL: " + curl_version_info()[ 5 ] )

   s_Line( "" )
   s_Line( "GET https://example.com/" )
   curl_easy_setopt( pCurl, CURLOPT_URL, "https://example.com/" )
   curl_easy_setopt( pCurl, CURLOPT_FOLLOWLOCATION, .T. )
   nCode := curl_easy_perform( pCurl )
   s_Line( "Code: " + LTrim( Str( nCode ) ) + " " + curl_easy_strerror( nCode ) )
   s_Line( "Status: " + LTrim( Str( curl_easy_getinfo( pCurl, CURLINFO_RESPONSE_CODE ) ) ) )
   s_Line( "Content-Type: " + s_Text( curl_easy_header( pCurl, "content-type" ) ) )
   s_Line( "Effective URL: " + s_Text( curl_easy_getinfo( pCurl, CURLINFO_EFFECTIVE_URL ) ) )
   s_Line( "Body length: " + LTrim( Str( Len( curl_easy_buffer( pCurl ) ) ) ) )
   aHeaders := curl_easy_headers( pCurl )
   FOR nIndex := 1 TO Len( aHeaders )
      s_Line( "  " + aHeaders[ nIndex ][ 1 ] + ": " + aHeaders[ nIndex ][ 2 ] )
   NEXT

   s_Line( "" )
   s_Line( "POST https://httpbin.org/post" )
   curl_easy_setopt( pCurl, CURLOPT_URL, "https://httpbin.org/post" )
   curl_easy_setopt( pCurl, CURLOPT_HTTPHEADER, { "Content-Type: application/json", "Accept: application/json" } )
   curl_easy_setopt( pCurl, CURLOPT_POSTFIELDS, '{"name":"xDev","city":"Kraków"}' )
   s_Line( "Code: " + LTrim( Str( curl_easy_perform( pCurl ) ) ) )
   s_Line( "Status: " + LTrim( Str( curl_easy_getinfo( pCurl, CURLINFO_RESPONSE_CODE ) ) ) )
   s_Line( curl_easy_buffer( pCurl ) )

   s_Line( "" )
   s_Line( "Download to example.html with progress" )
   curl_easy_reset( pCurl )
   curl_easy_setopt( pCurl, CURLOPT_URL, "https://example.com/" )
   curl_easy_download( pCurl, "example.html" )
   curl_easy_progress( pCurl, {| nDlTotal, nDlNow | s_Progress( nDlNow ) } )
   s_Line( "Code: " + LTrim( Str( curl_easy_perform( pCurl ) ) ) )
   s_Line( "Progress calls: " + LTrim( Str( s_nProgress ) ) + ", received " + LTrim( Str( s_nReceived ) ) + " bytes" )
   curl_easy_download( pCurl )
   curl_easy_progress( pCurl )

   s_Line( "" )
   s_Line( "Escape: " + curl_easy_escape( pCurl, "zażółć gęślą jaźń & 100%" ) )
   s_Line( "Unescape: " + curl_easy_unescape( pCurl, "za%C5%BC%C3%B3%C5%82%C4%87" ) )
   s_Line( "Date: " + LTrim( Str( curl_getdate( "Sun, 25 Oct 2026 08:00:00 GMT" ) ) ) )

   s_Line( "" )
   s_Line( "URL parts:" )
   pUrl := curl_url()
   curl_url_set( pUrl, CURLUPART_URL, "https://user@example.com:8080/path/file.xml?a=1#top" )
   s_Line( "  host: " + s_Text( curl_url_get( pUrl, CURLUPART_HOST ) ) )
   s_Line( "  port: " + s_Text( curl_url_get( pUrl, CURLUPART_PORT ) ) )
   s_Line( "  path: " + s_Text( curl_url_get( pUrl, CURLUPART_PATH ) ) )
   s_Line( "  query: " + s_Text( curl_url_get( pUrl, CURLUPART_QUERY ) ) )
   curl_url_set( pUrl, CURLUPART_QUERY, "b=2", CURLU_APPENDQUERY )
   s_Line( "  url: " + s_Text( curl_url_get( pUrl, CURLUPART_URL ) ) )
   curl_url_cleanup( pUrl )

   s_Line( "" )
   s_Line( "Two transfers at the same time:" )
   curl_easy_reset( pCurl )
   curl_easy_setopt( pCurl, CURLOPT_URL, "https://example.com/" )
   pCopy := curl_easy_duphandle( pCurl )
   curl_easy_setopt( pCopy, CURLOPT_URL, "https://httpbin.org/get" )
   pMulti := curl_multi_init()
   curl_multi_add_handle( pMulti, pCurl )
   curl_multi_add_handle( pMulti, pCopy )
   nRunning := 1
   DO WHILE nRunning > 0
      curl_multi_perform( pMulti, @nRunning )
      IF nRunning > 0
         curl_multi_poll( pMulti, 1000 )
      ENDIF
   ENDDO
   DO WHILE ( aMessage := curl_multi_info_read( pMulti ) ) != NIL
      s_Line( "  " + s_Text( curl_easy_getinfo( aMessage[ 2 ], CURLINFO_EFFECTIVE_URL ) ) + " -> " + ;
              LTrim( Str( aMessage[ 3 ] ) ) + ", " + LTrim( Str( Len( curl_easy_buffer( aMessage[ 2 ] ) ) ) ) + " bytes" )
   ENDDO
   curl_multi_cleanup( pMulti )
   curl_easy_cleanup( pCopy )

   s_Line( "" )
   s_Line( "Unknown host:" )
   curl_easy_reset( pCurl )
   curl_easy_setopt( pCurl, CURLOPT_URL, "https://no-such-host.invalid/" )
   curl_easy_setopt( pCurl, CURLOPT_TIMEOUT, 10 )
   nCode := curl_easy_perform( pCurl )
   s_Line( "Code: " + LTrim( Str( nCode ) ) + " " + curl_easy_strerror( nCode ) )
   s_Line( "Unknown option: " + LTrim( Str( curl_easy_setopt( pCurl, 99999, 1 ) ) ) )

   curl_easy_cleanup( pCurl )

   nFile := FCreate( "curl.txt" )
   FWrite( nFile, s_cReport )
   FClose( nFile )

   ? "Results saved to curl.txt"

   RETURN

STATIC PROCEDURE s_Line( cText )

   s_cReport += cText + Eol()

   RETURN

STATIC FUNCTION s_Text( cValue )

   RETURN IIF( cValue == NIL, "NIL", cValue )

STATIC FUNCTION s_Progress( nDlNow )

   s_nProgress++
   s_nReceived := nDlNow

   RETURN .F.
