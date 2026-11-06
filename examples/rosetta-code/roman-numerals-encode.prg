/*
 * Roman numerals/Encode
 */

PROCEDURE Main()

   LOCAL nValue

   FOR EACH nValue IN { 1990, 2008, 1666, 2026, 3999 }
      QOut( Str( nValue, 4 ) + " = " + ToRoman( nValue ) )
   NEXT

   RETURN

STATIC FUNCTION ToRoman( nValue AS INT ) AS STRING

   LOCAL aValue  := { 1000, 900, 500, 400, 100, 90, 50, 40, 10, 9, 5, 4, 1 }
   LOCAL aSymbol := { "M", "CM", "D", "CD", "C", "XC", "L", "XL", "X", "IX", "V", "IV", "I" }
   LOCAL cResult AS STRING := ""
   LOCAL nPart, cSymbol

   FOR EACH nPart, cSymbol IN aValue, aSymbol
      DO WHILE nValue >= nPart
         cResult += cSymbol
         nValue  -= nPart
      ENDDO
   NEXT

   RETURN cResult
