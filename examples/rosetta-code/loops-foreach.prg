/*
 * Loops/Foreach
 */

PROCEDURE Main()

   LOCAL aFruit := { "apple", "banana", "cherry" }
   LOCAL hStock := { "apple" => 10, "banana" => 0, "cherry" => 4 }
   LOCAL cFruit, nCount, c

   // an array; :__enumIndex is the position
   FOR EACH cFruit IN aFruit
      QOut( LTrim( Str( cFruit:__enumIndex ) ) + ". " + cFruit )
   NEXT

   // a hash; :__enumKey is the key
   FOR EACH nCount IN hStock
      QOut( nCount:__enumKey + ": " + LTrim( Str( nCount ) ) )
   NEXT

   // the characters of a string, backwards
   QOut( "" )
   FOR EACH c IN "xDev" DESCEND
      QQOut( c )
   NEXT

   RETURN
