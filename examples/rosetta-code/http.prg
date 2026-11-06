/*
 * HTTP
 */

#include "curl.ch"
PROCEDURE Main()

   LOCAL pCurl := curl_easy_init()
   LOCAL nCode

   curl_easy_setopt( pCurl, CURLOPT_URL, "https://httpbin.org/html" )
   curl_easy_setopt( pCurl, CURLOPT_FOLLOWLOCATION, .T. )
   nCode := curl_easy_perform( pCurl )

   IF nCode == 0
      OutStd( curl_easy_buffer( pCurl ) )
   ELSE
      OutErr( "Error: " + curl_easy_strerror( nCode ) )
   ENDIF
   curl_easy_cleanup( pCurl )

   RETURN
