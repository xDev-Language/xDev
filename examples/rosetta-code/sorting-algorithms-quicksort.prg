/*
 * $DOC$
 * $NAME$
 * Sorting algorithms/Quicksort
 *
 * $ONELINER$
 * Demonstrates the Sorting algorithms/Quicksort task.
 *
 * $DESCRIPTION$
 * Implements the Sorting algorithms/Quicksort task in xDev.
 *
 * $EXAMPLES$
 * examples/rosetta-code/sorting-algorithms-quicksort.prg
 *
 * $OUTPUT$
 * -7 0 2 5 13 13 41 65 77 98
 *
 * $DOCEND$
 */

PROCEDURE Main()

   LOCAL aList := { 65, 13, 2, 98, -7, 41, 0, 13, 77, 5 }

   s_QuickSort( aList, 1, Len( aList ) )
   AEval( aList, {| n | QQOut( LTrim( Str( n ) ) + " " ) } )

   RETURN

STATIC PROCEDURE s_QuickSort( aList AS ARRAY, nLeft AS INT, nRight AS INT )

   LOCAL xPivot := aList[ Int( ( nLeft + nRight ) / 2 ) ]
   LOCAL i      AS INT := nLeft
   LOCAL j      AS INT := nRight
   LOCAL xTemp

   DO WHILE i <= j
      DO WHILE aList[ i ] < xPivot
         i++
      ENDDO
      DO WHILE aList[ j ] > xPivot
         j--
      ENDDO
      IF i <= j
         xTemp      := aList[ i ]
         aList[ i ] := aList[ j ]
         aList[ j ] := xTemp
         i++
         j--
      ENDIF
   ENDDO

   IF nLeft < j
      s_QuickSort( aList, nLeft, j )
   ENDIF
   IF i < nRight
      s_QuickSort( aList, i, nRight )
   ENDIF

   RETURN
