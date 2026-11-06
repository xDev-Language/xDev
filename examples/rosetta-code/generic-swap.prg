/*
 * Generic swap
 */

PROCEDURE Main()

   LOCAL a := 1
   LOCAL b := "two"

   Swap( @a, @b )
   QOut( a, b )

   RETURN

// parameters passed by reference (@) may hold any type
STATIC PROCEDURE Swap( x, y )

   LOCAL t := x

   x := y
   y := t

   RETURN
