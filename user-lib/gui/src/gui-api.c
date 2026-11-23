/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev GUI
 * File......: gui-api.c
 *
 */

#include "internal-gui.h"

/* ---------------------------------------------------------------------------------------------
   guiWindow
   --------------------------------------------------------------------------------------------- */

// Create a hidden window with a client area of nWidth x nHeight logical pixels (default 640 x 480)
// guiWindow():New( [cTitle], [nWidth], [nHeight] ) --> self
X_FUNC_STATIC( GUIWINDOW_NEW )
{
   x_guiWindowNew( ctx );
}

// Show the window
// oWindow:Show() --> self
X_FUNC_STATIC( GUIWINDOW_SHOW )
{
   HWND window = x_guiSelfHandle( ctx, "GUIWINDOW:SHOW" );

   if( window != NULL )
   {
      x_guiShow( window, 1 );
      x_guiReturnSelf( ctx );
   }
}

// Hide the window without closing it
// oWindow:Hide() --> self
X_FUNC_STATIC( GUIWINDOW_HIDE )
{
   HWND window = x_guiSelfHandle( ctx, "GUIWINDOW:HIDE" );

   if( window != NULL )
   {
      x_guiShow( window, 0 );
      x_guiReturnSelf( ctx );
   }
}

// Close the window and its controls without calling OnClose()
// oWindow:Close() --> NIL
X_FUNC_STATIC( GUIWINDOW_CLOSE )
{
   const X_ITEM *handle = x_objDataSlot( ctx, 1 );
   HWND window = IIF( handle != NULL, ( HWND )x_itemGetPtr( handle ), NULL );

   if( window != NULL && IsWindow( window ) )
      x_guiDestroy( window );
}

// Called when the user closes the window; return .F. to keep it open
// oWindow:OnClose() --> lClose
X_FUNC_STATIC( GUIWINDOW_ONCLOSE )
{
   x_retl( ctx, 1 );
}

// Called after the client area changed its size (logical pixels)
// oWindow:OnResize( nWidth, nHeight ) --> NIL
X_FUNC_STATIC( GUIWINDOW_ONRESIZE )
{
}

// Called for a key pressed while the window has the focus (Windows virtual key code)
// oWindow:OnKey( nKey ) --> NIL
X_FUNC_STATIC( GUIWINDOW_ONKEY )
{
}

// Called for a menu command; runs the action of guiMenu:Add() with the window as its parameter
// oWindow:OnMenu( nId ) --> NIL
X_FUNC_STATIC( GUIWINDOW_ONMENU )
{
   x_guiWindowRunMenu( ctx, x_parni( ctx, 1 ) );
}

// Make the commands with bWhen of a menu that opens active or grey (internal)
// X_GUI_EVENT_MENUWHEN of oWindow( nMenu ) --> NIL
X_FUNC_STATIC( GUIWINDOW_MENUWHEN )
{
   x_guiWindowMenuWhen( ctx, ( HMENU )( intptr_t )x_parnint( ctx, 1 ) );
}

// Return the menu bar of the window
// oWindow:menu --> oMenu
X_FUNC_STATIC( GUIWINDOW_GETMENU )
{
   const X_ITEM *menu = x_objDataSlot( ctx, 2 );

   if( menu != NULL )
      x_itemReturn( ctx, menu );
}

// Set the menu bar; the window grows so that the client area keeps its size
// oWindow:menu := oMenu
X_FUNC_STATIC( GUIWINDOW_SETMENU )
{
   const X_ITEM *value = x_paritem( ctx, 1 );
   X_ITEM *slot = x_objDataSlot( ctx, 2 );
   HWND window = x_guiSelfHandle( ctx, "GUIWINDOW:MENU" );
   HMENU menu;

   if( window == NULL || slot == NULL || ( menu = x_guiObjectMenu( ctx, value, "GUIWINDOW:MENU" ) ) == NULL )
      return;

   x_guiWindowSetMenu( ctx, window, menu );
   x_itemCopy( slot, value );
   x_itemReturn( ctx, value );
}

// Return the title of the window
// oWindow:title --> cTitle
X_FUNC_STATIC( GUIWINDOW_GETTITLE )
{
   HWND window = x_guiSelfHandle( ctx, "GUIWINDOW:TITLE" );

   if( window != NULL )
      x_guiReturnWindowText( ctx, window );
}

// Set the title of the window
// oWindow:title := cTitle
X_FUNC_STATIC( GUIWINDOW_SETTITLE )
{
   HWND window = x_guiSelfHandle( ctx, "GUIWINDOW:TITLE" );

   if( window != NULL )
   {
      x_guiSetText( ctx, window, x_paritem( ctx, 1 ) );
      x_itemReturn( ctx, x_paritem( ctx, 1 ) );
   }
}

// Return .T. when the user may drag the size of the window
// oWindow:sizable --> lSizable
X_FUNC_STATIC( GUIWINDOW_GETSIZABLE )
{
   HWND window = x_guiSelfHandle( ctx, "GUIWINDOW:SIZABLE" );

   if( window != NULL )
      x_retl( ctx, x_guiWindowSizable( window ) );
}

// Let the user drag the size of the window and show it on the full screen; the controls follow their anchors
// oWindow:sizable := lSizable
X_FUNC_STATIC( GUIWINDOW_SETSIZABLE )
{
   HWND window = x_guiSelfHandle( ctx, "GUIWINDOW:SIZABLE" );

   if( window == NULL )
      return;

   if( !X_ISLOG( ctx, 1 ) )
   {
      x_guiRaise( ctx, "GUIWINDOW:SIZABLE" );
      return;
   }

   x_guiWindowSetSizable( window, x_parl( ctx, 1 ) );
   x_itemReturn( ctx, x_paritem( ctx, 1 ) );
}

// Return the width of the client area (logical pixels)
// oWindow:width --> nWidth
X_FUNC_STATIC( GUIWINDOW_GETWIDTH )
{
   HWND window = x_guiSelfHandle( ctx, "GUIWINDOW:WIDTH" );

   if( window != NULL )
      x_retni( ctx, x_guiClientSize( window, X_GUI_WIDTH ) );
}

// Set the width of the client area (logical pixels)
// oWindow:width := nWidth
X_FUNC_STATIC( GUIWINDOW_SETWIDTH )
{
   HWND window = x_guiSelfHandle( ctx, "GUIWINDOW:WIDTH" );

   if( window == NULL )
      return;

   if( !X_ISNUM( ctx, 1 ) )
   {
      x_guiRaise( ctx, "GUIWINDOW:WIDTH" );
      return;
   }

   x_guiSetClientSize( ctx, window, X_GUI_WIDTH, x_parni( ctx, 1 ) );
   x_retni( ctx, x_parni( ctx, 1 ) );
}

// Return the height of the client area (logical pixels)
// oWindow:height --> nHeight
X_FUNC_STATIC( GUIWINDOW_GETHEIGHT )
{
   HWND window = x_guiSelfHandle( ctx, "GUIWINDOW:HEIGHT" );

   if( window != NULL )
      x_retni( ctx, x_guiClientSize( window, X_GUI_HEIGHT ) );
}

// Set the height of the client area (logical pixels)
// oWindow:height := nHeight
X_FUNC_STATIC( GUIWINDOW_SETHEIGHT )
{
   HWND window = x_guiSelfHandle( ctx, "GUIWINDOW:HEIGHT" );

   if( window == NULL )
      return;

   if( !X_ISNUM( ctx, 1 ) )
   {
      x_guiRaise( ctx, "GUIWINDOW:HEIGHT" );
      return;
   }

   x_guiSetClientSize( ctx, window, X_GUI_HEIGHT, x_parni( ctx, 1 ) );
   x_retni( ctx, x_parni( ctx, 1 ) );
}

// Return the menu shown when the user right-clicks the window, NIL when it has none
// oWindow:contextMenu --> oMenu
X_FUNC_STATIC( GUIWINDOW_GETCONTEXTMENU )
{
   const X_ITEM *menu = x_objDataSlot( ctx, 3 );

   if( menu != NULL )
      x_itemReturn( ctx, menu );
}

// Set the context menu, a guiMenu():New( .T. ); controls without their own one show it too; NIL removes it
// oWindow:contextMenu := oMenu
X_FUNC_STATIC( GUIWINDOW_SETCONTEXTMENU )
{
   x_guiSetContextMenu( ctx, 3, "GUIWINDOW:CONTEXTMENU" );
}

// Center the window over its owner, or on the screen when it has none
// oWindow:Center() --> self
X_FUNC_STATIC( GUIWINDOW_CENTER )
{
   HWND window = x_guiSelfHandle( ctx, "GUIWINDOW:CENTER" );

   if( window != NULL )
   {
      x_guiCenter( window );
      x_guiReturnSelf( ctx );
   }
}

// Set the icon of the title bar and of the taskbar from the .ico file cFile; without it the window has
// the icon of the program
// oWindow:SetIcon( cFile ) --> self
X_FUNC_STATIC( GUIWINDOW_SETICON )
{
   HWND window = x_guiSelfHandle( ctx, "GUIWINDOW:SETICON" );

   if( window != NULL )
   {
      x_guiWindowSetIcon( ctx, window, x_paritem( ctx, 1 ) );
      x_guiReturnSelf( ctx );
   }
}

// Return the color of the window (0xRRGGBB), NIL for the system color
// oWindow:background --> nColor
X_FUNC_STATIC( GUIWINDOW_GETBACKGROUND )
{
   x_guiWindowColor( ctx, X_GUI_COLOR_BACKGROUND );
}

// Paint the window and its labels, frames and check boxes with nColor; NIL returns to the system color
// oWindow:background := nColor
X_FUNC_STATIC( GUIWINDOW_SETBACKGROUND )
{
   x_guiWindowSetColor( ctx, X_GUI_COLOR_BACKGROUND );
}

// Return the text color of labels, frames and check boxes, NIL for the system color
// oWindow:foreground --> nColor
X_FUNC_STATIC( GUIWINDOW_GETFOREGROUND )
{
   x_guiWindowColor( ctx, X_GUI_COLOR_FOREGROUND );
}

// Set the text color of labels, frames and check boxes; edit fields keep their own colors
// oWindow:foreground := nColor
X_FUNC_STATIC( GUIWINDOW_SETFOREGROUND )
{
   x_guiWindowSetColor( ctx, X_GUI_COLOR_FOREGROUND );
}

// Create the Windows window for the object (internal, called by New() of guiWindow and guiDialog)
// X_GUI_EVENT_CREATE of oWindow( oOwner, cTitle, nWidth, nHeight, lDialog ) --> self
X_FUNC_STATIC( GUIWINDOW_CREATE )
{
   x_guiWindowCreate( ctx );
}

// Release the window handle and the menu bar after the window was destroyed (internal)
// X_GUI_EVENT_DETACH of oWindow() --> NIL
X_FUNC_STATIC( GUIWINDOW_DETACH )
{
   x_guiWindowDetach( ctx );
}

// Run bAction, which receives the window, when cKey is pressed in the window or in one of its controls,
// as SetKey(); cKey is written as in guiMenu ("PgDn", "Esc", "Enter", "Ctrl+S"); NIL removes
// the key; it comes before the shortcuts of the menu and of the toolbars; returns the previous block
// oWindow:SetKey( cKey, [bAction] ) --> bPrevious
X_FUNC_STATIC( GUIWINDOW_SETKEY )
{
   HWND window = x_guiSelfHandle( ctx, "GUIWINDOW:SETKEY" );

   if( window != NULL )
      x_guiWindowSetKey( ctx, window, x_objDataSlot( ctx, 4 ) );
}

// Move the focus to the next control in the order of Tab, with lPrevious to the previous one
// oWindow:FocusNext( [lPrevious] ) --> self
X_FUNC_STATIC( GUIWINDOW_FOCUSNEXT )
{
   HWND window = x_guiSelfHandle( ctx, "GUIWINDOW:FOCUSNEXT" );

   if( window != NULL )
   {
      x_guiWindowFocusNext( window, x_parldef( ctx, 1, 0 ) );
      x_guiReturnSelf( ctx );
   }
}

// Let the user fill the edit fields of the window, as ReadModal(): they are editable only during
// ReadModal(), tables, toolbars and the menu wait; Enter goes to the next field, Enter on the last field and
// PgDn or PgUp end with .T. after OnValid of the field, Esc ends with .F.; the program waits for the end;
// LastKey() gives then K_ESC, K_PGDN, K_PGUP or K_ENTER; with GetList, an array of edit fields, check boxes and
// radio buttons as GetList of ReadModal(), only these fields take part, in this order (Space changes a check box
// or a radio button, Enter goes on); nGet is the field of GetList the editing starts in,
// without it the first field seen
// oWindow:ReadModal( [GetList], [nGet] ) --> lSaved
X_FUNC_STATIC( GUIWINDOW_READMODAL )
{
   HWND window = x_guiSelfHandle( ctx, "GUIWINDOW:READMODAL" );
   const X_ITEM *list = x_paritem( ctx, 1 );
   HWND *gets = NULL;
   size_t count = 0, i;
   int saved;

   if( window == NULL )
      return;

   if( list != NULL && !X_IS_NIL( list ) )
   {
      if( !X_IS_ARRAY( list ) )
      {
         x_guiRaise( ctx, "GUIWINDOW:READMODAL" );
         return;
      }

      count = x_arrayLen( list );
      gets = ( HWND * )x_xgrabArray( count + 1, sizeof( HWND ) );
      for( i = 0; i < count; ++i )
      {
         if( ( gets[i] = x_guiObjectControl( ctx, x_arrayGetItemPtr( list, i + 1 ), "GUIWINDOW:READMODAL" ) ) == NULL )
         {
            x_xfree( gets );
            return;
         }
      }
   }

   x_callbackEnterDll();
   saved = x_guiWindowRead( window, gets, count, IIF( X_ISNUM( ctx, 2 ), x_parni( ctx, 2 ), 0 ) );
   x_callbackLeaveDll();
   x_callbackRaisePending();
   x_xfree( gets );
   x_retl( ctx, saved );
}

// End the running ReadModal() as PgDn ( lSave .T., default ) or Esc ( .F. ), e.g. from the buttons Save and Cancel
// oWindow:EndRead( [lSave] ) --> self
X_FUNC_STATIC( GUIWINDOW_ENDREAD )
{
   if( !X_ISNIL( ctx, 1 ) && !X_ISLOG( ctx, 1 ) )
   {
      x_guiRaise( ctx, "GUIWINDOW:ENDREAD" );
      return;
   }

   x_guiWindowEndRead( IIF( X_ISLOG( ctx, 1 ), x_parl( ctx, 1 ), 1 ) );
   x_guiReturnSelf( ctx );
}

// A control of the window, also on its pages and panels, by the :name given to it: oWindow:NIP is the control
// with :name "NIP"; a message the window does not know otherwise raises the usual error
// oWindow:<cName> --> oControl
X_FUNC_STATIC( GUIWINDOW_ONERROR )
{
   X_ITEM message __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM arguments __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   HWND window;

   if( !x_objOnErrorInfo( ctx, ctx->result, &message, &arguments ) )
      return;

   window = x_guiSelfHandle( ctx, "GUIWINDOW:ONERROR" );
   if( window == NULL )
      return;

   if( !x_guiWindowControl( ctx, window, x_itemGetCPtr( &message ) ) )
      x_errorRaiseNamed( ctx->result, 1004, x_itemGetCPtr( &message ) );
}

// Run the block of key nKey of SetKey() (internal)
// X_GUI_EVENT_KEY of oWindow( nKey ) --> NIL
X_FUNC_STATIC( GUIWINDOW_KEY )
{
   const X_ITEM *blocks = x_objDataSlot( ctx, 4 );
   X_ITEM block __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ARG args[1];

   if( blocks == NULL || !X_IS_ARRAY( blocks ) || x_parni( ctx, 1 ) < 1 )
      return;

   if( !x_arrayGet( blocks, ( size_t )x_parni( ctx, 1 ), &block ) || !X_IS_BLOCK( &block ) )
      return;

   args[0] = ( X_ARG ){ ctx->self, 0 };
   x_evalBlock( ctx, &result, &block, 1, args );
}

// Top-level window; window events are methods overridden in a subclass
// guiWindow() --> oWindow
X_FUNC( GUIWINDOW )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 4, "GUIWINDOW" );

      // Instance variables
      x_clsAddData( s_class, "HANDLE", 1 );
      x_clsAddData( s_class, "MENUBAR", 2 );
      x_clsAddData( s_class, "POPUPMENU", 3 );
      x_clsAddData( s_class, "KEYBLOCKS", 4 );

      x_clsSetScope( s_class, "HANDLE",       X_OO_CLSTP_EXPORTED | X_OO_CLSTP_READONLY );
      x_clsSetScope( s_class, "MENUBAR",      X_OO_CLSTP_HIDDEN );
      x_clsSetScope( s_class, "POPUPMENU",    X_OO_CLSTP_HIDDEN );
      x_clsSetScope( s_class, "KEYBLOCKS",    X_OO_CLSTP_HIDDEN );

      // Methods
      x_clsAdd( s_class, "NEW",               X_FUNCNAME( GUIWINDOW_NEW ) );

      x_clsAdd( s_class, "SETKEY",            X_FUNCNAME( GUIWINDOW_SETKEY ) );
      x_clsAdd( s_class, "FOCUSNEXT",         X_FUNCNAME( GUIWINDOW_FOCUSNEXT ) );
      x_clsAdd( s_class, "READMODAL",         X_FUNCNAME( GUIWINDOW_READMODAL ) );
      x_clsAdd( s_class, "ENDREAD",           X_FUNCNAME( GUIWINDOW_ENDREAD ) );
      x_clsAdd( s_class, "SHOW",              X_FUNCNAME( GUIWINDOW_SHOW ) );
      x_clsAdd( s_class, "HIDE",              X_FUNCNAME( GUIWINDOW_HIDE ) );
      x_clsAdd( s_class, "CLOSE",             X_FUNCNAME( GUIWINDOW_CLOSE ) );
      x_clsAdd( s_class, "CENTER",            X_FUNCNAME( GUIWINDOW_CENTER ) );
      x_clsAdd( s_class, "SETICON",           X_FUNCNAME( GUIWINDOW_SETICON ) );
      x_clsAdd( s_class, "ONCLOSE",           X_FUNCNAME( GUIWINDOW_ONCLOSE ) );
      x_clsAdd( s_class, "ONRESIZE",          X_FUNCNAME( GUIWINDOW_ONRESIZE ) );
      x_clsAdd( s_class, "ONKEY",             X_FUNCNAME( GUIWINDOW_ONKEY ) );
      x_clsAdd( s_class, "ONMENU",            X_FUNCNAME( GUIWINDOW_ONMENU ) );

      // ACCESS and ASSIGN methods
      x_clsAddAccess( s_class, "MENU",        X_FUNCNAME( GUIWINDOW_GETMENU ) );
      x_clsAddAssign( s_class, "MENU",        X_FUNCNAME( GUIWINDOW_SETMENU ) );
      x_clsAddAccess( s_class, "CONTEXTMENU", X_FUNCNAME( GUIWINDOW_GETCONTEXTMENU ) );
      x_clsAddAssign( s_class, "CONTEXTMENU", X_FUNCNAME( GUIWINDOW_SETCONTEXTMENU ) );
      x_clsAddAccess( s_class, "TITLE",       X_FUNCNAME( GUIWINDOW_GETTITLE ) );
      x_clsAddAssign( s_class, "TITLE",       X_FUNCNAME( GUIWINDOW_SETTITLE ) );
      x_clsAddAccess( s_class, "SIZABLE",     X_FUNCNAME( GUIWINDOW_GETSIZABLE ) );
      x_clsAddAssign( s_class, "SIZABLE",     X_FUNCNAME( GUIWINDOW_SETSIZABLE ) );
      x_clsAddAccess( s_class, "WIDTH",       X_FUNCNAME( GUIWINDOW_GETWIDTH ) );
      x_clsAddAssign( s_class, "WIDTH",       X_FUNCNAME( GUIWINDOW_SETWIDTH ) );
      x_clsAddAccess( s_class, "HEIGHT",      X_FUNCNAME( GUIWINDOW_GETHEIGHT ) );
      x_clsAddAssign( s_class, "HEIGHT",      X_FUNCNAME( GUIWINDOW_SETHEIGHT ) );
      x_clsAddAccess( s_class, "BACKGROUND",  X_FUNCNAME( GUIWINDOW_GETBACKGROUND ) );
      x_clsAddAssign( s_class, "BACKGROUND",  X_FUNCNAME( GUIWINDOW_SETBACKGROUND ) );
      x_clsAddAccess( s_class, "FOREGROUND",  X_FUNCNAME( GUIWINDOW_GETFOREGROUND ) );
      x_clsAddAssign( s_class, "FOREGROUND",  X_FUNCNAME( GUIWINDOW_SETFOREGROUND ) );

      // ON ERROR method
      x_clsAddOnError( s_class,               X_FUNCNAME( GUIWINDOW_ONERROR ) );

      // Library events, not methods of the class
      x_guiHandlerAdd( s_class, X_GUI_EVENT_KEY, X_FUNCNAME( GUIWINDOW_KEY ) );
      x_guiHandlerAdd( s_class, X_GUI_EVENT_MENUWHEN, X_FUNCNAME( GUIWINDOW_MENUWHEN ) );
      x_guiHandlerAdd( s_class, X_GUI_EVENT_CREATE, X_FUNCNAME( GUIWINDOW_CREATE ) );
      x_guiHandlerAdd( s_class, X_GUI_EVENT_DETACH, X_FUNCNAME( GUIWINDOW_DETACH ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   guiPanel
   --------------------------------------------------------------------------------------------- */

// Create a panel for the controls of one part of a window, e.g. a module of the program; without nWidth
// and nHeight it fills the window between its toolbars and the status bar and follows its size;
// Close() removes the panel with all its controls
// guiPanel():New( oWindow, [nX], [nY], [nWidth], [nHeight] ) --> self
X_FUNC_STATIC( GUIPANEL_NEW )
{
   x_guiPanelNew( ctx );
}

// Return the edges of the window the panel is attached to
// oPanel:anchor --> cAnchor
X_FUNC_STATIC( GUIPANEL_GETANCHOR )
{
   HWND window = x_guiSelfHandle( ctx, "GUIPANEL:ANCHOR" );

   if( window != NULL )
      x_guiGetAnchor( ctx, window );
}

// Attach a panel with its own size to edges of the window: L, T, R, B; opposite edges stretch it
// oPanel:anchor := cAnchor
X_FUNC_STATIC( GUIPANEL_SETANCHOR )
{
   HWND window = x_guiSelfHandle( ctx, "GUIPANEL:ANCHOR" );

   if( window != NULL )
   {
      x_guiSetAnchor( ctx, window, x_paritem( ctx, 1 ) );
      x_itemReturn( ctx, x_paritem( ctx, 1 ) );
   }
}

// Child window holding controls; it has the colors, keys and focus order of a window
// guiPanel() --> oPanel
X_FUNC( GUIPANEL )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 0, "GUIPANEL" );

      // Parent class
      x_clsAddParent( s_class, x_clsRequire( "GUIWINDOW", GUIWINDOW ) );

      // Methods
      x_clsAdd( s_class, "NEW",          X_FUNCNAME( GUIPANEL_NEW ) );

      // ACCESS and ASSIGN methods
      x_clsAddAccess( s_class, "ANCHOR", X_FUNCNAME( GUIPANEL_GETANCHOR ) );
      x_clsAddAssign( s_class, "ANCHOR", X_FUNCNAME( GUIPANEL_SETANCHOR ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   guiDialog
   --------------------------------------------------------------------------------------------- */

// Create a hidden dialog window centered over its owner (default size 400 x 300)
// guiDialog():New( [oOwner], [cTitle], [nWidth], [nHeight] ) --> self
X_FUNC_STATIC( GUIDIALOG_NEW )
{
   x_guiDialogNew( ctx );
}

// Show the dialog, disable its owner and wait until the dialog is closed
// oDialog:ShowModal() --> nResult
X_FUNC_STATIC( GUIDIALOG_SHOWMODAL )
{
   X_ITEM *result = x_objDataSlot( ctx, 1 );
   HWND window = x_guiSelfHandle( ctx, "GUIDIALOG:SHOWMODAL" );

   if( window == NULL || result == NULL )
      return;

   x_itemPutNI( result, 0 );
   x_guiDialogRun( ctx, window );
   x_itemReturn( ctx, result );
}

// Close the dialog; ShowModal() returns nResult
// oDialog:EndDialog( nResult ) --> NIL
X_FUNC_STATIC( GUIDIALOG_ENDDIALOG )
{
   X_ITEM *result = x_objDataSlot( ctx, 1 );
   HWND window = x_guiSelfHandle( ctx, "GUIDIALOG:ENDDIALOG" );

   if( window == NULL || result == NULL )
      return;

   if( !X_ISNUM( ctx, 1 ) )
   {
      x_guiRaise( ctx, "GUIDIALOG:ENDDIALOG" );
      return;
   }

   x_itemPutNI( result, x_parni( ctx, 1 ) );
   x_guiDestroy( window );
}

// Return the result of the dialog; 0 when the user closed it without EndDialog()
// oDialog:result --> nResult
X_FUNC_STATIC( GUIDIALOG_RESULT )
{
   const X_ITEM *result = x_objDataSlot( ctx, 1 );

   if( result != NULL )
      x_retni( ctx, x_itemGetNI( result ) );
}

// Modal dialog window; members as in guiWindow
// guiDialog() --> oDialog
X_FUNC( GUIDIALOG )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 1, "GUIDIALOG" );

      // Parent class
      x_clsAddParent( s_class, x_clsRequire( "GUIWINDOW", GUIWINDOW ) );

      // Instance variables
      x_clsAddData( s_class, "DIALOGRESULT", 1 );
      x_clsSetScope( s_class, "DIALOGRESULT", X_OO_CLSTP_HIDDEN );

      // Methods
      x_clsAdd( s_class, "NEW",               X_FUNCNAME( GUIDIALOG_NEW ) );
      x_clsAdd( s_class, "SHOWMODAL",         X_FUNCNAME( GUIDIALOG_SHOWMODAL ) );
      x_clsAdd( s_class, "ENDDIALOG",         X_FUNCNAME( GUIDIALOG_ENDDIALOG ) );

      // ACCESS and ASSIGN methods
      x_clsAddAccess( s_class, "RESULT",      X_FUNCNAME( GUIDIALOG_RESULT ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   guiMenu
   --------------------------------------------------------------------------------------------- */

// Create an empty menu bar, or with lPopup .T. an empty context menu
// guiMenu():New( [lPopup] ) --> self
X_FUNC_STATIC( GUIMENU_NEW )
{
   X_ITEM *slot = x_objDataSlot( ctx, 1 );
   HMENU menu;

   if( !X_ISNIL( ctx, 1 ) && !X_ISLOG( ctx, 1 ) )
   {
      x_guiRaise( ctx, "GUIMENU:NEW" );
      return;
   }

   menu = x_guiMenuCreate( ctx, X_ISLOG( ctx, 1 ) && x_parl( ctx, 1 ) );

   if( slot != NULL && menu != NULL )
   {
      x_itemPutPtr( slot, menu );
      x_guiReturnSelf( ctx );
   }
}

// Add a drop-down menu with the given caption; cBitmap is a BMP picture of the item
// oMenu:AddMenu( cText, [cBitmap] ) --> oSubmenu
X_FUNC_STATIC( GUIMENU_ADDMENU )
{
   HMENU menu = x_guiObjectMenu( ctx, ctx->self, "GUIMENU:ADDMENU" );

   if( menu != NULL )
      x_guiMenuAddSubmenu( ctx, menu, x_paritem( ctx, 1 ), x_paritem( ctx, 2 ) );
}

// Add a command; the action runs with the window as its parameter; cShortcut such as "Ctrl+S" or "Shift+F5";
// cBitmap is a BMP picture shown at the left of the text; bWhen runs each time the menu opens and before the
// command runs: .F. makes the command grey and it cannot be chosen
// oMenu:Add( cText, [bAction], [cShortcut], [cBitmap], [bWhen] ) --> nId
X_FUNC_STATIC( GUIMENU_ADD )
{
   HMENU menu = x_guiObjectMenu( ctx, ctx->self, "GUIMENU:ADD" );
   const X_ITEM *image = x_paritem( ctx, 4 );
   const X_ITEM *when = x_paritem( ctx, 5 );
   int id;

   if( menu == NULL )
      return;

   id = x_guiMenuAdd( ctx, menu, x_paritem( ctx, 1 ), x_paritem( ctx, 2 ), x_paritem( ctx, 3 ), image, when );
   if( id != 0 )
      x_retni( ctx, id );
}

// Add a separator line
// oMenu:AddSeparator() --> self
X_FUNC_STATIC( GUIMENU_ADDSEPARATOR )
{
   HMENU menu = x_guiObjectMenu( ctx, ctx->self, "GUIMENU:ADDSEPARATOR" );

   if( menu != NULL )
   {
      x_guiMenuSeparator( ctx, menu );
      x_guiReturnSelf( ctx );
   }
}

// Turn a new menu object into a drop-down menu (internal)
// X_GUI_EVENT_POPUP of oMenu() --> self
X_FUNC_STATIC( GUIMENU_POPUP )
{
   X_ITEM *slot = x_objDataSlot( ctx, 1 );
   HMENU menu = x_guiMenuCreate( ctx, 1 );

   if( slot != NULL && menu != NULL )
   {
      x_itemPutPtr( slot, menu );
      x_guiReturnSelf( ctx );
   }
}

// Menu bar or drop-down menu
// guiMenu() --> oMenu
X_FUNC( GUIMENU )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 1, "GUIMENU" );

      // Instance variables
      x_clsAddData( s_class, "HANDLE", 1 );
      x_clsSetScope( s_class, "HANDLE",  X_OO_CLSTP_EXPORTED | X_OO_CLSTP_READONLY );

      // Methods
      x_clsAdd( s_class, "NEW",          X_FUNCNAME( GUIMENU_NEW ) );
      x_clsAdd( s_class, "ADDMENU",      X_FUNCNAME( GUIMENU_ADDMENU ) );
      x_clsAdd( s_class, "ADD",          X_FUNCNAME( GUIMENU_ADD ) );
      x_clsAdd( s_class, "ADDSEPARATOR", X_FUNCNAME( GUIMENU_ADDSEPARATOR ) );

      // Library events, not methods of the class
      x_guiHandlerAdd( s_class, X_GUI_EVENT_POPUP, X_FUNCNAME( GUIMENU_POPUP ) );
   }

   x_clsReturn( ctx, s_class );
}

// Draw the menus made from now on in the extended style, as SET MENUSTYLE EXTENDED: a strip with the pictures
// of nImageSize pixels (16 at the start) at the left of drop-down menus and a gradient under the menu bar;
// returns the previous setting
// guiMenuStyle( [lExtended], [nImageSize] ) --> lPrevious
X_FUNC( GUIMENUSTYLE )
{
   if( ( !X_ISNIL( ctx, 1 ) && !X_ISLOG( ctx, 1 ) ) || ( !X_ISNIL( ctx, 2 ) && !X_ISNUM( ctx, 2 ) ) )
   {
      x_guiRaise( ctx, "GUIMENUSTYLE" );
      return;
   }

   x_retl( ctx, x_guiMenuStyle( IIF( X_ISLOG( ctx, 1 ), x_parl( ctx, 1 ), 1 ), x_parnidef( ctx, 2, 0 ) ) );
}

/* ---------------------------------------------------------------------------------------------
   guiControl
   --------------------------------------------------------------------------------------------- */

// Move the keyboard focus to the control
// oControl:SetFocus() --> self
X_FUNC_STATIC( GUICONTROL_SETFOCUS )
{
   HWND window = x_guiSelfHandle( ctx, "GUICONTROL:SETFOCUS" );

   if( window != NULL )
   {
      x_guiSetFocus( window );
      x_guiReturnSelf( ctx );
   }
}

// Return the text of the control; guiMemo separates lines with CR LF
// oControl:text --> cText
X_FUNC_STATIC( GUICONTROL_GETTEXT )
{
   HWND window = x_guiSelfHandle( ctx, "GUICONTROL:TEXT" );

   if( window != NULL )
      x_guiReturnWindowText( ctx, window );
}

// Set the text of the control; guiMemo stores a single LF as CR LF
// oControl:text := cText
X_FUNC_STATIC( GUICONTROL_SETTEXT )
{
   HWND window = x_guiSelfHandle( ctx, "GUICONTROL:TEXT" );

   if( window != NULL )
   {
      x_guiSetText( ctx, window, x_paritem( ctx, 1 ) );
      x_itemReturn( ctx, x_paritem( ctx, 1 ) );
   }
}

// Return whether the control is visible
// oControl:visible --> lVisible
X_FUNC_STATIC( GUICONTROL_GETVISIBLE )
{
   HWND window = x_guiSelfHandle( ctx, "GUICONTROL:VISIBLE" );

   if( window != NULL )
      x_retl( ctx, IsWindowVisible( window ) );
}

// Show or hide the control
// oControl:visible := lVisible
X_FUNC_STATIC( GUICONTROL_SETVISIBLE )
{
   HWND window = x_guiSelfHandle( ctx, "GUICONTROL:VISIBLE" );

   if( window == NULL )
      return;

   if( !X_ISLOG( ctx, 1 ) )
   {
      x_guiRaise( ctx, "GUICONTROL:VISIBLE" );
      return;
   }

   x_guiSetVisible( window, x_parl( ctx, 1 ) );
   x_retl( ctx, x_parl( ctx, 1 ) );
}

// Return whether the control is enabled
// oControl:enabled --> lEnabled
X_FUNC_STATIC( GUICONTROL_GETENABLED )
{
   HWND window = x_guiSelfHandle( ctx, "GUICONTROL:ENABLED" );

   if( window != NULL )
      x_retl( ctx, IsWindowEnabled( window ) );
}

// Enable or disable the control
// oControl:enabled := lEnabled
X_FUNC_STATIC( GUICONTROL_SETENABLED )
{
   HWND window = x_guiSelfHandle( ctx, "GUICONTROL:ENABLED" );

   if( window == NULL )
      return;

   if( !X_ISLOG( ctx, 1 ) )
   {
      x_guiRaise( ctx, "GUICONTROL:ENABLED" );
      return;
   }

   x_guiSetEnabled( window, x_parl( ctx, 1 ) );
   x_retl( ctx, x_parl( ctx, 1 ) );
}

// Return the edges of the window the control is attached to ("LT" by default)
// oControl:anchor --> cAnchor
X_FUNC_STATIC( GUICONTROL_GETANCHOR )
{
   HWND window = x_guiSelfHandle( ctx, "GUICONTROL:ANCHOR" );

   if( window != NULL )
      x_guiGetAnchor( ctx, window );
}

// Attach the control to edges of the window: L, T, R, B; opposite edges stretch the control
// oControl:anchor := cAnchor
X_FUNC_STATIC( GUICONTROL_SETANCHOR )
{
   HWND window = x_guiSelfHandle( ctx, "GUICONTROL:ANCHOR" );

   if( window != NULL )
   {
      x_guiSetAnchor( ctx, window, x_paritem( ctx, 1 ) );
      x_itemReturn( ctx, x_paritem( ctx, 1 ) );
   }
}

// Create the Windows control for the object (internal, called by New() of the subclasses)
// X_GUI_EVENT_CREATE of oControl( oWindow, nKind, cText, nX, nY, nWidth, nHeight ) --> self
X_FUNC_STATIC( GUICONTROL_CREATE )
{
   x_guiControlCreate( ctx );
}

// Release the handle and the parent after the control was destroyed (internal)
// X_GUI_EVENT_DETACH of oControl() --> NIL
X_FUNC_STATIC( GUICONTROL_DETACH )
{
   x_guiControlDetach( ctx );
}

// Return the left edge of the control in its window (logical pixels)
// oControl:x --> nX
X_FUNC_STATIC( GUICONTROL_GETX )
{
   HWND window = x_guiSelfHandle( ctx, "GUICONTROL:X" );

   if( window != NULL )
      x_retni( ctx, x_guiBounds( window, X_GUI_X ) );
}

// Move the left edge of the control
// oControl:x := nX
X_FUNC_STATIC( GUICONTROL_SETX )
{
   HWND window = x_guiSelfHandle( ctx, "GUICONTROL:X" );

   if( window == NULL )
      return;

   if( !X_ISNUM( ctx, 1 ) )
   {
      x_guiRaise( ctx, "GUICONTROL:X" );
      return;
   }

   x_guiSetBounds( ctx, window, X_GUI_X, x_parni( ctx, 1 ) );
   x_retni( ctx, x_parni( ctx, 1 ) );
}

// Return the top edge of the control in its window (logical pixels)
// oControl:y --> nY
X_FUNC_STATIC( GUICONTROL_GETY )
{
   HWND window = x_guiSelfHandle( ctx, "GUICONTROL:Y" );

   if( window != NULL )
      x_retni( ctx, x_guiBounds( window, X_GUI_Y ) );
}

// Move the top edge of the control
// oControl:y := nY
X_FUNC_STATIC( GUICONTROL_SETY )
{
   HWND window = x_guiSelfHandle( ctx, "GUICONTROL:Y" );

   if( window == NULL )
      return;

   if( !X_ISNUM( ctx, 1 ) )
   {
      x_guiRaise( ctx, "GUICONTROL:Y" );
      return;
   }

   x_guiSetBounds( ctx, window, X_GUI_Y, x_parni( ctx, 1 ) );
   x_retni( ctx, x_parni( ctx, 1 ) );
}

// Return the width of the control (logical pixels)
// oControl:width --> nWidth
X_FUNC_STATIC( GUICONTROL_GETWIDTH )
{
   HWND window = x_guiSelfHandle( ctx, "GUICONTROL:WIDTH" );

   if( window != NULL )
      x_retni( ctx, x_guiBounds( window, X_GUI_WIDTH ) );
}

// Set the width of the control
// oControl:width := nWidth
X_FUNC_STATIC( GUICONTROL_SETWIDTH )
{
   HWND window = x_guiSelfHandle( ctx, "GUICONTROL:WIDTH" );

   if( window == NULL )
      return;

   if( !X_ISNUM( ctx, 1 ) )
   {
      x_guiRaise( ctx, "GUICONTROL:WIDTH" );
      return;
   }

   x_guiSetBounds( ctx, window, X_GUI_WIDTH, x_parni( ctx, 1 ) );
   x_retni( ctx, x_parni( ctx, 1 ) );
}

// Return the height of the control (logical pixels)
// oControl:height --> nHeight
X_FUNC_STATIC( GUICONTROL_GETHEIGHT )
{
   HWND window = x_guiSelfHandle( ctx, "GUICONTROL:HEIGHT" );

   if( window != NULL )
      x_retni( ctx, x_guiBounds( window, X_GUI_HEIGHT ) );
}

// Set the height of the control
// oControl:height := nHeight
X_FUNC_STATIC( GUICONTROL_SETHEIGHT )
{
   HWND window = x_guiSelfHandle( ctx, "GUICONTROL:HEIGHT" );

   if( window == NULL )
      return;

   if( !X_ISNUM( ctx, 1 ) )
   {
      x_guiRaise( ctx, "GUICONTROL:HEIGHT" );
      return;
   }

   x_guiSetBounds( ctx, window, X_GUI_HEIGHT, x_parni( ctx, 1 ) );
   x_retni( ctx, x_parni( ctx, 1 ) );
}

// Return the tooltip text of the control, "" when it has none
// oControl:toolTip --> cText
X_FUNC_STATIC( GUICONTROL_GETTOOLTIP )
{
   const X_ITEM *text = x_objDataSlot( ctx, 3 );

   if( text != NULL && X_IS_STRING( text ) )
      x_itemReturn( ctx, text );
   else
      x_retc( ctx, "" );
}

// Set the tooltip shown when the mouse rests on the control; "" removes it
// oControl:toolTip := cText
X_FUNC_STATIC( GUICONTROL_SETTOOLTIP )
{
   X_ITEM *slot = x_objDataSlot( ctx, 3 );
   HWND window = x_guiSelfHandle( ctx, "GUICONTROL:TOOLTIP" );

   if( window == NULL || slot == NULL )
      return;

   x_guiSetToolTip( ctx, window, x_paritem( ctx, 1 ) );
   x_itemCopy( slot, x_paritem( ctx, 1 ) );
   x_itemReturn( ctx, x_paritem( ctx, 1 ) );
}

// Return the menu shown when the user right-clicks the control, NIL when it has none
// oControl:contextMenu --> oMenu
X_FUNC_STATIC( GUICONTROL_GETCONTEXTMENU )
{
   const X_ITEM *menu = x_objDataSlot( ctx, 4 );

   if( menu != NULL )
      x_itemReturn( ctx, menu );
}

// Set the context menu, a guiMenu():New( .T. ); NIL removes it; guiEdit and guiMemo keep their own one
// oControl:contextMenu := oMenu
X_FUNC_STATIC( GUICONTROL_SETCONTEXTMENU )
{
   x_guiSetContextMenu( ctx, 4, "GUICONTROL:CONTEXTMENU" );
}

// Return whether the control has the input focus
// oControl:focused --> lFocused
X_FUNC_STATIC( GUICONTROL_GETFOCUSED )
{
   HWND window = x_guiSelfHandle( ctx, "GUICONTROL:FOCUSED" );

   if( window != NULL )
      x_retl( ctx, x_guiFocused( window ) );
}

// Return the guiFont of the control, NIL for the system font
// oControl:font --> oFont
X_FUNC_STATIC( GUICONTROL_GETFONT )
{
   const X_ITEM *font = x_objDataSlot( ctx, 5 );

   if( font != NULL )
      x_itemReturn( ctx, font );
}

// Give the control a guiFont; NIL returns it to the system font; a later change of the guiFont needs
// another assignment
// oControl:font := oFont
X_FUNC_STATIC( GUICONTROL_SETFONT )
{
   HWND window = x_guiSelfHandle( ctx, "GUICONTROL:FONT" );
   X_ITEM *slot = x_objDataSlot( ctx, 5 );

   if( window == NULL || slot == NULL )
      return;

   x_guiSetFont( ctx, window, x_paritem( ctx, 1 ) );
   if( x_paritem( ctx, 1 ) != NULL )
   {
      x_itemCopy( slot, x_paritem( ctx, 1 ) );
      x_itemReturn( ctx, x_paritem( ctx, 1 ) );
   }
}

// Receive a notification code of the control (internal)
// X_GUI_EVENT_COMMAND of oControl( nCode ) --> NIL
X_FUNC_STATIC( GUICONTROL_COMMAND )
{
}

// Remove the control from its window, as RestScreen() takes a GET away
// oControl:Destroy() --> NIL
X_FUNC_STATIC( GUICONTROL_DESTROY )
{
   HWND control = x_guiSelfHandle( ctx, "GUICONTROL:DESTROY" );

   if( control != NULL )
      x_guiControlDestroy( control );
}

// Base class of all controls; :value is the text, subclasses with another kind of value override it
// guiControl() --> oControl
X_FUNC( GUICONTROL )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 6, "GUICONTROL" );

      // Instance variables
      x_clsAddData( s_class, "HANDLE", 1 );
      x_clsSetScope( s_class, "HANDLE",       X_OO_CLSTP_EXPORTED | X_OO_CLSTP_READONLY );
      x_clsAddData( s_class, "PARENT", 2 );
      x_clsSetScope( s_class, "PARENT",       X_OO_CLSTP_EXPORTED | X_OO_CLSTP_READONLY );
      x_clsAddData( s_class, "TOOLTIPTEXT", 3 );
      x_clsSetScope( s_class, "TOOLTIPTEXT",  X_OO_CLSTP_HIDDEN );
      x_clsAddData( s_class, "POPUPMENU", 4 );
      x_clsSetScope( s_class, "POPUPMENU",    X_OO_CLSTP_HIDDEN );
      x_clsAddData( s_class, "FONTOBJECT", 5 );
      x_clsSetScope( s_class, "FONTOBJECT",   X_OO_CLSTP_HIDDEN );
      x_clsAddData( s_class, "NAME", 6 );

      // Methods
      x_clsAdd( s_class, "SETFOCUS",          X_FUNCNAME( GUICONTROL_SETFOCUS ) );
      x_clsAdd( s_class, "DESTROY",           X_FUNCNAME( GUICONTROL_DESTROY ) );

      // ACCESS and ASSIGN methods
      x_clsAddAccess( s_class, "FOCUSED",     X_FUNCNAME( GUICONTROL_GETFOCUSED ) );
      x_clsAddAccess( s_class, "FONT",        X_FUNCNAME( GUICONTROL_GETFONT ) );
      x_clsAddAssign( s_class, "FONT",        X_FUNCNAME( GUICONTROL_SETFONT ) );

      x_clsAddAccess( s_class, "TEXT",        X_FUNCNAME( GUICONTROL_GETTEXT ) );
      x_clsAddAssign( s_class, "TEXT",        X_FUNCNAME( GUICONTROL_SETTEXT ) );
      x_clsAddAccess( s_class, "VALUE",       X_FUNCNAME( GUICONTROL_GETTEXT ) );
      x_clsAddAssign( s_class, "VALUE",       X_FUNCNAME( GUICONTROL_SETTEXT ) );
      x_clsAddAccess( s_class, "X",           X_FUNCNAME( GUICONTROL_GETX ) );
      x_clsAddAssign( s_class, "X",           X_FUNCNAME( GUICONTROL_SETX ) );
      x_clsAddAccess( s_class, "Y",           X_FUNCNAME( GUICONTROL_GETY ) );
      x_clsAddAssign( s_class, "Y",           X_FUNCNAME( GUICONTROL_SETY ) );
      x_clsAddAccess( s_class, "WIDTH",       X_FUNCNAME( GUICONTROL_GETWIDTH ) );
      x_clsAddAssign( s_class, "WIDTH",       X_FUNCNAME( GUICONTROL_SETWIDTH ) );
      x_clsAddAccess( s_class, "HEIGHT",      X_FUNCNAME( GUICONTROL_GETHEIGHT ) );
      x_clsAddAssign( s_class, "HEIGHT",      X_FUNCNAME( GUICONTROL_SETHEIGHT ) );
      x_clsAddAccess( s_class, "TOOLTIP",     X_FUNCNAME( GUICONTROL_GETTOOLTIP ) );
      x_clsAddAssign( s_class, "TOOLTIP",     X_FUNCNAME( GUICONTROL_SETTOOLTIP ) );
      x_clsAddAccess( s_class, "CONTEXTMENU", X_FUNCNAME( GUICONTROL_GETCONTEXTMENU ) );
      x_clsAddAssign( s_class, "CONTEXTMENU", X_FUNCNAME( GUICONTROL_SETCONTEXTMENU ) );
      x_clsAddAccess( s_class, "VISIBLE",     X_FUNCNAME( GUICONTROL_GETVISIBLE ) );
      x_clsAddAssign( s_class, "VISIBLE",     X_FUNCNAME( GUICONTROL_SETVISIBLE ) );
      x_clsAddAccess( s_class, "ENABLED",     X_FUNCNAME( GUICONTROL_GETENABLED ) );
      x_clsAddAssign( s_class, "ENABLED",     X_FUNCNAME( GUICONTROL_SETENABLED ) );
      x_clsAddAccess( s_class, "ANCHOR",      X_FUNCNAME( GUICONTROL_GETANCHOR ) );
      x_clsAddAssign( s_class, "ANCHOR",      X_FUNCNAME( GUICONTROL_SETANCHOR ) );

      // Library events, not methods of the class
      x_guiHandlerAdd( s_class, X_GUI_EVENT_CREATE, X_FUNCNAME( GUICONTROL_CREATE ) );
      x_guiHandlerAdd( s_class, X_GUI_EVENT_DETACH, X_FUNCNAME( GUICONTROL_DETACH ) );
      x_guiHandlerAdd( s_class, X_GUI_EVENT_COMMAND, X_FUNCNAME( GUICONTROL_COMMAND ) );
      x_guiHandlerAdd( s_class, X_GUI_EVENT_NOTIFY, X_FUNCNAME( GUICONTROL_COMMAND ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   guiFont
   --------------------------------------------------------------------------------------------- */

// Describe a font: the size in points (NIL keeps the size of the system font), bold, italic and the name
// (NIL keeps the system font, Segoe UI); the control makes it for the scale of its screen
// guiFont():New( [nSize], [lBold], [lItalic], [cName] ) --> self
X_FUNC_STATIC( GUIFONT_NEW )
{
   X_ITEM *size = x_objDataSlot( ctx, 1 );
   X_ITEM *bold = x_objDataSlot( ctx, 2 );
   X_ITEM *italic = x_objDataSlot( ctx, 3 );
   X_ITEM *name = x_objDataSlot( ctx, 4 );
   int valid;

   valid = size != NULL && bold != NULL && italic != NULL && name != NULL;
   valid = valid && ( X_ISNIL( ctx, 1 ) || ( X_ISNUM( ctx, 1 ) && x_parni( ctx, 1 ) > 0 ) );
   valid = valid && ( X_ISNIL( ctx, 2 ) || X_ISLOG( ctx, 2 ) ) && ( X_ISNIL( ctx, 3 ) || X_ISLOG( ctx, 3 ) );
   valid = valid && ( X_ISNIL( ctx, 4 ) || X_ISCHAR( ctx, 4 ) );
   if( !valid )
   {
      x_guiRaise( ctx, "GUIFONT:NEW" );
      return;
   }

   x_itemPutNI( size, x_parnidef( ctx, 1, 0 ) );
   x_itemPutL( bold, x_parldef( ctx, 2, 0 ) );
   x_itemPutL( italic, x_parldef( ctx, 3, 0 ) );
   x_itemPutC( name, IIF( X_ISCHAR( ctx, 4 ), x_parc( ctx, 4 ), "" ) );
   x_guiReturnSelf( ctx );
}

// Font of controls; :size 0 and :name "" mean the system font
// guiFont() --> oFont
X_FUNC( GUIFONT )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 4, "GUIFONT" );

      // Instance variables
      x_clsAddData( s_class, "SIZE", 1 );
      x_clsAddData( s_class, "BOLD", 2 );
      x_clsAddData( s_class, "ITALIC", 3 );
      x_clsAddData( s_class, "NAME", 4 );

      // Methods
      x_clsAdd( s_class, "NEW", X_FUNCNAME( GUIFONT_NEW ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   guiFontIcon
   --------------------------------------------------------------------------------------------- */

// Describe an icon: the character nCode of the TrueType font in the file cFile, in nColor 0xRRGGBB (NIL keeps
// the color of the text of the system) on a square of nSize pixels (NIL keeps 24); it is given in place of
// cBitmap of the sidebar
// guiFontIcon():New( cFile, nCode, [nColor], [nSize] ) --> self
X_FUNC_STATIC( GUIFONTICON_NEW )
{
   X_ITEM *file = x_objDataSlot( ctx, 1 );
   X_ITEM *code = x_objDataSlot( ctx, 2 );
   X_ITEM *color = x_objDataSlot( ctx, 3 );
   X_ITEM *size = x_objDataSlot( ctx, 4 );
   int valid;

   valid = file != NULL && code != NULL && color != NULL && size != NULL && X_ISCHAR( ctx, 1 );
   valid = valid && X_ISNUM( ctx, 2 ) && x_parnint( ctx, 2 ) > 0 && x_parnint( ctx, 2 ) <= 0x10FFFF;
   valid = valid && ( X_ISNIL( ctx, 3 ) || ( X_ISNUM( ctx, 3 ) && x_parnint( ctx, 3 ) >= 0 ) );
   valid = valid && ( X_ISNIL( ctx, 3 ) || x_parnint( ctx, 3 ) <= 0xFFFFFF );
   valid = valid && ( X_ISNIL( ctx, 4 ) || ( X_ISNUM( ctx, 4 ) && x_parni( ctx, 4 ) > 0 ) );
   if( !valid )
   {
      x_guiRaise( ctx, "GUIFONTICON:NEW" );
      return;
   }

   x_itemPutC( file, x_parc( ctx, 1 ) );
   x_itemPutNInt( code, x_parnint( ctx, 2 ) );
   x_itemPutNInt( color, x_parnintdef( ctx, 3, -1 ) );
   x_itemPutNI( size, x_parnidef( ctx, 4, 24 ) );
   x_guiReturnSelf( ctx );
}

// Icon from a font for pictures of controls; :color -1 means the color of the text of the system
// guiFontIcon() --> oFontIcon
X_FUNC( GUIFONTICON )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 4, "GUIFONTICON" );

      // Instance variables
      x_clsAddData( s_class, "FILE", 1 );
      x_clsAddData( s_class, "CODE", 2 );
      x_clsAddData( s_class, "COLOR", 3 );
      x_clsAddData( s_class, "SIZE", 4 );

      // Methods
      x_clsAdd( s_class, "NEW", X_FUNCNAME( GUIFONTICON_NEW ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   guiLabel
   --------------------------------------------------------------------------------------------- */

// Create a label; default size 200 x 20
// guiLabel():New( oWindow, nX, nY, [nWidth], [nHeight], [cText] ) --> self
X_FUNC_STATIC( GUILABEL_NEW )
{
   x_guiControlNew( ctx, X_GUI_LABEL, 200, 20, 1 );
}

// Return .T. when the label has the sunken frame of a field
// oLabel:border --> lBorder
X_FUNC_STATIC( GUILABEL_GETBORDER )
{
   HWND label = x_guiSelfHandle( ctx, "GUILABEL:BORDER" );

   if( label != NULL )
      x_retl( ctx, x_guiLabelBorder( label ) );
}

// Give the label the sunken frame of a field and the fixed font 9x18 of the GETs, for showing a value as SAY does
// oLabel:border := lBorder
X_FUNC_STATIC( GUILABEL_SETBORDER )
{
   HWND label = x_guiSelfHandle( ctx, "GUILABEL:BORDER" );

   if( label == NULL )
      return;

   if( !X_ISLOG( ctx, 1 ) )
   {
      x_guiRaise( ctx, "GUILABEL:BORDER" );
      return;
   }

   x_guiLabelSetBorder( label, x_parl( ctx, 1 ) );
   x_itemReturn( ctx, x_paritem( ctx, 1 ) );
}

// Return the alignment of the text: 0 left, 1 right, 2 center
// oLabel:align --> nAlign
X_FUNC_STATIC( GUILABEL_GETALIGN )
{
   HWND label = x_guiSelfHandle( ctx, "GUILABEL:ALIGN" );

   if( label != NULL )
      x_retni( ctx, x_guiLabelAlign( label ) );
}

// Align the text: 0 left, 1 right (descriptions before the fields), 2 center
// oLabel:align := nAlign
X_FUNC_STATIC( GUILABEL_SETALIGN )
{
   HWND label = x_guiSelfHandle( ctx, "GUILABEL:ALIGN" );

   if( label == NULL )
      return;

   if( !X_ISNUM( ctx, 1 ) || x_parni( ctx, 1 ) < 0 || x_parni( ctx, 1 ) > 2 )
   {
      x_guiRaise( ctx, "GUILABEL:ALIGN" );
      return;
   }

   x_guiLabelSetAlign( label, x_parni( ctx, 1 ) );
   x_itemReturn( ctx, x_paritem( ctx, 1 ) );
}

// Return the text color of the label (0xRRGGBB), NIL when it takes the color of its window
// oLabel:color --> nColor
X_FUNC_STATIC( GUILABEL_GETCOLOR )
{
   x_guiGetColor( ctx, "GUILABEL:COLOR" );
}

// Set the text color of the label (0xRRGGBB); NIL gives it the color of its window, oWindow:foreground
// oLabel:color := nColor
X_FUNC_STATIC( GUILABEL_SETCOLOR )
{
   x_guiSetColor( ctx, "GUILABEL:COLOR" );
}

// Draw a line of one pixel from the end of the text to the right edge of the label, as after the title of a
// part of a form: "Faktura VAT -----"; nColor 0xRRGGBB, without it the color of frames
// oLabel:AddLineAfter( [nColor] ) --> self
X_FUNC_STATIC( GUILABEL_ADDLINEAFTER )
{
   HWND label = x_guiSelfHandle( ctx, "GUILABEL:ADDLINEAFTER" );

   if( label == NULL )
      return;

   if( !X_ISNIL( ctx, 1 ) && !X_ISNUM( ctx, 1 ) )
   {
      x_guiRaise( ctx, "GUILABEL:ADDLINEAFTER" );
      return;
   }

   x_guiLabelAddLineAfter( label, IIF( X_ISNUM( ctx, 1 ), x_parni( ctx, 1 ), -1 ) );
   x_guiReturnSelf( ctx );
}

// Static text
// guiLabel() --> oLabel
X_FUNC( GUILABEL )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 0, "GUILABEL" );

      // Parent class
      x_clsAddParent( s_class, x_clsRequire( "GUICONTROL", GUICONTROL ) );

      // Methods
      x_clsAdd( s_class, "NEW",          X_FUNCNAME( GUILABEL_NEW ) );

      // ACCESS and ASSIGN methods
      x_clsAddAccess( s_class, "BORDER", X_FUNCNAME( GUILABEL_GETBORDER ) );
      x_clsAddAssign( s_class, "BORDER", X_FUNCNAME( GUILABEL_SETBORDER ) );
      x_clsAddAccess( s_class, "ALIGN",  X_FUNCNAME( GUILABEL_GETALIGN ) );
      x_clsAddAssign( s_class, "ALIGN",  X_FUNCNAME( GUILABEL_SETALIGN ) );
      x_clsAddAccess( s_class, "COLOR",  X_FUNCNAME( GUILABEL_GETCOLOR ) );
      x_clsAddAssign( s_class, "COLOR",  X_FUNCNAME( GUILABEL_SETCOLOR ) );

      // Methods
      x_clsAdd( s_class, "ADDLINEAFTER", X_FUNCNAME( GUILABEL_ADDLINEAFTER ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   guiSay
   --------------------------------------------------------------------------------------------- */

// Text shown in the window, as @ ... SAY: a description or, with :border, a value from the data;
// nLen characters of the font 9x18 wide, as a guiGet of nLen characters, without it as long as the text;
// without nHeight as high as a guiGet; members as in guiLabel
// guiSay():New( oWindow, nX, nY, [nLen], [nHeight], [cText] ) --> self
X_FUNC_STATIC( GUISAY_NEW )
{
   x_guiSayNew( ctx );
}

// guiSay() --> oSay
X_FUNC( GUISAY )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 0, "GUISAY" );

      // Parent class
      x_clsAddParent( s_class, x_clsRequire( "GUILABEL", GUILABEL ) );

      // Methods
      x_clsAdd( s_class, "NEW", X_FUNCNAME( GUISAY_NEW ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   guiEdit
   --------------------------------------------------------------------------------------------- */

// Create a single-line text field; default size 200 x 23
// guiEdit():New( oWindow, nX, nY, [nWidth], [nHeight], [cText] ) --> self
X_FUNC_STATIC( GUIEDIT_NEW )
{
   x_guiControlNew( ctx, X_GUI_EDIT, 200, 23, 1 );
}

// Return whether the user may not change the text
// oEdit:readOnly --> lReadOnly
X_FUNC_STATIC( GUIEDIT_GETREADONLY )
{
   HWND window = x_guiSelfHandle( ctx, "GUIEDIT:READONLY" );

   if( window != NULL )
      x_retl( ctx, x_guiReadOnly( window ) );
}

// Allow or forbid changes of the text by the user; the program can always assign :text
// oEdit:readOnly := lReadOnly
X_FUNC_STATIC( GUIEDIT_SETREADONLY )
{
   HWND window = x_guiSelfHandle( ctx, "GUIEDIT:READONLY" );

   if( window == NULL )
      return;

   if( !X_ISLOG( ctx, 1 ) )
   {
      x_guiRaise( ctx, "GUIEDIT:READONLY" );
      return;
   }

   x_guiSetReadOnly( window, x_parl( ctx, 1 ) );
   x_retl( ctx, x_parl( ctx, 1 ) );
}

// Return the maximum number of characters the user can type
// oEdit:maxLength --> nLength
X_FUNC_STATIC( GUIEDIT_GETMAXLENGTH )
{
   HWND window = x_guiSelfHandle( ctx, "GUIEDIT:MAXLENGTH" );

   if( window != NULL )
      x_retni( ctx, x_guiMaxLength( window ) );
}

// Limit the number of characters the user can type; 0 restores the system maximum
// oEdit:maxLength := nLength
X_FUNC_STATIC( GUIEDIT_SETMAXLENGTH )
{
   HWND window = x_guiSelfHandle( ctx, "GUIEDIT:MAXLENGTH" );

   if( window == NULL )
      return;

   if( !X_ISNUM( ctx, 1 ) || x_parni( ctx, 1 ) < 0 )
   {
      x_guiRaise( ctx, "GUIEDIT:MAXLENGTH" );
      return;
   }

   x_guiSetMaxLength( window, x_parni( ctx, 1 ) );
   x_retni( ctx, x_parni( ctx, 1 ) );
}

// Return the picture of the field, "" without one
// oEdit:picture --> cPicture
X_FUNC_STATIC( GUIEDIT_GETPICTURE )
{
   HWND window = x_guiSelfHandle( ctx, "GUIEDIT:PICTURE" );

   if( window != NULL )
      x_guiEditPicture( ctx, window );
}

// Set a picture: template 9 # A N X L Y ! with literals, functions @! @K @R before a space;
// a number takes the digits of the template before and after the point, a date follows SET DATE
// oEdit:picture := cPicture
X_FUNC_STATIC( GUIEDIT_SETPICTURE )
{
   HWND window = x_guiSelfHandle( ctx, "GUIEDIT:PICTURE" );

   if( window != NULL )
      x_guiEditSetPicture( ctx, window, x_paritem( ctx, 1 ) );
}

// Return the value in the type the program gave it: text, number or date
// oEdit:value --> xValue
X_FUNC_STATIC( GUIEDIT_GETVALUE )
{
   HWND window = x_guiSelfHandle( ctx, "GUIEDIT:VALUE" );

   if( window != NULL )
      x_guiEditValue( ctx, window );
}

// Set the value; a number stands on the right and is shown through the picture, a date follows SET DATE
// oEdit:value := xValue
X_FUNC_STATIC( GUIEDIT_SETVALUE )
{
   HWND window = x_guiSelfHandle( ctx, "GUIEDIT:VALUE" );

   if( window != NULL )
      x_guiEditSetValue( ctx, window, x_paritem( ctx, 1 ) );
}

// Run OnChange after the text changed (internal)
// X_GUI_EVENT_COMMAND of oEdit( nCode ) --> NIL
X_FUNC_STATIC( GUIEDIT_COMMAND )
{
   if( x_parni( ctx, 1 ) == EN_CHANGE )
      x_guiEvent( ctx, 1 );
}

// Run OnValid when the field loses the focus; a result .F. keeps the focus in the field (internal)
// X_GUI_EVENT_VALID of oEdit() --> lValid
X_FUNC_STATIC( GUIEDIT_VALID )
{
   const X_ITEM *data = x_objDataSlot( ctx, 2 );
   X_ITEM block __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ARG args[1];

   x_retl( ctx, 1 );
   if( data == NULL || !X_IS_BLOCK( data ) )
      return;

   x_itemCopy( &block, data );
   args[0] = ( X_ARG ){ ctx->self, 0 };
   x_evalBlock( ctx, ctx->result, &block, 1, args );
}

// Single-line text field; OnChange runs after every change of the text, OnValid when the focus leaves
// the field and returns .F. to keep it there; both receive the control
// guiEdit() --> oEdit
X_FUNC( GUIEDIT )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 2, "GUIEDIT" );

      // Parent class
      x_clsAddParent( s_class, x_clsRequire( "GUICONTROL", GUICONTROL ) );

      // Instance variables
      x_clsAddData( s_class, "ONCHANGE", 1 );
      x_clsSetDataType( s_class, "ONCHANGE", X_OO_VALUE_CODEBLOCK );
      x_clsAddData( s_class, "ONVALID", 2 );
      x_clsSetDataType( s_class, "ONVALID",  X_OO_VALUE_CODEBLOCK );

      // Methods
      x_clsAdd( s_class, "NEW",              X_FUNCNAME( GUIEDIT_NEW ) );

      // ACCESS and ASSIGN methods
      x_clsAddAccess( s_class, "READONLY",   X_FUNCNAME( GUIEDIT_GETREADONLY ) );
      x_clsAddAssign( s_class, "READONLY",   X_FUNCNAME( GUIEDIT_SETREADONLY ) );
      x_clsAddAccess( s_class, "MAXLENGTH",  X_FUNCNAME( GUIEDIT_GETMAXLENGTH ) );
      x_clsAddAssign( s_class, "MAXLENGTH",  X_FUNCNAME( GUIEDIT_SETMAXLENGTH ) );
      x_clsAddAccess( s_class, "PICTURE",    X_FUNCNAME( GUIEDIT_GETPICTURE ) );
      x_clsAddAssign( s_class, "PICTURE",    X_FUNCNAME( GUIEDIT_SETPICTURE ) );
      x_clsAddAccess( s_class, "VALUE",      X_FUNCNAME( GUIEDIT_GETVALUE ) );
      x_clsAddAssign( s_class, "VALUE",      X_FUNCNAME( GUIEDIT_SETVALUE ) );

      // Library events, not methods of the class
      x_guiHandlerAdd( s_class, X_GUI_EVENT_COMMAND, X_FUNCNAME( GUIEDIT_COMMAND ) );
      x_guiHandlerAdd( s_class, X_GUI_EVENT_VALID, X_FUNCNAME( GUIEDIT_VALID ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   guiGet
   --------------------------------------------------------------------------------------------- */

// The edit field of a GET, as @ ... GET ... PICTURE, made for ReadModal( GetList ), in the fixed font
// 9x18: with @var the field shows the variable and ReadModal() gives the new value back to it when it ends
// with saving; the variable is a LOCAL of the function that runs ReadModal(); nLen characters are seen, without
// it the whole variable, and a longer value scrolls in the field; without cPicture no more characters than the
// variable has can be written, with nMax no more than nMax (for a memo field); with nRows the text wraps, the
// field shows one row and grows over the fields under it to at most nRows rows while it has the focus, and
// Shift+Enter begins a new line (CR LF); members as in guiEdit
// guiGet():New( oWindow, nX, nY, [nLen], [nHeight], [@var], [cPicture], [nRows], [nMax] ) --> self
X_FUNC_STATIC( GUIGET_NEW )
{
   x_guiGetNew( ctx );
}

// guiGet() --> oGet
X_FUNC( GUIGET )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 0, "GUIGET" );

      // Parent class
      x_clsAddParent( s_class, x_clsRequire( "GUIEDIT", GUIEDIT ) );

      // Methods
      x_clsAdd( s_class, "NEW", X_FUNCNAME( GUIGET_NEW ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   guiMemo
   --------------------------------------------------------------------------------------------- */

// Create a multi-line text field; default size 200 x 80
// guiMemo():New( oWindow, nX, nY, [nWidth], [nHeight], [cText] ) --> self
X_FUNC_STATIC( GUIMEMO_NEW )
{
   x_guiControlNew( ctx, X_GUI_MEMO, 200, 80, 1 );
}

// Multi-line text field with a vertical scroll bar; members as in guiEdit
// guiMemo() --> oMemo
X_FUNC( GUIMEMO )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 0, "GUIMEMO" );

      // Parent class
      x_clsAddParent( s_class, x_clsRequire( "GUIEDIT", GUIEDIT ) );

      // Methods
      x_clsAdd( s_class, "NEW", X_FUNCNAME( GUIMEMO_NEW ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   guiButton
   --------------------------------------------------------------------------------------------- */

// Create a push button; default size 90 x 25
// guiButton():New( oWindow, nX, nY, [nWidth], [nHeight], [cText] ) --> self
X_FUNC_STATIC( GUIBUTTON_NEW )
{
   x_guiControlNew( ctx, X_GUI_BUTTON, 90, 25, 1 );
}

// Return whether the button is the default button of its window
// oButton:isDefault --> lDefault
X_FUNC_STATIC( GUIBUTTON_GETDEFAULT )
{
   HWND window = x_guiSelfHandle( ctx, "GUIBUTTON:ISDEFAULT" );

   if( window != NULL )
      x_retl( ctx, x_guiButtonRole( window, 0 ) );
}

// Make the button the default button of its window, pressed by Enter
// oButton:isDefault := lDefault
X_FUNC_STATIC( GUIBUTTON_SETDEFAULT )
{
   HWND window = x_guiSelfHandle( ctx, "GUIBUTTON:ISDEFAULT" );

   if( window == NULL )
      return;

   if( !X_ISLOG( ctx, 1 ) )
   {
      x_guiRaise( ctx, "GUIBUTTON:ISDEFAULT" );
      return;
   }

   x_guiSetButtonRole( window, 0, x_parl( ctx, 1 ) );
   x_retl( ctx, x_parl( ctx, 1 ) );
}

// Return whether the button is the cancel button of its window
// oButton:isCancel --> lCancel
X_FUNC_STATIC( GUIBUTTON_GETCANCEL )
{
   HWND window = x_guiSelfHandle( ctx, "GUIBUTTON:ISCANCEL" );

   if( window != NULL )
      x_retl( ctx, x_guiButtonRole( window, 1 ) );
}

// Make the button the cancel button of its window, pressed by Escape
// oButton:isCancel := lCancel
X_FUNC_STATIC( GUIBUTTON_SETCANCEL )
{
   HWND window = x_guiSelfHandle( ctx, "GUIBUTTON:ISCANCEL" );

   if( window == NULL )
      return;

   if( !X_ISLOG( ctx, 1 ) )
   {
      x_guiRaise( ctx, "GUIBUTTON:ISCANCEL" );
      return;
   }

   x_guiSetButtonRole( window, 1, x_parl( ctx, 1 ) );
   x_retl( ctx, x_parl( ctx, 1 ) );
}

// Run OnClick when the button is clicked (internal)
// X_GUI_EVENT_COMMAND of oButton( nCode ) --> NIL
X_FUNC_STATIC( GUIBUTTON_COMMAND )
{
   if( x_parni( ctx, 1 ) == BN_CLICKED )
      x_guiEvent( ctx, 1 );
}

// Push button; OnClick runs when the button is clicked and receives the control
// guiButton() --> oButton
X_FUNC( GUIBUTTON )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 1, "GUIBUTTON" );

      // Parent class
      x_clsAddParent( s_class, x_clsRequire( "GUICONTROL", GUICONTROL ) );

      // Instance variables
      x_clsAddData( s_class, "ONCLICK", 1 );
      x_clsSetDataType( s_class, "ONCLICK", X_OO_VALUE_CODEBLOCK );

      // Methods
      x_clsAdd( s_class, "NEW",             X_FUNCNAME( GUIBUTTON_NEW ) );

      // ACCESS and ASSIGN methods
      x_clsAddAccess( s_class, "ISDEFAULT", X_FUNCNAME( GUIBUTTON_GETDEFAULT ) );
      x_clsAddAssign( s_class, "ISDEFAULT", X_FUNCNAME( GUIBUTTON_SETDEFAULT ) );
      x_clsAddAccess( s_class, "ISCANCEL",  X_FUNCNAME( GUIBUTTON_GETCANCEL ) );
      x_clsAddAssign( s_class, "ISCANCEL",  X_FUNCNAME( GUIBUTTON_SETCANCEL ) );

      // Library events, not methods of the class
      x_guiHandlerAdd( s_class, X_GUI_EVENT_COMMAND, X_FUNCNAME( GUIBUTTON_COMMAND ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   guiCheckBox
   --------------------------------------------------------------------------------------------- */

// Create a check box; default size 200 x 20
// guiCheckBox():New( oWindow, nX, nY, [nWidth], [nHeight], [cText] ) --> self
X_FUNC_STATIC( GUICHECKBOX_NEW )
{
   x_guiControlNew( ctx, X_GUI_CHECKBOX, 200, 20, 1 );
}

// Return whether the check box is checked
// oCheckBox:checked --> lChecked
X_FUNC_STATIC( GUICHECKBOX_GETCHECKED )
{
   HWND window = x_guiSelfHandle( ctx, "GUICHECKBOX:CHECKED" );

   if( window != NULL )
      x_retl( ctx, x_guiGetCheck( window ) );
}

// Set the state; checking a guiRadioButton unchecks the others in its group
// oCheckBox:checked := lChecked
X_FUNC_STATIC( GUICHECKBOX_SETCHECKED )
{
   HWND window = x_guiSelfHandle( ctx, "GUICHECKBOX:CHECKED" );

   if( window == NULL )
      return;

   if( !X_ISLOG( ctx, 1 ) )
   {
      x_guiRaise( ctx, "GUICHECKBOX:CHECKED" );
      return;
   }

   x_guiSetCheck( window, x_parl( ctx, 1 ) );
   x_retl( ctx, x_parl( ctx, 1 ) );
}

// Run OnChange when the user changes the state (internal)
// X_GUI_EVENT_COMMAND of oCheckBox( nCode ) --> NIL
X_FUNC_STATIC( GUICHECKBOX_COMMAND )
{
   if( x_parni( ctx, 1 ) == BN_CLICKED )
      x_guiEvent( ctx, 1 );
}

// Return the text color of the check box or radio button (0xRRGGBB), NIL when it takes the color of its window
// oCheckBox:color --> nColor
X_FUNC_STATIC( GUICHECKBOX_GETCOLOR )
{
   x_guiGetColor( ctx, "GUICHECKBOX:COLOR" );
}

// Set the text color of the check box or radio button (0xRRGGBB); NIL gives it the color of its window
// oCheckBox:color := nColor
X_FUNC_STATIC( GUICHECKBOX_SETCOLOR )
{
   x_guiSetColor( ctx, "GUICHECKBOX:COLOR" );
}

// Check box; OnChange runs when the user changes the state and receives the control
// guiCheckBox() --> oCheckBox
X_FUNC( GUICHECKBOX )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 1, "GUICHECKBOX" );

      // Parent class
      x_clsAddParent( s_class, x_clsRequire( "GUICONTROL", GUICONTROL ) );

      // Instance variables
      x_clsAddData( s_class, "ONCHANGE", 1 );
      x_clsSetDataType( s_class, "ONCHANGE", X_OO_VALUE_CODEBLOCK );

      // Methods
      x_clsAdd( s_class, "NEW",              X_FUNCNAME( GUICHECKBOX_NEW ) );

      // ACCESS and ASSIGN methods
      x_clsAddAccess( s_class, "CHECKED",    X_FUNCNAME( GUICHECKBOX_GETCHECKED ) );
      x_clsAddAssign( s_class, "CHECKED",    X_FUNCNAME( GUICHECKBOX_SETCHECKED ) );
      x_clsAddAccess( s_class, "VALUE",      X_FUNCNAME( GUICHECKBOX_GETCHECKED ) );
      x_clsAddAssign( s_class, "VALUE",      X_FUNCNAME( GUICHECKBOX_SETCHECKED ) );
      x_clsAddAccess( s_class, "COLOR",      X_FUNCNAME( GUICHECKBOX_GETCOLOR ) );
      x_clsAddAssign( s_class, "COLOR",      X_FUNCNAME( GUICHECKBOX_SETCOLOR ) );

      // Library events, not methods of the class
      x_guiHandlerAdd( s_class, X_GUI_EVENT_COMMAND, X_FUNCNAME( GUICHECKBOX_COMMAND ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   guiRadioButton
   --------------------------------------------------------------------------------------------- */

// Create a radio button; default size 200 x 20
// guiRadioButton():New( oWindow, nX, nY, [nWidth], [nHeight], [cText] ) --> self
X_FUNC_STATIC( GUIRADIOBUTTON_NEW )
{
   x_guiControlNew( ctx, X_GUI_RADIO, 200, 20, 1 );
}

// Radio button; buttons created one after another form a group, members as in guiCheckBox
// guiRadioButton() --> oRadioButton
X_FUNC( GUIRADIOBUTTON )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 0, "GUIRADIOBUTTON" );

      // Parent class
      x_clsAddParent( s_class, x_clsRequire( "GUICHECKBOX", GUICHECKBOX ) );

      // Methods
      x_clsAdd( s_class, "NEW", X_FUNCNAME( GUIRADIOBUTTON_NEW ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   guiGroupBox
   --------------------------------------------------------------------------------------------- */

// Create a group box; default size 200 x 100
// guiGroupBox():New( oWindow, nX, nY, [nWidth], [nHeight], [cText] ) --> self
X_FUNC_STATIC( GUIGROUPBOX_NEW )
{
   x_guiControlNew( ctx, X_GUI_GROUPBOX, 200, 100, 1 );
}

// Return the color of the caption of the group box (0xRRGGBB), NIL when it takes the color of its window
// oGroupBox:color --> nColor
X_FUNC_STATIC( GUIGROUPBOX_GETCOLOR )
{
   x_guiGetColor( ctx, "GUIGROUPBOX:COLOR" );
}

// Set the color of the caption of the group box (0xRRGGBB); NIL gives it the color of its window
// oGroupBox:color := nColor
X_FUNC_STATIC( GUIGROUPBOX_SETCOLOR )
{
   x_guiSetColor( ctx, "GUIGROUPBOX:COLOR" );
}

// Frame with a caption around other controls
// guiGroupBox() --> oGroupBox
X_FUNC( GUIGROUPBOX )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 0, "GUIGROUPBOX" );

      // Parent class
      x_clsAddParent( s_class, x_clsRequire( "GUICONTROL", GUICONTROL ) );

      // Methods
      x_clsAdd( s_class, "NEW", X_FUNCNAME( GUIGROUPBOX_NEW ) );

      // ACCESS and ASSIGN methods
      x_clsAddAccess( s_class, "COLOR", X_FUNCNAME( GUIGROUPBOX_GETCOLOR ) );
      x_clsAddAssign( s_class, "COLOR", X_FUNCNAME( GUIGROUPBOX_SETCOLOR ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   guiListBox
   --------------------------------------------------------------------------------------------- */

// Create a list box; default size 200 x 100
// guiListBox():New( oWindow, nX, nY, [nWidth], [nHeight] ) --> self
X_FUNC_STATIC( GUILISTBOX_NEW )
{
   x_guiControlNew( ctx, X_GUI_LISTBOX, 200, 100, 0 );
}

// Append an item and return its position
// oList:Add( cText ) --> nPosition
X_FUNC_STATIC( GUILISTBOX_ADD )
{
   HWND window = x_guiSelfHandle( ctx, "GUILISTBOX:ADD" );
   int index;

   if( window != NULL && ( index = x_guiListAdd( ctx, window, x_paritem( ctx, 1 ) ) ) != 0 )
      x_retni( ctx, index );
}

// Remove all items
// oList:Clear() --> self
X_FUNC_STATIC( GUILISTBOX_CLEAR )
{
   HWND window = x_guiSelfHandle( ctx, "GUILISTBOX:CLEAR" );

   if( window != NULL )
   {
      x_guiListClear( window );
      x_guiReturnSelf( ctx );
   }
}

// Return the text of the item at the position
// oList:Item( nPosition ) --> cText
X_FUNC_STATIC( GUILISTBOX_ITEM )
{
   HWND window = x_guiSelfHandle( ctx, "GUILISTBOX:ITEM" );

   if( window != NULL )
      x_guiListItem( ctx, window, x_parni( ctx, 1 ) );
}

// Return the number of items
// oList:count --> nCount
X_FUNC_STATIC( GUILISTBOX_COUNT )
{
   HWND window = x_guiSelfHandle( ctx, "GUILISTBOX:COUNT" );

   if( window != NULL )
      x_retni( ctx, x_guiListCount( window ) );
}

// Return the position of the selected item, 0 when nothing is selected
// oList:selected --> nPosition
X_FUNC_STATIC( GUILISTBOX_GETSELECTED )
{
   HWND window = x_guiSelfHandle( ctx, "GUILISTBOX:SELECTED" );

   if( window != NULL )
      x_retni( ctx, x_guiListSelected( window ) );
}

// Select the item at the position; 0 clears the selection
// oList:selected := nPosition
X_FUNC_STATIC( GUILISTBOX_SETSELECTED )
{
   HWND window = x_guiSelfHandle( ctx, "GUILISTBOX:SELECTED" );

   if( window == NULL )
      return;

   if( !X_ISNUM( ctx, 1 ) )
   {
      x_guiRaise( ctx, "GUILISTBOX:SELECTED" );
      return;
   }

   x_guiListSetSelected( ctx, window, x_parni( ctx, 1 ) );
   x_retni( ctx, x_parni( ctx, 1 ) );
}

// Return the text of the selected item, "" when nothing is selected
// oList:selectedText --> cText
X_FUNC_STATIC( GUILISTBOX_SELECTEDTEXT )
{
   HWND window = x_guiSelfHandle( ctx, "GUILISTBOX:SELECTEDTEXT" );
   int index;

   if( window == NULL )
      return;

   if( ( index = x_guiListSelected( window ) ) == 0 )
      x_retc( ctx, "" );
   else
      x_guiListItem( ctx, window, index );
}

// Run OnSelect or OnDoubleClick for the user action (internal)
// X_GUI_EVENT_COMMAND of oList( nCode ) --> NIL
X_FUNC_STATIC( GUILISTBOX_COMMAND )
{
   if( x_parni( ctx, 1 ) == LBN_SELCHANGE )
      x_guiEvent( ctx, 1 );
   else if( x_parni( ctx, 1 ) == LBN_DBLCLK )
      x_guiEvent( ctx, 2 );
}

// List of text items with 1-based positions; OnSelect and OnDoubleClick receive the control
// guiListBox() --> oList
X_FUNC( GUILISTBOX )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 2, "GUILISTBOX" );

      // Parent class
      x_clsAddParent( s_class, x_clsRequire( "GUICONTROL", GUICONTROL ) );

      // Instance variables
      x_clsAddData( s_class, "ONSELECT", 1 );
      x_clsSetDataType( s_class, "ONSELECT",      X_OO_VALUE_CODEBLOCK );
      x_clsAddData( s_class, "ONDOUBLECLICK", 2 );
      x_clsSetDataType( s_class, "ONDOUBLECLICK", X_OO_VALUE_CODEBLOCK );

      // Methods
      x_clsAdd( s_class, "NEW",                   X_FUNCNAME( GUILISTBOX_NEW ) );
      x_clsAdd( s_class, "ADD",                   X_FUNCNAME( GUILISTBOX_ADD ) );
      x_clsAdd( s_class, "CLEAR",                 X_FUNCNAME( GUILISTBOX_CLEAR ) );
      x_clsAdd( s_class, "ITEM",                  X_FUNCNAME( GUILISTBOX_ITEM ) );

      // ACCESS and ASSIGN methods
      x_clsAddAccess( s_class, "COUNT",           X_FUNCNAME( GUILISTBOX_COUNT ) );
      x_clsAddAccess( s_class, "SELECTED",        X_FUNCNAME( GUILISTBOX_GETSELECTED ) );
      x_clsAddAssign( s_class, "SELECTED",        X_FUNCNAME( GUILISTBOX_SETSELECTED ) );
      x_clsAddAccess( s_class, "VALUE",           X_FUNCNAME( GUILISTBOX_GETSELECTED ) );
      x_clsAddAssign( s_class, "VALUE",           X_FUNCNAME( GUILISTBOX_SETSELECTED ) );
      x_clsAddAccess( s_class, "SELECTEDTEXT",    X_FUNCNAME( GUILISTBOX_SELECTEDTEXT ) );

      // Library events, not methods of the class
      x_guiHandlerAdd( s_class, X_GUI_EVENT_COMMAND, X_FUNCNAME( GUILISTBOX_COMMAND ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   guiComboBox
   --------------------------------------------------------------------------------------------- */

// Create a drop-down list; default size 200 x 200 including the open list
// guiComboBox():New( oWindow, nX, nY, [nWidth], [nHeight] ) --> self
X_FUNC_STATIC( GUICOMBOBOX_NEW )
{
   x_guiControlNew( ctx, X_GUI_COMBOBOX, 200, 200, 0 );
}

// Drop-down list; members as in guiListBox, OnDoubleClick is not used
// guiComboBox() --> oComboBox
X_FUNC( GUICOMBOBOX )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 0, "GUICOMBOBOX" );

      // Parent class
      x_clsAddParent( s_class, x_clsRequire( "GUILISTBOX", GUILISTBOX ) );

      // Methods
      x_clsAdd( s_class, "NEW", X_FUNCNAME( GUICOMBOBOX_NEW ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   guiProgressBar
   --------------------------------------------------------------------------------------------- */

// Create a progress bar with the range 0 to 100; default size 200 x 20
// guiProgressBar():New( oWindow, nX, nY, [nWidth], [nHeight] ) --> self
X_FUNC_STATIC( GUIPROGRESSBAR_NEW )
{
   x_guiControlNew( ctx, X_GUI_PROGRESSBAR, 200, 20, 0 );
}

// Return the current position
// oProgress:value --> nValue
X_FUNC_STATIC( GUIPROGRESSBAR_GETVALUE )
{
   HWND window = x_guiSelfHandle( ctx, "GUIPROGRESSBAR:VALUE" );

   if( window != NULL )
      x_retni( ctx, x_guiProgressValue( window ) );
}

// Set the current position between 0 and the maximum
// oProgress:value := nValue
X_FUNC_STATIC( GUIPROGRESSBAR_SETVALUE )
{
   HWND window = x_guiSelfHandle( ctx, "GUIPROGRESSBAR:VALUE" );

   if( window == NULL )
      return;

   if( !X_ISNUM( ctx, 1 ) || x_parni( ctx, 1 ) < 0 || x_parni( ctx, 1 ) > x_guiProgressMaximum( window ) )
   {
      x_guiRaise( ctx, "GUIPROGRESSBAR:VALUE" );
      return;
   }

   x_guiProgressSetValue( window, x_parni( ctx, 1 ) );
   x_retni( ctx, x_parni( ctx, 1 ) );
}

// Return the maximum position
// oProgress:maximum --> nMaximum
X_FUNC_STATIC( GUIPROGRESSBAR_GETMAXIMUM )
{
   HWND window = x_guiSelfHandle( ctx, "GUIPROGRESSBAR:MAXIMUM" );

   if( window != NULL )
      x_retni( ctx, x_guiProgressMaximum( window ) );
}

// Set the maximum position; the minimum is always 0
// oProgress:maximum := nMaximum
X_FUNC_STATIC( GUIPROGRESSBAR_SETMAXIMUM )
{
   HWND window = x_guiSelfHandle( ctx, "GUIPROGRESSBAR:MAXIMUM" );

   if( window == NULL )
      return;

   if( !X_ISNUM( ctx, 1 ) || x_parni( ctx, 1 ) <= 0 )
   {
      x_guiRaise( ctx, "GUIPROGRESSBAR:MAXIMUM" );
      return;
   }

   x_guiProgressSetMaximum( window, x_parni( ctx, 1 ) );
   x_retni( ctx, x_parni( ctx, 1 ) );
}

// Progress bar
// guiProgressBar() --> oProgress
X_FUNC( GUIPROGRESSBAR )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 0, "GUIPROGRESSBAR" );

      // Parent class
      x_clsAddParent( s_class, x_clsRequire( "GUICONTROL", GUICONTROL ) );

      // Methods
      x_clsAdd( s_class, "NEW",           X_FUNCNAME( GUIPROGRESSBAR_NEW ) );

      // ACCESS and ASSIGN methods
      x_clsAddAccess( s_class, "VALUE",   X_FUNCNAME( GUIPROGRESSBAR_GETVALUE ) );
      x_clsAddAssign( s_class, "VALUE",   X_FUNCNAME( GUIPROGRESSBAR_SETVALUE ) );
      x_clsAddAccess( s_class, "MAXIMUM", X_FUNCNAME( GUIPROGRESSBAR_GETMAXIMUM ) );
      x_clsAddAssign( s_class, "MAXIMUM", X_FUNCNAME( GUIPROGRESSBAR_SETMAXIMUM ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   guiStatusBar
   --------------------------------------------------------------------------------------------- */

// Create a status bar at the bottom of the window; it follows the width of the window
// guiStatusBar():New( oWindow, [cText] ) --> self
X_FUNC_STATIC( GUISTATUSBAR_NEW )
{
   x_guiStatusBarNew( ctx );
}

// Add a panel; without nWidth (or 0) the panel shares what the other panels leave of the bar; the panel is
// sunken, lRaised makes it raised; the first panel takes the place of the single text of the bar;
// returns the number of the panel, counted from 1
// oStatusBar:Add( [nWidth], [cText], [lRaised] ) --> nPanel
X_FUNC_STATIC( GUISTATUSBAR_ADD )
{
   HWND window = x_guiSelfHandle( ctx, "GUISTATUSBAR:ADD" );
   int panel;

   if( window == NULL )
      return;

   if( ( !X_ISNIL( ctx, 1 ) && !X_ISNUM( ctx, 1 ) ) || ( !X_ISNIL( ctx, 3 ) && !X_ISLOG( ctx, 3 ) ) )
   {
      x_guiRaise( ctx, "GUISTATUSBAR:ADD" );
      return;
   }

   panel = x_guiStatusBarAdd( ctx, window, x_parnidef( ctx, 1, 0 ), x_paritem( ctx, 2 ), x_parl( ctx, 3 ) );
   if( panel != 0 )
      x_retni( ctx, panel );
}

// Set the text of a panel
// oStatusBar:SetText( nPanel, cText ) --> self
X_FUNC_STATIC( GUISTATUSBAR_SETTEXT )
{
   HWND window = x_guiSelfHandle( ctx, "GUISTATUSBAR:SETTEXT" );

   if( window == NULL )
      return;

   if( !X_ISNUM( ctx, 1 ) )
   {
      x_guiRaise( ctx, "GUISTATUSBAR:SETTEXT" );
      return;
   }

   x_guiStatusBarSetText( ctx, window, x_parni( ctx, 1 ), x_paritem( ctx, 2 ) );
   x_itemReturn( ctx, ctx->self );
}

// Panel that shows "Ins" during ReadModal() of the window while SET INSERT is on, as SET SCOREBOARD;
// 0 (at the start) for none
// oStatusBar:scoreboard --> nPanel
X_FUNC_STATIC( GUISTATUSBAR_GETSCOREBOARD )
{
   x_guiStatusBarScoreboard( ctx );
}

// oStatusBar:scoreboard := nPanel
X_FUNC_STATIC( GUISTATUSBAR_SETSCOREBOARD )
{
   x_guiStatusBarSetScoreboard( ctx );
}

// Text color of the bar (0xRRGGBB), NIL for the system color
// oStatusBar:foreground --> nColor
X_FUNC_STATIC( GUISTATUSBAR_GETFOREGROUND )
{
   x_guiStatusBarColor( ctx, X_GUI_COLOR_FOREGROUND );
}

// oStatusBar:foreground := nColor, NIL returns to the system color
X_FUNC_STATIC( GUISTATUSBAR_SETFOREGROUND )
{
   x_guiStatusBarSetColor( ctx, X_GUI_COLOR_FOREGROUND );
}

// Color of the bar, NIL for the system color
// oStatusBar:background --> nColor
X_FUNC_STATIC( GUISTATUSBAR_GETBACKGROUND )
{
   x_guiStatusBarColor( ctx, X_GUI_COLOR_BACKGROUND );
}

// oStatusBar:background := nColor, NIL returns to the system color
X_FUNC_STATIC( GUISTATUSBAR_SETBACKGROUND )
{
   x_guiStatusBarSetColor( ctx, X_GUI_COLOR_BACKGROUND );
}

// Status bar; :text is the text shown in the bar
// guiStatusBar() --> oStatusBar
X_FUNC( GUISTATUSBAR )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 0, "GUISTATUSBAR" );

      // Parent class
      x_clsAddParent( s_class, x_clsRequire( "GUICONTROL", GUICONTROL ) );

      // Methods
      x_clsAdd( s_class, "NEW",              X_FUNCNAME( GUISTATUSBAR_NEW ) );
      x_clsAdd( s_class, "ADD",              X_FUNCNAME( GUISTATUSBAR_ADD ) );
      x_clsAdd( s_class, "SETTEXT",          X_FUNCNAME( GUISTATUSBAR_SETTEXT ) );

      // ACCESS and ASSIGN methods
      x_clsAddAccess( s_class, "SCOREBOARD", X_FUNCNAME( GUISTATUSBAR_GETSCOREBOARD ) );
      x_clsAddAssign( s_class, "SCOREBOARD", X_FUNCNAME( GUISTATUSBAR_SETSCOREBOARD ) );
      x_clsAddAccess( s_class, "FOREGROUND", X_FUNCNAME( GUISTATUSBAR_GETFOREGROUND ) );
      x_clsAddAssign( s_class, "FOREGROUND", X_FUNCNAME( GUISTATUSBAR_SETFOREGROUND ) );
      x_clsAddAccess( s_class, "BACKGROUND", X_FUNCNAME( GUISTATUSBAR_GETBACKGROUND ) );
      x_clsAddAssign( s_class, "BACKGROUND", X_FUNCNAME( GUISTATUSBAR_SETBACKGROUND ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   guiBrowse
   --------------------------------------------------------------------------------------------- */

// Create a table of records; default size 400 x 200
// guiBrowse():New( oWindow, nX, nY, [nWidth], [nHeight] ) --> self
X_FUNC_STATIC( GUIBROWSE_NEW )
{
   X_ITEM *columns = x_objDataSlot( ctx, 7 );

   x_guiControlNew( ctx, X_GUI_BROWSE, 400, 200, 0 );
   if( columns != NULL && !X_IS_ARRAY( columns ) )
      x_arrayNew( columns, 0 );
}

// Add a column: bBlock returns the value of the current record, cPicture formats it as Transform();
// nWidth is the number of characters, as TBColumn:width; without it the column is as wide as the value of
// the block, a field of the data, or as the heading when it is longer; nAlign is 0 left, 1 right, 2 center,
// NIL by the type of the value (numbers on the right); a logical value shows the tick U+221A when true, and Space
// or a click on it gives bBlock the opposite value, as the get/set block of TBColumn:
// {| l | IIF( l == NIL, x, x := l ) }
// oBrowse:AddColumn( cHeading, bBlock, [nWidth], [cPicture], [nAlign] ) --> nColumn
X_FUNC_STATIC( GUIBROWSE_ADDCOLUMN )
{
   HWND window = x_guiSelfHandle( ctx, "GUIBROWSE:ADDCOLUMN" );
   X_ITEM *columns = x_objDataSlot( ctx, 7 );
   X_ITEM column __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM value __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM block __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   int number, width, align, valid;

   if( window == NULL || columns == NULL || !X_IS_ARRAY( columns ) )
      return;

   valid = X_ISBLOCK( ctx, 2 ) && ( X_ISNIL( ctx, 4 ) || X_ISCHAR( ctx, 4 ) );
   valid = valid && ( X_ISNIL( ctx, 3 ) || X_ISNUM( ctx, 3 ) ) && ( X_ISNIL( ctx, 5 ) || X_ISNUM( ctx, 5 ) );
   if( !valid )
   {
      x_guiRaise( ctx, "GUIBROWSE:ADDCOLUMN" );
      return;
   }

   if( X_ISNUM( ctx, 3 ) )
      width = x_parni( ctx, 3 );
   else
   {
      x_itemCopy( &block, x_paritem( ctx, 2 ) );
      x_evalBlock( ctx, &value, &block, 0, NULL );
      width = x_guiBrowseTextWidth( x_paritem( ctx, 1 ), &value, IIF( X_ISCHAR( ctx, 4 ), x_parc( ctx, 4 ), NULL ) );
   }

   align = IIF( X_ISNUM( ctx, 5 ), x_parni( ctx, 5 ), -1 );
   number = x_guiBrowseAddColumn( ctx, window, x_paritem( ctx, 1 ), width, align );
   if( number == 0 )
      return;

   x_arrayNew( &column, 2 );
   x_itemCopy( x_arrayGetItemPtr( &column, 1 ), x_paritem( ctx, 2 ) );
   x_itemPutC( x_arrayGetItemPtr( &column, 2 ), IIF( X_ISCHAR( ctx, 4 ), x_parc( ctx, 4 ), "" ) );
   x_arrayAdd( columns, &column );
   x_retni( ctx, number );
}

// Read the rows again around the current record, after the data changed or the record was moved
// oBrowse:Refresh() --> self
X_FUNC_STATIC( GUIBROWSE_REFRESH )
{
   HWND window = x_guiSelfHandle( ctx, "GUIBROWSE:REFRESH" );

   if( window != NULL )
   {
      x_guiBrowseRefresh( window );
      x_guiReturnSelf( ctx );
   }
}

// Number of the first columns that stay at the left when the table scrolls sideways, as TBrowse:freeze
// oBrowse:freeze --> nColumns
X_FUNC_STATIC( GUIBROWSE_GETFREEZE )
{
   x_guiBrowseFreeze( ctx );
}

// oBrowse:freeze := nColumns
X_FUNC_STATIC( GUIBROWSE_SETFREEZE )
{
   x_guiBrowseSetFreeze( ctx );
}

// Number of the column with the cursor, counted from 1, as in TBrowse
// oBrowse:colPos --> nColumn
X_FUNC_STATIC( GUIBROWSE_GETCOLPOS )
{
   x_guiBrowseColPos( ctx );
}

// Move the cursor to the column nColumn; the table scrolls so that the column is seen
// oBrowse:colPos := nColumn
X_FUNC_STATIC( GUIBROWSE_SETCOLPOS )
{
   x_guiBrowseSetColPos( ctx );
}

// Run goTopBlock, goBottomBlock, skipBlock, positionBlock or emptyBlock (internal)
// X_GUI_EVENT_NAVIGATE of oBrowse( nOperation, nArgument ) --> nResult
X_FUNC_STATIC( GUIBROWSE_NAVIGATE )
{
   const X_ITEM *goTop = x_objDataSlot( ctx, 1 );
   const X_ITEM *goBottom = x_objDataSlot( ctx, 2 );
   const X_ITEM *skip = x_objDataSlot( ctx, 3 );
   const X_ITEM *position = x_objDataSlot( ctx, 4 );
   const X_ITEM *empty = x_objDataSlot( ctx, 8 );

   x_guiBrowseNavigate( ctx, goTop, goBottom, skip, position, empty );
}

// Texts and types of the columns of the current record (internal)
// X_GUI_EVENT_ROW of oBrowse() --> aTextsAndTypes
X_FUNC_STATIC( GUIBROWSE_ROW )
{
   x_guiBrowseRow( ctx, x_objDataSlot( ctx, 7 ) );
}

// Give the column nColumn of logical values the opposite value of the current record (internal)
// X_GUI_EVENT_TOGGLE of oBrowse( nColumn ) --> NIL
X_FUNC_STATIC( GUIBROWSE_TOGGLE )
{
   x_guiBrowseToggle( ctx, x_objDataSlot( ctx, 7 ), x_parni( ctx, 1 ) );
}

// Run OnChange (1) or OnDoubleClick (2) (internal)
// X_GUI_EVENT_BROWSE of oBrowse( nEvent ) --> NIL
X_FUNC_STATIC( GUIBROWSE_EVENT )
{
   x_guiEvent( ctx, IIF( x_parni( ctx, 1 ) == 1, 5, 6 ) );
}

// Color of the header with the column titles (0xRRGGBB)
// oBrowse:headerBackground --> nColor
X_FUNC_STATIC( GUIBROWSE_GETHEADERBACKGROUND )
{
   x_guiBrowseColor( ctx, X_GUI_GRID_HEADBACKGROUND );
}

// oBrowse:headerBackground := nColor
X_FUNC_STATIC( GUIBROWSE_SETHEADERBACKGROUND )
{
   x_guiBrowseSetColor( ctx, X_GUI_GRID_HEADBACKGROUND );
}

// Text color of the column titles (0xRRGGBB)
// oBrowse:headerForeground --> nColor
X_FUNC_STATIC( GUIBROWSE_GETHEADERFOREGROUND )
{
   x_guiBrowseColor( ctx, X_GUI_GRID_HEADFOREGROUND );
}

// oBrowse:headerForeground := nColor
X_FUNC_STATIC( GUIBROWSE_SETHEADERFOREGROUND )
{
   x_guiBrowseSetColor( ctx, X_GUI_GRID_HEADFOREGROUND );
}

// Color of the lines between the column titles (0xRRGGBB)
// oBrowse:headerLine --> nColor
X_FUNC_STATIC( GUIBROWSE_GETHEADERLINE )
{
   x_guiBrowseColor( ctx, X_GUI_GRID_HEADLINE );
}

// oBrowse:headerLine := nColor
X_FUNC_STATIC( GUIBROWSE_SETHEADERLINE )
{
   x_guiBrowseSetColor( ctx, X_GUI_GRID_HEADLINE );
}

// Color of the rows (0xRRGGBB)
// oBrowse:background --> nColor
X_FUNC_STATIC( GUIBROWSE_GETBACKGROUND )
{
   x_guiBrowseColor( ctx, X_GUI_GRID_BACKGROUND );
}

// oBrowse:background := nColor
X_FUNC_STATIC( GUIBROWSE_SETBACKGROUND )
{
   x_guiBrowseSetColor( ctx, X_GUI_GRID_BACKGROUND );
}

// Color of every second row (0xRRGGBB)
// oBrowse:alternateBackground --> nColor
X_FUNC_STATIC( GUIBROWSE_GETALTERNATEBACKGROUND )
{
   x_guiBrowseColor( ctx, X_GUI_GRID_ALTERNATEBACKGROUND );
}

// oBrowse:alternateBackground := nColor
X_FUNC_STATIC( GUIBROWSE_SETALTERNATEBACKGROUND )
{
   x_guiBrowseSetColor( ctx, X_GUI_GRID_ALTERNATEBACKGROUND );
}

// Text color of the rows (0xRRGGBB)
// oBrowse:foreground --> nColor
X_FUNC_STATIC( GUIBROWSE_GETFOREGROUND )
{
   x_guiBrowseColor( ctx, X_GUI_GRID_FOREGROUND );
}

// oBrowse:foreground := nColor
X_FUNC_STATIC( GUIBROWSE_SETFOREGROUND )
{
   x_guiBrowseSetColor( ctx, X_GUI_GRID_FOREGROUND );
}

// Color of the lines between the rows and the columns (0xRRGGBB)
// oBrowse:gridLine --> nColor
X_FUNC_STATIC( GUIBROWSE_GETGRIDLINE )
{
   x_guiBrowseColor( ctx, X_GUI_GRID_LINE );
}

// oBrowse:gridLine := nColor
X_FUNC_STATIC( GUIBROWSE_SETGRIDLINE )
{
   x_guiBrowseSetColor( ctx, X_GUI_GRID_LINE );
}

// Color of the cursor row (0xRRGGBB)
// oBrowse:cursorBackground --> nColor
X_FUNC_STATIC( GUIBROWSE_GETCURSORBACKGROUND )
{
   x_guiBrowseColor( ctx, X_GUI_GRID_CURSORBACKGROUND );
}

// oBrowse:cursorBackground := nColor
X_FUNC_STATIC( GUIBROWSE_SETCURSORBACKGROUND )
{
   x_guiBrowseSetColor( ctx, X_GUI_GRID_CURSORBACKGROUND );
}

// Text color of the cursor row (0xRRGGBB)
// oBrowse:cursorForeground --> nColor
X_FUNC_STATIC( GUIBROWSE_GETCURSORFOREGROUND )
{
   x_guiBrowseColor( ctx, X_GUI_GRID_CURSORFOREGROUND );
}

// oBrowse:cursorForeground := nColor
X_FUNC_STATIC( GUIBROWSE_SETCURSORFOREGROUND )
{
   x_guiBrowseSetColor( ctx, X_GUI_GRID_CURSORFOREGROUND );
}

// Color of the cursor row while the table has no focus (0xRRGGBB)
// oBrowse:inactiveCursor --> nColor
X_FUNC_STATIC( GUIBROWSE_GETINACTIVECURSOR )
{
   x_guiBrowseColor( ctx, X_GUI_GRID_INACTIVECURSOR );
}

// oBrowse:inactiveCursor := nColor
X_FUNC_STATIC( GUIBROWSE_SETINACTIVECURSOR )
{
   x_guiBrowseSetColor( ctx, X_GUI_GRID_INACTIVECURSOR );
}

// Color of the tick of a logical column (0xRRGGBB); on the cursor the tick has the color of the cursor text
// oBrowse:checkForeground --> nColor
X_FUNC_STATIC( GUIBROWSE_GETCHECKFOREGROUND )
{
   x_guiBrowseColor( ctx, X_GUI_GRID_CHECKFOREGROUND );
}

// oBrowse:checkForeground := nColor
X_FUNC_STATIC( GUIBROWSE_SETCHECKFOREGROUND )
{
   x_guiBrowseSetColor( ctx, X_GUI_GRID_CHECKFOREGROUND );
}

// Table of records read through goTopBlock, goBottomBlock and skipBlock { |n| nSkipped }, like TBrowse;
// positionBlock may return 0..1 for the scroll bar; emptyBlock { || lEmpty } gives .T. when there is no
// record, and the table then shows no row; OnChange runs when the cursor reaches another record,
// OnDoubleClick on a double click or Enter; both receive the control
// guiBrowse() --> oBrowse
X_FUNC( GUIBROWSE )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 8, "GUIBROWSE" );

      // Parent class
      x_clsAddParent( s_class, x_clsRequire( "GUICONTROL", GUICONTROL ) );

      // Instance variables
      x_clsAddData( s_class, "GOTOPBLOCK", 1 );
      x_clsSetDataType( s_class, "GOTOPBLOCK",        X_OO_VALUE_CODEBLOCK );
      x_clsAddData( s_class, "GOBOTTOMBLOCK", 2 );
      x_clsSetDataType( s_class, "GOBOTTOMBLOCK",     X_OO_VALUE_CODEBLOCK );
      x_clsAddData( s_class, "SKIPBLOCK", 3 );
      x_clsSetDataType( s_class, "SKIPBLOCK",         X_OO_VALUE_CODEBLOCK );
      x_clsAddData( s_class, "POSITIONBLOCK", 4 );
      x_clsSetDataType( s_class, "POSITIONBLOCK",     X_OO_VALUE_CODEBLOCK );
      x_clsAddData( s_class, "ONCHANGE", 5 );
      x_clsSetDataType( s_class, "ONCHANGE",          X_OO_VALUE_CODEBLOCK );
      x_clsAddData( s_class, "ONDOUBLECLICK", 6 );
      x_clsSetDataType( s_class, "ONDOUBLECLICK",     X_OO_VALUE_CODEBLOCK );
      x_clsAddData( s_class, "COLUMNS", 7 );
      x_clsSetScope( s_class, "COLUMNS",              X_OO_CLSTP_HIDDEN );
      x_clsAddData( s_class, "EMPTYBLOCK", 8 );
      x_clsSetDataType( s_class, "EMPTYBLOCK",        X_OO_VALUE_CODEBLOCK );

      // Methods
      x_clsAdd( s_class, "NEW",                       X_FUNCNAME( GUIBROWSE_NEW ) );
      x_clsAdd( s_class, "ADDCOLUMN",                 X_FUNCNAME( GUIBROWSE_ADDCOLUMN ) );
      x_clsAdd( s_class, "REFRESH",                   X_FUNCNAME( GUIBROWSE_REFRESH ) );

      // ACCESS and ASSIGN methods
      x_clsAddAccess( s_class, "FREEZE",              X_FUNCNAME( GUIBROWSE_GETFREEZE ) );
      x_clsAddAssign( s_class, "FREEZE",              X_FUNCNAME( GUIBROWSE_SETFREEZE ) );
      x_clsAddAccess( s_class, "COLPOS",              X_FUNCNAME( GUIBROWSE_GETCOLPOS ) );
      x_clsAddAssign( s_class, "COLPOS",              X_FUNCNAME( GUIBROWSE_SETCOLPOS ) );
      x_clsAddAccess( s_class, "HEADERBACKGROUND",    X_FUNCNAME( GUIBROWSE_GETHEADERBACKGROUND ) );
      x_clsAddAssign( s_class, "HEADERBACKGROUND",    X_FUNCNAME( GUIBROWSE_SETHEADERBACKGROUND ) );
      x_clsAddAccess( s_class, "HEADERFOREGROUND",    X_FUNCNAME( GUIBROWSE_GETHEADERFOREGROUND ) );
      x_clsAddAssign( s_class, "HEADERFOREGROUND",    X_FUNCNAME( GUIBROWSE_SETHEADERFOREGROUND ) );
      x_clsAddAccess( s_class, "HEADERLINE",          X_FUNCNAME( GUIBROWSE_GETHEADERLINE ) );
      x_clsAddAssign( s_class, "HEADERLINE",          X_FUNCNAME( GUIBROWSE_SETHEADERLINE ) );
      x_clsAddAccess( s_class, "BACKGROUND",          X_FUNCNAME( GUIBROWSE_GETBACKGROUND ) );
      x_clsAddAssign( s_class, "BACKGROUND",          X_FUNCNAME( GUIBROWSE_SETBACKGROUND ) );
      x_clsAddAccess( s_class, "ALTERNATEBACKGROUND", X_FUNCNAME( GUIBROWSE_GETALTERNATEBACKGROUND ) );
      x_clsAddAssign( s_class, "ALTERNATEBACKGROUND", X_FUNCNAME( GUIBROWSE_SETALTERNATEBACKGROUND ) );
      x_clsAddAccess( s_class, "FOREGROUND",          X_FUNCNAME( GUIBROWSE_GETFOREGROUND ) );
      x_clsAddAssign( s_class, "FOREGROUND",          X_FUNCNAME( GUIBROWSE_SETFOREGROUND ) );
      x_clsAddAccess( s_class, "GRIDLINE",            X_FUNCNAME( GUIBROWSE_GETGRIDLINE ) );
      x_clsAddAssign( s_class, "GRIDLINE",            X_FUNCNAME( GUIBROWSE_SETGRIDLINE ) );
      x_clsAddAccess( s_class, "CURSORBACKGROUND",    X_FUNCNAME( GUIBROWSE_GETCURSORBACKGROUND ) );
      x_clsAddAssign( s_class, "CURSORBACKGROUND",    X_FUNCNAME( GUIBROWSE_SETCURSORBACKGROUND ) );
      x_clsAddAccess( s_class, "CURSORFOREGROUND",    X_FUNCNAME( GUIBROWSE_GETCURSORFOREGROUND ) );
      x_clsAddAssign( s_class, "CURSORFOREGROUND",    X_FUNCNAME( GUIBROWSE_SETCURSORFOREGROUND ) );
      x_clsAddAccess( s_class, "INACTIVECURSOR",      X_FUNCNAME( GUIBROWSE_GETINACTIVECURSOR ) );
      x_clsAddAssign( s_class, "INACTIVECURSOR",      X_FUNCNAME( GUIBROWSE_SETINACTIVECURSOR ) );
      x_clsAddAccess( s_class, "CHECKFOREGROUND",     X_FUNCNAME( GUIBROWSE_GETCHECKFOREGROUND ) );
      x_clsAddAssign( s_class, "CHECKFOREGROUND",     X_FUNCNAME( GUIBROWSE_SETCHECKFOREGROUND ) );

      // Library events, not methods of the class
      x_guiHandlerAdd( s_class, X_GUI_EVENT_NAVIGATE, X_FUNCNAME( GUIBROWSE_NAVIGATE ) );
      x_guiHandlerAdd( s_class, X_GUI_EVENT_ROW, X_FUNCNAME( GUIBROWSE_ROW ) );
      x_guiHandlerAdd( s_class, X_GUI_EVENT_BROWSE, X_FUNCNAME( GUIBROWSE_EVENT ) );
      x_guiHandlerAdd( s_class, X_GUI_EVENT_TOGGLE, X_FUNCNAME( GUIBROWSE_TOGGLE ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   guiToolbar
   --------------------------------------------------------------------------------------------- */

// Create a toolbar at the side cSide of the window: "Top" (default), "Bottom" (above the status bar, also .T.),
// the buttons sharing the whole width of the window in one row, "Left" or "Right" (between the top and bottom
// bars, the buttons one under another with the text under the picture)
// guiToolbar():New( oWindow, [cSide] ) --> self
X_FUNC_STATIC( GUITOOLBAR_NEW )
{
   X_ITEM *actions = x_objDataSlot( ctx, 1 );

   x_guiToolbarNew( ctx );
   if( actions != NULL && !X_IS_ARRAY( actions ) )
      x_arrayNew( actions, 0 );
}

// Add a button with the text and an optional BMP picture; bAction runs on click and receives the toolbar;
// cShortcut is written as in guiMenu, e.g. "Ctrl+N", "Del", "F2", and without cToolTip it is shown in the tip;
// returns the number of the button, counted from 1 without separators
// oToolbar:Add( cText, [cBitmap], [bAction], [cToolTip], [cShortcut] ) --> nButton
X_FUNC_STATIC( GUITOOLBAR_ADD )
{
   HWND window = x_guiSelfHandle( ctx, "GUITOOLBAR:ADD" );
   X_ITEM *actions = x_objDataSlot( ctx, 1 );
   const X_ITEM *toolTip = x_paritem( ctx, 4 );
   const X_ITEM *shortcut = x_paritem( ctx, 5 );
   X_ITEM nil = X_ITEM_NIL_INIT;
   int button;

   if( window == NULL || actions == NULL || !X_IS_ARRAY( actions ) )
      return;

   if( !X_ISNIL( ctx, 3 ) && !X_ISBLOCK( ctx, 3 ) )
   {
      x_guiRaise( ctx, "GUITOOLBAR:ADD" );
      return;
   }

   button = x_guiToolbarAdd( ctx, window, x_paritem( ctx, 1 ), x_paritem( ctx, 2 ), toolTip, shortcut, 0 );
   if( button != 0 )
   {
      x_arrayAdd( actions, IIF( x_paritem( ctx, 3 ) != NULL, x_paritem( ctx, 3 ), &nil ) );
      x_retni( ctx, button );
   }
}

// Add a button with an arrow that opens oMenu, a guiMenu():New( .T. ); a click or cShortcut opens the menu
// under the button, or above it on a bar at the bottom; the commands of the menu run as those of the menu bar
// and their bWhen makes them active or grey each time the menu opens
// oToolbar:AddMenu( cText, oMenu, [cBitmap], [cToolTip], [cShortcut] ) --> nButton
X_FUNC_STATIC( GUITOOLBAR_ADDMENU )
{
   HWND window = x_guiSelfHandle( ctx, "GUITOOLBAR:ADDMENU" );
   X_ITEM *actions = x_objDataSlot( ctx, 1 );
   const X_ITEM *toolTip = x_paritem( ctx, 4 );
   const X_ITEM *shortcut = x_paritem( ctx, 5 );
   int button;

   if( window == NULL || actions == NULL || !X_IS_ARRAY( actions ) )
      return;

   if( x_guiObjectMenu( ctx, x_paritem( ctx, 2 ), "GUITOOLBAR:ADDMENU" ) == NULL )
      return;

   button = x_guiToolbarAdd( ctx, window, x_paritem( ctx, 1 ), x_paritem( ctx, 3 ), toolTip, shortcut, 1 );
   if( button != 0 )
   {
      x_arrayAdd( actions, x_paritem( ctx, 2 ) );
      x_retni( ctx, button );
   }
}

// Add a gap between buttons
// oToolbar:AddSeparator() --> self
X_FUNC_STATIC( GUITOOLBAR_ADDSEPARATOR )
{
   HWND window = x_guiSelfHandle( ctx, "GUITOOLBAR:ADDSEPARATOR" );

   if( window != NULL )
   {
      x_guiToolbarSeparator( ctx, window );
      x_guiReturnSelf( ctx );
   }
}

// Remove all buttons, e.g. when another part of the program takes the bar
// oToolbar:Clear() --> self
X_FUNC_STATIC( GUITOOLBAR_CLEAR )
{
   HWND window = x_guiSelfHandle( ctx, "GUITOOLBAR:CLEAR" );
   X_ITEM *actions = x_objDataSlot( ctx, 1 );

   if( window == NULL || actions == NULL )
      return;

   x_guiToolbarClear( window );
   x_arrayNew( actions, 0 );
   x_guiReturnSelf( ctx );
}

// Make button nButton active or grey; a grey button and its cShortcut do nothing
// oToolbar:Enable( nButton, lEnable ) --> self
X_FUNC_STATIC( GUITOOLBAR_ENABLE )
{
   HWND window = x_guiSelfHandle( ctx, "GUITOOLBAR:ENABLE" );

   if( window == NULL )
      return;

   if( !X_ISNUM( ctx, 1 ) || !X_ISLOG( ctx, 2 ) || !x_guiToolbarEnable( window, x_parni( ctx, 1 ), x_parl( ctx, 2 ) ) )
   {
      x_guiRaise( ctx, "GUITOOLBAR:ENABLE" );
      return;
   }

   x_guiReturnSelf( ctx );
}

// Text color of the buttons (0xRRGGBB), NIL for the system color
// oToolbar:foreground --> nColor
X_FUNC_STATIC( GUITOOLBAR_GETFOREGROUND )
{
   x_guiToolbarColor( ctx, X_GUI_COLOR_FOREGROUND );
}

// oToolbar:foreground := nColor, NIL returns to the system color
X_FUNC_STATIC( GUITOOLBAR_SETFOREGROUND )
{
   x_guiToolbarSetColor( ctx, X_GUI_COLOR_FOREGROUND );
}

// Color of the bar and of the buttons, NIL for the system color
// oToolbar:background --> nColor
X_FUNC_STATIC( GUITOOLBAR_GETBACKGROUND )
{
   x_guiToolbarColor( ctx, X_GUI_COLOR_BACKGROUND );
}

// oToolbar:background := nColor, NIL returns to the system color
X_FUNC_STATIC( GUITOOLBAR_SETBACKGROUND )
{
   x_guiToolbarSetColor( ctx, X_GUI_COLOR_BACKGROUND );
}

// Text color of the button under the mouse, NIL for the system color
// oToolbar:hoverForeground --> nColor
X_FUNC_STATIC( GUITOOLBAR_GETHOVERFOREGROUND )
{
   x_guiToolbarColor( ctx, X_GUI_COLOR_HOVERFOREGROUND );
}

// oToolbar:hoverForeground := nColor, NIL returns to the system color
X_FUNC_STATIC( GUITOOLBAR_SETHOVERFOREGROUND )
{
   x_guiToolbarSetColor( ctx, X_GUI_COLOR_HOVERFOREGROUND );
}

// Color of the button under the mouse, NIL for the system color
// oToolbar:hoverBackground --> nColor
X_FUNC_STATIC( GUITOOLBAR_GETHOVERBACKGROUND )
{
   x_guiToolbarColor( ctx, X_GUI_COLOR_HOVERBACKGROUND );
}

// oToolbar:hoverBackground := nColor, NIL returns to the system color
X_FUNC_STATIC( GUITOOLBAR_SETHOVERBACKGROUND )
{
   x_guiToolbarSetColor( ctx, X_GUI_COLOR_HOVERBACKGROUND );
}

// Text color of the button held down by the mouse, NIL for the system color
// oToolbar:pressForeground --> nColor
X_FUNC_STATIC( GUITOOLBAR_GETPRESSFOREGROUND )
{
   x_guiToolbarColor( ctx, X_GUI_COLOR_PRESSFOREGROUND );
}

// oToolbar:pressForeground := nColor, NIL returns to the system color
X_FUNC_STATIC( GUITOOLBAR_SETPRESSFOREGROUND )
{
   x_guiToolbarSetColor( ctx, X_GUI_COLOR_PRESSFOREGROUND );
}

// Color of the button held down by the mouse, NIL for the system color
// oToolbar:pressBackground --> nColor
X_FUNC_STATIC( GUITOOLBAR_GETPRESSBACKGROUND )
{
   x_guiToolbarColor( ctx, X_GUI_COLOR_PRESSBACKGROUND );
}

// oToolbar:pressBackground := nColor, NIL returns to the system color
X_FUNC_STATIC( GUITOOLBAR_SETPRESSBACKGROUND )
{
   x_guiToolbarSetColor( ctx, X_GUI_COLOR_PRESSBACKGROUND );
}

// Receive a click of button nButton (internal)
// X_GUI_EVENT_COMMAND of oToolbar( nCode, nButton ) --> NIL
X_FUNC_STATIC( GUITOOLBAR_COMMAND )
{
   const X_ITEM *actions = x_objDataSlot( ctx, 1 );
   X_ITEM block __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ARG args[1];

   if( x_parni( ctx, 1 ) != BN_CLICKED || actions == NULL || !X_IS_ARRAY( actions ) || x_parni( ctx, 2 ) < 1 )
      return;

   if( !x_arrayGet( actions, ( size_t )x_parni( ctx, 2 ), &block ) )
      return;

   /* A button of AddMenu() opens its menu */
   if( X_IS_OBJECT( &block ) )
   {
      HWND toolbar = x_guiSelfHandle( ctx, "GUITOOLBAR:ADDMENU" );
      HMENU menu = x_guiObjectMenu( ctx, &block, "GUITOOLBAR:ADDMENU" );

      x_guiToolbarPopup( toolbar, x_parni( ctx, 2 ), menu );
      return;
   }

   if( !X_IS_BLOCK( &block ) )
      return;

   args[0] = ( X_ARG ){ ctx->self, 0 };
   x_evalBlock( ctx, &result, &block, 1, args );
}

// Bar of buttons with a picture and the text on the right, drawn by the system or in the colors set
// guiToolbar() --> oToolbar
X_FUNC( GUITOOLBAR )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 1, "GUITOOLBAR" );

      // Parent class
      x_clsAddParent( s_class, x_clsRequire( "GUICONTROL", GUICONTROL ) );

      // Instance variables
      x_clsAddData( s_class, "ACTIONS", 1 );
      x_clsSetScope( s_class, "ACTIONS",          X_OO_CLSTP_HIDDEN );

      // Methods
      x_clsAdd( s_class, "NEW",                   X_FUNCNAME( GUITOOLBAR_NEW ) );
      x_clsAdd( s_class, "ADD",                   X_FUNCNAME( GUITOOLBAR_ADD ) );
      x_clsAdd( s_class, "ADDMENU",               X_FUNCNAME( GUITOOLBAR_ADDMENU ) );
      x_clsAdd( s_class, "ADDSEPARATOR",          X_FUNCNAME( GUITOOLBAR_ADDSEPARATOR ) );
      x_clsAdd( s_class, "CLEAR",                 X_FUNCNAME( GUITOOLBAR_CLEAR ) );
      x_clsAdd( s_class, "ENABLE",                X_FUNCNAME( GUITOOLBAR_ENABLE ) );

      // ACCESS and ASSIGN methods
      x_clsAddAccess( s_class, "FOREGROUND",      X_FUNCNAME( GUITOOLBAR_GETFOREGROUND ) );
      x_clsAddAssign( s_class, "FOREGROUND",      X_FUNCNAME( GUITOOLBAR_SETFOREGROUND ) );
      x_clsAddAccess( s_class, "BACKGROUND",      X_FUNCNAME( GUITOOLBAR_GETBACKGROUND ) );
      x_clsAddAssign( s_class, "BACKGROUND",      X_FUNCNAME( GUITOOLBAR_SETBACKGROUND ) );
      x_clsAddAccess( s_class, "HOVERFOREGROUND", X_FUNCNAME( GUITOOLBAR_GETHOVERFOREGROUND ) );
      x_clsAddAssign( s_class, "HOVERFOREGROUND", X_FUNCNAME( GUITOOLBAR_SETHOVERFOREGROUND ) );
      x_clsAddAccess( s_class, "HOVERBACKGROUND", X_FUNCNAME( GUITOOLBAR_GETHOVERBACKGROUND ) );
      x_clsAddAssign( s_class, "HOVERBACKGROUND", X_FUNCNAME( GUITOOLBAR_SETHOVERBACKGROUND ) );
      x_clsAddAccess( s_class, "PRESSFOREGROUND", X_FUNCNAME( GUITOOLBAR_GETPRESSFOREGROUND ) );
      x_clsAddAssign( s_class, "PRESSFOREGROUND", X_FUNCNAME( GUITOOLBAR_SETPRESSFOREGROUND ) );
      x_clsAddAccess( s_class, "PRESSBACKGROUND", X_FUNCNAME( GUITOOLBAR_GETPRESSBACKGROUND ) );
      x_clsAddAssign( s_class, "PRESSBACKGROUND", X_FUNCNAME( GUITOOLBAR_SETPRESSBACKGROUND ) );

      // Library events, not methods of the class
      x_guiHandlerAdd( s_class, X_GUI_EVENT_COMMAND, X_FUNCNAME( GUITOOLBAR_COMMAND ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   guiSidebar
   --------------------------------------------------------------------------------------------- */

// Create a sidebar without pages at the left edge of the window, between the bars at the top and at the bottom;
// the panels filling the window make room for it; a toolbar "Left" does not go together with it
// guiSidebar():New( oWindow ) --> self
X_FUNC_STATIC( GUISIDEBAR_NEW )
{
   X_ITEM *actions = x_objDataSlot( ctx, 1 );
   X_ITEM *items = x_objDataSlot( ctx, 3 );
   X_ITEM buttons __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;

   x_guiSidebarNew( ctx );
   if( actions != NULL && !X_IS_ARRAY( actions ) )
   {
      x_arrayNew( actions, 0 );
      x_arrayNew( &buttons, 0 );
      x_arrayAdd( actions, &buttons );
   }

   if( items != NULL && !X_IS_ARRAY( items ) )
      x_arrayNew( items, 0 );
}

// Add a page: its PNG or BMP picture or guiFontIcon on the strip and a panel for its controls, under the header of its
// actions at the top; a click on the picture shows the panel, a click on the picture of the shown page hides it;
// cToolTip is the tip of the picture and the name of the page in the menu of the strip, cShortcut is written as in
// guiMenu; with oMenu the panel shows the items of the guiMenu one under another: a click on a command runs it,
// a click on a drop-down menu opens it at the right; returns the number of the page, counted from 1
// oSidebar:AddButton( cBitmap, [cToolTip], [cShortcut], [oMenu] ) --> nPage
X_FUNC_STATIC( GUISIDEBAR_ADDBUTTON )
{
   X_ITEM page __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM blocks __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   HWND window = x_guiSelfHandle( ctx, "GUISIDEBAR:ADDBUTTON" );
   X_ITEM *actions = x_objDataSlot( ctx, 1 );
   HMENU menu = NULL;
   int number, valid;

   if( window == NULL || actions == NULL || !X_IS_ARRAY( actions ) )
      return;

   valid = x_guiIsPicture( x_paritem( ctx, 1 ) ) && ( X_ISNIL( ctx, 2 ) || X_ISCHAR( ctx, 2 ) );
   if( !valid || ( !X_ISNIL( ctx, 3 ) && !X_ISCHAR( ctx, 3 ) ) )
   {
      x_guiRaise( ctx, "GUISIDEBAR:ADDBUTTON" );
      return;
   }

   if( !X_ISNIL( ctx, 4 ) )
   {
      menu = x_guiObjectMenu( ctx, x_paritem( ctx, 4 ), "GUISIDEBAR:ADDBUTTON" );
      if( menu == NULL )
         return;
   }

   if( !x_objCreate( ctx, &page, x_clsRequire( "GUIPANEL", GUIPANEL ) ) )
      return;

   number = x_guiSidebarAdd( ctx, window, &page, menu );
   if( number != 0 )
   {
      x_arrayNew( &blocks, 0 );
      x_arrayAdd( actions, &blocks );
      x_retni( ctx, number );
   }
}

// Return the panel of page nPage; controls are created on it as on a window
// oSidebar:Panel( nPage ) --> oPanel
X_FUNC_STATIC( GUISIDEBAR_PANEL )
{
   HWND window = x_guiSelfHandle( ctx, "GUISIDEBAR:PANEL" );

   if( window != NULL )
      x_guiSidebarPanel( ctx, window, x_parni( ctx, 1 ) );
}

// Add a button with a PNG or BMP picture or a guiFontIcon at the right of the header of page nPage, shown with the
// page; bAction runs on click and receives the sidebar
// oSidebar:AddAction( nPage, cBitmap, bAction, [cToolTip] ) --> self
X_FUNC_STATIC( GUISIDEBAR_ADDACTION )
{
   X_ITEM blocks __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   HWND window = x_guiSelfHandle( ctx, "GUISIDEBAR:ADDACTION" );
   X_ITEM *actions = x_objDataSlot( ctx, 1 );
   int valid;

   if( window == NULL || actions == NULL )
      return;

   valid = X_ISNUM( ctx, 1 ) && x_parni( ctx, 1 ) >= 1 && x_guiIsPicture( x_paritem( ctx, 2 ) ) && X_ISBLOCK( ctx, 3 );
   if( !valid || ( !X_ISNIL( ctx, 4 ) && !X_ISCHAR( ctx, 4 ) ) )
   {
      x_guiRaise( ctx, "GUISIDEBAR:ADDACTION" );
      return;
   }

   if( !x_guiSidebarAddButton( ctx, window, x_parni( ctx, 1 ), x_paritem( ctx, 2 ), x_paritem( ctx, 4 ) ) )
      return;

   if( x_arrayGet( actions, ( size_t )x_parni( ctx, 1 ) + 1, &blocks ) )
      x_arrayAdd( &blocks, x_paritem( ctx, 3 ) );

   x_guiReturnSelf( ctx );
}

// Add a button with a PNG or BMP picture or a guiFontIcon at the bottom of the strip, e.g. for the settings; bAction
// runs on click and receives the sidebar
// oSidebar:AddGlobal( cBitmap, bAction, [cToolTip] ) --> self
X_FUNC_STATIC( GUISIDEBAR_ADDGLOBAL )
{
   X_ITEM blocks __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   HWND window = x_guiSelfHandle( ctx, "GUISIDEBAR:ADDGLOBAL" );
   X_ITEM *actions = x_objDataSlot( ctx, 1 );

   if( window == NULL || actions == NULL )
      return;

   if( !x_guiIsPicture( x_paritem( ctx, 1 ) ) || !X_ISBLOCK( ctx, 2 ) || ( !X_ISNIL( ctx, 3 ) && !X_ISCHAR( ctx, 3 ) ) )
   {
      x_guiRaise( ctx, "GUISIDEBAR:ADDGLOBAL" );
      return;
   }

   if( !x_guiSidebarAddButton( ctx, window, 0, x_paritem( ctx, 1 ), x_paritem( ctx, 3 ) ) )
      return;

   if( x_arrayGet( actions, 1, &blocks ) )
      x_arrayAdd( &blocks, x_paritem( ctx, 2 ) );

   x_guiReturnSelf( ctx );
}

// Add a section to page nPage: a panel for its controls under a header with an arrow and cTitle; a click on the
// header opens or closes the panel, the open panels share the height of the page
// oSidebar:AddSection( nPage, cTitle ) --> oPanel
X_FUNC_STATIC( GUISIDEBAR_ADDSECTION )
{
   X_ITEM section __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   HWND window = x_guiSelfHandle( ctx, "GUISIDEBAR:ADDSECTION" );

   if( window == NULL )
      return;

   if( !X_ISNUM( ctx, 1 ) || !X_ISCHAR( ctx, 2 ) )
   {
      x_guiRaise( ctx, "GUISIDEBAR:ADDSECTION" );
      return;
   }

   if( x_objCreate( ctx, &section, x_clsRequire( "GUIPANEL", GUIPANEL ) ) )
      x_guiSidebarAddSection( ctx, window, &section );
}

// Add a row to the list of page nPage, as in a list of extensions: the PNG or BMP picture or guiFontIcon at the left,
// cTitle in bold, cText under it and cInfo at the bottom; a click on the row runs bAction, a click on the button of
// the settings at the right of cInfo, shown with bSettings, runs bSettings; both receive the sidebar and the number
// of the row; the list scrolls when it does not fit the panel; returns the number of the row, counted from 1
// oSidebar:AddItem( nPage, cBitmap, cTitle, [cText], [cInfo], [bAction], [bSettings] ) --> nItem
X_FUNC_STATIC( GUISIDEBAR_ADDITEM )
{
   X_ITEM empty __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM rows __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM blocks __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   HWND window = x_guiSelfHandle( ctx, "GUISIDEBAR:ADDITEM" );
   X_ITEM *items = x_objDataSlot( ctx, 3 );
   int number, valid;

   if( window == NULL || items == NULL )
      return;

   valid = X_ISNUM( ctx, 1 ) && x_guiIsPicture( x_paritem( ctx, 2 ) ) && X_ISCHAR( ctx, 3 );
   valid = valid && ( X_ISNIL( ctx, 4 ) || X_ISCHAR( ctx, 4 ) ) && ( X_ISNIL( ctx, 5 ) || X_ISCHAR( ctx, 5 ) );
   valid = valid && ( X_ISNIL( ctx, 6 ) || X_ISBLOCK( ctx, 6 ) ) && ( X_ISNIL( ctx, 7 ) || X_ISBLOCK( ctx, 7 ) );
   if( !valid )
   {
      x_guiRaise( ctx, "GUISIDEBAR:ADDITEM" );
      return;
   }

   number = x_guiSidebarAddItem( ctx, window, x_parni( ctx, 1 ) );
   if( number == 0 )
      return;

   // ITEMS keeps for every page the array of its rows, each row { bAction, bSettings }
   while( x_arrayLen( items ) < x_parni( ctx, 1 ) )
   {
      x_arrayNew( &empty, 0 );
      x_arrayAdd( items, &empty );
   }

   x_arrayGet( items, ( size_t )x_parni( ctx, 1 ), &rows );
   x_arrayNew( &blocks, 2 );
   if( X_ISBLOCK( ctx, 6 ) )
      x_arraySet( &blocks, 1, x_paritem( ctx, 6 ) );

   if( X_ISBLOCK( ctx, 7 ) )
      x_arraySet( &blocks, 2, x_paritem( ctx, 7 ) );

   x_arrayAdd( &rows, &blocks );
   x_retni( ctx, number );
}

// Remove every row of the list of page nPage, e.g. before the list is filled again
// oSidebar:ClearItems( nPage ) --> self
X_FUNC_STATIC( GUISIDEBAR_CLEARITEMS )
{
   X_ITEM empty __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   HWND window = x_guiSelfHandle( ctx, "GUISIDEBAR:CLEARITEMS" );
   X_ITEM *items = x_objDataSlot( ctx, 3 );

   if( window == NULL || items == NULL )
      return;

   if( !X_ISNUM( ctx, 1 ) || !x_guiSidebarClearItems( window, x_parni( ctx, 1 ) ) )
   {
      x_guiRaise( ctx, "GUISIDEBAR:CLEARITEMS" );
      return;
   }

   if( x_arrayLen( items ) >= x_parni( ctx, 1 ) )
   {
      x_arrayNew( &empty, 0 );
      x_arraySet( items, ( size_t )x_parni( ctx, 1 ), &empty );
   }

   x_guiReturnSelf( ctx );
}

// Show nCount in a circle at the bottom right of the picture of page nPage, e.g. the number of new things;
// 0 hides it, above 99 it shows 99+
// oSidebar:Badge( nPage, nCount ) --> self
X_FUNC_STATIC( GUISIDEBAR_BADGE )
{
   HWND window = x_guiSelfHandle( ctx, "GUISIDEBAR:BADGE" );
   int valid;

   if( window == NULL )
      return;

   valid = X_ISNUM( ctx, 1 ) && X_ISNUM( ctx, 2 ) && x_parni( ctx, 2 ) >= 0;
   if( !valid || !x_guiSidebarBadge( window, x_parni( ctx, 1 ), x_parni( ctx, 2 ) ) )
   {
      x_guiRaise( ctx, "GUISIDEBAR:BADGE" );
      return;
   }

   x_guiReturnSelf( ctx );
}

// Return the number of the shown page, 0 when the panel is hidden
// oSidebar:page --> nPage
X_FUNC_STATIC( GUISIDEBAR_GETPAGE )
{
   HWND window = x_guiSelfHandle( ctx, "GUISIDEBAR:PAGE" );

   if( window != NULL )
      x_retni( ctx, x_guiSidebarPage( window ) );
}

// Show page nPage; 0 hides the panel and the window takes its room
// oSidebar:page := nPage
X_FUNC_STATIC( GUISIDEBAR_SETPAGE )
{
   HWND window = x_guiSelfHandle( ctx, "GUISIDEBAR:PAGE" );

   if( window == NULL )
      return;

   if( !X_ISNUM( ctx, 1 ) )
   {
      x_guiRaise( ctx, "GUISIDEBAR:PAGE" );
      return;
   }

   x_guiSidebarSelect( ctx, window, x_parni( ctx, 1 ) );
   x_retni( ctx, x_parni( ctx, 1 ) );
}

// Return the width of the panel (logical pixels), without the strip of pictures; 260 at the start
// oSidebar:width --> nWidth
X_FUNC_STATIC( GUISIDEBAR_GETWIDTH )
{
   HWND window = x_guiSelfHandle( ctx, "GUISIDEBAR:WIDTH" );

   if( window != NULL )
      x_retni( ctx, x_guiSidebarWidth( window ) );
}

// Set the width of the panel, at least 170; the mouse changes it by dragging the right edge of the panel
// oSidebar:width := nWidth
X_FUNC_STATIC( GUISIDEBAR_SETWIDTH )
{
   HWND window = x_guiSelfHandle( ctx, "GUISIDEBAR:WIDTH" );

   if( window == NULL )
      return;

   if( !X_ISNUM( ctx, 1 ) )
   {
      x_guiRaise( ctx, "GUISIDEBAR:WIDTH" );
      return;
   }

   x_guiSidebarSetWidth( window, x_parni( ctx, 1 ) );
   x_retni( ctx, x_guiSidebarWidth( window ) );
}

// Color of the strip of pictures (0xRRGGBB), NIL for the color of the panel a shade towards its text
// oSidebar:barBackground --> nColor
X_FUNC_STATIC( GUISIDEBAR_GETBARBACKGROUND )
{
   x_guiSidebarColor( ctx, X_GUI_SIDEBAR_COLOR_BAR );
}

// oSidebar:barBackground := nColor, NIL returns to the default color
X_FUNC_STATIC( GUISIDEBAR_SETBARBACKGROUND )
{
   x_guiSidebarSetColor( ctx, X_GUI_SIDEBAR_COLOR_BAR );
}

// Color of the mark at the picture of the shown page, NIL for the selection color of the system
// oSidebar:accent --> nColor
X_FUNC_STATIC( GUISIDEBAR_GETACCENT )
{
   x_guiSidebarColor( ctx, X_GUI_SIDEBAR_COLOR_ACCENT );
}

// oSidebar:accent := nColor, NIL returns to the system color
X_FUNC_STATIC( GUISIDEBAR_SETACCENT )
{
   x_guiSidebarSetColor( ctx, X_GUI_SIDEBAR_COLOR_ACCENT );
}

// Run the block of button nButton of page nPage, of the strip for 0 (internal)
// X_GUI_EVENT_COMMAND of oSidebar( nPage, nButton ) --> NIL
X_FUNC_STATIC( GUISIDEBAR_COMMAND )
{
   const X_ITEM *actions = x_objDataSlot( ctx, 1 );
   X_ITEM blocks __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM block __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ARG args[1];

   if( actions == NULL || x_parni( ctx, 1 ) < 0 || x_parni( ctx, 2 ) < 1 )
      return;

   if( !x_arrayGet( actions, ( size_t )x_parni( ctx, 1 ) + 1, &blocks ) )
      return;

   if( !x_arrayGet( &blocks, ( size_t )x_parni( ctx, 2 ), &block ) || !X_IS_BLOCK( &block ) )
      return;

   args[0] = ( X_ARG ){ ctx->self, 0 };
   x_evalBlock( ctx, &result, &block, 1, args );
}

// Run OnChange with the sidebar and the number of the shown page after a click on the strip (internal)
// X_GUI_EVENT_CHANGE of oSidebar() --> NIL
X_FUNC_STATIC( GUISIDEBAR_CHANGE )
{
   const X_ITEM *data = x_objDataSlot( ctx, 2 );
   HWND window = x_guiSelfHandle( ctx, "GUISIDEBAR" );
   X_ITEM block __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM page __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ARG args[2];

   if( window == NULL || data == NULL || !X_IS_BLOCK( data ) )
      return;

   x_itemCopy( &block, data );
   x_itemPutNI( &page, x_guiSidebarPage( window ) );
   args[0] = ( X_ARG ){ ctx->self, 0 };
   args[1] = ( X_ARG ){ &page, 0 };
   x_evalBlock( ctx, &result, &block, 2, args );
}

// Run the block of row nItem of the list of page nPage: bAction for nPart 1, bSettings for nPart 2 (internal)
// X_GUI_EVENT_ITEM of oSidebar( nPage, nItem, nPart ) --> NIL
X_FUNC_STATIC( GUISIDEBAR_ITEM )
{
   const X_ITEM *items = x_objDataSlot( ctx, 3 );
   X_ITEM rows __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM blocks __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM block __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM number __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ARG args[2];

   if( !x_arrayGet( items, ( size_t )x_parni( ctx, 1 ), &rows ) )
      return;

   if( !x_arrayGet( &rows, ( size_t )x_parni( ctx, 2 ), &blocks ) )
      return;

   if( !x_arrayGet( &blocks, ( size_t )x_parni( ctx, 3 ), &block ) || !X_IS_BLOCK( &block ) )
      return;

   x_itemPutNI( &number, x_parni( ctx, 2 ) );
   args[0] = ( X_ARG ){ ctx->self, 0 };
   args[1] = ( X_ARG ){ &number, 0 };
   x_evalBlock( ctx, &result, &block, 2, args );
}

// Sidebar as in code editors: a strip of page pictures at the left with buttons at its bottom and the panel of
// the shown page with its title and actions; OnChange receives the sidebar and the number of the shown page
// guiSidebar() --> oSidebar
X_FUNC( GUISIDEBAR )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 3, "GUISIDEBAR" );

      // Parent class
      x_clsAddParent( s_class, x_clsRequire( "GUIWINDOW", GUIWINDOW ) );

      // Instance variables
      x_clsAddData( s_class, "ACTIONS", 1 );
      x_clsSetScope( s_class, "ACTIONS",        X_OO_CLSTP_HIDDEN );
      x_clsAddData( s_class, "ONCHANGE", 2 );
      x_clsSetDataType( s_class, "ONCHANGE",    X_OO_VALUE_CODEBLOCK );
      x_clsAddData( s_class, "ITEMS", 3 );
      x_clsSetScope( s_class, "ITEMS",          X_OO_CLSTP_HIDDEN );

      // Methods
      x_clsAdd( s_class, "NEW",                 X_FUNCNAME( GUISIDEBAR_NEW ) );
      x_clsAdd( s_class, "ADDBUTTON",           X_FUNCNAME( GUISIDEBAR_ADDBUTTON ) );
      x_clsAdd( s_class, "PANEL",               X_FUNCNAME( GUISIDEBAR_PANEL ) );
      x_clsAdd( s_class, "ADDACTION",           X_FUNCNAME( GUISIDEBAR_ADDACTION ) );
      x_clsAdd( s_class, "ADDGLOBAL",           X_FUNCNAME( GUISIDEBAR_ADDGLOBAL ) );
      x_clsAdd( s_class, "BADGE",               X_FUNCNAME( GUISIDEBAR_BADGE ) );
      x_clsAdd( s_class, "ADDSECTION",          X_FUNCNAME( GUISIDEBAR_ADDSECTION ) );
      x_clsAdd( s_class, "ADDITEM",             X_FUNCNAME( GUISIDEBAR_ADDITEM ) );
      x_clsAdd( s_class, "CLEARITEMS",          X_FUNCNAME( GUISIDEBAR_CLEARITEMS ) );

      // ACCESS and ASSIGN methods
      x_clsAddAccess( s_class, "PAGE",          X_FUNCNAME( GUISIDEBAR_GETPAGE ) );
      x_clsAddAssign( s_class, "PAGE",          X_FUNCNAME( GUISIDEBAR_SETPAGE ) );
      x_clsAddAccess( s_class, "WIDTH",         X_FUNCNAME( GUISIDEBAR_GETWIDTH ) );
      x_clsAddAssign( s_class, "WIDTH",         X_FUNCNAME( GUISIDEBAR_SETWIDTH ) );
      x_clsAddAccess( s_class, "BARBACKGROUND", X_FUNCNAME( GUISIDEBAR_GETBARBACKGROUND ) );
      x_clsAddAssign( s_class, "BARBACKGROUND", X_FUNCNAME( GUISIDEBAR_SETBARBACKGROUND ) );
      x_clsAddAccess( s_class, "ACCENT",        X_FUNCNAME( GUISIDEBAR_GETACCENT ) );
      x_clsAddAssign( s_class, "ACCENT",        X_FUNCNAME( GUISIDEBAR_SETACCENT ) );

      // Library events, not methods of the class
      x_guiHandlerAdd( s_class, X_GUI_EVENT_COMMAND, X_FUNCNAME( GUISIDEBAR_COMMAND ) );
      x_guiHandlerAdd( s_class, X_GUI_EVENT_CHANGE, X_FUNCNAME( GUISIDEBAR_CHANGE ) );
      x_guiHandlerAdd( s_class, X_GUI_EVENT_ITEM, X_FUNCNAME( GUISIDEBAR_ITEM ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   guiDatePicker
   --------------------------------------------------------------------------------------------- */

// Create a date field with a drop-down calendar; default size 120 x 23
// guiDatePicker():New( oWindow, nX, nY, [nWidth], [nHeight] ) --> self
X_FUNC_STATIC( GUIDATEPICKER_NEW )
{
   x_guiControlNew( ctx, X_GUI_DATEPICKER, 120, 23, 0 );
}

// Return the selected date
// oDate:value --> dDate
X_FUNC_STATIC( GUIDATEPICKER_GETVALUE )
{
   HWND window = x_guiSelfHandle( ctx, "GUIDATEPICKER:VALUE" );

   if( window != NULL )
      x_retdl( ctx, x_guiDateValue( window ) );
}

// Select a date; an empty date is not allowed
// oDate:value := dDate
X_FUNC_STATIC( GUIDATEPICKER_SETVALUE )
{
   const X_ITEM *value = x_paritem( ctx, 1 );
   HWND window = x_guiSelfHandle( ctx, "GUIDATEPICKER:VALUE" );

   if( window == NULL )
      return;

   if( value == NULL || !X_IS_DATE( value ) || !x_guiDateSetValue( window, x_pardl( ctx, 1 ) ) )
   {
      x_guiRaise( ctx, "GUIDATEPICKER:VALUE" );
      return;
   }

   x_itemReturn( ctx, value );
}

// Run OnChange when the user changes the date (internal)
// X_GUI_EVENT_NOTIFY of oDate( nCode ) --> NIL
X_FUNC_STATIC( GUIDATEPICKER_NOTIFY )
{
   if( x_parni( ctx, 1 ) == ( int )DTN_DATETIMECHANGE )
      x_guiEvent( ctx, 1 );
}

// Date field; :value is a DATE, OnChange runs when the user changes it and receives the control
// guiDatePicker() --> oDate
X_FUNC( GUIDATEPICKER )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 1, "GUIDATEPICKER" );

      // Parent class
      x_clsAddParent( s_class, x_clsRequire( "GUICONTROL", GUICONTROL ) );

      // Instance variables
      x_clsAddData( s_class, "ONCHANGE", 1 );
      x_clsSetDataType( s_class, "ONCHANGE", X_OO_VALUE_CODEBLOCK );

      // Methods
      x_clsAdd( s_class, "NEW",              X_FUNCNAME( GUIDATEPICKER_NEW ) );

      // ACCESS and ASSIGN methods
      x_clsAddAccess( s_class, "VALUE",      X_FUNCNAME( GUIDATEPICKER_GETVALUE ) );
      x_clsAddAssign( s_class, "VALUE",      X_FUNCNAME( GUIDATEPICKER_SETVALUE ) );

      // Library events, not methods of the class
      x_guiHandlerAdd( s_class, X_GUI_EVENT_NOTIFY, X_FUNCNAME( GUIDATEPICKER_NOTIFY ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   guiSlider
   --------------------------------------------------------------------------------------------- */

// Create a horizontal slider with the range 0 to 100; default size 200 x 30
// guiSlider():New( oWindow, nX, nY, [nWidth], [nHeight] ) --> self
X_FUNC_STATIC( GUISLIDER_NEW )
{
   x_guiControlNew( ctx, X_GUI_SLIDER, 200, 30, 0 );
}

// Return the position of the slider
// oSlider:value --> nValue
X_FUNC_STATIC( GUISLIDER_GETVALUE )
{
   HWND window = x_guiSelfHandle( ctx, "GUISLIDER:VALUE" );

   if( window != NULL )
      x_retni( ctx, x_guiSliderGet( window, X_GUI_RANGE_VALUE ) );
}

// Move the slider to a position between the minimum and the maximum
// oSlider:value := nValue
X_FUNC_STATIC( GUISLIDER_SETVALUE )
{
   HWND window = x_guiSelfHandle( ctx, "GUISLIDER:VALUE" );
   int minimum, maximum;

   if( window == NULL )
      return;

   minimum = x_guiSliderGet( window, X_GUI_RANGE_MINIMUM );
   maximum = x_guiSliderGet( window, X_GUI_RANGE_MAXIMUM );
   if( !X_ISNUM( ctx, 1 ) || x_parni( ctx, 1 ) < minimum || x_parni( ctx, 1 ) > maximum )
   {
      x_guiRaise( ctx, "GUISLIDER:VALUE" );
      return;
   }

   x_guiSliderSet( window, X_GUI_RANGE_VALUE, x_parni( ctx, 1 ) );
   x_retni( ctx, x_parni( ctx, 1 ) );
}

// Return the lowest position
// oSlider:minimum --> nMinimum
X_FUNC_STATIC( GUISLIDER_GETMINIMUM )
{
   HWND window = x_guiSelfHandle( ctx, "GUISLIDER:MINIMUM" );

   if( window != NULL )
      x_retni( ctx, x_guiSliderGet( window, X_GUI_RANGE_MINIMUM ) );
}

// Set the lowest position; it must not be above the maximum
// oSlider:minimum := nMinimum
X_FUNC_STATIC( GUISLIDER_SETMINIMUM )
{
   HWND window = x_guiSelfHandle( ctx, "GUISLIDER:MINIMUM" );

   if( window == NULL )
      return;

   if( !X_ISNUM( ctx, 1 ) || x_parni( ctx, 1 ) > x_guiSliderGet( window, X_GUI_RANGE_MAXIMUM ) )
   {
      x_guiRaise( ctx, "GUISLIDER:MINIMUM" );
      return;
   }

   x_guiSliderSet( window, X_GUI_RANGE_MINIMUM, x_parni( ctx, 1 ) );
   x_retni( ctx, x_parni( ctx, 1 ) );
}

// Return the highest position
// oSlider:maximum --> nMaximum
X_FUNC_STATIC( GUISLIDER_GETMAXIMUM )
{
   HWND window = x_guiSelfHandle( ctx, "GUISLIDER:MAXIMUM" );

   if( window != NULL )
      x_retni( ctx, x_guiSliderGet( window, X_GUI_RANGE_MAXIMUM ) );
}

// Set the highest position; it must not be below the minimum
// oSlider:maximum := nMaximum
X_FUNC_STATIC( GUISLIDER_SETMAXIMUM )
{
   HWND window = x_guiSelfHandle( ctx, "GUISLIDER:MAXIMUM" );

   if( window == NULL )
      return;

   if( !X_ISNUM( ctx, 1 ) || x_parni( ctx, 1 ) < x_guiSliderGet( window, X_GUI_RANGE_MINIMUM ) )
   {
      x_guiRaise( ctx, "GUISLIDER:MAXIMUM" );
      return;
   }

   x_guiSliderSet( window, X_GUI_RANGE_MAXIMUM, x_parni( ctx, 1 ) );
   x_retni( ctx, x_parni( ctx, 1 ) );
}

// Run OnChange while the user moves the slider (internal)
// X_GUI_EVENT_COMMAND of oSlider( nCode ) --> NIL
X_FUNC_STATIC( GUISLIDER_COMMAND )
{
   if( x_parni( ctx, 1 ) != TB_ENDTRACK )
      x_guiEvent( ctx, 1 );
}

// Slider; OnChange runs while the user moves it and receives the control
// guiSlider() --> oSlider
X_FUNC( GUISLIDER )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 1, "GUISLIDER" );

      // Parent class
      x_clsAddParent( s_class, x_clsRequire( "GUICONTROL", GUICONTROL ) );

      // Instance variables
      x_clsAddData( s_class, "ONCHANGE", 1 );
      x_clsSetDataType( s_class, "ONCHANGE", X_OO_VALUE_CODEBLOCK );

      // Methods
      x_clsAdd( s_class, "NEW",              X_FUNCNAME( GUISLIDER_NEW ) );

      // ACCESS and ASSIGN methods
      x_clsAddAccess( s_class, "VALUE",      X_FUNCNAME( GUISLIDER_GETVALUE ) );
      x_clsAddAssign( s_class, "VALUE",      X_FUNCNAME( GUISLIDER_SETVALUE ) );
      x_clsAddAccess( s_class, "MINIMUM",    X_FUNCNAME( GUISLIDER_GETMINIMUM ) );
      x_clsAddAssign( s_class, "MINIMUM",    X_FUNCNAME( GUISLIDER_SETMINIMUM ) );
      x_clsAddAccess( s_class, "MAXIMUM",    X_FUNCNAME( GUISLIDER_GETMAXIMUM ) );
      x_clsAddAssign( s_class, "MAXIMUM",    X_FUNCNAME( GUISLIDER_SETMAXIMUM ) );

      // Library events, not methods of the class
      x_guiHandlerAdd( s_class, X_GUI_EVENT_COMMAND, X_FUNCNAME( GUISLIDER_COMMAND ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   guiListView
   --------------------------------------------------------------------------------------------- */

// Create a table with columns and rows of text; default size 300 x 200
// guiListView():New( oWindow, nX, nY, [nWidth], [nHeight] ) --> self
X_FUNC_STATIC( GUILISTVIEW_NEW )
{
   x_guiControlNew( ctx, X_GUI_LISTVIEW, 300, 200, 0 );
}

// Add a column; nAlign is 0 left, 1 right, 2 center (the first column is always left)
// oList:AddColumn( cTitle, [nWidth], [nAlign] ) --> nColumn
X_FUNC_STATIC( GUILISTVIEW_ADDCOLUMN )
{
   HWND window = x_guiSelfHandle( ctx, "GUILISTVIEW:ADDCOLUMN" );
   int width = x_parnidef( ctx, 2, 100 );
   int align = x_parnidef( ctx, 3, 0 );
   int column;

   if( window == NULL )
      return;

   column = x_guiListViewAddColumn( ctx, window, x_paritem( ctx, 1 ), width, align );
   if( column != 0 )
      x_retni( ctx, column );
}

// Append a row; aValues holds the texts of the first columns
// oList:AddRow( aValues ) --> nRow
X_FUNC_STATIC( GUILISTVIEW_ADDROW )
{
   HWND window = x_guiSelfHandle( ctx, "GUILISTVIEW:ADDROW" );
   int row;

   if( window != NULL && ( row = x_guiListViewAddRow( ctx, window, x_paritem( ctx, 1 ) ) ) != 0 )
      x_retni( ctx, row );
}

// Return the text of a cell
// oList:Cell( nRow, nColumn ) --> cText
X_FUNC_STATIC( GUILISTVIEW_CELL )
{
   HWND window = x_guiSelfHandle( ctx, "GUILISTVIEW:CELL" );

   if( window != NULL )
      x_guiListViewCell( ctx, window, x_parni( ctx, 1 ), x_parni( ctx, 2 ) );
}

// Change the text of a cell
// oList:SetCell( nRow, nColumn, cText ) --> self
X_FUNC_STATIC( GUILISTVIEW_SETCELL )
{
   HWND window = x_guiSelfHandle( ctx, "GUILISTVIEW:SETCELL" );

   if( window != NULL )
   {
      x_guiListViewSetCell( ctx, window, x_parni( ctx, 1 ), x_parni( ctx, 2 ), x_paritem( ctx, 3 ) );
      x_guiReturnSelf( ctx );
   }
}

// Delete a row
// oList:Delete( nRow ) --> self
X_FUNC_STATIC( GUILISTVIEW_DELETE )
{
   HWND window = x_guiSelfHandle( ctx, "GUILISTVIEW:DELETE" );

   if( window != NULL )
   {
      x_guiListViewDelete( ctx, window, x_parni( ctx, 1 ) );
      x_guiReturnSelf( ctx );
   }
}

// Delete all rows; the columns stay
// oList:Clear() --> self
X_FUNC_STATIC( GUILISTVIEW_CLEAR )
{
   HWND window = x_guiSelfHandle( ctx, "GUILISTVIEW:CLEAR" );

   if( window != NULL )
   {
      x_guiListViewClear( window );
      x_guiReturnSelf( ctx );
   }
}

// Return the number of rows
// oList:count --> nCount
X_FUNC_STATIC( GUILISTVIEW_COUNT )
{
   HWND window = x_guiSelfHandle( ctx, "GUILISTVIEW:COUNT" );

   if( window != NULL )
      x_retni( ctx, x_guiListViewCount( window ) );
}

// Return the number of columns
// oList:columnCount --> nCount
X_FUNC_STATIC( GUILISTVIEW_COLUMNCOUNT )
{
   HWND window = x_guiSelfHandle( ctx, "GUILISTVIEW:COLUMNCOUNT" );

   if( window != NULL )
      x_retni( ctx, x_guiListViewColumnCount( window ) );
}

// Return the selected row, 0 when nothing is selected
// oList:selected --> nRow
X_FUNC_STATIC( GUILISTVIEW_GETSELECTED )
{
   HWND window = x_guiSelfHandle( ctx, "GUILISTVIEW:SELECTED" );

   if( window != NULL )
      x_retni( ctx, x_guiListViewSelected( window ) );
}

// Select a row and scroll it into view; 0 clears the selection
// oList:selected := nRow
X_FUNC_STATIC( GUILISTVIEW_SETSELECTED )
{
   HWND window = x_guiSelfHandle( ctx, "GUILISTVIEW:SELECTED" );

   if( window == NULL )
      return;

   if( !X_ISNUM( ctx, 1 ) )
   {
      x_guiRaise( ctx, "GUILISTVIEW:SELECTED" );
      return;
   }

   x_guiListViewSetSelected( ctx, window, x_parni( ctx, 1 ) );
   x_retni( ctx, x_parni( ctx, 1 ) );
}

// Run OnSelect or OnDoubleClick for the user action (internal)
// X_GUI_EVENT_NOTIFY of oList( nCode, nRow ) --> NIL
X_FUNC_STATIC( GUILISTVIEW_NOTIFY )
{
   if( x_parni( ctx, 1 ) == ( int )LVN_ITEMCHANGED && x_parni( ctx, 2 ) > 0 )
      x_guiEvent( ctx, 1 );
   else if( x_parni( ctx, 1 ) == ( int )NM_DBLCLK )
      x_guiEvent( ctx, 2 );
}

// Table of text rows; OnSelect and OnDoubleClick receive the control
// guiListView() --> oList
X_FUNC( GUILISTVIEW )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 2, "GUILISTVIEW" );

      // Parent class
      x_clsAddParent( s_class, x_clsRequire( "GUICONTROL", GUICONTROL ) );

      // Instance variables
      x_clsAddData( s_class, "ONSELECT", 1 );
      x_clsSetDataType( s_class, "ONSELECT",      X_OO_VALUE_CODEBLOCK );
      x_clsAddData( s_class, "ONDOUBLECLICK", 2 );
      x_clsSetDataType( s_class, "ONDOUBLECLICK", X_OO_VALUE_CODEBLOCK );

      // Methods
      x_clsAdd( s_class, "NEW",                   X_FUNCNAME( GUILISTVIEW_NEW ) );
      x_clsAdd( s_class, "ADDCOLUMN",             X_FUNCNAME( GUILISTVIEW_ADDCOLUMN ) );
      x_clsAdd( s_class, "ADDROW",                X_FUNCNAME( GUILISTVIEW_ADDROW ) );
      x_clsAdd( s_class, "CELL",                  X_FUNCNAME( GUILISTVIEW_CELL ) );
      x_clsAdd( s_class, "SETCELL",               X_FUNCNAME( GUILISTVIEW_SETCELL ) );
      x_clsAdd( s_class, "DELETE",                X_FUNCNAME( GUILISTVIEW_DELETE ) );
      x_clsAdd( s_class, "CLEAR",                 X_FUNCNAME( GUILISTVIEW_CLEAR ) );

      // ACCESS and ASSIGN methods
      x_clsAddAccess( s_class, "COUNT",           X_FUNCNAME( GUILISTVIEW_COUNT ) );
      x_clsAddAccess( s_class, "COLUMNCOUNT",     X_FUNCNAME( GUILISTVIEW_COLUMNCOUNT ) );
      x_clsAddAccess( s_class, "SELECTED",        X_FUNCNAME( GUILISTVIEW_GETSELECTED ) );
      x_clsAddAssign( s_class, "SELECTED",        X_FUNCNAME( GUILISTVIEW_SETSELECTED ) );
      x_clsAddAccess( s_class, "VALUE",           X_FUNCNAME( GUILISTVIEW_GETSELECTED ) );
      x_clsAddAssign( s_class, "VALUE",           X_FUNCNAME( GUILISTVIEW_SETSELECTED ) );

      // Library events, not methods of the class
      x_guiHandlerAdd( s_class, X_GUI_EVENT_NOTIFY, X_FUNCNAME( GUILISTVIEW_NOTIFY ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   guiTreeView
   --------------------------------------------------------------------------------------------- */

// Create a tree; default size 200 x 200
// guiTreeView():New( oWindow, nX, nY, [nWidth], [nHeight] ) --> self
X_FUNC_STATIC( GUITREEVIEW_NEW )
{
   x_guiControlNew( ctx, X_GUI_TREEVIEW, 200, 200, 0 );
}

// Add an item at the end of the root or of a parent item; the handle stays valid until Clear()
// oTree:Add( cText, [hParent] ) --> hItem
X_FUNC_STATIC( GUITREEVIEW_ADD )
{
   HWND window = x_guiSelfHandle( ctx, "GUITREEVIEW:ADD" );

   if( window != NULL )
      x_guiTreeAdd( ctx, window, x_paritem( ctx, 1 ), x_paritem( ctx, 2 ) );
}

// Return the text of an item
// oTree:ItemText( hItem ) --> cText
X_FUNC_STATIC( GUITREEVIEW_ITEMTEXT )
{
   HWND window = x_guiSelfHandle( ctx, "GUITREEVIEW:ITEMTEXT" );

   if( window != NULL )
      x_guiTreeText( ctx, window, x_paritem( ctx, 1 ) );
}

// Change the text of an item
// oTree:SetItemText( hItem, cText ) --> self
X_FUNC_STATIC( GUITREEVIEW_SETITEMTEXT )
{
   HWND window = x_guiSelfHandle( ctx, "GUITREEVIEW:SETITEMTEXT" );

   if( window != NULL )
   {
      x_guiTreeSetText( ctx, window, x_paritem( ctx, 1 ), x_paritem( ctx, 2 ) );
      x_guiReturnSelf( ctx );
   }
}

// Show the children of an item
// oTree:Expand( hItem ) --> self
X_FUNC_STATIC( GUITREEVIEW_EXPAND )
{
   HWND window = x_guiSelfHandle( ctx, "GUITREEVIEW:EXPAND" );

   if( window != NULL )
   {
      x_guiTreeExpand( ctx, window, x_paritem( ctx, 1 ), 1 );
      x_guiReturnSelf( ctx );
   }
}

// Hide the children of an item
// oTree:Collapse( hItem ) --> self
X_FUNC_STATIC( GUITREEVIEW_COLLAPSE )
{
   HWND window = x_guiSelfHandle( ctx, "GUITREEVIEW:COLLAPSE" );

   if( window != NULL )
   {
      x_guiTreeExpand( ctx, window, x_paritem( ctx, 1 ), 0 );
      x_guiReturnSelf( ctx );
   }
}

// Delete all items
// oTree:Clear() --> self
X_FUNC_STATIC( GUITREEVIEW_CLEAR )
{
   HWND window = x_guiSelfHandle( ctx, "GUITREEVIEW:CLEAR" );

   if( window != NULL )
   {
      x_guiTreeClear( window );
      x_guiReturnSelf( ctx );
   }
}

// Return the number of items
// oTree:count --> nCount
X_FUNC_STATIC( GUITREEVIEW_COUNT )
{
   HWND window = x_guiSelfHandle( ctx, "GUITREEVIEW:COUNT" );

   if( window != NULL )
      x_retni( ctx, x_guiTreeCount( window ) );
}

// Return the selected item, NIL when nothing is selected
// oTree:selected --> hItem
X_FUNC_STATIC( GUITREEVIEW_GETSELECTED )
{
   HWND window = x_guiSelfHandle( ctx, "GUITREEVIEW:SELECTED" );

   if( window != NULL )
      x_guiTreeSelected( ctx, window );
}

// Select an item; NIL clears the selection
// oTree:selected := hItem
X_FUNC_STATIC( GUITREEVIEW_SETSELECTED )
{
   HWND window = x_guiSelfHandle( ctx, "GUITREEVIEW:SELECTED" );

   if( window != NULL )
   {
      x_guiTreeSelect( ctx, window, x_paritem( ctx, 1 ) );
      x_itemReturn( ctx, x_paritem( ctx, 1 ) );
   }
}

// Run OnSelect or OnDoubleClick for the user action (internal)
// X_GUI_EVENT_NOTIFY of oTree( nCode, lUser ) --> NIL
X_FUNC_STATIC( GUITREEVIEW_NOTIFY )
{
   if( x_parni( ctx, 1 ) == ( int )TVN_SELCHANGEDW && x_parni( ctx, 2 ) != 0 )
      x_guiEvent( ctx, 1 );
   else if( x_parni( ctx, 1 ) == ( int )NM_DBLCLK )
      x_guiEvent( ctx, 2 );
}

// Tree of text items; OnSelect and OnDoubleClick receive the control
// guiTreeView() --> oTree
X_FUNC( GUITREEVIEW )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 2, "GUITREEVIEW" );

      // Parent class
      x_clsAddParent( s_class, x_clsRequire( "GUICONTROL", GUICONTROL ) );

      // Instance variables
      x_clsAddData( s_class, "ONSELECT", 1 );
      x_clsSetDataType( s_class, "ONSELECT",      X_OO_VALUE_CODEBLOCK );
      x_clsAddData( s_class, "ONDOUBLECLICK", 2 );
      x_clsSetDataType( s_class, "ONDOUBLECLICK", X_OO_VALUE_CODEBLOCK );

      // Methods
      x_clsAdd( s_class, "NEW",                   X_FUNCNAME( GUITREEVIEW_NEW ) );
      x_clsAdd( s_class, "ADD",                   X_FUNCNAME( GUITREEVIEW_ADD ) );
      x_clsAdd( s_class, "ITEMTEXT",              X_FUNCNAME( GUITREEVIEW_ITEMTEXT ) );
      x_clsAdd( s_class, "SETITEMTEXT",           X_FUNCNAME( GUITREEVIEW_SETITEMTEXT ) );
      x_clsAdd( s_class, "EXPAND",                X_FUNCNAME( GUITREEVIEW_EXPAND ) );
      x_clsAdd( s_class, "COLLAPSE",              X_FUNCNAME( GUITREEVIEW_COLLAPSE ) );
      x_clsAdd( s_class, "CLEAR",                 X_FUNCNAME( GUITREEVIEW_CLEAR ) );

      // ACCESS and ASSIGN methods
      x_clsAddAccess( s_class, "COUNT",           X_FUNCNAME( GUITREEVIEW_COUNT ) );
      x_clsAddAccess( s_class, "SELECTED",        X_FUNCNAME( GUITREEVIEW_GETSELECTED ) );
      x_clsAddAssign( s_class, "SELECTED",        X_FUNCNAME( GUITREEVIEW_SETSELECTED ) );
      x_clsAddAccess( s_class, "VALUE",           X_FUNCNAME( GUITREEVIEW_GETSELECTED ) );
      x_clsAddAssign( s_class, "VALUE",           X_FUNCNAME( GUITREEVIEW_SETSELECTED ) );

      // Library events, not methods of the class
      x_guiHandlerAdd( s_class, X_GUI_EVENT_NOTIFY, X_FUNCNAME( GUITREEVIEW_NOTIFY ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   guiTabPage
   --------------------------------------------------------------------------------------------- */

// Pages are created by guiTabControl:AddPage(), not by New()
// guiTabPage():New() --> NIL
X_FUNC_STATIC( GUITABPAGE_NEW )
{
   x_guiRaise( ctx, "GUITABPAGE:NEW" );
}

// Page of a tab control; controls are created on it like on a window
// guiTabPage() --> oPage
X_FUNC( GUITABPAGE )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 0, "GUITABPAGE" );

      // Parent class
      x_clsAddParent( s_class, x_clsRequire( "GUIWINDOW", GUIWINDOW ) );

      // Methods
      x_clsAdd( s_class, "NEW", X_FUNCNAME( GUITABPAGE_NEW ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   guiTabControl
   --------------------------------------------------------------------------------------------- */

// Create a tab control without pages; default size 300 x 200
// guiTabControl():New( oWindow, nX, nY, [nWidth], [nHeight] ) --> self
X_FUNC_STATIC( GUITABCONTROL_NEW )
{
   x_guiControlNew( ctx, X_GUI_TABCONTROL, 300, 200, 0 );
}

// Add a tab with a page; the page follows the size of the tab control
// oTab:AddPage( cTitle ) --> oPage
X_FUNC_STATIC( GUITABCONTROL_ADDPAGE )
{
   X_ITEM page __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   HWND window = x_guiSelfHandle( ctx, "GUITABCONTROL:ADDPAGE" );

   if( window != NULL && x_objCreate( ctx, &page, x_clsRequire( "GUITABPAGE", GUITABPAGE ) ) )
      x_guiTabAddPage( ctx, window, x_paritem( ctx, 1 ), &page );
}

// Return the page at the position
// oTab:Page( nPosition ) --> oPage
X_FUNC_STATIC( GUITABCONTROL_PAGE )
{
   HWND window = x_guiSelfHandle( ctx, "GUITABCONTROL:PAGE" );

   if( window != NULL )
      x_guiTabPage( ctx, window, x_parni( ctx, 1 ) );
}

// Return the number of pages
// oTab:count --> nCount
X_FUNC_STATIC( GUITABCONTROL_COUNT )
{
   HWND window = x_guiSelfHandle( ctx, "GUITABCONTROL:COUNT" );

   if( window != NULL )
      x_retni( ctx, x_guiTabCount( window ) );
}

// Return the position of the selected page, 0 when there are no pages
// oTab:selected --> nPosition
X_FUNC_STATIC( GUITABCONTROL_GETSELECTED )
{
   HWND window = x_guiSelfHandle( ctx, "GUITABCONTROL:SELECTED" );

   if( window != NULL )
      x_retni( ctx, x_guiTabSelected( window ) );
}

// Select the page at the position and show it
// oTab:selected := nPosition
X_FUNC_STATIC( GUITABCONTROL_SETSELECTED )
{
   HWND window = x_guiSelfHandle( ctx, "GUITABCONTROL:SELECTED" );

   if( window == NULL )
      return;

   if( !X_ISNUM( ctx, 1 ) )
   {
      x_guiRaise( ctx, "GUITABCONTROL:SELECTED" );
      return;
   }

   x_guiTabSelect( ctx, window, x_parni( ctx, 1 ) );
   x_retni( ctx, x_parni( ctx, 1 ) );
}

// Show the selected page and run OnSelect when the user selects a tab (internal)
// X_GUI_EVENT_NOTIFY of oTab( nCode ) --> NIL
X_FUNC_STATIC( GUITABCONTROL_NOTIFY )
{
   HWND window;

   if( x_parni( ctx, 1 ) != ( int )TCN_SELCHANGE || ( window = x_guiSelfHandle( ctx, "GUITABCONTROL" ) ) == NULL )
      return;

   x_guiTabRefresh( window );
   x_guiEvent( ctx, 1 );
}

// Tab control with pages; :value is the position of the selected page, OnSelect receives the control
// guiTabControl() --> oTab
X_FUNC( GUITABCONTROL )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 1, "GUITABCONTROL" );

      // Parent class
      x_clsAddParent( s_class, x_clsRequire( "GUICONTROL", GUICONTROL ) );

      // Instance variables
      x_clsAddData( s_class, "ONSELECT", 1 );
      x_clsSetDataType( s_class, "ONSELECT", X_OO_VALUE_CODEBLOCK );

      // Methods
      x_clsAdd( s_class, "NEW",              X_FUNCNAME( GUITABCONTROL_NEW ) );
      x_clsAdd( s_class, "ADDPAGE",          X_FUNCNAME( GUITABCONTROL_ADDPAGE ) );
      x_clsAdd( s_class, "PAGE",             X_FUNCNAME( GUITABCONTROL_PAGE ) );

      // ACCESS and ASSIGN methods
      x_clsAddAccess( s_class, "COUNT",      X_FUNCNAME( GUITABCONTROL_COUNT ) );
      x_clsAddAccess( s_class, "SELECTED",   X_FUNCNAME( GUITABCONTROL_GETSELECTED ) );
      x_clsAddAssign( s_class, "SELECTED",   X_FUNCNAME( GUITABCONTROL_SETSELECTED ) );
      x_clsAddAccess( s_class, "VALUE",      X_FUNCNAME( GUITABCONTROL_GETSELECTED ) );
      x_clsAddAssign( s_class, "VALUE",      X_FUNCNAME( GUITABCONTROL_SETSELECTED ) );

      // Library events, not methods of the class
      x_guiHandlerAdd( s_class, X_GUI_EVENT_NOTIFY, X_FUNCNAME( GUITABCONTROL_NOTIFY ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   guiTimer
   --------------------------------------------------------------------------------------------- */

// Create a stopped timer of the window; the action runs every nInterval milliseconds
// guiTimer():New( oWindow, nInterval, [bAction] ) --> self
X_FUNC_STATIC( GUITIMER_NEW )
{
   const X_ITEM *parent = x_paritem( ctx, 1 );
   const X_ITEM *action = x_paritem( ctx, 3 );
   X_ITEM *onTimer = x_objDataSlot( ctx, 1 );
   X_ITEM *owner = x_objDataSlot( ctx, 2 );
   X_ITEM *interval = x_objDataSlot( ctx, 3 );

   if( onTimer == NULL || owner == NULL || interval == NULL )
      return;

   if( x_guiObjectWindow( ctx, parent, "GUITIMER:NEW" ) == NULL )
      return;

   if( x_parni( ctx, 2 ) <= 0 || ( action != NULL && !X_IS_NIL( action ) && !X_IS_BLOCK( action ) ) )
   {
      x_guiRaise( ctx, "GUITIMER:NEW" );
      return;
   }

   x_itemCopy( owner, parent );
   x_itemPutNI( interval, x_parni( ctx, 2 ) );
   if( action != NULL )
      x_itemCopy( onTimer, action );

   x_guiReturnSelf( ctx );
}

// Start the timer; a running timer keeps its object alive until it stops or the window closes
// oTimer:Start() --> self
X_FUNC_STATIC( GUITIMER_START )
{
   const X_ITEM *interval = x_objDataSlot( ctx, 3 );
   X_ITEM *id = x_objDataSlot( ctx, 4 );
   HWND window;
   UINT_PTR timer;

   if( interval == NULL || id == NULL )
      return;

   window = x_guiObjectWindow( ctx, x_objDataSlot( ctx, 2 ), "GUITIMER:START" );
   if( window == NULL )
      return;

   timer = x_guiTimerStart( ctx, window, ( UINT_PTR )x_itemGetNInt( id ), x_itemGetNI( interval ) );
   if( timer != 0 )
   {
      x_itemPutNInt( id, ( int64_t )timer );
      x_guiReturnSelf( ctx );
   }
}

// Stop the timer
// oTimer:Stop() --> self
X_FUNC_STATIC( GUITIMER_STOP )
{
   const X_ITEM *id = x_objDataSlot( ctx, 4 );

   if( id != NULL )
      x_guiTimerStop( ( UINT_PTR )x_itemGetNInt( id ) );

   x_guiReturnSelf( ctx );
}

// Return whether the timer is running
// oTimer:active --> lActive
X_FUNC_STATIC( GUITIMER_ACTIVE )
{
   const X_ITEM *id = x_objDataSlot( ctx, 4 );

   x_retl( ctx, id != NULL && x_guiTimerActive( ( UINT_PTR )x_itemGetNInt( id ) ) );
}

// Return the interval in milliseconds
// oTimer:interval --> nInterval
X_FUNC_STATIC( GUITIMER_GETINTERVAL )
{
   const X_ITEM *interval = x_objDataSlot( ctx, 3 );

   if( interval != NULL )
      x_retni( ctx, x_itemGetNI( interval ) );
}

// Set the interval in milliseconds; a running timer continues with the new interval
// oTimer:interval := nInterval
X_FUNC_STATIC( GUITIMER_SETINTERVAL )
{
   X_ITEM *interval = x_objDataSlot( ctx, 3 );
   const X_ITEM *id = x_objDataSlot( ctx, 4 );
   HWND window;

   if( interval == NULL || id == NULL )
      return;

   if( !X_ISNUM( ctx, 1 ) || x_parni( ctx, 1 ) <= 0 )
   {
      x_guiRaise( ctx, "GUITIMER:INTERVAL" );
      return;
   }

   x_itemPutNI( interval, x_parni( ctx, 1 ) );

   /* A running timer starts again with the new interval */
   if( x_guiTimerActive( ( UINT_PTR )x_itemGetNInt( id ) ) )
   {
      window = x_guiObjectWindow( ctx, x_objDataSlot( ctx, 2 ), "GUITIMER:INTERVAL" );
      if( window != NULL )
         x_guiTimerStart( ctx, window, ( UINT_PTR )x_itemGetNInt( id ), x_parni( ctx, 1 ) );
   }

   x_retni( ctx, x_parni( ctx, 1 ) );
}

// Run OnTimer for a tick of the timer (internal)
// X_GUI_EVENT_TIMER of oTimer() --> NIL
X_FUNC_STATIC( GUITIMER_EVENT )
{
   x_guiEvent( ctx, 1 );
}

// Timer of a window; OnTimer runs for every tick and receives the timer
// guiTimer() --> oTimer
X_FUNC( GUITIMER )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 4, "GUITIMER" );

      // Instance variables
      x_clsAddData( s_class, "ONTIMER", 1 );
      x_clsSetDataType( s_class, "ONTIMER", X_OO_VALUE_CODEBLOCK );
      x_clsAddData( s_class, "PARENT", 2 );
      x_clsSetScope( s_class, "PARENT",     X_OO_CLSTP_EXPORTED | X_OO_CLSTP_READONLY );
      x_clsAddData( s_class, "INTERVALMS", 3 );
      x_clsSetScope( s_class, "INTERVALMS", X_OO_CLSTP_HIDDEN );
      x_clsAddData( s_class, "TIMERID", 4 );
      x_clsSetScope( s_class, "TIMERID",    X_OO_CLSTP_HIDDEN );

      // Methods
      x_clsAdd( s_class, "NEW",             X_FUNCNAME( GUITIMER_NEW ) );
      x_clsAdd( s_class, "START",           X_FUNCNAME( GUITIMER_START ) );
      x_clsAdd( s_class, "STOP",            X_FUNCNAME( GUITIMER_STOP ) );

      // ACCESS and ASSIGN methods
      x_clsAddAccess( s_class, "ACTIVE",    X_FUNCNAME( GUITIMER_ACTIVE ) );
      x_clsAddAccess( s_class, "INTERVAL",  X_FUNCNAME( GUITIMER_GETINTERVAL ) );
      x_clsAddAssign( s_class, "INTERVAL",  X_FUNCNAME( GUITIMER_SETINTERVAL ) );

      // Library events, not methods of the class
      x_guiHandlerAdd( s_class, X_GUI_EVENT_TIMER, X_FUNCNAME( GUITIMER_EVENT ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   guiApplication
   --------------------------------------------------------------------------------------------- */

// Show the window and process messages until it is closed; an error in an event is raised again
// guiApplication():Run( oWindow ) --> .T.
X_FUNC_STATIC( GUIAPPLICATION_RUN )
{
   HWND window = x_guiObjectWindow( ctx, x_paritem( ctx, 1 ), "GUIAPPLICATION:RUN" );

   if( window != NULL )
      x_guiRun( ctx, window );
}

// Message loop of the program
// guiApplication() --> oApplication
X_FUNC( GUIAPPLICATION )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 0, "GUIAPPLICATION" );

      // Methods
      x_clsAdd( s_class, "RUN", X_FUNCNAME( GUIAPPLICATION_RUN ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   Dialogs
   --------------------------------------------------------------------------------------------- */

// Show an information box with an OK button; without a title the active window title is used
// guiMsgInfo( cText, [cTitle] ) --> NIL
X_FUNC( GUIMSGINFO )
{
   x_guiMessage( ctx, X_GUI_MESSAGE_INFO );
}

// Show a warning box with an OK button
// guiMsgWarning( cText, [cTitle] ) --> NIL
X_FUNC( GUIMSGWARNING )
{
   x_guiMessage( ctx, X_GUI_MESSAGE_WARNING );
}

// Show an error box with an OK button
// guiMsgError( cText, [cTitle] ) --> NIL
X_FUNC( GUIMSGERROR )
{
   x_guiMessage( ctx, X_GUI_MESSAGE_ERROR );
}

// Ask a question with Yes and No buttons
// guiMsgYesNo( cText, [cTitle] ) --> lYes
X_FUNC( GUIMSGYESNO )
{
   x_guiMessage( ctx, X_GUI_MESSAGE_YESNO );
}

// Select an existing file; aFilters is { { "Text files", "*.txt" }, ... }, "" when cancelled
// guiFileOpenDialog( [cTitle], [aFilters], [cFile] ) --> cPath
X_FUNC( GUIFILEOPENDIALOG )
{
   x_guiFileDialog( ctx, 0 );
}

// Select a file to save; adds the extension of the first filter, "" when cancelled
// guiFileSaveDialog( [cTitle], [aFilters], [cFile] ) --> cPath
X_FUNC( GUIFILESAVEDIALOG )
{
   x_guiFileDialog( ctx, 1 );
}

// Open the file in the program the system has for its type, e.g. a PDF in the PDF viewer
// guiOpenFile( cFile ) --> lOpened
X_FUNC( GUIOPENFILE )
{
   x_guiOpenFile( ctx );
}
