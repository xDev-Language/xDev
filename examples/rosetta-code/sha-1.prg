/*
 * SHA-1
 */

PROCEDURE Main()

   QOut( Lower( StrToHex( HashData( "SHA1", "Rosetta Code" ) ) ) )

   RETURN
