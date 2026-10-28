/*
 *
 */

#include "mytiff.ch"

#if OS_LINUX
   REQUEST GTXWC
#else
   REQUEST GTWVT
#endif

PROCEDURE Main()

   LOCAL app, shell, label, button

   app    := MtAppInitialize( "Hello" )
   shell  := MtCreateShell( app, "main", { { MtNtitle, "Hello, xDev" }, { MtNwidth, 360 }, { MtNheight, 180 } } )
   label  := MtCreateLabel( shell, "text", { { MtNlabelString, "Hello, xDev" }, { MtNx, 20 }, { MtNy, 20 } } )
   button := MtCreatePushButton( shell, "quit", { { MtNlabelString, "Quit" }, { MtNx, 20 }, { MtNy, 60 } } )

   MtAddCallback( button, MtNactivateCallback, {| w, aCallData | MtAppQuit( app ) } )
   MtRealizeWidget( shell )
   MtAppMainLoop( app )

RETURN
