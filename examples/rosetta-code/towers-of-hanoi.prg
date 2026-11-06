/*
 * Towers of Hanoi
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
