/*
 * $DOC$
 * $NAME$
 * XML processing
 *
 * $ONELINER$
 * Demonstrates XML parsing, DOM operations, and serialization.
 *
 * $DESCRIPTION$
 * Runs XML examples using the xDev XML library and writes the results to a text report.
 *
 * $EXAMPLES$
 * examples/xml.prg
 *
 * $OUTPUT$
 * <?xml version="1.0" encoding="UTF-8"?>
 * <library city="Kraków">
 *   <book id="1">
 *     <title>Pan Tadeusz</title>
 *     <author>Adam Mickiewicz</author>
 *   </book>
 *   <book id="2">
 *     <title>Lalka &amp; &lt;Emancypantki&gt;</title>
 *     <author>Bolesław Prus</author>
 *     <borrowed/>
 *   </book>
 * </library>
 *
 * City: Kraków
 * First title: Pan Tadeusz
 * Second title: Lalka & <Emancypantki>
 * Books: 2
 * Last book id: 2
 * Before last: 1
 * Missing: NIL
 *
 * 1 Pan Tadeusz - Adam Mickiewicz
 * 2 Lalka & <Emancypantki> - Bolesław Prus (borrowed)
 *
 * Tree:
 * a x=1 [<raw>]
 *   b [text & A]
 *   c
 *
 * Errors:
 * NIL line 1: expected </b>
 * NIL line 2: invalid entity reference
 * NIL line 1: more than one root element
 * NIL line 1: unsupported encoding, only UTF-8 is accepted
 * NIL line 1: invalid element name
 * NIL line 1: ']]>' in text
 *
 * Namespaces:
 * inv | Invoice | urn:invoice
 * inv:Line | urn:invoice
 * Note | urn:default
 *
 * Editing:
 * Move d to front: OK
 * Move list into a: refused
 * <list><d/><a/><b/><c/><a/></list>
 *
 * Node types:
 * 2  [Hello ]
 * 1 b [big]
 * 2  [ world]
 * 4  [ c ]
 * 3  [x<y]
 * 5 php [echo 1]
 * Text: Hello  worldx<y
 * <?xml version="1.0" encoding="UTF-8"?>
 * <!-- top -->
 * <p>Hello <b>big</b> world<!-- c --><![CDATA[x<y]]><?php echo 1?></p>
 * AddPI xml refused: yes
 * Text at document level refused: yes
 * <?xml version="1.0" encoding="UTF-8"?>
 * <!--generated - - by xDev-->
 * <note>a <![CDATA[]]]]><![CDATA[>]]><?app mode=1?></note>
 *
 * $DOCEND$
 */

STATIC cReport := ""

PROCEDURE Main()

   LOCAL pDoc := xml_New()
   LOCAL pRoot
   LOCAL pItem
   LOCAL nFile
   LOCAL aNodes
   LOCAL nIndex

   pRoot := xml_Add( pDoc, "library" )
   xml_SetAttr( pRoot, "city", "Kraków" )

   pItem := xml_Add( pRoot, "book" )
   xml_SetAttr( pItem, "id", "1" )
   xml_Add( pItem, "title", "Pan Tadeusz" )
   xml_Add( pItem, "author", "Adam Mickiewicz" )

   pItem := xml_Add( pRoot, "book" )
   xml_SetAttr( pItem, "id", "2" )
   xml_Add( pItem, "title", "Lalka & <Emancypantki>" )
   xml_Add( pItem, "author", "Bolesław Prus" )
   xml_Add( pItem, "borrowed" )

   cReport += xml_ToString( pDoc, .T., Eol() )

   IF ! xml_Save( pDoc, "library.xml" )
      s_Line( "Save error: " + xml_Error() )
   ENDIF

   xml_Free( pDoc )

   pDoc := xml_Load( "library.xml" )
   IF pDoc == NIL
      s_Line( "Load error: " + xml_Error() )
   ELSE
      s_Line( "" )
      s_Line( "City: " + xml_Attr( xml_Root( pDoc ), "city" ) )
      s_Line( "First title: " + xml_Text( xml_Find( pDoc, "library/book/title" ) ) )
      s_Line( "Second title: " + xml_Text( xml_Find( pDoc, "library/book[2]/title" ) ) )
      s_Line( "Books: " + LTrim( Str( Len( xml_Children( xml_Root( pDoc ), "book" ) ) ) ) )
      s_Line( "Last book id: " + xml_Attr( xml_Last( xml_Root( pDoc ) ), "id" ) )
      s_Line( "Before last: " + xml_Attr( xml_Prev( xml_Last( xml_Root( pDoc ) ) ), "id" ) )
      s_Line( "Missing: " + IIF( xml_Find( pDoc, "library/book[3]" ) == NIL, "NIL", "found" ) )
      s_Line( "" )

      pItem := xml_First( xml_Root( pDoc ) )
      DO WHILE pItem != NIL
         s_Line( xml_Attr( pItem, "id" ) + " " + xml_Text( xml_Child( pItem, "title" ) ) + " - " + ;
                 xml_Text( xml_Child( pItem, "author" ) ) + ;
                 IIF( xml_Child( pItem, "borrowed" ) != NIL, " (borrowed)", "" ) )
         pItem := xml_Next( pItem )
      ENDDO

      xml_Free( pDoc )
   ENDIF

   s_Line( "" )
   s_Line( "Tree:" )
   pDoc := xml_Parse( '<a x="1"><b>text &amp; &#x41;</b><![CDATA[<raw>]]><c/></a>' )
   s_Dump( xml_Root( pDoc ), 0 )
   xml_Free( pDoc )

   s_Line( "" )
   s_Line( "Errors:" )
   s_Error( "<a><b></a>" )
   s_Error( e'<a>\n&unknown;</a>' )
   s_Error( "<a/><b/>" )
   s_Error( '<?xml version="1.0" encoding="windows-1250"?><a/>' )
   s_Error( "<1a/>" )
   s_Error( "<a>x ]]> y</a>" )

   s_Line( "" )
   s_Line( "Namespaces:" )
   pDoc := xml_Parse( '<inv:Invoice xmlns:inv="urn:invoice" xmlns="urn:default"><inv:Line/><Note/></inv:Invoice>' )
   pRoot := xml_Root( pDoc )
   s_Line( xml_Prefix( pRoot ) + " | " + xml_LocalName( pRoot ) + " | " + xml_Namespace( pRoot ) )
   s_Line( xml_Name( xml_First( pRoot ) ) + " | " + xml_Namespace( xml_First( pRoot ) ) )
   s_Line( xml_Name( xml_Last( pRoot ) ) + " | " + xml_Namespace( xml_Last( pRoot ) ) )
   xml_Free( pDoc )

   s_Line( "" )
   s_Line( "Editing:" )
   pDoc  := xml_Parse( "<list><b/><d/></list>" )
   pRoot := xml_Root( pDoc )
   xml_Insert( xml_First( pRoot ), "a" )
   xml_Insert( xml_Child( pRoot, "b" ), "c", , .T. )
   xml_Clone( xml_Child( pRoot, "a" ), pRoot )
   s_Line( "Move d to front: " + IIF( xml_Move( xml_Child( pRoot, "d" ), pRoot, xml_First( pRoot ) ), "OK", "failed" ) )
   s_Line( "Move list into a: " + IIF( xml_Move( pRoot, xml_Child( pRoot, "a" ) ), "OK", "refused" ) )
   s_Line( xml_ToString( pRoot, .F. ) )
   xml_Free( pDoc )

   s_Line( "" )
   s_Line( "Node types:" )
   pDoc := xml_Parse( '<!-- top --><p>Hello <b>big</b> world<!-- c --><![CDATA[x<y]]><?php echo 1?></p>' )
   aNodes := xml_Nodes( xml_Root( pDoc ) )
   FOR nIndex := 1 TO Len( aNodes )
      s_Line( LTrim( Str( xml_Type( aNodes[ nIndex ] ) ) ) + " " + xml_Name( aNodes[ nIndex ] ) + ;
              " [" + xml_Text( aNodes[ nIndex ] ) + "]" )
   NEXT
   s_Line( "Text: " + xml_Text( xml_Root( pDoc ) ) )
   cReport += xml_ToString( pDoc, .T., Eol() )
   xml_Free( pDoc )

   pDoc := xml_New()
   xml_AddComment( pDoc, "generated -- by xDev" )
   pRoot := xml_Add( pDoc, "note" )
   xml_AddText( pRoot, "a " )
   xml_AddCData( pRoot, "]]>" )
   xml_AddPI( pRoot, "app", "mode=1" )
   s_Line( "AddPI xml refused: " + IIF( xml_AddPI( pDoc, "xml" ) == NIL, "yes", "no" ) )
   s_Line( "Text at document level refused: " + IIF( xml_AddText( pDoc, "x" ) == NIL, "yes", "no" ) )
   cReport += xml_ToString( pDoc, .T., Eol() )
   xml_Free( pDoc )

   nFile := FCreate( "xml.txt" )
   FWrite( nFile, cReport )
   FClose( nFile )

   QOut( "Results saved to xml.txt" )

   RETURN

STATIC PROCEDURE s_Line( cText )

   cReport += cText + Eol()

   RETURN

STATIC PROCEDURE s_Error( cXml )

   s_Line( IIF( xml_Parse( cXml ) == NIL, "NIL ", "document " ) + xml_Error() )

   RETURN

STATIC PROCEDURE s_Dump( pNode, nDepth )

   LOCAL aAttrs := xml_Attrs( pNode )
   LOCAL cLine  := Space( nDepth * 2 ) + xml_Name( pNode )
   LOCAL nIndex

   FOR nIndex := 1 TO Len( aAttrs )
      cLine += " " + aAttrs[ nIndex ][ 1 ] + "=" + aAttrs[ nIndex ][ 2 ]
   NEXT
   IF ! Empty( xml_Text( pNode ) )
      cLine += " [" + xml_Text( pNode ) + "]"
   ENDIF
   s_Line( cLine )

   pNode := xml_First( pNode )
   DO WHILE pNode != NIL
      s_Dump( pNode, nDepth + 1 )
      pNode := xml_Next( pNode )
   ENDDO

   RETURN
