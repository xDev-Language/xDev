/*
 * $DOC$
 * $NAME$
 * OpenSSL cryptography
 *
 * $ONELINER$
 * Demonstrates hashes, encodings, encryption, key generation, and signatures.
 *
 * $DESCRIPTION$
 * Uses the xDev OpenSSL library for SHA and HMAC digests, Base64 and hexadecimal conversion, random bytes, AES, RSA, and digital signatures. Also demonstrates error reporting for invalid inputs.
 *
 * $EXAMPLES$
 * examples/openssl.prg
 *
 * $OUTPUT$
 * Version: OpenSSL 3.6.0 1 Oct 2025
 *
 * SHA256('abc'): ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad
 *   expected:    ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad
 * HMAC-SHA256:   f7bc83f430538424b13298e6aa6fb143ef4d59a14946175997479dbc2d1a3cd8
 *   expected:    f7bc83f430538424b13298e6aa6fb143ef4d59a14946175997479dbc2d1a3cd8
 * Base64: S3Jha8Ozdw== -> Kraków
 *   expected: S3Jha8Ozdw== -> Kraków
 * Hex: 41:5A -> AZ
 * SHA512 length: 64
 * Unknown digest: NIL / unknown digest algorithm
 *
 * Random 32 bytes: 32, different: yes
 *
 * AES-256-CBC: 48 bytes, back: OK
 * AES-256-GCM: 60 bytes, back: OK
 * GCM damaged: NIL / GCM tag does not match: wrong key or damaged data
 * Bad key: NIL / key must have 16, 24 or 32 bytes and the mode must be AES_MODE_*
 *
 * RSA keys: -----BEGIN PRIVATE KEY----- / -----BEGIN PUBLIC KEY-----
 * RSA-OAEP-SHA256: 256 bytes, back: OK
 * RSA-PKCS1: back: OK
 * Wrong padding: NIL / error:02000079:rsa routines::oaep decoding error
 *
 * Signature: 256 bytes
 * Verify: valid
 * Verify changed text: invalid
 * PSS verify: valid
 * Bad key: NIL / error:0688010A:asn1 encoding routines::nested asn1 error
 *
 * $DOCEND$
 */

#include "openssl.ch"

STATIC s_cReport := ""

PROCEDURE Main()

   LOCAL cKey  := RandomBytes( 32 )
   LOCAL cIv   := RandomBytes( 16 )
   LOCAL cText := "Faktura FA(3) – zażółć gęślą jaźń"
   LOCAL cData
   LOCAL aKeys
   LOCAL cSignature
   LOCAL nFile

   s_Line( "Version: " + OpenSslVersion() )

   s_Line( "" )
   s_Line( "SHA256('abc'): " + Lower( StrToHex( HashData( "SHA256", "abc" ) ) ) )
   s_Line( "  expected:    ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad" )
   s_Line( "HMAC-SHA256:   " + Lower( StrToHex( HmacData( "SHA256", "key", "The quick brown fox jumps over the lazy dog" ) ) ) )
   s_Line( "  expected:    f7bc83f430538424b13298e6aa6fb143ef4d59a14946175997479dbc2d1a3cd8" )
   s_Line( "Base64: " + Base64Encode( "Kraków" ) + " -> " + Base64Decode( Base64Encode( "Kraków" ) ) )
   s_Line( "  expected: S3Jha8Ozdw== -> Kraków" )
   s_Line( "Hex: " + StrToHex( "AZ", ":" ) + " -> " + HexToStr( "41:5A" ) )
   s_Line( "SHA512 length: " + LTrim( Str( Len( HashData( "SHA512", cText ) ) ) ) )
   s_Line( "Unknown digest: " + s_Text( HashData( "NOSUCH", "abc" ) ) + " / " + CryptoError() )

   s_Line( "" )
   s_Line( "Random 32 bytes: " + LTrim( Str( Len( cKey ) ) ) + ", different: " + IIF( cKey == RandomBytes( 32 ), "no", "yes" ) )

   s_Line( "" )
   cData := AesEncrypt( cKey, cIv, cText )
   s_Line( "AES-256-CBC: " + LTrim( Str( Len( cData ) ) ) + " bytes, back: " + IIF( AesDecrypt( cKey, cIv, cData ) == cText, "OK", "failed" ) )
   cData := AesEncrypt( cKey, Left( cIv, 12 ), cText, AES_MODE_GCM )
   s_Line( "AES-256-GCM: " + LTrim( Str( Len( cData ) ) ) + " bytes, back: " + ;
           IIF( AesDecrypt( cKey, Left( cIv, 12 ), cData, AES_MODE_GCM ) == cText, "OK", "failed" ) )
   cData := Stuff( cData, 1, 1, IIF( Left( cData, 1 ) == "A", "B", "A" ) )
   s_Line( "GCM damaged: " + s_Text( AesDecrypt( cKey, Left( cIv, 12 ), cData, AES_MODE_GCM ) ) + " / " + CryptoError() )
   s_Line( "Bad key: " + s_Text( AesEncrypt( "short", cIv, cText ) ) + " / " + CryptoError() )

   s_Line( "" )
   aKeys := RsaGenerateKey( 2048 )
   s_Line( "RSA keys: " + Left( aKeys[ 1 ], 27 ) + " / " + Left( aKeys[ 2 ], 26 ) )
   cData := RsaEncrypt( aKeys[ 2 ], cKey )
   s_Line( "RSA-OAEP-SHA256: " + LTrim( Str( Len( cData ) ) ) + " bytes, back: " + IIF( RsaDecrypt( aKeys[ 1 ], cData ) == cKey, "OK", "failed" ) )
   cData := RsaEncrypt( aKeys[ 2 ], cKey, RSA_PADDING_PKCS1 )
   s_Line( "RSA-PKCS1: back: " + IIF( RsaDecrypt( aKeys[ 1 ], cData, RSA_PADDING_PKCS1 ) == cKey, "OK", "failed" ) )
   s_Line( "Wrong padding: " + s_Text( RsaDecrypt( aKeys[ 1 ], cData ) ) + " / " + CryptoError() )

   s_Line( "" )
   cSignature := SignData( aKeys[ 1 ], cText )
   s_Line( "Signature: " + LTrim( Str( Len( cSignature ) ) ) + " bytes" )
   s_Line( "Verify: " + IIF( VerifyData( aKeys[ 2 ], cText, cSignature ), "valid", "invalid" ) )
   s_Line( "Verify changed text: " + IIF( VerifyData( aKeys[ 2 ], cText + ".", cSignature ), "valid", "invalid" ) )
   cSignature := SignData( aKeys[ 1 ], cText, "SHA256", , RSA_PADDING_PSS )
   s_Line( "PSS verify: " + IIF( VerifyData( aKeys[ 2 ], cText, cSignature, "SHA256", RSA_PADDING_PSS ), "valid", "invalid" ) )
   s_Line( "Bad key: " + s_Text( SignData( "not a key", cText ) ) + " / " + CryptoError() )

   nFile := FCreate( "openssl.txt" )
   FWrite( nFile, s_cReport )
   FClose( nFile )

   QOut( "Results saved to openssl.txt" )
   RETURN

STATIC PROCEDURE s_Line( cText )

   s_cReport += cText + Eol()

   RETURN

STATIC FUNCTION s_Text( xValue )

   RETURN IIF( xValue == NIL, "NIL", "not NIL" )
