/*
 * $DOC$
 * $NAME$
 * Levenshtein distance
 *
 * $ONELINER$
 * Demonstrates the Levenshtein distance task.
 *
 * $DESCRIPTION$
 * Implements the Levenshtein distance task in xDev.
 *
 * $EXAMPLES$
 * examples/rosetta-code/levenshtein-distance.prg
 *
 * $OUTPUT$
 * kitten -> sitting:      3
 * rosettacode -> raisethysword: 8
 *
 * $DOCEND$
 */

PROCEDURE Main()

   QOut( "kitten -> sitting:     ", s_Levenshtein( "kitten", "sitting" ) )
   QOut( "rosettacode -> raisethysword:", s_Levenshtein( "rosettacode", "raisethysword" ) )

   RETURN

STATIC FUNCTION s_Levenshtein( cA AS STRING, cB AS STRING ) AS INT

   LOCAL nA    AS INT := Len( cA )
   LOCAL nB    AS INT := Len( cB )
   LOCAL aPrev := Array( nB + 1 )
   LOCAL aCurr
   LOCAL i     AS INT
   LOCAL j     AS INT

   FOR j := 0 TO nB
      aPrev[ j + 1 ] := j
   NEXT

   FOR i := 1 TO nA
      aCurr := Array( nB + 1 )
      aCurr[ 1 ] := i
      FOR j := 1 TO nB
         aCurr[ j + 1 ] := Min( Min( aPrev[ j + 1 ] + 1, aCurr[ j ] + 1 ), ;
                                aPrev[ j ] + IIF( SubStr( cA, i, 1 ) == SubStr( cB, j, 1 ), 0, 1 ) )
      NEXT
      aPrev := aCurr
   NEXT

   RETURN aPrev[ nB + 1 ]
