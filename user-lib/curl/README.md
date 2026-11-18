# xDev CURL

[libcurl](https://curl.se/libcurl/) for the [xDev](https://github.com/xDev-Language) language. A program can send HTTP requests, download and upload files and use the other protocols of libcurl. The functions keep the names of the libcurl C API: `curl_easy_init()`, `curl_easy_setopt()`, `curl_easy_perform()` and others.

## Key features

- Easy, multi, share, URL and MIME interfaces of libcurl.
- Received data goes to a memory buffer (`curl_easy_buffer()`) or to a file (`curl_easy_download()`).
- Upload from a string or from a file (`curl_easy_upload()`, `curl_easy_upload_file()`).
- Code blocks for the progress of a transfer and for the verbose trace.
- Lists of strings as arrays, e.g. `CURLOPT_HTTPHEADER`.
- The `CURLOPT_*`, `CURLINFO_*` and other constants in `curl.ch`.

## Platforms

- [x] Windows
- [x] Linux
- [ ] macOS

## Building

```
make
make install
```

`make` builds `libxcurl.a` with the static libcurl library and its dependencies inside. libcurl comes from the system (`pkg-config libcurl`). OpenSSL comes from `libxopenssl.a` and zlib from `libxzlib.a`. `make install` installs the library into the xDev directory. Libraries in `lib/xdev` are linked automatically.

## Example

```xBase
#include "curl.ch"

PROCEDURE Main()

   LOCAL pCurl := curl_easy_init()

   curl_easy_setopt( pCurl, CURLOPT_URL, "https://example.com" )
   curl_easy_setopt( pCurl, CURLOPT_FOLLOWLOCATION, .T. )

   IF curl_easy_perform( pCurl ) == CURLE_OK
      ? curl_easy_getinfo( pCurl, CURLINFO_RESPONSE_CODE )
      ? curl_easy_buffer( pCurl )
   ENDIF

   curl_easy_cleanup( pCurl )

RETURN
```

## License

GPL-3.0. See [LICENSE](LICENSE). libcurl has its own curl license.
