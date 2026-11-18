# xDev 7.0.0 RDD

DBF table library for the [xDev](https://github.com/xDev-Language) language. It provides the database functions known from the xBase family: `DbUseArea()`, `DbSkip()`, `DbSeek()`, `RLock()` and others.

## Key features

- Text in UTF-8 only. No code pages and no conversions.
- xDev 7 format: field names up to 32 characters, DBT memo, binary field `P`.
- NTX and CDX indexes.
- Several workstations on the same files: record and file locks; short internal locks wait instead of raising an error.
- A write error does not block the table permanently. A work area can always be closed.

## Building

```
make
make install
```

`make` builds `libxrdd.a`. `make install` installs the library into the xDev directory. Libraries in `lib/xdev` are linked automatically.

## Example

```xBase
PROCEDURE Main()

   dbCreate( "klienci", { { "NAZWA", "C", 40, 0 }, { "MIASTO", "C", 20, 0 } } )
   dbUseArea( .T., NIL, "klienci" )
   dbAppend()
   klienci->NAZWA  := "Zakład Usług Wiśniewski"
   klienci->MIASTO := "Gdańsk"
   dbCloseArea()

RETURN
```

An existing index is opened with compiled key blocks, because xDev has no `&` macro:

```
dbSetIndex( "klienci", { { NIL, "NAZWA", {|| klienci->NAZWA }, NIL, NIL } } )
```

## License

GPL-3.0. See [LICENSE](LICENSE).
