/*
 * XML/DOM serialization
 */

PROCEDURE Main()

   LOCAL pDoc  := xml_New()
   LOCAL pRoot := xml_Add( pDoc, "root" )

   xml_Add( pRoot, "element", "Some text here" )
   QOut( xml_ToString( pDoc ) )
   xml_Free( pDoc )

   RETURN
