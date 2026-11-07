/*
 * $DOC$
 * $NAME$
 * XML/DOM serialization
 *
 * $ONELINER$
 * Demonstrates the XML/DOM serialization task.
 *
 * $DESCRIPTION$
 * Implements the XML/DOM serialization task in xDev.
 *
 * $EXAMPLES$
 * examples/rosetta-code/xml-dom-serialization.prg
 *
 * $OUTPUT$
 * <?xml version="1.0" encoding="UTF-8"?>
 * <root>
 *   <element>Some text here</element>
 * </root>
 *
 * $DOCEND$
 */

PROCEDURE Main()

   LOCAL pDoc  := xml_New()
   LOCAL pRoot := xml_Add( pDoc, "root" )

   xml_Add( pRoot, "element", "Some text here" )
   QOut( xml_ToString( pDoc ) )
   xml_Free( pDoc )

   RETURN
