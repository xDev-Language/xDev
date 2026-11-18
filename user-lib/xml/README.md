# xDev XML

XML library for the [xDev](https://github.com/xDev-Language) language, written in C without other libraries. A document is parsed into a tree of nodes, read and changed through pointers to the nodes, and written back as text or into a file.

## Key features

- `xml_Parse()` and `xml_Load()` read UTF-8 documents; `xml_Error()` tells the line and the reason of a parse error.
- `xml_ToString()` and `xml_Save()` write a document with the XML declaration, indented or not, with the line end of your choice.
- Navigation by elements: `xml_Root()`, `xml_Parent()`, `xml_First()`, `xml_Next()`, `xml_Children()`, `xml_Child()`.
- `xml_Find()` with a path of names, e.g. `"library/book[2]/title"`.
- Navigation by all nodes, including text, CDATA, comments and processing instructions.
- `xml_Name()`, `xml_Prefix()`, `xml_LocalName()`, `xml_Namespace()`, `xml_Text()`, `xml_Attr()` and `xml_Attrs()`.
- Building: `xml_New()`, `xml_Add()`, `xml_AddText()`, `xml_AddCData()`, `xml_AddComment()`, `xml_AddPI()`, `xml_Insert()`, `xml_Clone()`, `xml_Move()`, `xml_SetAttr()`, `xml_SetText()` and `xml_Remove()`.
- `xml_Escape()` for text in element content.
- The `XML_TYPE_*` constants in `xml.ch`.

## Platforms

- [x] Windows
- [x] Linux
- [ ] macOS

## Building

```
make
make install
```

`make` builds `libxxml.a`. `make install` installs the library into the xDev directory. Libraries in `lib/xdev` are linked automatically. The xDev XAdES library uses this library too.

## Example

```xBase
#include "xml.ch"

PROCEDURE Main()

   LOCAL pDoc := xml_New()
   LOCAL pRoot := xml_Add( pDoc, "library" )
   LOCAL pBook := xml_Add( pRoot, "book" )
   LOCAL pFound

   xml_SetAttr( pBook, "id", "1" )
   xml_Add( pBook, "title", "Zażółć gęślą jaźń" )

   ? xml_ToString( pDoc )

   pFound := xml_Find( pDoc, "library/book[1]/title" )
   IF pFound != NIL
      ? xml_Text( pFound )
   ENDIF

   xml_Free( pDoc )

RETURN
```

## License

GPL-3.0. See [LICENSE](LICENSE).
