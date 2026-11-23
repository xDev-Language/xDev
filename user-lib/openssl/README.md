# xDev OpenSSL

Cryptography for the [xDev](https://github.com/xDev-Language) language, built on [OpenSSL](https://www.openssl.org) 3: digests, HMAC, random bytes, AES, RSA, signatures, certificates and PKCS#12 files. Data and results are strings of bytes.

## Key features

- `HashData()` and `HmacData()` with every digest of OpenSSL: SHA-256, SHA-512, SHA-3, MD5 and others.
- `RandomBytes()` for keys and IVs.
- `AesEncrypt()` and `AesDecrypt()`: AES-128/192/256 in CBC, CTR, GCM (with the tag) and ECB.
- `RsaEncrypt()`, `RsaDecrypt()` and `RsaGenerateKey()` with OAEP or PKCS#1 padding.
- `SignData()` and `VerifyData()` with RSA (PKCS#1 or PSS), EC and Ed25519 keys.
- Keys and certificates in PEM or DER, encrypted private keys with a password.
- `CertInfo()` for the subject, issuer, serial number and dates of a certificate; `Pkcs12Read()` for `.p12` and `.pfx` files.
- `CryptoError()` reports what went wrong; a failed function returns NIL.
- The `AES_MODE_*` and `RSA_PADDING_*` constants in `openssl.ch`.

## Platforms

- [x] Windows
- [x] Linux
- [ ] macOS

## Building

```
make
make install
```

`make` builds `libxopenssl.a` with the static OpenSSL libraries inside. OpenSSL comes from the system (`pkg-config libssl libcrypto`). Its functions with names in capitals (`MD5`, `SHA256`, `HMAC`...) get the prefix `OSSL_` in the library, so they do not collide with xDev functions of the same name. `make install` installs the library into the xDev directory. Libraries in `lib/xdev` are linked automatically.

## Example

```xBase
#include "openssl.ch"

PROCEDURE Main()

   LOCAL cKey := RandomBytes( 32 )
   LOCAL cIv := RandomBytes( 12 )
   LOCAL cSecret := AesEncrypt( cKey, cIv, "Zażółć gęślą jaźń", AES_MODE_GCM )

   ? StrToHex( HashData( "SHA256", "xDev" ) )
   ? AesDecrypt( cKey, cIv, cSecret, AES_MODE_GCM )

   IF cSecret == NIL
      ? CryptoError()
   ENDIF

RETURN
```

## License

xDev jest na licencji GNU GPL v3, patrz plik LICENSE.
