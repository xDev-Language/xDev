/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev Tests
 * File......: Class.prg
 *
 */

/*
 * $DOC$
 * $NAME$
 * Class
 *
 * $ONELINER$
 * Tests classes: DATA, CLASSDATA, methods, inheritance, accessors, operators, ON ERROR, DELEGATE and scopes.
 *
 * $DESCRIPTION$
 * Exercises CREATE CLASS with typed and untyped DATA, a DATE DATA without INIT, CLASSDATA shared by all objects, methods that return Self, typed methods called on typed and untyped receivers, a typed receiver that holds an object of a subclass, inheritance with Super:, ACCESS and ASSIGN, OPERATOR + and ==, ON ERROR, DELEGATE, and HIDDEN, PROTECTED and READONLY members. The classes come before Main(), so typed LOCAL variables can name them.
 *
 * $EXAMPLES$
 *
 * $OUTPUT$
 * class function gives an object      OK
 * New() sets a DATA                   OK
 * INT DATA starts at 0                OK
 * STRING DATA starts empty            OK
 * LOGIC DATA starts .F.               OK
 * untyped DATA starts NIL             OK
 * objects keep their own DATA         OK
 * methods return Self                 OK
 * CLASSDATA is shared                 OK
 * typed DATA rejects text             OK
 * unknown message is an error         OK
 * DATE DATA without INIT is empty     OK
 * DATE DATA without INIT is a date    OK
 * typed receiver: empty DATE DATA     OK
 * method sets a DATE DATA             OK
 * DATE arithmetic in a method         OK
 * typed receiver sets a DATE DATA     OK
 * typed method, typed receiver        OK
 * typed method, untyped receiver      OK
 * typed receiver: method chain        OK
 * method in a loop                    OK
 * typed receiver with a child object  OK
 * child inherits DATA                 OK
 * child overrides a method            OK
 * child keeps parent methods          OK
 * child DATA with INIT                OK
 * child: empty DATE DATA              OK
 * ACCESS computes a value             OK
 * ASSIGN runs a method                OK
 * ACCESS after ASSIGN                 OK
 * OPERATOR + gives a new object       OK
 * OPERATOR + keeps the operands       OK
 * OPERATOR == compares objects        OK
 * ON ERROR gets the message           OK
 * ON ERROR runs as a method           OK
 * DELEGATE sends to a DATA            OK
 * HIDDEN DATA inside the class        OK
 * HIDDEN DATA from outside fails      OK
 * PROTECTED DATA in a subclass        OK
 * PROTECTED DATA from outside fails   OK
 * READONLY DATA can be read           OK
 * READONLY DATA from outside fails    OK
 * READONLY DATA inside the class      OK
 *
 * $DOCEND$
 */

CREATE CLASS TestCounter

   DATA value AS INT
   DATA label AS STRING
   DATA active AS LOGIC
   DATA born AS DATE
   DATA note
   CLASSDATA instances INIT 0

   METHOD New( nStart )
   METHOD Add( nStep )
   METHOD Twice( n )
   METHOD SetBorn( dDate )
   METHOD BirthDay()
   METHOD Age( dToday )

ENDCLASS

CREATE CLASS TestChild FROM TestCounter

   DATA extra AS INT INIT 100

   METHOD New( nStart )
   METHOD Add( nStep )
   METHOD Total()

ENDCLASS

CREATE CLASS TestRect

   DATA width AS INT INIT 0
   DATA height AS INT INIT 0

   METHOD New( nWidth, nHeight )
   ACCESS area
   ASSIGN area( nArea )

ENDCLASS

CREATE CLASS TestMoney

   DATA amount AS INT INIT 0

   METHOD New( nAmount )
   METHOD Plus( oOther ) OPERATOR +
   METHOD Same( oOther ) OPERATOR ==

ENDCLASS

CREATE CLASS TestProxy

   DATA calls AS INT INIT 0

   ON ERROR Missing( cMessage, aArgs )

ENDCLASS

CREATE CLASS TestHolder

   DATA counter

   METHOD New()
   DELEGATE Twice TO counter

ENDCLASS

CREATE CLASS TestSafe

   DATA fixed INIT 7 READONLY

   METHOD Reveal()
   METHOD Bump()

HIDDEN:

   DATA secret INIT "hidden"

PROTECTED:

   DATA guarded INIT "protected"

ENDCLASS

CREATE CLASS TestVault FROM TestSafe

   METHOD ShowGuarded()

ENDCLASS

PROCEDURE Main()

   s_Basics()
   s_Dates()
   s_DirectCalls()
   s_Inheritance()
   s_Accessors()
   s_Operators()
   s_Messages()
   s_Scopes()

RETURN

STATIC PROCEDURE s_Basics()

   LOCAL oA := TestCounter():New( 5 )
   LOCAL oB := TestCounter():New()
   LOCAL xAny
   LOCAL nCount

   s_Check( "class function gives an object      ", ValType( oA ), "O" )
   s_Check( "New() sets a DATA                   ", oA:value, 5 )
   s_Check( "INT DATA starts at 0                ", oB:value, 0 )
   s_Check( "STRING DATA starts empty            ", oB:label, "" )
   s_Check( "LOGIC DATA starts .F.               ", oB:active, .F. )
   s_Check( "untyped DATA starts NIL             ", oB:note, NIL )
   s_Check( "objects keep their own DATA         ", oA:value + oB:value, 5 )
   s_Check( "methods return Self                 ", oB:Add( 2 ):Add( 3 ):value, 5 )
   nCount := oA:instances
   TestCounter():New()
   s_Check( "CLASSDATA is shared                 ", oB:instances, nCount + 1 )
   xAny := oA
   s_Check( "typed DATA rejects text             ", s_Fails( {|| xAny:value := "text" } ), .T. )
   s_Check( "unknown message is an error         ", s_Fails( {|| xAny:Missing() } ), .T. )

RETURN

STATIC PROCEDURE s_Dates()

   LOCAL oA := TestCounter():New( 1 )
   LOCAL oTyped AS TestCounter := TestCounter():New( 2 )
   LOCAL dToday := Date()

   s_Check( "DATE DATA without INIT is empty     ", Empty( oA:born ), .T. )
   s_Check( "DATE DATA without INIT is a date    ", ValType( oA:born ), "D" )
   s_Check( "typed receiver: empty DATE DATA     ", Empty( oTyped:born ), .T. )
   s_Check( "method sets a DATE DATA             ", oA:SetBorn( dToday ):BirthDay(), dToday )
   s_Check( "DATE arithmetic in a method         ", oA:Age( dToday + 10 ), 10 )
   oTyped:born := dToday
   s_Check( "typed receiver sets a DATE DATA     ", oTyped:BirthDay(), dToday )

RETURN

STATIC PROCEDURE s_DirectCalls()

   LOCAL oTyped AS TestCounter := TestCounter():New( 0 )
   LOCAL oSum := TestCounter():New( 0 )
   LOCAL oBase AS TestCounter
   LOCAL xAny := TestCounter():New( 0 )
   LOCAL i

   s_Check( "typed method, typed receiver        ", oTyped:Twice( 21 ), 42 )
   s_Check( "typed method, untyped receiver      ", xAny:Twice( 21 ), 42 )
   s_Check( "typed receiver: method chain        ", oTyped:Add( 1 ):Add( 2 ):value, 3 )
   FOR i := 1 TO 10
      oSum:Add( i )
   NEXT
   s_Check( "method in a loop                    ", oSum:value, 55 )
   xAny := TestChild():New( 1 )
   oBase := xAny
   s_Check( "typed receiver with a child object  ", oBase:Add( 1 ):value, 11 )

RETURN

STATIC PROCEDURE s_Inheritance()

   LOCAL xChild := TestChild():New( 1 )

   s_Check( "child inherits DATA                 ", xChild:value, 1 )
   s_Check( "child overrides a method            ", xChild:Add( 2 ):value, 21 )
   s_Check( "child keeps parent methods          ", xChild:Twice( 4 ), 8 )
   s_Check( "child DATA with INIT                ", xChild:Total(), 121 )
   s_Check( "child: empty DATE DATA              ", Empty( xChild:born ), .T. )

RETURN

STATIC PROCEDURE s_Accessors()

   LOCAL xRect := TestRect():New( 2, 3 )

   s_Check( "ACCESS computes a value             ", xRect:area, 6 )
   xRect:area := 10
   s_Check( "ASSIGN runs a method                ", xRect:width, 10 )
   s_Check( "ACCESS after ASSIGN                 ", xRect:area, 10 )

RETURN

STATIC PROCEDURE s_Operators()

   LOCAL xLeft := TestMoney():New( 2 )
   LOCAL xRight := TestMoney():New( 3 )
   LOCAL xSum

   xSum := xLeft + xRight
   s_Check( "OPERATOR + gives a new object       ", xSum:amount, 5 )
   s_Check( "OPERATOR + keeps the operands       ", xLeft:amount, 2 )
   s_Check( "OPERATOR == compares objects        ", xLeft == TestMoney():New( 2 ), .T. )

RETURN

STATIC PROCEDURE s_Messages()

   LOCAL xProxy := TestProxy()
   LOCAL xHolder := TestHolder():New()

   s_Check( "ON ERROR gets the message           ", xProxy:Hello( 1, 2 ), "HELLO:2" )
   s_Check( "ON ERROR runs as a method           ", xProxy:calls, 1 )
   s_Check( "DELEGATE sends to a DATA            ", xHolder:Twice( 5 ), 10 )

RETURN

STATIC PROCEDURE s_Scopes()

   LOCAL xSafe := TestSafe()
   LOCAL xVault := TestVault()

   s_Check( "HIDDEN DATA inside the class        ", xSafe:Reveal(), "hidden" )
   s_Check( "HIDDEN DATA from outside fails      ", s_Fails( {|| xSafe:secret } ), .T. )
   s_Check( "PROTECTED DATA in a subclass        ", xVault:ShowGuarded(), "protected" )
   s_Check( "PROTECTED DATA from outside fails   ", s_Fails( {|| xVault:guarded } ), .T. )
   s_Check( "READONLY DATA can be read           ", xSafe:fixed, 7 )
   s_Check( "READONLY DATA from outside fails    ", s_Fails( {|| xSafe:fixed := 8 } ), .T. )
   s_Check( "READONLY DATA inside the class      ", xSafe:Bump():fixed, 8 )

RETURN

// Run the block and tell whether it raised a runtime error
STATIC FUNCTION s_Fails( bAction )

   LOCAL lFailed := .F.
   LOCAL bError  := ErrorBlock( {| oError | Break( oError ) } )

   BEGIN SEQUENCE
      Eval( bAction )
   RECOVER
      lFailed := .T.
   ENDSEQUENCE
   ErrorBlock( bError )

RETURN lFailed

STATIC PROCEDURE s_Check( cTitle, xResult, xExpected )

   QOut( cTitle + IIF( ValType( xResult ) == ValType( xExpected ) .AND. xResult == xExpected, "OK", "ERROR" ) )

RETURN

METHOD New( nStart ) CLASS TestCounter

   IF nStart != NIL
      ::value := nStart
   ENDIF
   ::instances++

RETURN Self

METHOD Add( nStep ) CLASS TestCounter

   ::value += nStep

RETURN Self

METHOD Twice( n AS INT ) AS INT CLASS TestCounter
RETURN n * 2

METHOD SetBorn( dDate ) CLASS TestCounter

   ::born := dDate

RETURN Self

METHOD BirthDay() CLASS TestCounter
RETURN ::born

METHOD Age( dToday ) CLASS TestCounter
RETURN dToday - ::born

METHOD New( nStart ) CLASS TestChild

   Super:New( nStart )

RETURN Self

// The child adds ten times the step
METHOD Add( nStep ) CLASS TestChild

   Super:Add( nStep * 10 )

RETURN Self

METHOD Total() CLASS TestChild
RETURN ::value + ::extra

METHOD New( nWidth, nHeight ) CLASS TestRect

   ::width := nWidth
   ::height := nHeight

RETURN Self

ACCESS area CLASS TestRect
RETURN ::width * ::height

ASSIGN area( nArea ) CLASS TestRect

   ::width := nArea
   ::height := 1

RETURN nArea

METHOD New( nAmount ) CLASS TestMoney

   ::amount := nAmount

RETURN Self

METHOD Plus( oOther ) CLASS TestMoney
RETURN TestMoney():New( ::amount + oOther:amount )

METHOD Same( oOther ) CLASS TestMoney
RETURN ::amount == oOther:amount

// Any message the class does not know: its name and the number of its arguments
METHOD Missing( cMessage, aArgs ) CLASS TestProxy

   ::calls++

RETURN cMessage + ":" + LTrim( Str( Len( aArgs ) ) )

METHOD New() CLASS TestHolder

   ::counter := TestCounter():New( 0 )

RETURN Self

METHOD Reveal() CLASS TestSafe
RETURN ::secret

METHOD Bump() CLASS TestSafe

   ::fixed++

RETURN Self

METHOD ShowGuarded() CLASS TestVault
RETURN ::guarded


