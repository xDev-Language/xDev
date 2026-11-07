/*
 * $DOC$
 * $NAME$
 * Loops/Foreach
 *
 * $ONELINER$
 * Demonstrates the Loops/Foreach task.
 *
 * $DESCRIPTION$
 * Implements the Loops/Foreach task in xDev.
 * An array; :__enumIndex is the position.
 * A hash; :__enumKey is the key.
 * The characters of a string, backwards.
 *
 * $EXAMPLES$
 * examples/rosetta-code/loops-foreach.prg
 *
 * $OUTPUT$
 * 1. apple
 * 2. banana
 * 3. cherry
 * apple: 10
 * banana: 0
 * cherry: 4
 * veDx
 *
 * $DOCEND$
 */

PROCEDURE Main()

   LOCAL aFruit := { "apple", "banana", "cherry" }
   LOCAL hStock := { "apple" => 10, "banana" => 0, "cherry" => 4 }
   LOCAL cFruit, nCount, c
   FOR EACH cFruit IN aFruit
      QOut( LTrim( Str( cFruit:__enumIndex ) ) + ". " + cFruit )
   NEXT
   FOR EACH nCount IN hStock
      QOut( nCount:__enumKey + ": " + LTrim( Str( nCount ) ) )
   NEXT
   QOut( "" )
   FOR EACH c IN "xDev" DESCEND
      QQOut( c )
   NEXT

   RETURN
