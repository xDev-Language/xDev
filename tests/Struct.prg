/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev Tests
 * File......: Struct.prg
 *
 */

/*
 * $DOC$
 * $NAME$
 * STRUCT
 *
 * $ONELINER$
 * Check structure fields, fixed arrays, shared values, SClone() copies, function results and DLL arguments.
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

STRUCT AppSettings
   year    AS INT32
   rate    AS FLOAT64
   enabled AS LOGIC
   origin  AS PointRecord
ENDSTRUCT

STRUCT TextRecord
   year       AS INT32
   firm[ 16 ] AS CHAR
   letter     AS CHAR
ENDSTRUCT

STRUCT Person
   name AS STRING
   city AS STRING
   age  AS INT32
ENDSTRUCT

STRUCT Team
   leader       AS Person
   members[ 3 ] AS Person
   title        AS STRING
ENDSTRUCT

STATIC s_stSettings AS AppSettings
STATIC s_stDefaults AS AppSettings
STATIC s_stText     AS TextRecord
STATIC s_stPerson   AS Person

PROCEDURE Main()

   s_Scalars()
   s_Packets()
   s_DottedNames()
   s_Results()
   s_Statics()
   s_Parameters()
   s_Texts()
   s_Strings()
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
   LOCAL snapshot AS TestPacket := SClone( packet )
   LOCAL point    AS PointRecord
   LOCAL n        AS INT

   Check( "nested array initially zero", snapshot.points[ 2 ].y, 0 )
   packet.tag := 17
   packet.tail := 4000000000
   point.x := 10
   point.y := 20
   packet.origin.x := point.x
   packet.origin.y := point.y

   FOR n := 1 TO 2
      packet.points[ n ].x := point.x + n
      packet.points[ n ].y := point.y
   NEXT

   FOR n := 1 TO 3
      packet.samples[ n ] := n * 4
   NEXT

   snapshot := SClone( packet )
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

STATIC PROCEDURE s_Results()

   LOCAL point    AS PointRecord := s_MakePoint( 1, 2 )
   LOCAL other    AS PointRecord
   LOCAL packet   AS TestPacket
   LOCAL bOld     := ErrorBlock( {| oError | Break( oError ) } )
   LOCAL xValue   := 5
   LOCAL xText    := "five"
   LOCAL oError
   LOCAL nCode    := 0

   Check( "result in LOCAL initializer",   point.x + point.y, 3 )
   other := s_MakePoint( 30, 40 )
   Check( "result assigned",               other.y, 40 )
   other := s_MakePoint( other.y, other.x )
   Check( "result from its own fields",    other.x * 100 + other.y, 4030 )
   packet := s_MakePacket( 7, 10 )
   Check( "large result scalar field",     packet.tag, 7 )
   Check( "large result nested field",     packet.origin.y, 11 )
   Check( "large result array element",    packet.points[ 2 ].x, 12 )
   Check( "large result fixed array",      packet.samples[ 3 ], 30 )
   Check( "large result trailing field",   packet.tail, 4000000000 )
   other := s_MakePoint( -1, -2 )
   packet.origin.y := other.y
   Check( "result field set in a field",   packet.origin.y, -2 )
   other := s_MakePoint( 8, 9 )
   packet.points[ 1 ].x := other.x
   Check( "result field set in an element", packet.points[ 1 ].x, 8 )
   s_MakePoint( 5, 6 )
   Check( "unused result",                 point.x, 1 )
   other := s_MakePoint( xValue, -2 )
   Check( "result with ANY arguments",     other.x + other.y, 3 )

   BEGIN SEQUENCE
      other := s_MakePoint( xText, 1 )
   RECOVER USING oError
      nCode := oError:SubCode
   ENDSEQUENCE
   Check( "ANY argument of a wrong type",  nCode != 0, .T. )
   Check( "result kept after the error",   other.x, 5 )
   ErrorBlock( bOld )

RETURN

STATIC FUNCTION s_MakePoint( nX AS INT32, nY AS INT32 ) AS PointRecord

   LOCAL point AS PointRecord

   point.x := nX
   point.y := nY

RETURN point

STATIC FUNCTION s_MakePacket( nTag AS UINT8, nBase AS INT32 ) AS TestPacket

   LOCAL packet AS TestPacket
   LOCAL n      AS INT

   packet.tag := nTag
   packet.origin.x := nBase
   packet.origin.y := nBase + 1
   FOR n := 1 TO 2
      packet.points[ n ].x := nBase + n
   NEXT

   FOR n := 1 TO 3
      packet.samples[ n ] := n * nBase
   NEXT

   packet.tail := 4000000000

RETURN packet

STATIC PROCEDURE s_Statics()

   LOCAL bYear := {|| s_stSettings.year }
   LOCAL copy  AS AppSettings

   Check( "STATIC STRUCT initially zero",     s_stSettings.year, 0 )
   s_stSettings.year := 2027
   s_stSettings.rate := 0.23
   s_stSettings.enabled := .T.
   s_stSettings.origin.y := 7
   Check( "STATIC STRUCT seen by a function", s_SettingsYear(), 2027 )
   s_SetSettingsYear( 2028 )
   Check( "STATIC STRUCT set by a function",  s_stSettings.year, 2028 )
   Check( "STATIC STRUCT in a code block",    Eval( bYear ), 2028 )
   Check( "STATIC STRUCT nested field",       s_stSettings.origin.y, 7 )
   copy := SClone( s_stSettings )
   copy.year := 1
   Check( "SClone of STATIC STRUCT is its own", s_stSettings.year, 2028 )
   s_stDefaults := s_MakeSettings( 2026 )
   Check( "STATIC STRUCT from a function",    s_stDefaults.year, 2026 )
   Check( "STATIC STRUCT default rate",       s_stDefaults.rate, 0.23 )
   Check( "function STATIC STRUCT first",     s_Counter(), 1 )
   Check( "function STATIC STRUCT kept",      s_Counter(), 2 )

RETURN

STATIC FUNCTION s_MakeSettings( nYear AS INT32 ) AS AppSettings

   LOCAL settings AS AppSettings

   settings.year := nYear
   settings.rate := 0.23
   settings.enabled := .T.

RETURN settings

STATIC FUNCTION s_SettingsYear() AS INT32

RETURN s_stSettings.year

STATIC PROCEDURE s_SetSettingsYear( nYear AS INT32 )

   s_stSettings.year := nYear

RETURN

STATIC FUNCTION s_Counter() AS INT

   STATIC counter AS PointRecord

   counter.x += 1

RETURN counter.x

STATIC PROCEDURE s_Parameters()

   LOCAL point  AS PointRecord := s_MakePoint( 3, 4 )
   LOCAL moved  AS PointRecord
   LOCAL packet AS TestPacket

   Check( "STRUCT parameter",                 s_PointSum( point ), 7 )
   moved := s_MovePoint( point, 10 )
   Check( "callee changes the shared STRUCT", point.x, 13 )
   Check( "shared STRUCT returned",           moved.x, 13 )
   Check( "result passed as a parameter",     s_PointSum( s_MakePoint( 20, 30 ) ), 50 )
   packet.origin.x := 5
   packet.origin.y := 6
   Check( "SClone of a nested field",         s_PointSum( SClone( packet.origin ) ), 11 )
   Check( "STATIC STRUCT as a parameter",     s_SettingsRate( s_stDefaults ), 0.23 )

RETURN

STATIC FUNCTION s_PointSum( point AS PointRecord ) AS INT

RETURN point.x + point.y

STATIC FUNCTION s_MovePoint( point AS PointRecord, nBy AS INT32 ) AS PointRecord

   point.x += nBy

RETURN point

STATIC FUNCTION s_SettingsRate( settings AS AppSettings ) AS FLOAT64

RETURN settings.rate

STATIC PROCEDURE s_Texts()

   LOCAL rec   AS TextRecord
   LOCAL copy  AS TextRecord
   LOCAL bOld  := ErrorBlock( {| oError | Break( oError ) } )
   LOCAL oError
   LOCAL nCode

   Check( "CHAR text initially empty",        rec.firm, "" )
   rec.firm := "Biuro"
   Check( "CHAR text stored",                 rec.firm, "Biuro" )
   Check( "CHAR text length in bytes",        Len( rec.firm ), 5 )
   Check( "CHAR element is a byte",           rec.firm[ 1 ], 66 )
   rec.firm[ 1 ] := 98
   Check( "CHAR element changed",             rec.firm, "biuro" )
   Check( "CHAR text comparison",             rec.firm == "biuro", .T. )
   Check( "CHAR text joined",                 rec.firm + "!", "biuro!" )
   rec.firm := "Łódź"
   Check( "CHAR text in UTF-8",               Len( rec.firm ), 7 )
   rec.firm := Replicate( "x", 15 )
   Check( "CHAR text fills N - 1 bytes",      Len( rec.firm ), 15 )
   copy := SClone( rec )
   copy.firm := "kopia"
   Check( "SClone has its own text",          rec.firm, Replicate( "x", 15 ) )
   Check( "copied text",                      copy.firm, "kopia" )
   Check( "CHAR text through a parameter",    s_TextOf( copy ), "kopia" )
   rec.letter := 65
   Check( "single CHAR is a byte",            rec.letter, 65 )
   s_stText.firm := "Biuro Kowalska"
   Check( "STATIC STRUCT CHAR text",          s_stText.firm, "Biuro Kowalska" )
   rec.firm := ""
   Check( "CHAR text emptied",                rec.firm, "" )

   rec.firm := "ok"
   nCode := 0
   BEGIN SEQUENCE
      rec.firm := Replicate( "x", 16 )
   RECOVER USING oError
      nCode := oError:SubCode
   ENDSEQUENCE
   Check( "CHAR text too long is an error",   nCode, 3105 )
   Check( "CHAR text kept after the error",   rec.firm, "ok" )

   nCode := 0
   BEGIN SEQUENCE
      rec.firm := "a" + Chr( 0 ) + "b"
   RECOVER USING oError
      nCode := oError:SubCode
   ENDSEQUENCE
   Check( "CHAR text with zero byte: error",  nCode, 3105 )
   ErrorBlock( bOld )

RETURN

STATIC FUNCTION s_TextOf( rec AS TextRecord ) AS STRING

RETURN rec.firm

STATIC PROCEDURE s_Strings()

   LOCAL person AS Person
   LOCAL copy   AS Person
   LOCAL team   AS Team
   LOCAL other  AS Team
   LOCAL n      AS INT

   Check( "STRING field initially empty",     person.name, "" )
   person.name := "Anna"
   person.city := "Kraków"
   person.age := 30
   Check( "STRING field stored",              person.name + " " + person.city, "Anna Kraków" )
   person.name := Replicate( "x", 1000 )
   Check( "STRING field of any length",       Len( person.name ), 1000 )
   person.name := "Anna"
   person.name += "!"
   Check( "STRING field +=",                  person.name, "Anna!" )
   copy := SClone( person )
   copy.name := "Ewa"
   Check( "SClone has its own STRING",        person.name, "Anna!" )
   Check( "copied STRING",                    copy.name + copy.city, "EwaKraków" )
   person := person
   Check( "STRUCT assigned to itself",        person.name, "Anna!" )
   Check( "STRING field as a parameter",      s_PersonName( person ), "Anna!" )
   s_RenamePerson( person )
   Check( "callee changes the shared STRUCT", person.name, "Inna" )
   person.name := "Anna!"
   copy := s_MakePerson( "Jan", 40 )
   Check( "STRING field of a result",         copy.name, "Jan" )
   s_MakePerson( "unused", 1 )
   FOR n := 1 TO 100
      copy := s_MakePerson( "N" + LTrim( Str( n ) ), n )
   NEXT
   Check( "results in a loop",                copy.name, "N100" )
   Check( "result as a parameter",            s_PersonName( s_MakePerson( "Ola", 5 ) ), "Ola" )

   team.title := "Biuro"
   team.leader.name := person.name
   team.members[ 2 ].name := copy.name
   team.members[ 3 ].name := "Piotr"
   other := SClone( team )
   other.leader.name := "Zmiana"
   other.members[ 2 ].name := "Zmiana"
   Check( "nested STRING field",              team.leader.name, "Anna!" )
   Check( "STRING in an array element",       team.members[ 2 ].name, "N100" )
   Check( "STRING set in an element",         team.members[ 3 ].name, "Piotr" )
   Check( "copied nested STRING",             other.leader.name + other.title, "ZmianaBiuro" )

   s_stPerson.name := "Statyczna"
   Check( "STATIC STRUCT STRING field",       s_StaticName(), "Statyczna" )

RETURN

STATIC FUNCTION s_PersonName( person AS Person ) AS STRING

RETURN person.name

STATIC PROCEDURE s_RenamePerson( person AS Person )

   person.name := "Inna"

RETURN

STATIC FUNCTION s_MakePerson( cName AS STRING, nAge AS INT32 ) AS Person

   LOCAL person AS Person

   person.name := cName
   person.age := nAge

RETURN person

STATIC FUNCTION s_StaticName() AS STRING

RETURN s_stPerson.name

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
      ENDSEQUENCE

      Check( "Out-of-range read " + LTrim( Str( nIndex ) ), nCode, 3105 )
      nCode := 0

      BEGIN SEQUENCE
         s_WriteAt( nIndex )
      RECOVER USING oError
         nCode := oError:SubCode
      ENDSEQUENCE

      Check( "Out-of-range write " + LTrim( Str( nIndex ) ), nCode, 3105 )
   NEXT

   FOR EACH nIndex IN { -1, 256 }
      nCode := 0

      BEGIN SEQUENCE
         s_SetByte( nIndex )
      RECOVER USING oError
         nCode := oError:SubCode
      ENDSEQUENCE

      Check( "UINT8 field range " + LTrim( Str( nIndex ) ), nCode, 3105 )
   NEXT

   FOR EACH nIndex IN { -129, 128 }
      nCode := 0

      BEGIN SEQUENCE
         s_SetSmall( nIndex )
      RECOVER USING oError
         nCode := oError:SubCode
      ENDSEQUENCE

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
// static structure with an initializer that is not a constant
STATIC PROCEDURE InvalidCase()
   STATIC a AS PointRecord := s_MakePoint( 1, 2 )
   RETURN

#elif STRUCT_INVALID == 29
// cast to structure
STATIC PROCEDURE InvalidCase()
   LOCAL a AS PointRecord
   a := CAST( 1 AS PointRecord )
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
// structures passed to a DLL without @
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
// whole nested structure field assigned
STATIC PROCEDURE InvalidCase()
   LOCAL a AS TestPacket
   LOCAL b AS ScalarFields
   a.origin := b
   RETURN

#elif STRUCT_INVALID == 40
// whole structure element of an array field assigned
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

#elif STRUCT_INVALID == 42
// result of an unknown structure type
STATIC FUNCTION InvalidResult() AS UnknownRecord
   RETURN NIL

#elif STRUCT_INVALID == 43
// result of a different structure type
STATIC FUNCTION InvalidResult() AS PointRecord
   LOCAL a AS TestPacket
   RETURN a

#elif STRUCT_INVALID == 44
// structure result assigned to a different structure type
STATIC PROCEDURE InvalidCase()
   LOCAL a AS TestPacket
   a := s_MakePoint( 1, 2 )
   RETURN

#elif STRUCT_INVALID == 47
// structure result of a DLL function
#if OS_WINDOWS
DLL FUNCTION BadStamp() AS FileStamp FROM GetTickCount LIB "kernel32.dll"
#else
#error This case requires a Windows DLL
#endif

#elif STRUCT_INVALID == 49
// different structure type as a parameter
STATIC PROCEDURE InvalidCase()
   LOCAL a AS TestPacket
   QOut( s_PointSum( a ) )
   RETURN

#elif STRUCT_INVALID == 52
// number assigned to a CHAR text field
STATIC PROCEDURE InvalidCase()
   LOCAL a AS TextRecord
   a.firm := 1
   RETURN

#elif STRUCT_INVALID == 53
// CHAR outside a STRUCT field
STATIC PROCEDURE InvalidCase()
   LOCAL a AS CHAR
   RETURN

#elif STRUCT_INVALID == 54
// text assigned to a single CHAR byte
STATIC PROCEDURE InvalidCase()
   LOCAL a AS TextRecord
   a.letter := "A"
   RETURN

#elif STRUCT_INVALID == 55
// structure with a STRING field as a DLL parameter
#if OS_WINDOWS
DLL FUNCTION BadPerson( person AS Person ) AS INT32 FROM GetTickCount LIB "kernel32.dll"
#else
#error This case requires a Windows DLL
#endif

#elif STRUCT_INVALID == 56
// structure with a STRING field passed by @ to a DLL
#if OS_WINDOWS
STATIC PROCEDURE InvalidCase()
   LOCAL a AS Person
   LOCAL b AS SystemStamp
   ToSystemStamp( @a, @b )
   RETURN
#else
#error This case requires the Windows DLL declaration
#endif

#elif STRUCT_INVALID == 57
// number assigned to a STRING field
STATIC PROCEDURE InvalidCase()
   LOCAL a AS Person
   a.name := 1
   RETURN
#endif


