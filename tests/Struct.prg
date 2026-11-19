/*
 * $DOC$
 * $NAME$
 * STRUCT
 *
 * $ONELINER$
 * Check native structure fields, fixed arrays, copies and DLL arguments.
 *
 * $DESCRIPTION$
 * The default program checks values and recoverable errors. On Windows it
 * also converts a known timestamp through both supported DLL pointer forms.
 * To check a compiler rejection, select one numbered STRUCT_INVALID case
 * at the end of this file. Each selected case is expected to fail to build.
 *
 * $EXAMPLES$
 *
 * $OUTPUT$
 *
 * $DOCEND$
 */

STRUCT ScalarFields
   small        AS INT8
   byteValue    AS UINT8
   shortValue   AS INT16
   wordValue    AS UINT16
   longValue    AS INT32
   unsignedLong AS UINT32
   wide         AS INT64
   unsignedWide AS UINT64
   integerValue AS INT
   singleValue  AS FLOAT32
   doubleValue  AS FLOAT64
   floatValue   AS FLOAT
   enabled      AS LOGIC
   address      AS PTR
ENDSTRUCT

STRUCT PointRecord
   x AS INT32
   y AS INT32
ENDSTRUCT

STRUCT TestPacket
   tag          AS UINT8
   origin       AS PointRecord
   points[ 2 ]  AS PointRecord
   samples[ 3 ] AS INT16
   tail         AS UINT32
ENDSTRUCT

STRUCT DottedNames
   t   AS INT32
   f   AS INT32
   n   AS INT32
   not AS INT32
   y   AS PointRecord
ENDSTRUCT

#if OS_WINDOWS
STRUCT SystemStamp
   year         AS UINT16
   month        AS UINT16
   weekDay      AS UINT16
   day          AS UINT16
   hour         AS UINT16
   minute       AS UINT16
   second       AS UINT16
   milliseconds AS UINT16
ENDSTRUCT

STRUCT FileStamp
   lowPart  AS UINT32
   highPart AS UINT32
ENDSTRUCT

DLL FUNCTION ToFileStamp( stamp AS SystemStamp, result AS FileStamp ) AS INT32 FROM SystemTimeToFileTime LIB "kernel32.dll"
DLL FUNCTION ToSystemStamp( stamp AS PTR, result AS PTR ) AS INT32 FROM FileTimeToSystemTime LIB "kernel32.dll"
#endif

PROCEDURE Main()

   s_Scalars()
   s_Packets()
   s_DottedNames()
   s_ErrorCases()
#if OS_WINDOWS
   s_NativeLayout()
#endif

RETURN

STATIC PROCEDURE s_Scalars()

   LOCAL value AS ScalarFields
   LOCAL pNull AS PTR

   Check( "INT8 default",    value.small, 0 )
   Check( "UINT8 default",   value.byteValue, 0 )
   Check( "INT16 default",   value.shortValue, 0 )
   Check( "UINT16 default",  value.wordValue, 0 )
   Check( "INT32 default",   value.longValue, 0 )
   Check( "UINT32 default",  value.unsignedLong, 0 )
   Check( "INT64 default",   value.wide, 0 )
   Check( "UINT64 default",  value.unsignedWide, 0 )
   Check( "INT default",     value.integerValue, 0 )
   Check( "FLOAT32 default", value.singleValue, 0 )
   Check( "FLOAT64 default", value.doubleValue, 0 )
   Check( "FLOAT default",   value.floatValue, 0 )
   Check( "LOGIC default",   value.enabled, .F. )
   Check( "PTR default",     value.address == NIL, .T. )

   value.small        := -128
   value.byteValue    := 255
   value.shortValue   := -32768
   value.wordValue    := 65535
   value.longValue    := -2147483648
   value.unsignedLong := 4294967295
   value.wide         := 9007199254740993
   value.unsignedWide := 18446744073709551615
   value.integerValue := 9007199254740993
   value.singleValue  := 1.25
   value.doubleValue  := 0.1
   value.floatValue   := 0.125
   value.enabled      := .T.
   value.address      := pNull

   Check( "INT8 field",     value.small, -128 )
   Check( "UINT8 field",    value.byteValue, 255 )
   Check( "INT16 field",    value.shortValue, -32768 )
   Check( "UINT16 field",   value.wordValue, 65535 )
   Check( "INT32 field",    value.longValue, -2147483648 )
   Check( "UINT32 field",   value.unsignedLong, 4294967295 )
   Check( "INT64 field",    value.wide, 9007199254740993 )
   Check( "UINT64 maximum", Str( value.unsignedWide, 20 ), "18446744073709551615" )
   Check( "INT field",      value.integerValue, 9007199254740993 )
   Check( "FLOAT32 field",  value.singleValue, 1.25 )
   Check( "FLOAT64 field",  value.doubleValue, 0.1 )
   Check( "FLOAT field",    value.floatValue, 0.125 )
   Check( "LOGIC field",    value.enabled, .T. )
   Check( "PTR field",      value.address == NIL, .T. )
   value.singleValue := 0.1
   Check( "FLOAT32 rounds on store", value.singleValue == value.doubleValue, .F. )

RETURN

STATIC PROCEDURE s_Packets()

   LOCAL packet   AS TestPacket
   LOCAL snapshot AS TestPacket := packet
   LOCAL point    AS PointRecord
   LOCAL n        AS INT

   Check( "nested array initially zero", snapshot.points[ 2 ].y, 0 )
   packet.tag := 17
   packet.tail := 4000000000
   point.x := 10
   point.y := 20
   packet.origin := point

   FOR n := 1 TO 2
      packet.points[ n ] := point
      packet.points[ n ].x += n
   NEXT

   FOR n := 1 TO 3
      packet.samples[ n ] := n * 4
   NEXT

   snapshot := packet
   packet.origin.y := -1
   packet.points[ 2 ].x := 99
   packet.samples[ 2 ] += 3

   Check( "copied nested field",            snapshot.origin.y, 20 )
   Check( "first structure element",        packet.points[ 1 ].x, 11 )
   Check( "copied last structure element",  snapshot.points[ 2 ].x, 12 )
   Check( "updated last structure element", packet.points[ 2 ].x, 99 )
   Check( "separate source record",         point.x, 10 )
   Check( "first scalar element",           packet.samples[ 1 ], 4 )
   Check( "updated middle element",         packet.samples[ 2 ], 11 )
   Check( "copied middle element",          snapshot.samples[ 2 ], 8 )
   Check( "last scalar element",            packet.samples[ 3 ], 12 )
   Check( "field name ignores case",        packet.TAG, 17 )
   Check( "trailing field unchanged",       packet.tail, 4000000000 )

RETURN

STATIC PROCEDURE s_DottedNames()

   LOCAL value AS DottedNames

   value.t   := 1
   value.f   := 2
   value.n   := 3
   value.not := 4
   value.y.x := 5

   Check( "field named T", value.t, 1 )
   Check( "field named F", value.f, 2 )
   Check( "field named N", value.n, 3 )
   Check( "field named NOT", value.not, 4 )
   Check( "nested field after Y", value.y.x, 5 )
   Check( ".AND. after a field", value.y.x > value.t .AND. value.f < value.n, .T. )

RETURN

STATIC PROCEDURE s_ErrorCases()

   LOCAL bOld := ErrorBlock( {| oError | Break( oError ) } )
   LOCAL nIndex
   LOCAL oError
   LOCAL nCode

   FOR EACH nIndex IN { -1, 0, 4 }
      nCode := 0

      BEGIN SEQUENCE
         s_ReadAt( nIndex )
      RECOVER USING oError
         nCode := oError:SubCode
      END SEQUENCE

      Check( "Out-of-range read " + LTrim( Str( nIndex ) ), nCode, 3105 )
      nCode := 0

      BEGIN SEQUENCE
         s_WriteAt( nIndex )
      RECOVER USING oError
         nCode := oError:SubCode
      END SEQUENCE

      Check( "Out-of-range write " + LTrim( Str( nIndex ) ), nCode, 3105 )
   NEXT

   FOR EACH nIndex IN { -1, 256 }
      nCode := 0

      BEGIN SEQUENCE
         s_SetByte( nIndex )
      RECOVER USING oError
         nCode := oError:SubCode
      END SEQUENCE

      Check( "UINT8 field range " + LTrim( Str( nIndex ) ), nCode, 3105 )
   NEXT

   FOR EACH nIndex IN { -129, 128 }
      nCode := 0

      BEGIN SEQUENCE
         s_SetSmall( nIndex )
      RECOVER USING oError
         nCode := oError:SubCode
      END SEQUENCE

      Check( "INT8 field range " + LTrim( Str( nIndex ) ), nCode, 3105 )
   NEXT
   ErrorBlock( bOld )

RETURN

STATIC PROCEDURE s_SetByte( n AS INT )

   LOCAL packet AS TestPacket

   packet.tag := n

RETURN

STATIC PROCEDURE s_SetSmall( n AS INT )

   LOCAL value AS ScalarFields

   value.small := n

RETURN

STATIC PROCEDURE s_ReadAt( n AS INT )

   LOCAL packet AS TestPacket
   Check( "Unexpected in-range read", packet.samples[ n ], 0 )

RETURN

STATIC PROCEDURE s_WriteAt( n AS INT )

   LOCAL packet AS TestPacket

   packet.samples[ n ] := 1

RETURN

#if OS_WINDOWS
STATIC PROCEDURE s_NativeLayout()

   LOCAL stamp AS SystemStamp
   LOCAL ticks AS FileStamp
   LOCAL restored AS SystemStamp
   LOCAL nStatus

   // One day and one millisecond after the native epoch.
   stamp.year := 1601
   stamp.month := 1
   stamp.day := 2
   stamp.milliseconds := 1
   nStatus := ToFileStamp( @stamp, @ticks )
   Check( "Named STRUCT DLL arguments", nStatus != 0, .T. )
   Check( "Native low word", ticks.lowPart, 711583504 )
   Check( "Native high word", ticks.highPart, 201 )
   nStatus := ToSystemStamp( @ticks, @restored )
   Check( "PTR DLL arguments", nStatus != 0, .T. )
   Check( "Round trip year", restored.year, 1601 )
   Check( "Round trip month", restored.month, 1 )
   Check( "Round trip day", restored.day, 2 )
   Check( "Round trip milliseconds", restored.milliseconds, 1 )

RETURN
#endif

STATIC PROCEDURE Check( cTitle, xResult, xExpected )

   QOut( PadR( cTitle, 34 ) + IIF( ValType( xResult ) == ValType( xExpected ) .AND. xResult == xExpected, "OK", "ERROR" ) )

RETURN

// Select one rejection case with -DSTRUCT_INVALID=N.
#if STRUCT_INVALID == 1
// duplicate type
STRUCT Repeated
   value AS INT
ENDSTRUCT
STRUCT Repeated
   other AS INT
ENDSTRUCT

#elif STRUCT_INVALID == 2
// empty declaration
STRUCT EmptyRecord
ENDSTRUCT

#elif STRUCT_INVALID == 3
// duplicate field ignoring case
STRUCT DuplicateField
   value AS INT
   VALUE AS INT
ENDSTRUCT

#elif STRUCT_INVALID == 4
// missing field type
STRUCT UntypedRecord
   value
ENDSTRUCT

#elif STRUCT_INVALID == 5
// unsupported STRING field
STRUCT BadMember
   value AS STRING
ENDSTRUCT

#elif STRUCT_INVALID == 6
// unsupported DATE field
STRUCT BadMember
   value AS DATE
ENDSTRUCT

#elif STRUCT_INVALID == 7
// unsupported ARRAY field
STRUCT BadMember
   value AS ARRAY
ENDSTRUCT

#elif STRUCT_INVALID == 8
// unsupported HASH field
STRUCT BadMember
   value AS HASH
ENDSTRUCT

#elif STRUCT_INVALID == 9
// unsupported CODEBLOCK field
STRUCT BadMember
   value AS CODEBLOCK
ENDSTRUCT

#elif STRUCT_INVALID == 10
// unsupported OBJECT field
STRUCT BadMember
   value AS OBJECT
ENDSTRUCT

#elif STRUCT_INVALID == 11
// unsupported ANY field
STRUCT BadMember
   value AS ANY
ENDSTRUCT

#elif STRUCT_INVALID == 12
// unsupported PSZ field
STRUCT BadMember
   value AS PSZ
ENDSTRUCT

#elif STRUCT_INVALID == 13
// invalid field dimension 0
STRUCT BadSize
   value[0] AS INT
ENDSTRUCT

#elif STRUCT_INVALID == 14
// invalid field dimension -1
STRUCT BadSize
   value[-1] AS INT
ENDSTRUCT

#elif STRUCT_INVALID == 15
// invalid field dimension 1.5
STRUCT BadSize
   value[1.5] AS INT
ENDSTRUCT

#elif STRUCT_INVALID == 16
// invalid field dimension nCount
STRUCT BadSize
   value[nCount] AS INT
ENDSTRUCT

#elif STRUCT_INVALID == 17
// different named types
STATIC PROCEDURE InvalidCase()
   LOCAL a AS PointRecord
   LOCAL b AS TestPacket
   a := b
   RETURN

#elif STRUCT_INVALID == 18
// STRUCT assigned to ANY
STATIC PROCEDURE InvalidCase()
   LOCAL a AS PointRecord
   LOCAL b AS ANY
   b := a
   RETURN

#elif STRUCT_INVALID == 19
// ANY assigned to STRUCT
STATIC PROCEDURE InvalidCase()
   LOCAL a AS PointRecord
   LOCAL b AS ANY
   a := b
   RETURN

#elif STRUCT_INVALID == 20
// structure in array
STATIC PROCEDURE InvalidCase()
   LOCAL a AS PointRecord
   LOCAL b := { a }
   RETURN

#elif STRUCT_INVALID == 21
// unknown field
STATIC PROCEDURE InvalidCase()
   LOCAL a AS PointRecord
   a.missing := 1
   RETURN

#elif STRUCT_INVALID == 22
// message instead of field
STATIC PROCEDURE InvalidCase()
   LOCAL a AS PointRecord
   QOut( a:x )
   RETURN

#elif STRUCT_INVALID == 23
// whole fixed array assignment
STATIC PROCEDURE InvalidCase()
   LOCAL a AS TestPacket
   a.samples := { 1, 2, 3 }
   RETURN

#elif STRUCT_INVALID == 24
// arithmetic on structures
STATIC PROCEDURE InvalidCase()
   LOCAL a AS PointRecord
   QOut( a + a )
   RETURN

#elif STRUCT_INVALID == 25
// static structure
STATIC PROCEDURE InvalidCase()
   STATIC a AS PointRecord
   RETURN

#elif STRUCT_INVALID == 26
// structure result
STATIC FUNCTION InvalidResult() AS PointRecord
   LOCAL a AS PointRecord
   RETURN a

#elif STRUCT_INVALID == 27
// structure parameter by value
STATIC PROCEDURE InvalidParameter( a AS PointRecord )
   RETURN

#elif STRUCT_INVALID == 28
// structure in IIF
STATIC PROCEDURE InvalidCase()
   LOCAL a AS PointRecord
   LOCAL b AS PointRecord
   a := IIF( .T., a, b )
   RETURN

#elif STRUCT_INVALID == 29
// cast to structure
STATIC PROCEDURE InvalidCase()
   LOCAL a AS PointRecord
   a := CAST( 1 AS PointRecord )
   RETURN

#elif STRUCT_INVALID == 30
// captured structure
STATIC PROCEDURE InvalidCase()
   LOCAL a AS PointRecord
   LOCAL b := {|| a.x }
   QOut( Eval( b ) )
   RETURN

#elif STRUCT_INVALID == 31
// unknown field type
STRUCT BadMember
   value AS UnknownRecord
ENDSTRUCT

#elif STRUCT_INVALID == 32
// missing ENDSTRUCT
STRUCT MissingEnd
   value AS INT

#elif STRUCT_INVALID == 33
// class data of structure type
CREATE CLASS BadOwner
   DATA value AS PointRecord
ENDCLASS

#elif STRUCT_INVALID == 34
// incompatible typed DLL pointer
#if OS_WINDOWS

STATIC PROCEDURE BadPointer()
   LOCAL a AS PointRecord
   LOCAL b AS FileStamp
   ToFileStamp( @a, @b )
   RETURN

#else
#error This case requires the Windows DLL declaration
#endif

#elif STRUCT_INVALID == 35
// field of its own structure type
STRUCT Node
   value AS INT
   child AS Node
ENDSTRUCT

#elif STRUCT_INVALID == 36
// field of a structure declared later
STRUCT EarlyRecord
   later AS LateRecord
ENDSTRUCT
STRUCT LateRecord
   value AS INT
ENDSTRUCT

#elif STRUCT_INVALID == 37
// field passed with @ to a DLL
#if OS_WINDOWS
STATIC PROCEDURE BadPointer()
   LOCAL a AS FileStamp
   LOCAL b AS SystemStamp
   ToSystemStamp( @a.lowPart, @b )
   RETURN
#else
#error This case requires the Windows DLL declaration
#endif

#elif STRUCT_INVALID == 38
// different structure types passed by value to a DLL
#if OS_WINDOWS
STATIC PROCEDURE BadValue()
   LOCAL a AS FileStamp
   LOCAL b AS SystemStamp
   ToFileStamp( a, b )
   RETURN
#else
#error This case requires the Windows DLL declaration
#endif

#elif STRUCT_INVALID == 39
// different structure type assigned to a field
STATIC PROCEDURE InvalidCase()
   LOCAL a AS TestPacket
   LOCAL b AS ScalarFields
   a.origin := b
   RETURN

#elif STRUCT_INVALID == 40
// different structure type assigned to an array element
STATIC PROCEDURE InvalidCase()
   LOCAL a AS TestPacket
   LOCAL b AS ScalarFields
   a.points[ 1 ] := b
   RETURN

#elif STRUCT_INVALID == 41
// field named like the .AND. operator
STRUCT BadName
   and AS INT
ENDSTRUCT
#endif
