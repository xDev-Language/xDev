/*
 * $DOC$
 * $NAME$
 * Classes
 *
 * $ONELINER$
 * Demonstrates the Classes task.
 *
 * $DESCRIPTION$
 * Implements the Classes task in xDev.
 * A square is a rectangle with equal sides.
 *
 * $EXAMPLES$
 * examples/rosetta-code/classes.prg
 *
 * $OUTPUT$
 * circle       3.142
 * rectangle   12.000
 * square       4.000
 *
 * $DOCEND$
 */

PROCEDURE Main()

   LOCAL oShape

   FOR EACH oShape IN { Circle():New( 1 ), Rectangle():New( 3, 4 ), Square():New( 2 ) }
      QOut( PadR( oShape:Name(), 10 ) + Str( oShape:Area(), 8, 3 ) )
   NEXT

   RETURN

CREATE CLASS Shape

   METHOD Name()
   METHOD Area()

ENDCLASS

METHOD Name() CLASS Shape

   RETURN "shape"

METHOD Area() CLASS Shape

   RETURN 0

CREATE CLASS Circle FROM Shape

   DATA radius INIT 0

   METHOD New( nRadius )
   METHOD Name()
   METHOD Area()

ENDCLASS

METHOD New( nRadius ) CLASS Circle

   ::radius := nRadius

   RETURN Self

METHOD Name() CLASS Circle

   RETURN "circle"

METHOD Area() CLASS Circle

   RETURN Pi() * ::radius ^ 2

CREATE CLASS Rectangle FROM Shape

   DATA width  INIT 0
   DATA height INIT 0

   METHOD New( nWidth, nHeight )
   METHOD Name()
   METHOD Area()

ENDCLASS

METHOD New( nWidth, nHeight ) CLASS Rectangle

   ::width  := nWidth
   ::height := nHeight

   RETURN Self

METHOD Name() CLASS Rectangle

   RETURN "rectangle"

METHOD Area() CLASS Rectangle

   RETURN ::width * ::height
CREATE CLASS Square FROM Rectangle

   METHOD New( nSide )
   METHOD Name()

ENDCLASS

METHOD New( nSide ) CLASS Square

   ::width  := nSide
   ::height := nSide

   RETURN Self

METHOD Name() CLASS Square

   RETURN "square"
