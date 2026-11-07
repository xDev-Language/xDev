/*
 * $DOC$
 * $NAME$
 * XML/Input
 *
 * $ONELINER$
 * Demonstrates the XML/Input task.
 *
 * $DESCRIPTION$
 * Implements the XML/Input task in xDev.
 *
 * $EXAMPLES$
 * examples/rosetta-code/xml-input.prg
 *
 * $OUTPUT$
 * April
 * Bob
 * Chad
 * Dave
 * Émily
 *
 * $DOCEND$
 */

PROCEDURE Main()

   LOCAL cXml := '<Students>' + ;
      '<Student Name="April" Gender="F" DateOfBirth="1989-01-02" />' + ;
      '<Student Name="Bob" Gender="M"  DateOfBirth="1990-03-04" />' + ;
      '<Student Name="Chad" Gender="M"  DateOfBirth="1991-05-06" />' + ;
      '<Student Name="Dave" Gender="M"  DateOfBirth="1992-07-08">' + ;
      '<Pet Type="dog" Name="Rover" />' + ;
      '</Student>' + ;
      '<Student DateOfBirth="1993-09-10" Gender="F" Name="&#x00C9;mily" />' + ;
      '</Students>'
   LOCAL pDoc := xml_Parse( cXml )
   LOCAL pStudent

   FOR EACH pStudent IN xml_Children( xml_Root( pDoc ), "Student" )
      QOut( xml_Attr( pStudent, "Name" ) )
   NEXT
   xml_Free( pDoc )

   RETURN
