/*
 * $DOC$
 * $NAME$
 * Stack
 *
 * $ONELINER$
 * Demonstrates the Stack task.
 *
 * $DESCRIPTION$
 * Implements the Stack task in xDev.
 * Pop() and Top() require a non-empty stack.
 *
 * $EXAMPLES$
 * examples/rosetta-code/stack.prg
 *
 * $OUTPUT$
 * top:   3
 * pop:   3
 * pop:   two
 * empty: .F.
 * pop:   1
 * empty: .T.
 *
 * $DOCEND$
 */

PROCEDURE Main()

   LOCAL oStack := Stack():New()

   oStack:Push( 1 )
   oStack:Push( "two" )
   oStack:Push( 3.0 )
   QOut( "top:  ", oStack:Top() )
   QOut( "pop:  ", oStack:Pop() )
   QOut( "pop:  ", oStack:Pop() )
   QOut( "empty:", oStack:Empty() )
   QOut( "pop:  ", oStack:Pop() )
   QOut( "empty:", oStack:Empty() )

   RETURN

CREATE CLASS Stack

   DATA items AS ARRAY INIT {}

   METHOD New()
   METHOD Push( xValue )
   METHOD Pop()
   METHOD Top()
   METHOD Empty()

ENDCLASS

METHOD New() CLASS Stack

   ::items := {}

   RETURN Self

METHOD Push( xValue ) CLASS Stack

   AAdd( ::items, xValue )

   RETURN Self
METHOD Pop() CLASS Stack

   LOCAL xValue := ATail( ::items )

   ASize( ::items, Len( ::items ) - 1 )

   RETURN xValue

METHOD Top() CLASS Stack

   RETURN ATail( ::items )

METHOD Empty() CLASS Stack

   RETURN Len( ::items ) == 0
