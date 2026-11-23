# xDev XAdES

XAdES-BES electronic signatures for XML documents in the [xDev](https://github.com/xDev-Language) language, built on [OpenSSL](https://www.openssl.org) 3 and the xDev XML library. A document can be signed with a key and certificate, then verified in only a few calls.

## Key features

- `XadesSign()`: an enveloped XAdES-BES signature with SHA-256 and exclusive canonicalization (exc-c14n).
- EC (ECDSA) and RSA keys; keys and certificates in PEM or DER, encrypted private keys with a password.
- The signing time, the digest of the certificate and its issuer and serial number in the signed properties.
- `XadesVerify()` checks the digests of all references and the signature value with the certificate from `KeyInfo`. The certificate itself is not checked.
- `XmlCanonical()` gives the exclusive canonical form of a document, without comments.
- `XadesError()` reports what went wrong; a failed function returns NIL.
- Documents in UTF-8.

## Platforms

- [x] Windows
- [x] Linux
- [ ] macOS

## Building

```
make
make install
```

`make` builds `libxxades.a`. It uses the headers of OpenSSL (`pkg-config libssl libcrypto`) and of the xDev OpenSSL and XML libraries. The code of OpenSSL comes with `libxopenssl.a` and the XML parser with the XML library, so both must be installed too. `make install` installs the library into the xDev directory. Libraries in `lib/xdev` are linked automatically.

## Example

```xBase
PROCEDURE Main()

   LOCAL cXml := '<?xml version="1.0" encoding="UTF-8"?><Invoice><Name>Zażółć gęślą jaźń</Name></Invoice>'
   LOCAL cKey := MemoRead( "key.pem" )
   LOCAL cCert := MemoRead( "cert.pem" )
   LOCAL cSigned := XadesSign( cXml, cKey, cCert )

   IF cSigned == NIL
      ? XadesError()
   ELSE
      MemoWrit( "signed.xml", cSigned )
      ? XadesVerify( cSigned )
   ENDIF

RETURN
```

## License

xDev jest na licencji GNU GPL v3, patrz plik LICENSE.
