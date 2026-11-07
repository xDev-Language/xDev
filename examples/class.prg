/*
 * $DOC$
 * $NAME$
 * Greeting class
 *
 * $ONELINER$
 * Demonstrates a class with a constructor and instance methods.
 *
 * $DESCRIPTION$
 * Defines Greeting, whose New() method sets its name and whose Show() method displays a greeting.
 *
 * $EXAMPLES$
 * examples/class.prg
 *
 * $OUTPUT$
 * Hello from xDev!
 *
 * $DOCEND$
 */

PROCEDURE Main()

   LOCAL oGreeting := Greeting():New( "xDev" )

   oGreeting:Show()

   RETURN

CREATE CLASS Greeting

   DATA name AS STRING INIT ""

   METHOD New( cName )
   METHOD Show()

ENDCLASS

METHOD New( cName ) CLASS Greeting

   ::name := cName

   RETURN Self

METHOD Show() CLASS Greeting

   QOut( "Hello from " + ::name + "!" )

   RETURN Self
