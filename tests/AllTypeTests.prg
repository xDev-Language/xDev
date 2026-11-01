/*
 * Every type declared with AS (constitution, point 3.d): integer types with range and overflow
 * checks, floating point, CAST, mixed signed and unsigned arithmetic, STRING, LOGIC, DATE, ARRAY,
 * HASH, CODEBLOCK, OBJECT, PTR, ANY, typed parameters and results, and runtime checks of ANY.
 * Compile errors cannot be checked in a running program; this file must compile.
 */

STATIC s_hCache AS HASH

PROCEDURE Main()

   s_Integers()
   s_Floats()
   s_Values()
   s_Hashes()
   s_Functions()

RETURN

STATIC PROCEDURE s_Integers()

   LOCAL n8     AS INT8   := 127
   LOCAL nU8    AS UINT8  := 255
   LOCAL n16    AS INT16  := -32768
   LOCAL n32    AS INT32  := 2147483647
   LOCAL nU32   AS UINT32 := 4294967295
   LOCAL n      AS INT    := 9007199254740993
   LOCAL nU64   AS UINT64 := 18446744073709551615
   LOCAL xValue AS ANY

   Check( "INT8 maximum                  ", n8, 127 )
   Check( "INT8 overflow is an error     ", s_Fails( {|| n8 := n8 + 1 } ), .T. )
   Check( "UINT8 maximum                 ", nU8, 255 )
   Check( "UINT8 below zero is an error  ", s_Fails( {|| nU8 := nU8 - 256 } ), .T. )
   Check( "INT16 minimum                 ", n16, -32768 )
   Check( "INT32 maximum                 ", n32, 2147483647 )
   Check( "INT32 + UINT32 gives INT64    ", n32 + nU32, 6442450942 )
   Check( "INT is INT64, exact           ", n - 1, 9007199254740992 )
   Check( "UINT64 maximum as text        ", Str( nU64, 20 ), "18446744073709551615" )
   Check( "CAST truncates toward zero    ", CAST( -2.7 AS INT ), -2 )
   Check( "CAST out of range is an error ", s_Fails( {|| n8 := CAST( 300 AS INT8 ) } ), .T. )
   xValue := "text"
   Check( "ANY with text -> INT error    ", s_Fails( {|| n := xValue } ), .T. )

RETURN

STATIC PROCEDURE s_Floats()

   LOCAL f64 AS FLOAT64 := 0.1
   LOCAL f32 AS FLOAT32 := 0.1
   LOCAL f   AS FLOAT   := 1.5
   LOCAL n   AS INT     := 3

   Check( "FLOAT64 keeps 0.1          ", f64 == 0.1, .T. )
   Check( "FLOAT32 rounds 0.1         ", f32 == 0.1, .F. )
   Check( "FLOAT is FLOAT64           ", f * 2, 3.0 )
   Check( "INT / INT gives a fraction ", n / 2, 1.5 )

RETURN

STATIC PROCEDURE s_Values()

   LOCAL cText     AS STRING    := "Łódź"
   LOCAL lFlag     AS LOGIC     := .T.
   LOCAL dEmpty    AS DATE
   LOCAL dToday    AS DATE      := Date()
   LOCAL aList     AS ARRAY     := { 10, 20, 30 }
   LOCAL bDouble   AS CODEBLOCK := {| x | x * 2 }
   LOCAL oGreeting AS OBJECT    := TypeGreeting():New( "xDev" )
   LOCAL pNone     AS PTR
   LOCAL xAny      AS ANY       := 5

   Check( "STRING counts bytes           ", Len( cText ), 7 )
   Check( "LOGIC                         ", .NOT. lFlag, .F. )
   Check( "DATE without a value is empty ", Empty( dEmpty ), .T. )
   Check( "DATE arithmetic               ", dToday + 1 - dToday, 1 )
   Check( "ARRAY with a numeric index    ", aList[ 2 ], 20 )
   Check( "CODEBLOCK                     ", Eval( bDouble, 21 ), 42 )
   Check( "OBJECT message                ", oGreeting:Text(), "Hello xDev" )
   Check( "PTR without a value is NIL    ", pNone == NIL, .T. )
   Check( "ANY changes its type          ", ValType( xAny ) + ValType( xAny := "a" ), "NC" )

RETURN

STATIC PROCEDURE s_Hashes()

   LOCAL hAny   AS ANY
   LOCAL hMap   AS HASH
   LOCAL hEmpty AS HASH := { => }
   LOCAL dKey   AS DATE := Date()
   LOCAL xValue
   LOCAL nCount AS INT  := 0

   hAny := { "accessToken" => { "validUntil" => "2026-11-01T12:00:00" } }
   Check( "ANY: nested character keys    ", hAny[ "accessToken" ][ "validUntil" ], "2026-11-01T12:00:00" )
   hMap := { "NIP" => "5260001246", 7 => "seven", dKey => "today" }
   Check( "HASH: character key           ", hMap[ "NIP" ], "5260001246" )
   Check( "HASH: numeric key             ", hMap[ 7 ], "seven" )
   Check( "HASH: date key                ", hMap[ dKey ], "today" )
   hMap[ "COMPANY" ] := { "ADDRESS" => { "CITY" => "Łódź" } }
   Check( "HASH: nested hashes           ", hMap[ "COMPANY" ][ "ADDRESS" ][ "CITY" ], "Łódź" )
   HSet( hMap, "X", 1 )
   Check( "HSet() and HGet()             ", HGet( hMap, "X" ), 1 )
   HDel( hMap, "X" )
   Check( "HDel() and HHasKey()          ", HHasKey( hMap, "X" ), .F. )
   Check( "Len() of an empty literal     ", Len( hEmpty ), 0 )
   FOR EACH xValue IN hMap
      nCount++
   NEXT
   Check( "FOR EACH over HASH            ", nCount, Len( hMap ) )
   Check( "detached HASH in a code block ", Eval( {|| hMap[ 7 ] } ), "seven" )
   s_hCache := hMap
   Check( "STATIC AS HASH                ", s_hCache[ "NIP" ], "5260001246" )
   xValue := hMap
   Check( "HASH -> ANY                   ", ValType( xValue ), "H" )
   hMap := hAny
   Check( "ANY -> HASH                   ", HHasKey( hMap, "accessToken" ), .T. )
   hMap := NIL
   Check( "NIL -> HASH                   ", hMap == NIL, .T. )
   xValue := "text"
   Check( "ANY with text -> HASH error   ", s_Fails( {|| hMap := xValue } ), .T. )

RETURN

STATIC PROCEDURE s_Functions()

   LOCAL hMap AS HASH := { "A" => 1 }
   LOCAL n    AS INT  := 1

   Check( "typed parameters and result   ", s_Sum( 2, 3 ), 5 )
   Check( "HASH result of a function     ", s_Make()[ "MADE" ], 1 )
   s_Replace( @hMap )
   Check( "@ reference to HASH parameter ", hMap[ "REPLACED" ], .T. )
   s_Increment( @n )
   Check( "@ reference to INT parameter  ", n, 2 )
   Check( "function without AS           ", s_Untyped(), "value" )

RETURN

STATIC FUNCTION s_Sum( a AS INT, b AS INT ) AS INT
RETURN a + b

STATIC FUNCTION s_Make() AS HASH
RETURN { "MADE" => 1 }

STATIC PROCEDURE s_Replace( hTarget AS HASH )

   hTarget := { "REPLACED" => .T. }

RETURN

STATIC PROCEDURE s_Increment( nValue AS INT )

   nValue := nValue + 1

RETURN

STATIC FUNCTION s_Untyped()

   LOCAL h := { "key" => "value" }

RETURN h[ "key" ]

STATIC FUNCTION s_Fails( bAction )

   LOCAL lFailed := .F.
   LOCAL bError  := ErrorBlock( {| oError | Break( oError ) } )

   BEGIN SEQUENCE
      Eval( bAction )
   RECOVER
      lFailed := .T.
   END SEQUENCE
   ErrorBlock( bError )

RETURN lFailed

STATIC PROCEDURE Check( cTitle, xResult, xExpected )

   QOut( cTitle + IIF( ValType( xResult ) == ValType( xExpected ) .AND. xResult == xExpected, "OK", "ERROR" ) )

RETURN

CREATE CLASS TypeGreeting

   DATA name AS STRING INIT ""

   METHOD New( cName )
   METHOD Text()

ENDCLASS

METHOD New( cName ) CLASS TypeGreeting

   ::name := cName

RETURN Self

METHOD Text() CLASS TypeGreeting
RETURN "Hello " + ::name
