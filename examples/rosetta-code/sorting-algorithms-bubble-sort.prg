/*
 * $DOC$
 * $NAME$
 * Sorting algorithms/Bubble sort
 *
 * $ONELINER$
 * Demonstrates the Sorting algorithms/Bubble sort task.
 *
 * $DESCRIPTION$
 * Implements the Sorting algorithms/Bubble sort task in xDev.
 *
 * $EXAMPLES$
 * examples/rosetta-code/sorting-algorithms-bubble-sort.prg
 *
 * $OUTPUT$
 * -7 0 2 5 13 13 41 65 77 98
 *
 * $DOCEND$
 */

PROCEDURE Main()

   LOCAL aList := { 65, 13, 2, 98, -7, 41, 0, 13, 77, 5 }

   BubbleSort( aList )
   AEval( aList, {| n | QQOut( LTrim( Str( n ) ) + " " ) } )

   RETURN

STATIC PROCEDURE BubbleSort( aList AS ARRAY )

   LOCAL lSwapped AS LOGIC := .T.
   LOCAL nLast    AS INT   := Len( aList )
   LOCAL i        AS INT
   LOCAL xTemp

   DO WHILE lSwapped
      lSwapped := .F.
      FOR i := 1 TO nLast - 1
         IF aList[ i ] > aList[ i + 1 ]
            xTemp          := aList[ i ]
            aList[ i ]     := aList[ i + 1 ]
            aList[ i + 1 ] := xTemp
            lSwapped       := .T.
         ENDIF
      NEXT
      nLast--
   ENDDO

   RETURN
