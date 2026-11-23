/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev Tests
 * File......: DllClass.prg
 *
 */

/*
 * $DOC$
 * $NAME$
 * DLL module with a class
 *
 * $ONELINER$
 * A module built with --shared that defines a class, used by the test DllLoad.prg.
 *
 * $DESCRIPTION$
 * ModPoint() returns an object of the class DllPoint. Once the class is registered, ModuleUnload() refuses to free the module, because the methods of the class are code of the DLL. Build it with xdev --shared DllClass.prg.
 *
 * $EXAMPLES$
 *
 * $OUTPUT$
 *
 * $DOCEND$
 */

CREATE CLASS DllPoint

   DATA x INIT 0
   DATA y INIT 0

   METHOD New( nX, nY )
   METHOD Sum()

ENDCLASS

METHOD New( nX, nY ) CLASS DllPoint

   ::x := nX
   ::y := nY

RETURN Self

METHOD Sum() CLASS DllPoint
RETURN ::x + ::y

// An object of the class of the module
FUNCTION ModPoint( nX, nY )
RETURN DllPoint():New( nX, nY )


