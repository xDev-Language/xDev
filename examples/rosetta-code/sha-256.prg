/*
 * SHA-256
 */

PROCEDURE Main()

   // HashData() comes from the xopenssl library shipped with xDev
   QOut( Lower( StrToHex( HashData( "SHA256", "Rosetta code" ) ) ) )

   RETURN
