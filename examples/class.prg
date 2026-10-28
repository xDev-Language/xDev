/*
 *
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
