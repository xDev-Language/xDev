/*
 * $DOC$
 * $NAME$
 * Towers of Hanoi
 *
 * $ONELINER$
 * Demonstrates the Towers of Hanoi task.
 *
 * $DESCRIPTION$
 * Implements the Towers of Hanoi task in xDev.
 *
 * $EXAMPLES$
 * examples/rosetta-code/towers-of-hanoi.prg
 *
 * $OUTPUT$
 * Move disk 1 from peg 1 to peg 3
 * Move disk 2 from peg 1 to peg 2
 * Move disk 1 from peg 3 to peg 2
 * Move disk 3 from peg 1 to peg 3
 * Move disk 1 from peg 2 to peg 1
 * Move disk 2 from peg 2 to peg 3
 * Move disk 1 from peg 1 to peg 3
 * Move disk 4 from peg 1 to peg 2
 * Move disk 1 from peg 3 to peg 2
 * Move disk 2 from peg 3 to peg 1
 * Move disk 1 from peg 2 to peg 1
 * Move disk 3 from peg 3 to peg 2
 * Move disk 1 from peg 1 to peg 3
 * Move disk 2 from peg 1 to peg 2
 * Move disk 1 from peg 3 to peg 2
 *
 * $DOCEND$
 */

PROCEDURE Main()

   Hanoi( 4, 1, 2, 3 )

   RETURN

STATIC PROCEDURE Hanoi( nDisks AS INT, nFrom AS INT, nTo AS INT, nVia AS INT )

   IF nDisks > 0
      Hanoi( nDisks - 1, nFrom, nVia, nTo )
      QOut( "Move disk " + LTrim( Str( nDisks ) ) + " from peg " + LTrim( Str( nFrom ) ) + " to peg " + LTrim( Str( nTo ) ) )
      Hanoi( nDisks - 1, nVia, nTo, nFrom )
   ENDIF

   RETURN
