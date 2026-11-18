/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library: xDev GUI
 * File: gui-api.c
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "internal-gui.h"

/* ---------------------------------------------------------------------------------------------
   gui_Window
   --------------------------------------------------------------------------------------------- */

// Create a hidden window with a client area of nWidth x nHeight logical pixels (default 640 x 480)
// gui_Window():New( [cTitle], [nWidth], [nHeight] ) --> self
X_FUNC_STATIC( GUI_WINDOW_NEW )
{
   x_guiWindowNew( ctx );
}

// Show the window
// oWindow:Show() --> self
X_FUNC_STATIC( GUI_WINDOW_SHOW )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_WINDOW:SHOW" );

   if( window != NULL )
   {
      x_guiShow( window, 1 );
      x_guiReturnSelf( ctx );
   }
}

// Hide the window without closing it
// oWindow:Hide() --> self
X_FUNC_STATIC( GUI_WINDOW_HIDE )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_WINDOW:HIDE" );

   if( window != NULL )
   {
      x_guiShow( window, 0 );
      x_guiReturnSelf( ctx );
   }
}

// Close the window and its controls without calling OnClose()
// oWindow:Close() --> NIL
X_FUNC_STATIC( GUI_WINDOW_CLOSE )
{
   const X_ITEM *handle = x_objDataSlot( ctx, 1 );
   HWND window = IIF( handle != NULL, ( HWND )x_itemGetPtr( handle ), NULL );

   if( window != NULL && IsWindow( window ) )
      x_guiDestroy( window );
}

// Called when the user closes the window; return .F. to keep it open
// oWindow:OnClose() --> lClose
X_FUNC_STATIC( GUI_WINDOW_ONCLOSE )
{
   x_retl( ctx, 1 );
}

// Called after the client area changed its size (logical pixels)
// oWindow:OnResize( nWidth, nHeight ) --> NIL
X_FUNC_STATIC( GUI_WINDOW_ONRESIZE )
{
}

// Called for a key pressed while the window has the focus (Windows virtual key code)
// oWindow:OnKey( nKey ) --> NIL
X_FUNC_STATIC( GUI_WINDOW_ONKEY )
{
}

// Called for a menu command; runs the action of gui_Menu:Add() with the window as its parameter
// oWindow:OnMenu( nId ) --> NIL
X_FUNC_STATIC( GUI_WINDOW_ONMENU )
{
   x_guiWindowRunMenu( ctx, x_parni( ctx, 1 ) );
}

// Make the commands with bWhen of a menu that opens active or grey (internal)
// oWindow:__GuiMenuWhen( nMenu ) --> NIL
X_FUNC_STATIC( GUI_WINDOW_MENUWHEN )
{
   x_guiWindowMenuWhen( ctx, ( HMENU )( intptr_t )x_parnint( ctx, 1 ) );
}

// Return the menu bar of the window
// oWindow:menu --> oMenu
X_FUNC_STATIC( GUI_WINDOW_GETMENU )
{
   const X_ITEM *menu = x_objDataSlot( ctx, 2 );

   if( menu != NULL )
      x_itemReturn( ctx, menu );
}

// Set the menu bar; the window grows so that the client area keeps its size
// oWindow:menu := oMenu
X_FUNC_STATIC( GUI_WINDOW_SETMENU )
{
   const X_ITEM *value = x_paritem( ctx, 1 );
   X_ITEM *slot = x_objDataSlot( ctx, 2 );
   HWND window = x_guiSelfHandle( ctx, "GUI_WINDOW:MENU" );
   HMENU menu;

   if( window == NULL || slot == NULL || ( menu = x_guiObjectMenu( ctx, value, "GUI_WINDOW:MENU" ) ) == NULL )
      return;

   x_guiWindowSetMenu( ctx, window, menu );
   x_itemCopy( slot, value );
   x_itemReturn( ctx, value );
}

// Return the title of the window
// oWindow:title --> cTitle
X_FUNC_STATIC( GUI_WINDOW_GETTITLE )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_WINDOW:TITLE" );

   if( window != NULL )
      x_guiReturnWindowText( ctx, window );
}

// Set the title of the window
// oWindow:title := cTitle
X_FUNC_STATIC( GUI_WINDOW_SETTITLE )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_WINDOW:TITLE" );

   if( window != NULL )
   {
      x_guiSetText( ctx, window, x_paritem( ctx, 1 ) );
      x_itemReturn( ctx, x_paritem( ctx, 1 ) );
   }
}

// Return .T. when the user may drag the size of the window
// oWindow:sizable --> lSizable
X_FUNC_STATIC( GUI_WINDOW_GETSIZABLE )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_WINDOW:SIZABLE" );

   if( window != NULL )
      x_retl( ctx, x_guiWindowSizable( window ) );
}

// Let the user drag the size of the window and show it on the full screen; the controls follow their anchors
// oWindow:sizable := lSizable
X_FUNC_STATIC( GUI_WINDOW_SETSIZABLE )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_WINDOW:SIZABLE" );

   if( window == NULL )
      return;

   if( !X_ISLOG( ctx, 1 ) )
   {
      x_guiRaise( ctx, "GUI_WINDOW:SIZABLE" );
      return;
   }

   x_guiWindowSetSizable( window, x_parl( ctx, 1 ) );
   x_itemReturn( ctx, x_paritem( ctx, 1 ) );
}

// Return the width of the client area (logical pixels)
// oWindow:width --> nWidth
X_FUNC_STATIC( GUI_WINDOW_GETWIDTH )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_WINDOW:WIDTH" );

   if( window != NULL )
      x_retni( ctx, x_guiClientSize( window, X_GUI_WIDTH ) );
}

// Set the width of the client area (logical pixels)
// oWindow:width := nWidth
X_FUNC_STATIC( GUI_WINDOW_SETWIDTH )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_WINDOW:WIDTH" );

   if( window == NULL )
      return;

   if( !X_ISNUM( ctx, 1 ) )
   {
      x_guiRaise( ctx, "GUI_WINDOW:WIDTH" );
      return;
   }

   x_guiSetClientSize( ctx, window, X_GUI_WIDTH, x_parni( ctx, 1 ) );
   x_retni( ctx, x_parni( ctx, 1 ) );
}

// Return the height of the client area (logical pixels)
// oWindow:height --> nHeight
X_FUNC_STATIC( GUI_WINDOW_GETHEIGHT )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_WINDOW:HEIGHT" );

   if( window != NULL )
      x_retni( ctx, x_guiClientSize( window, X_GUI_HEIGHT ) );
}

// Set the height of the client area (logical pixels)
// oWindow:height := nHeight
X_FUNC_STATIC( GUI_WINDOW_SETHEIGHT )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_WINDOW:HEIGHT" );

   if( window == NULL )
      return;

   if( !X_ISNUM( ctx, 1 ) )
   {
      x_guiRaise( ctx, "GUI_WINDOW:HEIGHT" );
      return;
   }

   x_guiSetClientSize( ctx, window, X_GUI_HEIGHT, x_parni( ctx, 1 ) );
   x_retni( ctx, x_parni( ctx, 1 ) );
}

// Return the menu shown when the user right-clicks the window, NIL when it has none
// oWindow:contextMenu --> oMenu
X_FUNC_STATIC( GUI_WINDOW_GETCONTEXTMENU )
{
   const X_ITEM *menu = x_objDataSlot( ctx, 3 );

   if( menu != NULL )
      x_itemReturn( ctx, menu );
}

// Set the context menu, a gui_Menu():New( .T. ); controls without their own one show it too; NIL removes it
// oWindow:contextMenu := oMenu
X_FUNC_STATIC( GUI_WINDOW_SETCONTEXTMENU )
{
   x_guiSetContextMenu( ctx, 3, "GUI_WINDOW:CONTEXTMENU" );
}

// Center the window over its owner, or on the screen when it has none
// oWindow:Center() --> self
X_FUNC_STATIC( GUI_WINDOW_CENTER )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_WINDOW:CENTER" );

   if( window != NULL )
   {
      x_guiCenter( window );
      x_guiReturnSelf( ctx );
   }
}

// Return the color of the window (0xRRGGBB), NIL for the system color
// oWindow:background --> nColor
X_FUNC_STATIC( GUI_WINDOW_GETBACKGROUND )
{
   x_guiWindowColor( ctx, X_GUI_COLOR_BACKGROUND );
}

// Paint the window and its labels, frames and check boxes with nColor; NIL returns to the system color
// oWindow:background := nColor
X_FUNC_STATIC( GUI_WINDOW_SETBACKGROUND )
{
   x_guiWindowSetColor( ctx, X_GUI_COLOR_BACKGROUND );
}

// Return the text color of labels, frames and check boxes, NIL for the system color
// oWindow:foreground --> nColor
X_FUNC_STATIC( GUI_WINDOW_GETFOREGROUND )
{
   x_guiWindowColor( ctx, X_GUI_COLOR_FOREGROUND );
}

// Set the text color of labels, frames and check boxes; edit fields keep their own colors
// oWindow:foreground := nColor
X_FUNC_STATIC( GUI_WINDOW_SETFOREGROUND )
{
   x_guiWindowSetColor( ctx, X_GUI_COLOR_FOREGROUND );
}

// Create the Windows window for the object (internal, called by New() of gui_Window and gui_Dialog)
// oWindow:__GuiCreate( oOwner, cTitle, nWidth, nHeight, lDialog ) --> self
X_FUNC_STATIC( GUI_WINDOW_CREATE )
{
   x_guiWindowCreate( ctx );
}

// Release the window handle and the menu bar after the window was destroyed (internal)
// oWindow:__GuiDetach() --> NIL
X_FUNC_STATIC( GUI_WINDOW_DETACH )
{
   x_guiWindowDetach( ctx );
}

// Run bAction, which receives the window, when cKey is pressed in the window or in one of its controls,
// as SetKey(); cKey is written as in gui_Menu ("PgDn", "Esc", "Enter", "Ctrl+S"); NIL removes
// the key; it comes before the shortcuts of the menu and of the toolbars; returns the previous block
// oWindow:SetKey( cKey, [bAction] ) --> bPrevious
X_FUNC_STATIC( GUI_WINDOW_SETKEY )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_WINDOW:SETKEY" );

   if( window != NULL )
      x_guiWindowSetKey( ctx, window, x_objDataSlot( ctx, 4 ) );
}

// Move the focus to the next control in the order of Tab, with lPrevious to the previous one
// oWindow:FocusNext( [lPrevious] ) --> self
X_FUNC_STATIC( GUI_WINDOW_FOCUSNEXT )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_WINDOW:FOCUSNEXT" );

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
X_FUNC_STATIC( GUI_WINDOW_READMODAL )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_WINDOW:READMODAL" );
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
         x_guiRaise( ctx, "GUI_WINDOW:READMODAL" );
         return;
      }

      count = x_arrayLen( list );
      gets = ( HWND * )x_xgrabArray( count + 1, sizeof( HWND ) );
      for( i = 0; i < count; ++i )
      {
         if( ( gets[i] = x_guiObjectControl( ctx, x_arrayGetItemPtr( list, i + 1 ), "GUI_WINDOW:READMODAL" ) ) == NULL )
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
X_FUNC_STATIC( GUI_WINDOW_ENDREAD )
{
   if( !X_ISNIL( ctx, 1 ) && !X_ISLOG( ctx, 1 ) )
   {
      x_guiRaise( ctx, "GUI_WINDOW:ENDREAD" );
      return;
   }

   x_guiWindowEndRead( IIF( X_ISLOG( ctx, 1 ), x_parl( ctx, 1 ), 1 ) );
   x_guiReturnSelf( ctx );
}

// A control of the window, also on its pages and panels, by the :name given to it: oWindow:NIP is the control
// with :name "NIP"; a message the window does not know otherwise raises the usual error
// oWindow:<cName> --> oControl
X_FUNC_STATIC( GUI_WINDOW_ONERROR )
{
   X_ITEM message __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM arguments __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   HWND window;

   if( !x_objOnErrorInfo( ctx, ctx->result, &message, &arguments ) )
      return;

   window = x_guiSelfHandle( ctx, "GUI_WINDOW:ONERROR" );
   if( window == NULL )
      return;

   if( !x_guiWindowControl( ctx, window, x_itemGetCPtr( &message ) ) )
      x_errorRaiseNamed( ctx->result, 1004, x_itemGetCPtr( &message ) );
}

// Run the block of key nKey of SetKey() (internal)
// oWindow:__GuiKey( nKey ) --> NIL
X_FUNC_STATIC( GUI_WINDOW_KEY )
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
// gui_Window() --> oWindow
X_FUNC( GUI_WINDOW )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 4, "GUI_WINDOW" );

      x_clsAddData( s_class, "HANDLE", 1 );
      x_clsSetScope( s_class, "HANDLE", X_OO_CLSTP_EXPORTED | X_OO_CLSTP_READONLY );
      x_clsAddData( s_class, "MENUBAR", 2 );
      x_clsSetScope( s_class, "MENUBAR", X_OO_CLSTP_HIDDEN );
      x_clsAddData( s_class, "POPUPMENU", 3 );
      x_clsSetScope( s_class, "POPUPMENU", X_OO_CLSTP_HIDDEN );
      x_clsAddData( s_class, "KEYBLOCKS", 4 );
      x_clsSetScope( s_class, "KEYBLOCKS", X_OO_CLSTP_HIDDEN );

      x_clsAdd( s_class, "NEW", X_FUNCNAME( GUI_WINDOW_NEW ) );
      x_clsAddOnError( s_class, X_FUNCNAME( GUI_WINDOW_ONERROR ) );
      x_clsAdd( s_class, "SETKEY", X_FUNCNAME( GUI_WINDOW_SETKEY ) );
      x_clsAdd( s_class, "FOCUSNEXT", X_FUNCNAME( GUI_WINDOW_FOCUSNEXT ) );
      x_clsAdd( s_class, "READMODAL", X_FUNCNAME( GUI_WINDOW_READMODAL ) );
      x_clsAdd( s_class, "ENDREAD", X_FUNCNAME( GUI_WINDOW_ENDREAD ) );
      x_clsAdd( s_class, "__GUIKEY", X_FUNCNAME( GUI_WINDOW_KEY ) );
      x_clsAdd( s_class, "SHOW", X_FUNCNAME( GUI_WINDOW_SHOW ) );
      x_clsAdd( s_class, "HIDE", X_FUNCNAME( GUI_WINDOW_HIDE ) );
      x_clsAdd( s_class, "CLOSE", X_FUNCNAME( GUI_WINDOW_CLOSE ) );
      x_clsAdd( s_class, "CENTER", X_FUNCNAME( GUI_WINDOW_CENTER ) );
      x_clsAdd( s_class, "ONCLOSE", X_FUNCNAME( GUI_WINDOW_ONCLOSE ) );
      x_clsAdd( s_class, "ONRESIZE", X_FUNCNAME( GUI_WINDOW_ONRESIZE ) );
      x_clsAdd( s_class, "ONKEY", X_FUNCNAME( GUI_WINDOW_ONKEY ) );
      x_clsAdd( s_class, "ONMENU", X_FUNCNAME( GUI_WINDOW_ONMENU ) );
      x_clsAdd( s_class, "__GUIMENUWHEN", X_FUNCNAME( GUI_WINDOW_MENUWHEN ) );

      x_clsAddAccess( s_class, "MENU", X_FUNCNAME( GUI_WINDOW_GETMENU ) );
      x_clsAddAssign( s_class, "MENU", X_FUNCNAME( GUI_WINDOW_SETMENU ) );
      x_clsAddAccess( s_class, "CONTEXTMENU", X_FUNCNAME( GUI_WINDOW_GETCONTEXTMENU ) );
      x_clsAddAssign( s_class, "CONTEXTMENU", X_FUNCNAME( GUI_WINDOW_SETCONTEXTMENU ) );
      x_clsAddAccess( s_class, "TITLE", X_FUNCNAME( GUI_WINDOW_GETTITLE ) );
      x_clsAddAssign( s_class, "TITLE", X_FUNCNAME( GUI_WINDOW_SETTITLE ) );
      x_clsAddAccess( s_class, "SIZABLE", X_FUNCNAME( GUI_WINDOW_GETSIZABLE ) );
      x_clsAddAssign( s_class, "SIZABLE", X_FUNCNAME( GUI_WINDOW_SETSIZABLE ) );
      x_clsAddAccess( s_class, "WIDTH", X_FUNCNAME( GUI_WINDOW_GETWIDTH ) );
      x_clsAddAssign( s_class, "WIDTH", X_FUNCNAME( GUI_WINDOW_SETWIDTH ) );
      x_clsAddAccess( s_class, "HEIGHT", X_FUNCNAME( GUI_WINDOW_GETHEIGHT ) );
      x_clsAddAssign( s_class, "HEIGHT", X_FUNCNAME( GUI_WINDOW_SETHEIGHT ) );
      x_clsAddAccess( s_class, "BACKGROUND", X_FUNCNAME( GUI_WINDOW_GETBACKGROUND ) );
      x_clsAddAssign( s_class, "BACKGROUND", X_FUNCNAME( GUI_WINDOW_SETBACKGROUND ) );
      x_clsAddAccess( s_class, "FOREGROUND", X_FUNCNAME( GUI_WINDOW_GETFOREGROUND ) );
      x_clsAddAssign( s_class, "FOREGROUND", X_FUNCNAME( GUI_WINDOW_SETFOREGROUND ) );
      x_clsAdd( s_class, "__GUICREATE", X_FUNCNAME( GUI_WINDOW_CREATE ) );
      x_clsAdd( s_class, "__GUIDETACH", X_FUNCNAME( GUI_WINDOW_DETACH ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   gui_Panel
   --------------------------------------------------------------------------------------------- */

// Create a panel for the controls of one part of a window, e.g. a module of the program; without nWidth
// and nHeight it fills the window between its toolbars and the status bar and follows its size;
// Close() removes the panel with all its controls
// gui_Panel():New( oWindow, [nX], [nY], [nWidth], [nHeight] ) --> self
X_FUNC_STATIC( GUI_PANEL_NEW )
{
   x_guiPanelNew( ctx );
}

// Return the edges of the window the panel is attached to
// oPanel:anchor --> cAnchor
X_FUNC_STATIC( GUI_PANEL_GETANCHOR )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_PANEL:ANCHOR" );

   if( window != NULL )
      x_guiGetAnchor( ctx, window );
}

// Attach a panel with its own size to edges of the window: L, T, R, B; opposite edges stretch it
// oPanel:anchor := cAnchor
X_FUNC_STATIC( GUI_PANEL_SETANCHOR )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_PANEL:ANCHOR" );

   if( window != NULL )
   {
      x_guiSetAnchor( ctx, window, x_paritem( ctx, 1 ) );
      x_itemReturn( ctx, x_paritem( ctx, 1 ) );
   }
}

// Child window holding controls; it has the colors, keys and focus order of a window
// gui_Panel() --> oPanel
X_FUNC( GUI_PANEL )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 0, "GUI_PANEL" );
      x_clsAddParent( s_class, x_clsRequire( "GUI_WINDOW", GUI_WINDOW ) );

      x_clsAdd( s_class, "NEW", X_FUNCNAME( GUI_PANEL_NEW ) );
      x_clsAddAccess( s_class, "ANCHOR", X_FUNCNAME( GUI_PANEL_GETANCHOR ) );
      x_clsAddAssign( s_class, "ANCHOR", X_FUNCNAME( GUI_PANEL_SETANCHOR ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   gui_Dialog
   --------------------------------------------------------------------------------------------- */

// Create a hidden dialog window centered over its owner (default size 400 x 300)
// gui_Dialog():New( [oOwner], [cTitle], [nWidth], [nHeight] ) --> self
X_FUNC_STATIC( GUI_DIALOG_NEW )
{
   x_guiDialogNew( ctx );
}

// Show the dialog, disable its owner and wait until the dialog is closed
// oDialog:ShowModal() --> nResult
X_FUNC_STATIC( GUI_DIALOG_SHOWMODAL )
{
   X_ITEM *result = x_objDataSlot( ctx, 1 );
   HWND window = x_guiSelfHandle( ctx, "GUI_DIALOG:SHOWMODAL" );

   if( window == NULL || result == NULL )
      return;

   x_itemPutNI( result, 0 );
   x_guiDialogRun( ctx, window );
   x_itemReturn( ctx, result );
}

// Close the dialog; ShowModal() returns nResult
// oDialog:EndDialog( nResult ) --> NIL
X_FUNC_STATIC( GUI_DIALOG_ENDDIALOG )
{
   X_ITEM *result = x_objDataSlot( ctx, 1 );
   HWND window = x_guiSelfHandle( ctx, "GUI_DIALOG:ENDDIALOG" );

   if( window == NULL || result == NULL )
      return;

   if( !X_ISNUM( ctx, 1 ) )
   {
      x_guiRaise( ctx, "GUI_DIALOG:ENDDIALOG" );
      return;
   }

   x_itemPutNI( result, x_parni( ctx, 1 ) );
   x_guiDestroy( window );
}

// Return the result of the dialog; 0 when the user closed it without EndDialog()
// oDialog:result --> nResult
X_FUNC_STATIC( GUI_DIALOG_RESULT )
{
   const X_ITEM *result = x_objDataSlot( ctx, 1 );

   if( result != NULL )
      x_retni( ctx, x_itemGetNI( result ) );
}

// Modal dialog window; members as in gui_Window
// gui_Dialog() --> oDialog
X_FUNC( GUI_DIALOG )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 1, "GUI_DIALOG" );
      x_clsAddParent( s_class, x_clsRequire( "GUI_WINDOW", GUI_WINDOW ) );
      x_clsAddData( s_class, "DIALOGRESULT", 1 );
      x_clsSetScope( s_class, "DIALOGRESULT", X_OO_CLSTP_HIDDEN );

      x_clsAdd( s_class, "NEW", X_FUNCNAME( GUI_DIALOG_NEW ) );
      x_clsAdd( s_class, "SHOWMODAL", X_FUNCNAME( GUI_DIALOG_SHOWMODAL ) );
      x_clsAdd( s_class, "ENDDIALOG", X_FUNCNAME( GUI_DIALOG_ENDDIALOG ) );

      x_clsAddAccess( s_class, "RESULT", X_FUNCNAME( GUI_DIALOG_RESULT ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   gui_Menu
   --------------------------------------------------------------------------------------------- */

// Create an empty menu bar, or with lPopup .T. an empty context menu
// gui_Menu():New( [lPopup] ) --> self
X_FUNC_STATIC( GUI_MENU_NEW )
{
   X_ITEM *slot = x_objDataSlot( ctx, 1 );
   HMENU menu;

   if( !X_ISNIL( ctx, 1 ) && !X_ISLOG( ctx, 1 ) )
   {
      x_guiRaise( ctx, "GUI_MENU:NEW" );
      return;
   }

   menu = x_guiMenuCreate( ctx, X_ISLOG( ctx, 1 ) && x_parl( ctx, 1 ) );

   if( slot != NULL && menu != NULL )
   {
      x_itemPutPtr( slot, menu );
      x_guiReturnSelf( ctx );
   }
}

// Add a drop-down menu with the given caption; "&" marks the access key; cBitmap is a BMP picture of the item
// oMenu:AddMenu( cText, [cBitmap] ) --> oSubmenu
X_FUNC_STATIC( GUI_MENU_ADDMENU )
{
   HMENU menu = x_guiObjectMenu( ctx, ctx->self, "GUI_MENU:ADDMENU" );

   if( menu != NULL )
      x_guiMenuAddSubmenu( ctx, menu, x_paritem( ctx, 1 ), x_paritem( ctx, 2 ) );
}

// Add a command; the action runs with the window as its parameter; cShortcut such as "Ctrl+S" or "Shift+F5";
// cBitmap is a BMP picture shown at the left of the text; bWhen runs each time the menu opens and before the
// command runs: .F. makes the command grey and it cannot be chosen
// oMenu:Add( cText, [bAction], [cShortcut], [cBitmap], [bWhen] ) --> nId
X_FUNC_STATIC( GUI_MENU_ADD )
{
   HMENU menu = x_guiObjectMenu( ctx, ctx->self, "GUI_MENU:ADD" );
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
X_FUNC_STATIC( GUI_MENU_ADDSEPARATOR )
{
   HMENU menu = x_guiObjectMenu( ctx, ctx->self, "GUI_MENU:ADDSEPARATOR" );

   if( menu != NULL )
   {
      x_guiMenuSeparator( ctx, menu );
      x_guiReturnSelf( ctx );
   }
}

// Turn a new menu object into a drop-down menu (internal)
// oMenu:__GuiPopup() --> self
X_FUNC_STATIC( GUI_MENU_POPUP )
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
// gui_Menu() --> oMenu
X_FUNC( GUI_MENU )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 1, "GUI_MENU" );
      x_clsAddData( s_class, "HANDLE", 1 );
      x_clsSetScope( s_class, "HANDLE", X_OO_CLSTP_EXPORTED | X_OO_CLSTP_READONLY );

      x_clsAdd( s_class, "NEW", X_FUNCNAME( GUI_MENU_NEW ) );
      x_clsAdd( s_class, "ADDMENU", X_FUNCNAME( GUI_MENU_ADDMENU ) );
      x_clsAdd( s_class, "ADD", X_FUNCNAME( GUI_MENU_ADD ) );
      x_clsAdd( s_class, "ADDSEPARATOR", X_FUNCNAME( GUI_MENU_ADDSEPARATOR ) );
      x_clsAdd( s_class, "__GUIPOPUP", X_FUNCNAME( GUI_MENU_POPUP ) );
   }

   x_clsReturn( ctx, s_class );
}

// Draw the menus made from now on in the extended style, as SET MENUSTYLE EXTENDED: a strip with the pictures
// of nImageSize pixels (16 at the start) at the left of drop-down menus and a gradient under the menu bar;
// returns the previous setting
// gui_MenuStyle( [lExtended], [nImageSize] ) --> lPrevious
X_FUNC( GUI_MENUSTYLE )
{
   if( ( !X_ISNIL( ctx, 1 ) && !X_ISLOG( ctx, 1 ) ) || ( !X_ISNIL( ctx, 2 ) && !X_ISNUM( ctx, 2 ) ) )
   {
      x_guiRaise( ctx, "GUI_MENUSTYLE" );
      return;
   }

   x_retl( ctx, x_guiMenuStyle( IIF( X_ISLOG( ctx, 1 ), x_parl( ctx, 1 ), 1 ), x_parnidef( ctx, 2, 0 ) ) );
}

/* ---------------------------------------------------------------------------------------------
   gui_Control
   --------------------------------------------------------------------------------------------- */

// Move the keyboard focus to the control
// oControl:SetFocus() --> self
X_FUNC_STATIC( GUI_CONTROL_SETFOCUS )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_CONTROL:SETFOCUS" );

   if( window != NULL )
   {
      x_guiSetFocus( window );
      x_guiReturnSelf( ctx );
   }
}

// Return the text of the control; gui_Memo separates lines with CR LF
// oControl:text --> cText
X_FUNC_STATIC( GUI_CONTROL_GETTEXT )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_CONTROL:TEXT" );

   if( window != NULL )
      x_guiReturnWindowText( ctx, window );
}

// Set the text of the control; gui_Memo stores a single LF as CR LF
// oControl:text := cText
X_FUNC_STATIC( GUI_CONTROL_SETTEXT )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_CONTROL:TEXT" );

   if( window != NULL )
   {
      x_guiSetText( ctx, window, x_paritem( ctx, 1 ) );
      x_itemReturn( ctx, x_paritem( ctx, 1 ) );
   }
}

// Return whether the control is visible
// oControl:visible --> lVisible
X_FUNC_STATIC( GUI_CONTROL_GETVISIBLE )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_CONTROL:VISIBLE" );

   if( window != NULL )
      x_retl( ctx, IsWindowVisible( window ) );
}

// Show or hide the control
// oControl:visible := lVisible
X_FUNC_STATIC( GUI_CONTROL_SETVISIBLE )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_CONTROL:VISIBLE" );

   if( window == NULL )
      return;

   if( !X_ISLOG( ctx, 1 ) )
   {
      x_guiRaise( ctx, "GUI_CONTROL:VISIBLE" );
      return;
   }

   x_guiSetVisible( window, x_parl( ctx, 1 ) );
   x_retl( ctx, x_parl( ctx, 1 ) );
}

// Return whether the control is enabled
// oControl:enabled --> lEnabled
X_FUNC_STATIC( GUI_CONTROL_GETENABLED )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_CONTROL:ENABLED" );

   if( window != NULL )
      x_retl( ctx, IsWindowEnabled( window ) );
}

// Enable or disable the control
// oControl:enabled := lEnabled
X_FUNC_STATIC( GUI_CONTROL_SETENABLED )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_CONTROL:ENABLED" );

   if( window == NULL )
      return;

   if( !X_ISLOG( ctx, 1 ) )
   {
      x_guiRaise( ctx, "GUI_CONTROL:ENABLED" );
      return;
   }

   x_guiSetEnabled( window, x_parl( ctx, 1 ) );
   x_retl( ctx, x_parl( ctx, 1 ) );
}

// Return the edges of the window the control is attached to ("LT" by default)
// oControl:anchor --> cAnchor
X_FUNC_STATIC( GUI_CONTROL_GETANCHOR )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_CONTROL:ANCHOR" );

   if( window != NULL )
      x_guiGetAnchor( ctx, window );
}

// Attach the control to edges of the window: L, T, R, B; opposite edges stretch the control
// oControl:anchor := cAnchor
X_FUNC_STATIC( GUI_CONTROL_SETANCHOR )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_CONTROL:ANCHOR" );

   if( window != NULL )
   {
      x_guiSetAnchor( ctx, window, x_paritem( ctx, 1 ) );
      x_itemReturn( ctx, x_paritem( ctx, 1 ) );
   }
}

// Create the Windows control for the object (internal, called by New() of the subclasses)
// oControl:__GuiCreate( oWindow, nKind, cText, nX, nY, nWidth, nHeight ) --> self
X_FUNC_STATIC( GUI_CONTROL_CREATE )
{
   x_guiControlCreate( ctx );
}

// Release the handle and the parent after the control was destroyed (internal)
// oControl:__GuiDetach() --> NIL
X_FUNC_STATIC( GUI_CONTROL_DETACH )
{
   x_guiControlDetach( ctx );
}

// Return the left edge of the control in its window (logical pixels)
// oControl:x --> nX
X_FUNC_STATIC( GUI_CONTROL_GETX )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_CONTROL:X" );

   if( window != NULL )
      x_retni( ctx, x_guiBounds( window, X_GUI_X ) );
}

// Move the left edge of the control
// oControl:x := nX
X_FUNC_STATIC( GUI_CONTROL_SETX )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_CONTROL:X" );

   if( window == NULL )
      return;

   if( !X_ISNUM( ctx, 1 ) )
   {
      x_guiRaise( ctx, "GUI_CONTROL:X" );
      return;
   }

   x_guiSetBounds( ctx, window, X_GUI_X, x_parni( ctx, 1 ) );
   x_retni( ctx, x_parni( ctx, 1 ) );
}

// Return the top edge of the control in its window (logical pixels)
// oControl:y --> nY
X_FUNC_STATIC( GUI_CONTROL_GETY )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_CONTROL:Y" );

   if( window != NULL )
      x_retni( ctx, x_guiBounds( window, X_GUI_Y ) );
}

// Move the top edge of the control
// oControl:y := nY
X_FUNC_STATIC( GUI_CONTROL_SETY )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_CONTROL:Y" );

   if( window == NULL )
      return;

   if( !X_ISNUM( ctx, 1 ) )
   {
      x_guiRaise( ctx, "GUI_CONTROL:Y" );
      return;
   }

   x_guiSetBounds( ctx, window, X_GUI_Y, x_parni( ctx, 1 ) );
   x_retni( ctx, x_parni( ctx, 1 ) );
}

// Return the width of the control (logical pixels)
// oControl:width --> nWidth
X_FUNC_STATIC( GUI_CONTROL_GETWIDTH )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_CONTROL:WIDTH" );

   if( window != NULL )
      x_retni( ctx, x_guiBounds( window, X_GUI_WIDTH ) );
}

// Set the width of the control
// oControl:width := nWidth
X_FUNC_STATIC( GUI_CONTROL_SETWIDTH )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_CONTROL:WIDTH" );

   if( window == NULL )
      return;

   if( !X_ISNUM( ctx, 1 ) )
   {
      x_guiRaise( ctx, "GUI_CONTROL:WIDTH" );
      return;
   }

   x_guiSetBounds( ctx, window, X_GUI_WIDTH, x_parni( ctx, 1 ) );
   x_retni( ctx, x_parni( ctx, 1 ) );
}

// Return the height of the control (logical pixels)
// oControl:height --> nHeight
X_FUNC_STATIC( GUI_CONTROL_GETHEIGHT )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_CONTROL:HEIGHT" );

   if( window != NULL )
      x_retni( ctx, x_guiBounds( window, X_GUI_HEIGHT ) );
}

// Set the height of the control
// oControl:height := nHeight
X_FUNC_STATIC( GUI_CONTROL_SETHEIGHT )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_CONTROL:HEIGHT" );

   if( window == NULL )
      return;

   if( !X_ISNUM( ctx, 1 ) )
   {
      x_guiRaise( ctx, "GUI_CONTROL:HEIGHT" );
      return;
   }

   x_guiSetBounds( ctx, window, X_GUI_HEIGHT, x_parni( ctx, 1 ) );
   x_retni( ctx, x_parni( ctx, 1 ) );
}

// Return the tooltip text of the control, "" when it has none
// oControl:toolTip --> cText
X_FUNC_STATIC( GUI_CONTROL_GETTOOLTIP )
{
   const X_ITEM *text = x_objDataSlot( ctx, 3 );

   if( text != NULL && X_IS_STRING( text ) )
      x_itemReturn( ctx, text );
   else
      x_retc( ctx, "" );
}

// Set the tooltip shown when the mouse rests on the control; "" removes it
// oControl:toolTip := cText
X_FUNC_STATIC( GUI_CONTROL_SETTOOLTIP )
{
   X_ITEM *slot = x_objDataSlot( ctx, 3 );
   HWND window = x_guiSelfHandle( ctx, "GUI_CONTROL:TOOLTIP" );

   if( window == NULL || slot == NULL )
      return;

   x_guiSetToolTip( ctx, window, x_paritem( ctx, 1 ) );
   x_itemCopy( slot, x_paritem( ctx, 1 ) );
   x_itemReturn( ctx, x_paritem( ctx, 1 ) );
}

// Return the menu shown when the user right-clicks the control, NIL when it has none
// oControl:contextMenu --> oMenu
X_FUNC_STATIC( GUI_CONTROL_GETCONTEXTMENU )
{
   const X_ITEM *menu = x_objDataSlot( ctx, 4 );

   if( menu != NULL )
      x_itemReturn( ctx, menu );
}

// Set the context menu, a gui_Menu():New( .T. ); NIL removes it; gui_Edit and gui_Memo keep their own one
// oControl:contextMenu := oMenu
X_FUNC_STATIC( GUI_CONTROL_SETCONTEXTMENU )
{
   x_guiSetContextMenu( ctx, 4, "GUI_CONTROL:CONTEXTMENU" );
}

// Return whether the control has the input focus
// oControl:focused --> lFocused
X_FUNC_STATIC( GUI_CONTROL_GETFOCUSED )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_CONTROL:FOCUSED" );

   if( window != NULL )
      x_retl( ctx, x_guiFocused( window ) );
}

// Return the gui_Font of the control, NIL for the system font
// oControl:font --> oFont
X_FUNC_STATIC( GUI_CONTROL_GETFONT )
{
   const X_ITEM *font = x_objDataSlot( ctx, 5 );

   if( font != NULL )
      x_itemReturn( ctx, font );
}

// Give the control a gui_Font; NIL returns it to the system font; a later change of the gui_Font needs
// another assignment
// oControl:font := oFont
X_FUNC_STATIC( GUI_CONTROL_SETFONT )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_CONTROL:FONT" );
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
// oControl:__GuiCommand( nCode ) --> NIL
X_FUNC_STATIC( GUI_CONTROL_COMMAND )
{
}

// Remove the control from its window, as RestScreen() takes a GET away
// oControl:Destroy() --> NIL
X_FUNC_STATIC( GUI_CONTROL_DESTROY )
{
   HWND control = x_guiSelfHandle( ctx, "GUI_CONTROL:DESTROY" );

   if( control != NULL )
      x_guiControlDestroy( control );
}

// Base class of all controls; :value is the text, subclasses with another kind of value override it
// gui_Control() --> oControl
X_FUNC( GUI_CONTROL )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 6, "GUI_CONTROL" );
      x_clsAddData( s_class, "HANDLE", 1 );
      x_clsSetScope( s_class, "HANDLE", X_OO_CLSTP_EXPORTED | X_OO_CLSTP_READONLY );
      x_clsAddData( s_class, "PARENT", 2 );
      x_clsSetScope( s_class, "PARENT", X_OO_CLSTP_EXPORTED | X_OO_CLSTP_READONLY );
      x_clsAddData( s_class, "TOOLTIPTEXT", 3 );
      x_clsSetScope( s_class, "TOOLTIPTEXT", X_OO_CLSTP_HIDDEN );
      x_clsAddData( s_class, "POPUPMENU", 4 );
      x_clsSetScope( s_class, "POPUPMENU", X_OO_CLSTP_HIDDEN );
      x_clsAddData( s_class, "FONTOBJECT", 5 );
      x_clsSetScope( s_class, "FONTOBJECT", X_OO_CLSTP_HIDDEN );
      x_clsAddData( s_class, "NAME", 6 );

      x_clsAdd( s_class, "SETFOCUS", X_FUNCNAME( GUI_CONTROL_SETFOCUS ) );
      x_clsAdd( s_class, "DESTROY", X_FUNCNAME( GUI_CONTROL_DESTROY ) );
      x_clsAddAccess( s_class, "FOCUSED", X_FUNCNAME( GUI_CONTROL_GETFOCUSED ) );
      x_clsAddAccess( s_class, "FONT", X_FUNCNAME( GUI_CONTROL_GETFONT ) );
      x_clsAddAssign( s_class, "FONT", X_FUNCNAME( GUI_CONTROL_SETFONT ) );

      x_clsAddAccess( s_class, "TEXT", X_FUNCNAME( GUI_CONTROL_GETTEXT ) );
      x_clsAddAssign( s_class, "TEXT", X_FUNCNAME( GUI_CONTROL_SETTEXT ) );
      x_clsAddAccess( s_class, "VALUE", X_FUNCNAME( GUI_CONTROL_GETTEXT ) );
      x_clsAddAssign( s_class, "VALUE", X_FUNCNAME( GUI_CONTROL_SETTEXT ) );
      x_clsAddAccess( s_class, "X", X_FUNCNAME( GUI_CONTROL_GETX ) );
      x_clsAddAssign( s_class, "X", X_FUNCNAME( GUI_CONTROL_SETX ) );
      x_clsAddAccess( s_class, "Y", X_FUNCNAME( GUI_CONTROL_GETY ) );
      x_clsAddAssign( s_class, "Y", X_FUNCNAME( GUI_CONTROL_SETY ) );
      x_clsAddAccess( s_class, "WIDTH", X_FUNCNAME( GUI_CONTROL_GETWIDTH ) );
      x_clsAddAssign( s_class, "WIDTH", X_FUNCNAME( GUI_CONTROL_SETWIDTH ) );
      x_clsAddAccess( s_class, "HEIGHT", X_FUNCNAME( GUI_CONTROL_GETHEIGHT ) );
      x_clsAddAssign( s_class, "HEIGHT", X_FUNCNAME( GUI_CONTROL_SETHEIGHT ) );
      x_clsAddAccess( s_class, "TOOLTIP", X_FUNCNAME( GUI_CONTROL_GETTOOLTIP ) );
      x_clsAddAssign( s_class, "TOOLTIP", X_FUNCNAME( GUI_CONTROL_SETTOOLTIP ) );
      x_clsAddAccess( s_class, "CONTEXTMENU", X_FUNCNAME( GUI_CONTROL_GETCONTEXTMENU ) );
      x_clsAddAssign( s_class, "CONTEXTMENU", X_FUNCNAME( GUI_CONTROL_SETCONTEXTMENU ) );
      x_clsAddAccess( s_class, "VISIBLE", X_FUNCNAME( GUI_CONTROL_GETVISIBLE ) );
      x_clsAddAssign( s_class, "VISIBLE", X_FUNCNAME( GUI_CONTROL_SETVISIBLE ) );
      x_clsAddAccess( s_class, "ENABLED", X_FUNCNAME( GUI_CONTROL_GETENABLED ) );
      x_clsAddAssign( s_class, "ENABLED", X_FUNCNAME( GUI_CONTROL_SETENABLED ) );
      x_clsAddAccess( s_class, "ANCHOR", X_FUNCNAME( GUI_CONTROL_GETANCHOR ) );
      x_clsAddAssign( s_class, "ANCHOR", X_FUNCNAME( GUI_CONTROL_SETANCHOR ) );
      x_clsAdd( s_class, "__GUICREATE", X_FUNCNAME( GUI_CONTROL_CREATE ) );
      x_clsAdd( s_class, "__GUIDETACH", X_FUNCNAME( GUI_CONTROL_DETACH ) );
      x_clsAdd( s_class, "__GUICOMMAND", X_FUNCNAME( GUI_CONTROL_COMMAND ) );
      x_clsAdd( s_class, "__GUINOTIFY", X_FUNCNAME( GUI_CONTROL_COMMAND ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   gui_Font
   --------------------------------------------------------------------------------------------- */

// Describe a font: the size in points (NIL keeps the size of the system font), bold, italic and the name
// (NIL keeps the system font, Segoe UI); the control makes it for the scale of its screen
// gui_Font():New( [nSize], [lBold], [lItalic], [cName] ) --> self
X_FUNC_STATIC( GUI_FONT_NEW )
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
      x_guiRaise( ctx, "GUI_FONT:NEW" );
      return;
   }

   x_itemPutNI( size, x_parnidef( ctx, 1, 0 ) );
   x_itemPutL( bold, x_parldef( ctx, 2, 0 ) );
   x_itemPutL( italic, x_parldef( ctx, 3, 0 ) );
   x_itemPutC( name, IIF( X_ISCHAR( ctx, 4 ), x_parc( ctx, 4 ), "" ) );
   x_guiReturnSelf( ctx );
}

// Font of controls; :size 0 and :name "" mean the system font
// gui_Font() --> oFont
X_FUNC( GUI_FONT )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 4, "GUI_FONT" );
      x_clsAddData( s_class, "SIZE", 1 );
      x_clsAddData( s_class, "BOLD", 2 );
      x_clsAddData( s_class, "ITALIC", 3 );
      x_clsAddData( s_class, "NAME", 4 );

      x_clsAdd( s_class, "NEW", X_FUNCNAME( GUI_FONT_NEW ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   gui_Label
   --------------------------------------------------------------------------------------------- */

// Create a label; default size 200 x 20
// gui_Label():New( oWindow, nX, nY, [nWidth], [nHeight], [cText] ) --> self
X_FUNC_STATIC( GUI_LABEL_NEW )
{
   x_guiControlNew( ctx, X_GUI_LABEL, 200, 20, 1 );
}

// Return .T. when the label has the sunken frame of a field
// oLabel:border --> lBorder
X_FUNC_STATIC( GUI_LABEL_GETBORDER )
{
   HWND label = x_guiSelfHandle( ctx, "GUI_LABEL:BORDER" );

   if( label != NULL )
      x_retl( ctx, x_guiLabelBorder( label ) );
}

// Give the label the sunken frame of a field and the fixed font 9x18 of the GETs, for showing a value as SAY does
// oLabel:border := lBorder
X_FUNC_STATIC( GUI_LABEL_SETBORDER )
{
   HWND label = x_guiSelfHandle( ctx, "GUI_LABEL:BORDER" );

   if( label == NULL )
      return;

   if( !X_ISLOG( ctx, 1 ) )
   {
      x_guiRaise( ctx, "GUI_LABEL:BORDER" );
      return;
   }

   x_guiLabelSetBorder( label, x_parl( ctx, 1 ) );
   x_itemReturn( ctx, x_paritem( ctx, 1 ) );
}

// Return the alignment of the text: 0 left, 1 right, 2 center
// oLabel:align --> nAlign
X_FUNC_STATIC( GUI_LABEL_GETALIGN )
{
   HWND label = x_guiSelfHandle( ctx, "GUI_LABEL:ALIGN" );

   if( label != NULL )
      x_retni( ctx, x_guiLabelAlign( label ) );
}

// Align the text: 0 left, 1 right (descriptions before the fields), 2 center
// oLabel:align := nAlign
X_FUNC_STATIC( GUI_LABEL_SETALIGN )
{
   HWND label = x_guiSelfHandle( ctx, "GUI_LABEL:ALIGN" );

   if( label == NULL )
      return;

   if( !X_ISNUM( ctx, 1 ) || x_parni( ctx, 1 ) < 0 || x_parni( ctx, 1 ) > 2 )
   {
      x_guiRaise( ctx, "GUI_LABEL:ALIGN" );
      return;
   }

   x_guiLabelSetAlign( label, x_parni( ctx, 1 ) );
   x_itemReturn( ctx, x_paritem( ctx, 1 ) );
}

// Draw a line of one pixel from the end of the text to the right edge of the label, as after the title of a
// part of a form: "Faktura VAT -----"; nColor 0xRRGGBB, without it the color of frames
// oLabel:AddLineAfter( [nColor] ) --> self
X_FUNC_STATIC( GUI_LABEL_ADDLINEAFTER )
{
   HWND label = x_guiSelfHandle( ctx, "GUI_LABEL:ADDLINEAFTER" );

   if( label == NULL )
      return;

   if( !X_ISNIL( ctx, 1 ) && !X_ISNUM( ctx, 1 ) )
   {
      x_guiRaise( ctx, "GUI_LABEL:ADDLINEAFTER" );
      return;
   }

   x_guiLabelAddLineAfter( label, IIF( X_ISNUM( ctx, 1 ), x_parni( ctx, 1 ), -1 ) );
   x_guiReturnSelf( ctx );
}

// Static text
// gui_Label() --> oLabel
X_FUNC( GUI_LABEL )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 0, "GUI_LABEL" );
      x_clsAddParent( s_class, x_clsRequire( "GUI_CONTROL", GUI_CONTROL ) );

      x_clsAdd( s_class, "NEW", X_FUNCNAME( GUI_LABEL_NEW ) );

      x_clsAddAccess( s_class, "BORDER", X_FUNCNAME( GUI_LABEL_GETBORDER ) );
      x_clsAddAssign( s_class, "BORDER", X_FUNCNAME( GUI_LABEL_SETBORDER ) );
      x_clsAddAccess( s_class, "ALIGN", X_FUNCNAME( GUI_LABEL_GETALIGN ) );
      x_clsAddAssign( s_class, "ALIGN", X_FUNCNAME( GUI_LABEL_SETALIGN ) );
      x_clsAdd( s_class, "ADDLINEAFTER", X_FUNCNAME( GUI_LABEL_ADDLINEAFTER ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   gui_Say
   --------------------------------------------------------------------------------------------- */

// Text shown in the window, as @ ... SAY: a description or, with :border, a value from the data;
// nLen characters of the font 9x18 wide, as a gui_Get of nLen characters, without it as long as the text;
// without nHeight as high as a gui_Get; members as in gui_Label
// gui_Say():New( oWindow, nX, nY, [nLen], [nHeight], [cText] ) --> self
X_FUNC_STATIC( GUI_SAY_NEW )
{
   x_guiSayNew( ctx );
}

// gui_Say() --> oSay
X_FUNC( GUI_SAY )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 0, "GUI_SAY" );
      x_clsAddParent( s_class, x_clsRequire( "GUI_LABEL", GUI_LABEL ) );

      x_clsAdd( s_class, "NEW", X_FUNCNAME( GUI_SAY_NEW ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   gui_Edit
   --------------------------------------------------------------------------------------------- */

// Create a single-line text field; default size 200 x 23
// gui_Edit():New( oWindow, nX, nY, [nWidth], [nHeight], [cText] ) --> self
X_FUNC_STATIC( GUI_EDIT_NEW )
{
   x_guiControlNew( ctx, X_GUI_EDIT, 200, 23, 1 );
}

// Return whether the user may not change the text
// oEdit:readOnly --> lReadOnly
X_FUNC_STATIC( GUI_EDIT_GETREADONLY )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_EDIT:READONLY" );

   if( window != NULL )
      x_retl( ctx, x_guiReadOnly( window ) );
}

// Allow or forbid changes of the text by the user; the program can always assign :text
// oEdit:readOnly := lReadOnly
X_FUNC_STATIC( GUI_EDIT_SETREADONLY )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_EDIT:READONLY" );

   if( window == NULL )
      return;

   if( !X_ISLOG( ctx, 1 ) )
   {
      x_guiRaise( ctx, "GUI_EDIT:READONLY" );
      return;
   }

   x_guiSetReadOnly( window, x_parl( ctx, 1 ) );
   x_retl( ctx, x_parl( ctx, 1 ) );
}

// Return the maximum number of characters the user can type
// oEdit:maxLength --> nLength
X_FUNC_STATIC( GUI_EDIT_GETMAXLENGTH )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_EDIT:MAXLENGTH" );

   if( window != NULL )
      x_retni( ctx, x_guiMaxLength( window ) );
}

// Limit the number of characters the user can type; 0 restores the system maximum
// oEdit:maxLength := nLength
X_FUNC_STATIC( GUI_EDIT_SETMAXLENGTH )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_EDIT:MAXLENGTH" );

   if( window == NULL )
      return;

   if( !X_ISNUM( ctx, 1 ) || x_parni( ctx, 1 ) < 0 )
   {
      x_guiRaise( ctx, "GUI_EDIT:MAXLENGTH" );
      return;
   }

   x_guiSetMaxLength( window, x_parni( ctx, 1 ) );
   x_retni( ctx, x_parni( ctx, 1 ) );
}

// Return the picture of the field, "" without one
// oEdit:picture --> cPicture
X_FUNC_STATIC( GUI_EDIT_GETPICTURE )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_EDIT:PICTURE" );

   if( window != NULL )
      x_guiEditPicture( ctx, window );
}

// Set a picture: template 9 # A N X L Y ! with literals, functions @! @K @R before a space;
// a number takes the digits of the template before and after the point, a date follows SET DATE
// oEdit:picture := cPicture
X_FUNC_STATIC( GUI_EDIT_SETPICTURE )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_EDIT:PICTURE" );

   if( window != NULL )
      x_guiEditSetPicture( ctx, window, x_paritem( ctx, 1 ) );
}

// Return the value in the type the program gave it: text, number or date
// oEdit:value --> xValue
X_FUNC_STATIC( GUI_EDIT_GETVALUE )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_EDIT:VALUE" );

   if( window != NULL )
      x_guiEditValue( ctx, window );
}

// Set the value; a number stands on the right and is shown through the picture, a date follows SET DATE
// oEdit:value := xValue
X_FUNC_STATIC( GUI_EDIT_SETVALUE )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_EDIT:VALUE" );

   if( window != NULL )
      x_guiEditSetValue( ctx, window, x_paritem( ctx, 1 ) );
}

// Run OnChange after the text changed (internal)
// oEdit:__GuiCommand( nCode ) --> NIL
X_FUNC_STATIC( GUI_EDIT_COMMAND )
{
   if( x_parni( ctx, 1 ) == EN_CHANGE )
      x_guiEvent( ctx, 1 );
}

// Run OnValid when the field loses the focus; a result .F. keeps the focus in the field (internal)
// oEdit:__GuiValid() --> lValid
X_FUNC_STATIC( GUI_EDIT_VALID )
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
// gui_Edit() --> oEdit
X_FUNC( GUI_EDIT )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 2, "GUI_EDIT" );
      x_clsAddParent( s_class, x_clsRequire( "GUI_CONTROL", GUI_CONTROL ) );
      x_clsAddData( s_class, "ONCHANGE", 1 );
      x_clsSetDataType( s_class, "ONCHANGE", X_OO_VALUE_CODEBLOCK );
      x_clsAddData( s_class, "ONVALID", 2 );
      x_clsSetDataType( s_class, "ONVALID", X_OO_VALUE_CODEBLOCK );

      x_clsAdd( s_class, "NEW", X_FUNCNAME( GUI_EDIT_NEW ) );

      x_clsAddAccess( s_class, "READONLY", X_FUNCNAME( GUI_EDIT_GETREADONLY ) );
      x_clsAddAssign( s_class, "READONLY", X_FUNCNAME( GUI_EDIT_SETREADONLY ) );
      x_clsAddAccess( s_class, "MAXLENGTH", X_FUNCNAME( GUI_EDIT_GETMAXLENGTH ) );
      x_clsAddAssign( s_class, "MAXLENGTH", X_FUNCNAME( GUI_EDIT_SETMAXLENGTH ) );
      x_clsAddAccess( s_class, "PICTURE", X_FUNCNAME( GUI_EDIT_GETPICTURE ) );
      x_clsAddAssign( s_class, "PICTURE", X_FUNCNAME( GUI_EDIT_SETPICTURE ) );
      x_clsAddAccess( s_class, "VALUE", X_FUNCNAME( GUI_EDIT_GETVALUE ) );
      x_clsAddAssign( s_class, "VALUE", X_FUNCNAME( GUI_EDIT_SETVALUE ) );
      x_clsAdd( s_class, "__GUICOMMAND", X_FUNCNAME( GUI_EDIT_COMMAND ) );
      x_clsAdd( s_class, "__GUIVALID", X_FUNCNAME( GUI_EDIT_VALID ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   gui_Get
   --------------------------------------------------------------------------------------------- */

// The edit field of a GET, as @ ... GET ... PICTURE, made for ReadModal( GetList ), in the fixed font
// 9x18: with @var the field shows the variable and ReadModal() gives the new value back to it when it ends
// with saving; the variable is a LOCAL of the function that runs ReadModal(); nLen characters are seen, without
// it the whole variable, and a longer value scrolls in the field; without cPicture no more characters than the
// variable has can be written, with nMax no more than nMax (for a memo field); with nRows the text wraps, the
// field shows one row and grows over the fields under it to at most nRows rows while it has the focus, and
// Shift+Enter begins a new line (CR LF); members as in gui_Edit
// gui_Get():New( oWindow, nX, nY, [nLen], [nHeight], [@var], [cPicture], [nRows], [nMax] ) --> self
X_FUNC_STATIC( GUI_GET_NEW )
{
   x_guiGetNew( ctx );
}

// gui_Get() --> oGet
X_FUNC( GUI_GET )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 0, "GUI_GET" );
      x_clsAddParent( s_class, x_clsRequire( "GUI_EDIT", GUI_EDIT ) );

      x_clsAdd( s_class, "NEW", X_FUNCNAME( GUI_GET_NEW ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   gui_Memo
   --------------------------------------------------------------------------------------------- */

// Create a multi-line text field; default size 200 x 80
// gui_Memo():New( oWindow, nX, nY, [nWidth], [nHeight], [cText] ) --> self
X_FUNC_STATIC( GUI_MEMO_NEW )
{
   x_guiControlNew( ctx, X_GUI_MEMO, 200, 80, 1 );
}

// Multi-line text field with a vertical scroll bar; members as in gui_Edit
// gui_Memo() --> oMemo
X_FUNC( GUI_MEMO )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 0, "GUI_MEMO" );
      x_clsAddParent( s_class, x_clsRequire( "GUI_EDIT", GUI_EDIT ) );

      x_clsAdd( s_class, "NEW", X_FUNCNAME( GUI_MEMO_NEW ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   gui_Button
   --------------------------------------------------------------------------------------------- */

// Create a push button; default size 90 x 25
// gui_Button():New( oWindow, nX, nY, [nWidth], [nHeight], [cText] ) --> self
X_FUNC_STATIC( GUI_BUTTON_NEW )
{
   x_guiControlNew( ctx, X_GUI_BUTTON, 90, 25, 1 );
}

// Return whether the button is the default button of its window
// oButton:isDefault --> lDefault
X_FUNC_STATIC( GUI_BUTTON_GETDEFAULT )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_BUTTON:ISDEFAULT" );

   if( window != NULL )
      x_retl( ctx, x_guiButtonRole( window, 0 ) );
}

// Make the button the default button of its window, pressed by Enter
// oButton:isDefault := lDefault
X_FUNC_STATIC( GUI_BUTTON_SETDEFAULT )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_BUTTON:ISDEFAULT" );

   if( window == NULL )
      return;

   if( !X_ISLOG( ctx, 1 ) )
   {
      x_guiRaise( ctx, "GUI_BUTTON:ISDEFAULT" );
      return;
   }

   x_guiSetButtonRole( window, 0, x_parl( ctx, 1 ) );
   x_retl( ctx, x_parl( ctx, 1 ) );
}

// Return whether the button is the cancel button of its window
// oButton:isCancel --> lCancel
X_FUNC_STATIC( GUI_BUTTON_GETCANCEL )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_BUTTON:ISCANCEL" );

   if( window != NULL )
      x_retl( ctx, x_guiButtonRole( window, 1 ) );
}

// Make the button the cancel button of its window, pressed by Escape
// oButton:isCancel := lCancel
X_FUNC_STATIC( GUI_BUTTON_SETCANCEL )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_BUTTON:ISCANCEL" );

   if( window == NULL )
      return;

   if( !X_ISLOG( ctx, 1 ) )
   {
      x_guiRaise( ctx, "GUI_BUTTON:ISCANCEL" );
      return;
   }

   x_guiSetButtonRole( window, 1, x_parl( ctx, 1 ) );
   x_retl( ctx, x_parl( ctx, 1 ) );
}

// Run OnClick when the button is clicked (internal)
// oButton:__GuiCommand( nCode ) --> NIL
X_FUNC_STATIC( GUI_BUTTON_COMMAND )
{
   if( x_parni( ctx, 1 ) == BN_CLICKED )
      x_guiEvent( ctx, 1 );
}

// Push button; OnClick runs when the button is clicked and receives the control
// gui_Button() --> oButton
X_FUNC( GUI_BUTTON )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 1, "GUI_BUTTON" );
      x_clsAddParent( s_class, x_clsRequire( "GUI_CONTROL", GUI_CONTROL ) );
      x_clsAddData( s_class, "ONCLICK", 1 );
      x_clsSetDataType( s_class, "ONCLICK", X_OO_VALUE_CODEBLOCK );

      x_clsAdd( s_class, "NEW", X_FUNCNAME( GUI_BUTTON_NEW ) );

      x_clsAddAccess( s_class, "ISDEFAULT", X_FUNCNAME( GUI_BUTTON_GETDEFAULT ) );
      x_clsAddAssign( s_class, "ISDEFAULT", X_FUNCNAME( GUI_BUTTON_SETDEFAULT ) );
      x_clsAddAccess( s_class, "ISCANCEL", X_FUNCNAME( GUI_BUTTON_GETCANCEL ) );
      x_clsAddAssign( s_class, "ISCANCEL", X_FUNCNAME( GUI_BUTTON_SETCANCEL ) );
      x_clsAdd( s_class, "__GUICOMMAND", X_FUNCNAME( GUI_BUTTON_COMMAND ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   gui_CheckBox
   --------------------------------------------------------------------------------------------- */

// Create a check box; default size 200 x 20
// gui_CheckBox():New( oWindow, nX, nY, [nWidth], [nHeight], [cText] ) --> self
X_FUNC_STATIC( GUI_CHECKBOX_NEW )
{
   x_guiControlNew( ctx, X_GUI_CHECKBOX, 200, 20, 1 );
}

// Return whether the check box is checked
// oCheckBox:checked --> lChecked
X_FUNC_STATIC( GUI_CHECKBOX_GETCHECKED )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_CHECKBOX:CHECKED" );

   if( window != NULL )
      x_retl( ctx, x_guiGetCheck( window ) );
}

// Set the state; checking a gui_RadioButton unchecks the others in its group
// oCheckBox:checked := lChecked
X_FUNC_STATIC( GUI_CHECKBOX_SETCHECKED )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_CHECKBOX:CHECKED" );

   if( window == NULL )
      return;

   if( !X_ISLOG( ctx, 1 ) )
   {
      x_guiRaise( ctx, "GUI_CHECKBOX:CHECKED" );
      return;
   }

   x_guiSetCheck( window, x_parl( ctx, 1 ) );
   x_retl( ctx, x_parl( ctx, 1 ) );
}

// Run OnChange when the user changes the state (internal)
// oCheckBox:__GuiCommand( nCode ) --> NIL
X_FUNC_STATIC( GUI_CHECKBOX_COMMAND )
{
   if( x_parni( ctx, 1 ) == BN_CLICKED )
      x_guiEvent( ctx, 1 );
}

// Check box; OnChange runs when the user changes the state and receives the control
// gui_CheckBox() --> oCheckBox
X_FUNC( GUI_CHECKBOX )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 1, "GUI_CHECKBOX" );
      x_clsAddParent( s_class, x_clsRequire( "GUI_CONTROL", GUI_CONTROL ) );
      x_clsAddData( s_class, "ONCHANGE", 1 );
      x_clsSetDataType( s_class, "ONCHANGE", X_OO_VALUE_CODEBLOCK );

      x_clsAdd( s_class, "NEW", X_FUNCNAME( GUI_CHECKBOX_NEW ) );

      x_clsAddAccess( s_class, "CHECKED", X_FUNCNAME( GUI_CHECKBOX_GETCHECKED ) );
      x_clsAddAssign( s_class, "CHECKED", X_FUNCNAME( GUI_CHECKBOX_SETCHECKED ) );
      x_clsAddAccess( s_class, "VALUE", X_FUNCNAME( GUI_CHECKBOX_GETCHECKED ) );
      x_clsAddAssign( s_class, "VALUE", X_FUNCNAME( GUI_CHECKBOX_SETCHECKED ) );
      x_clsAdd( s_class, "__GUICOMMAND", X_FUNCNAME( GUI_CHECKBOX_COMMAND ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   gui_RadioButton
   --------------------------------------------------------------------------------------------- */

// Create a radio button; default size 200 x 20
// gui_RadioButton():New( oWindow, nX, nY, [nWidth], [nHeight], [cText] ) --> self
X_FUNC_STATIC( GUI_RADIOBUTTON_NEW )
{
   x_guiControlNew( ctx, X_GUI_RADIO, 200, 20, 1 );
}

// Radio button; buttons created one after another form a group, members as in gui_CheckBox
// gui_RadioButton() --> oRadioButton
X_FUNC( GUI_RADIOBUTTON )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 0, "GUI_RADIOBUTTON" );
      x_clsAddParent( s_class, x_clsRequire( "GUI_CHECKBOX", GUI_CHECKBOX ) );

      x_clsAdd( s_class, "NEW", X_FUNCNAME( GUI_RADIOBUTTON_NEW ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   gui_GroupBox
   --------------------------------------------------------------------------------------------- */

// Create a group box; default size 200 x 100
// gui_GroupBox():New( oWindow, nX, nY, [nWidth], [nHeight], [cText] ) --> self
X_FUNC_STATIC( GUI_GROUPBOX_NEW )
{
   x_guiControlNew( ctx, X_GUI_GROUPBOX, 200, 100, 1 );
}

// Frame with a caption around other controls
// gui_GroupBox() --> oGroupBox
X_FUNC( GUI_GROUPBOX )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 0, "GUI_GROUPBOX" );
      x_clsAddParent( s_class, x_clsRequire( "GUI_CONTROL", GUI_CONTROL ) );

      x_clsAdd( s_class, "NEW", X_FUNCNAME( GUI_GROUPBOX_NEW ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   gui_ListBox
   --------------------------------------------------------------------------------------------- */

// Create a list box; default size 200 x 100
// gui_ListBox():New( oWindow, nX, nY, [nWidth], [nHeight] ) --> self
X_FUNC_STATIC( GUI_LISTBOX_NEW )
{
   x_guiControlNew( ctx, X_GUI_LISTBOX, 200, 100, 0 );
}

// Append an item and return its position
// oList:Add( cText ) --> nPosition
X_FUNC_STATIC( GUI_LISTBOX_ADD )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_LISTBOX:ADD" );
   int index;

   if( window != NULL && ( index = x_guiListAdd( ctx, window, x_paritem( ctx, 1 ) ) ) != 0 )
      x_retni( ctx, index );
}

// Remove all items
// oList:Clear() --> self
X_FUNC_STATIC( GUI_LISTBOX_CLEAR )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_LISTBOX:CLEAR" );

   if( window != NULL )
   {
      x_guiListClear( window );
      x_guiReturnSelf( ctx );
   }
}

// Return the text of the item at the position
// oList:Item( nPosition ) --> cText
X_FUNC_STATIC( GUI_LISTBOX_ITEM )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_LISTBOX:ITEM" );

   if( window != NULL )
      x_guiListItem( ctx, window, x_parni( ctx, 1 ) );
}

// Return the number of items
// oList:count --> nCount
X_FUNC_STATIC( GUI_LISTBOX_COUNT )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_LISTBOX:COUNT" );

   if( window != NULL )
      x_retni( ctx, x_guiListCount( window ) );
}

// Return the position of the selected item, 0 when nothing is selected
// oList:selected --> nPosition
X_FUNC_STATIC( GUI_LISTBOX_GETSELECTED )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_LISTBOX:SELECTED" );

   if( window != NULL )
      x_retni( ctx, x_guiListSelected( window ) );
}

// Select the item at the position; 0 clears the selection
// oList:selected := nPosition
X_FUNC_STATIC( GUI_LISTBOX_SETSELECTED )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_LISTBOX:SELECTED" );

   if( window == NULL )
      return;

   if( !X_ISNUM( ctx, 1 ) )
   {
      x_guiRaise( ctx, "GUI_LISTBOX:SELECTED" );
      return;
   }

   x_guiListSetSelected( ctx, window, x_parni( ctx, 1 ) );
   x_retni( ctx, x_parni( ctx, 1 ) );
}

// Return the text of the selected item, "" when nothing is selected
// oList:selectedText --> cText
X_FUNC_STATIC( GUI_LISTBOX_SELECTEDTEXT )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_LISTBOX:SELECTEDTEXT" );
   int index;

   if( window == NULL )
      return;

   if( ( index = x_guiListSelected( window ) ) == 0 )
      x_retc( ctx, "" );
   else
      x_guiListItem( ctx, window, index );
}

// Run OnSelect or OnDoubleClick for the user action (internal)
// oList:__GuiCommand( nCode ) --> NIL
X_FUNC_STATIC( GUI_LISTBOX_COMMAND )
{
   if( x_parni( ctx, 1 ) == LBN_SELCHANGE )
      x_guiEvent( ctx, 1 );
   else if( x_parni( ctx, 1 ) == LBN_DBLCLK )
      x_guiEvent( ctx, 2 );
}

// List of text items with 1-based positions; OnSelect and OnDoubleClick receive the control
// gui_ListBox() --> oList
X_FUNC( GUI_LISTBOX )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 2, "GUI_LISTBOX" );
      x_clsAddParent( s_class, x_clsRequire( "GUI_CONTROL", GUI_CONTROL ) );
      x_clsAddData( s_class, "ONSELECT", 1 );
      x_clsSetDataType( s_class, "ONSELECT", X_OO_VALUE_CODEBLOCK );
      x_clsAddData( s_class, "ONDOUBLECLICK", 2 );
      x_clsSetDataType( s_class, "ONDOUBLECLICK", X_OO_VALUE_CODEBLOCK );

      x_clsAdd( s_class, "NEW", X_FUNCNAME( GUI_LISTBOX_NEW ) );
      x_clsAdd( s_class, "ADD", X_FUNCNAME( GUI_LISTBOX_ADD ) );
      x_clsAdd( s_class, "CLEAR", X_FUNCNAME( GUI_LISTBOX_CLEAR ) );
      x_clsAdd( s_class, "ITEM", X_FUNCNAME( GUI_LISTBOX_ITEM ) );

      x_clsAddAccess( s_class, "COUNT", X_FUNCNAME( GUI_LISTBOX_COUNT ) );
      x_clsAddAccess( s_class, "SELECTED", X_FUNCNAME( GUI_LISTBOX_GETSELECTED ) );
      x_clsAddAssign( s_class, "SELECTED", X_FUNCNAME( GUI_LISTBOX_SETSELECTED ) );
      x_clsAddAccess( s_class, "VALUE", X_FUNCNAME( GUI_LISTBOX_GETSELECTED ) );
      x_clsAddAssign( s_class, "VALUE", X_FUNCNAME( GUI_LISTBOX_SETSELECTED ) );
      x_clsAddAccess( s_class, "SELECTEDTEXT", X_FUNCNAME( GUI_LISTBOX_SELECTEDTEXT ) );
      x_clsAdd( s_class, "__GUICOMMAND", X_FUNCNAME( GUI_LISTBOX_COMMAND ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   gui_ComboBox
   --------------------------------------------------------------------------------------------- */

// Create a drop-down list; default size 200 x 200 including the open list
// gui_ComboBox():New( oWindow, nX, nY, [nWidth], [nHeight] ) --> self
X_FUNC_STATIC( GUI_COMBOBOX_NEW )
{
   x_guiControlNew( ctx, X_GUI_COMBOBOX, 200, 200, 0 );
}

// Drop-down list; members as in gui_ListBox, OnDoubleClick is not used
// gui_ComboBox() --> oComboBox
X_FUNC( GUI_COMBOBOX )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 0, "GUI_COMBOBOX" );
      x_clsAddParent( s_class, x_clsRequire( "GUI_LISTBOX", GUI_LISTBOX ) );

      x_clsAdd( s_class, "NEW", X_FUNCNAME( GUI_COMBOBOX_NEW ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   gui_ProgressBar
   --------------------------------------------------------------------------------------------- */

// Create a progress bar with the range 0 to 100; default size 200 x 20
// gui_ProgressBar():New( oWindow, nX, nY, [nWidth], [nHeight] ) --> self
X_FUNC_STATIC( GUI_PROGRESSBAR_NEW )
{
   x_guiControlNew( ctx, X_GUI_PROGRESSBAR, 200, 20, 0 );
}

// Return the current position
// oProgress:value --> nValue
X_FUNC_STATIC( GUI_PROGRESSBAR_GETVALUE )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_PROGRESSBAR:VALUE" );

   if( window != NULL )
      x_retni( ctx, x_guiProgressValue( window ) );
}

// Set the current position between 0 and the maximum
// oProgress:value := nValue
X_FUNC_STATIC( GUI_PROGRESSBAR_SETVALUE )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_PROGRESSBAR:VALUE" );

   if( window == NULL )
      return;

   if( !X_ISNUM( ctx, 1 ) || x_parni( ctx, 1 ) < 0 || x_parni( ctx, 1 ) > x_guiProgressMaximum( window ) )
   {
      x_guiRaise( ctx, "GUI_PROGRESSBAR:VALUE" );
      return;
   }

   x_guiProgressSetValue( window, x_parni( ctx, 1 ) );
   x_retni( ctx, x_parni( ctx, 1 ) );
}

// Return the maximum position
// oProgress:maximum --> nMaximum
X_FUNC_STATIC( GUI_PROGRESSBAR_GETMAXIMUM )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_PROGRESSBAR:MAXIMUM" );

   if( window != NULL )
      x_retni( ctx, x_guiProgressMaximum( window ) );
}

// Set the maximum position; the minimum is always 0
// oProgress:maximum := nMaximum
X_FUNC_STATIC( GUI_PROGRESSBAR_SETMAXIMUM )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_PROGRESSBAR:MAXIMUM" );

   if( window == NULL )
      return;

   if( !X_ISNUM( ctx, 1 ) || x_parni( ctx, 1 ) <= 0 )
   {
      x_guiRaise( ctx, "GUI_PROGRESSBAR:MAXIMUM" );
      return;
   }

   x_guiProgressSetMaximum( window, x_parni( ctx, 1 ) );
   x_retni( ctx, x_parni( ctx, 1 ) );
}

// Progress bar
// gui_ProgressBar() --> oProgress
X_FUNC( GUI_PROGRESSBAR )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 0, "GUI_PROGRESSBAR" );
      x_clsAddParent( s_class, x_clsRequire( "GUI_CONTROL", GUI_CONTROL ) );

      x_clsAdd( s_class, "NEW", X_FUNCNAME( GUI_PROGRESSBAR_NEW ) );

      x_clsAddAccess( s_class, "VALUE", X_FUNCNAME( GUI_PROGRESSBAR_GETVALUE ) );
      x_clsAddAssign( s_class, "VALUE", X_FUNCNAME( GUI_PROGRESSBAR_SETVALUE ) );
      x_clsAddAccess( s_class, "MAXIMUM", X_FUNCNAME( GUI_PROGRESSBAR_GETMAXIMUM ) );
      x_clsAddAssign( s_class, "MAXIMUM", X_FUNCNAME( GUI_PROGRESSBAR_SETMAXIMUM ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   gui_StatusBar
   --------------------------------------------------------------------------------------------- */

// Create a status bar at the bottom of the window; it follows the width of the window
// gui_StatusBar():New( oWindow, [cText] ) --> self
X_FUNC_STATIC( GUI_STATUSBAR_NEW )
{
   x_guiStatusBarNew( ctx );
}

// Add a panel; without nWidth (or 0) the panel shares what the other panels leave of the bar; the panel is
// sunken, lRaised makes it raised; the first panel takes the place of the single text of the bar;
// returns the number of the panel, counted from 1
// oStatusBar:Add( [nWidth], [cText], [lRaised] ) --> nPanel
X_FUNC_STATIC( GUI_STATUSBAR_ADD )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_STATUSBAR:ADD" );
   int panel;

   if( window == NULL )
      return;

   if( ( !X_ISNIL( ctx, 1 ) && !X_ISNUM( ctx, 1 ) ) || ( !X_ISNIL( ctx, 3 ) && !X_ISLOG( ctx, 3 ) ) )
   {
      x_guiRaise( ctx, "GUI_STATUSBAR:ADD" );
      return;
   }

   panel = x_guiStatusBarAdd( ctx, window, x_parnidef( ctx, 1, 0 ), x_paritem( ctx, 2 ), x_parl( ctx, 3 ) );
   if( panel != 0 )
      x_retni( ctx, panel );
}

// Set the text of a panel
// oStatusBar:SetText( nPanel, cText ) --> self
X_FUNC_STATIC( GUI_STATUSBAR_SETTEXT )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_STATUSBAR:SETTEXT" );

   if( window == NULL )
      return;

   if( !X_ISNUM( ctx, 1 ) )
   {
      x_guiRaise( ctx, "GUI_STATUSBAR:SETTEXT" );
      return;
   }

   x_guiStatusBarSetText( ctx, window, x_parni( ctx, 1 ), x_paritem( ctx, 2 ) );
   x_itemReturn( ctx, ctx->self );
}

// Panel that shows "Ins" during ReadModal() of the window while SET INSERT is on, as SET SCOREBOARD;
// 0 (at the start) for none
// oStatusBar:scoreboard --> nPanel
X_FUNC_STATIC( GUI_STATUSBAR_GETSCOREBOARD )
{
   x_guiStatusBarScoreboard( ctx );
}

// oStatusBar:scoreboard := nPanel
X_FUNC_STATIC( GUI_STATUSBAR_SETSCOREBOARD )
{
   x_guiStatusBarSetScoreboard( ctx );
}

// Text color of the bar (0xRRGGBB), NIL for the system color
// oStatusBar:foreground --> nColor
X_FUNC_STATIC( GUI_STATUSBAR_GETFOREGROUND )
{
   x_guiStatusBarColor( ctx, X_GUI_COLOR_FOREGROUND );
}

// oStatusBar:foreground := nColor, NIL returns to the system color
X_FUNC_STATIC( GUI_STATUSBAR_SETFOREGROUND )
{
   x_guiStatusBarSetColor( ctx, X_GUI_COLOR_FOREGROUND );
}

// Color of the bar, NIL for the system color
// oStatusBar:background --> nColor
X_FUNC_STATIC( GUI_STATUSBAR_GETBACKGROUND )
{
   x_guiStatusBarColor( ctx, X_GUI_COLOR_BACKGROUND );
}

// oStatusBar:background := nColor, NIL returns to the system color
X_FUNC_STATIC( GUI_STATUSBAR_SETBACKGROUND )
{
   x_guiStatusBarSetColor( ctx, X_GUI_COLOR_BACKGROUND );
}

// Status bar; :text is the text shown in the bar
// gui_StatusBar() --> oStatusBar
X_FUNC( GUI_STATUSBAR )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 0, "GUI_STATUSBAR" );
      x_clsAddParent( s_class, x_clsRequire( "GUI_CONTROL", GUI_CONTROL ) );

      x_clsAdd( s_class, "NEW", X_FUNCNAME( GUI_STATUSBAR_NEW ) );
      x_clsAdd( s_class, "ADD", X_FUNCNAME( GUI_STATUSBAR_ADD ) );
      x_clsAdd( s_class, "SETTEXT", X_FUNCNAME( GUI_STATUSBAR_SETTEXT ) );

      x_clsAddAccess( s_class, "SCOREBOARD", X_FUNCNAME( GUI_STATUSBAR_GETSCOREBOARD ) );
      x_clsAddAssign( s_class, "SCOREBOARD", X_FUNCNAME( GUI_STATUSBAR_SETSCOREBOARD ) );
      x_clsAddAccess( s_class, "FOREGROUND", X_FUNCNAME( GUI_STATUSBAR_GETFOREGROUND ) );
      x_clsAddAssign( s_class, "FOREGROUND", X_FUNCNAME( GUI_STATUSBAR_SETFOREGROUND ) );
      x_clsAddAccess( s_class, "BACKGROUND", X_FUNCNAME( GUI_STATUSBAR_GETBACKGROUND ) );
      x_clsAddAssign( s_class, "BACKGROUND", X_FUNCNAME( GUI_STATUSBAR_SETBACKGROUND ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   gui_Browse
   --------------------------------------------------------------------------------------------- */

// Create a table of records; default size 400 x 200
// gui_Browse():New( oWindow, nX, nY, [nWidth], [nHeight] ) --> self
X_FUNC_STATIC( GUI_BROWSE_NEW )
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
X_FUNC_STATIC( GUI_BROWSE_ADDCOLUMN )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_BROWSE:ADDCOLUMN" );
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
      x_guiRaise( ctx, "GUI_BROWSE:ADDCOLUMN" );
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
X_FUNC_STATIC( GUI_BROWSE_REFRESH )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_BROWSE:REFRESH" );

   if( window != NULL )
   {
      x_guiBrowseRefresh( window );
      x_guiReturnSelf( ctx );
   }
}

// Number of the first columns that stay at the left when the table scrolls sideways, as TBrowse:freeze
// oBrowse:freeze --> nColumns
X_FUNC_STATIC( GUI_BROWSE_GETFREEZE )
{
   x_guiBrowseFreeze( ctx );
}

// oBrowse:freeze := nColumns
X_FUNC_STATIC( GUI_BROWSE_SETFREEZE )
{
   x_guiBrowseSetFreeze( ctx );
}

// Number of the column with the cursor, counted from 1, as in TBrowse
// oBrowse:colPos --> nColumn
X_FUNC_STATIC( GUI_BROWSE_GETCOLPOS )
{
   x_guiBrowseColPos( ctx );
}

// Move the cursor to the column nColumn; the table scrolls so that the column is seen
// oBrowse:colPos := nColumn
X_FUNC_STATIC( GUI_BROWSE_SETCOLPOS )
{
   x_guiBrowseSetColPos( ctx );
}

// Run goTopBlock, goBottomBlock, skipBlock, positionBlock or emptyBlock (internal)
// oBrowse:__GuiNavigate( nOperation, nArgument ) --> nResult
X_FUNC_STATIC( GUI_BROWSE_NAVIGATE )
{
   const X_ITEM *goTop = x_objDataSlot( ctx, 1 );
   const X_ITEM *goBottom = x_objDataSlot( ctx, 2 );
   const X_ITEM *skip = x_objDataSlot( ctx, 3 );
   const X_ITEM *position = x_objDataSlot( ctx, 4 );
   const X_ITEM *empty = x_objDataSlot( ctx, 8 );

   x_guiBrowseNavigate( ctx, goTop, goBottom, skip, position, empty );
}

// Texts and types of the columns of the current record (internal)
// oBrowse:__GuiRow() --> aTextsAndTypes
X_FUNC_STATIC( GUI_BROWSE_ROW )
{
   x_guiBrowseRow( ctx, x_objDataSlot( ctx, 7 ) );
}

// Give the column nColumn of logical values the opposite value of the current record (internal)
// oBrowse:__GuiToggle( nColumn ) --> NIL
X_FUNC_STATIC( GUI_BROWSE_TOGGLE )
{
   x_guiBrowseToggle( ctx, x_objDataSlot( ctx, 7 ), x_parni( ctx, 1 ) );
}

// Run OnChange (1) or OnDoubleClick (2) (internal)
// oBrowse:__GuiEvent( nEvent ) --> NIL
X_FUNC_STATIC( GUI_BROWSE_EVENT )
{
   x_guiEvent( ctx, IIF( x_parni( ctx, 1 ) == 1, 5, 6 ) );
}

// Color of the header with the column titles (0xRRGGBB)
// oBrowse:headerBackground --> nColor
X_FUNC_STATIC( GUI_BROWSE_GETHEADERBACKGROUND )
{
   x_guiBrowseColor( ctx, X_GUI_GRID_HEADBACKGROUND );
}

// oBrowse:headerBackground := nColor
X_FUNC_STATIC( GUI_BROWSE_SETHEADERBACKGROUND )
{
   x_guiBrowseSetColor( ctx, X_GUI_GRID_HEADBACKGROUND );
}

// Text color of the column titles (0xRRGGBB)
// oBrowse:headerForeground --> nColor
X_FUNC_STATIC( GUI_BROWSE_GETHEADERFOREGROUND )
{
   x_guiBrowseColor( ctx, X_GUI_GRID_HEADFOREGROUND );
}

// oBrowse:headerForeground := nColor
X_FUNC_STATIC( GUI_BROWSE_SETHEADERFOREGROUND )
{
   x_guiBrowseSetColor( ctx, X_GUI_GRID_HEADFOREGROUND );
}

// Color of the lines between the column titles (0xRRGGBB)
// oBrowse:headerLine --> nColor
X_FUNC_STATIC( GUI_BROWSE_GETHEADERLINE )
{
   x_guiBrowseColor( ctx, X_GUI_GRID_HEADLINE );
}

// oBrowse:headerLine := nColor
X_FUNC_STATIC( GUI_BROWSE_SETHEADERLINE )
{
   x_guiBrowseSetColor( ctx, X_GUI_GRID_HEADLINE );
}

// Color of the rows (0xRRGGBB)
// oBrowse:background --> nColor
X_FUNC_STATIC( GUI_BROWSE_GETBACKGROUND )
{
   x_guiBrowseColor( ctx, X_GUI_GRID_BACKGROUND );
}

// oBrowse:background := nColor
X_FUNC_STATIC( GUI_BROWSE_SETBACKGROUND )
{
   x_guiBrowseSetColor( ctx, X_GUI_GRID_BACKGROUND );
}

// Color of every second row (0xRRGGBB)
// oBrowse:alternateBackground --> nColor
X_FUNC_STATIC( GUI_BROWSE_GETALTERNATEBACKGROUND )
{
   x_guiBrowseColor( ctx, X_GUI_GRID_ALTERNATEBACKGROUND );
}

// oBrowse:alternateBackground := nColor
X_FUNC_STATIC( GUI_BROWSE_SETALTERNATEBACKGROUND )
{
   x_guiBrowseSetColor( ctx, X_GUI_GRID_ALTERNATEBACKGROUND );
}

// Text color of the rows (0xRRGGBB)
// oBrowse:foreground --> nColor
X_FUNC_STATIC( GUI_BROWSE_GETFOREGROUND )
{
   x_guiBrowseColor( ctx, X_GUI_GRID_FOREGROUND );
}

// oBrowse:foreground := nColor
X_FUNC_STATIC( GUI_BROWSE_SETFOREGROUND )
{
   x_guiBrowseSetColor( ctx, X_GUI_GRID_FOREGROUND );
}

// Color of the lines between the rows and the columns (0xRRGGBB)
// oBrowse:gridLine --> nColor
X_FUNC_STATIC( GUI_BROWSE_GETGRIDLINE )
{
   x_guiBrowseColor( ctx, X_GUI_GRID_LINE );
}

// oBrowse:gridLine := nColor
X_FUNC_STATIC( GUI_BROWSE_SETGRIDLINE )
{
   x_guiBrowseSetColor( ctx, X_GUI_GRID_LINE );
}

// Color of the cursor row (0xRRGGBB)
// oBrowse:cursorBackground --> nColor
X_FUNC_STATIC( GUI_BROWSE_GETCURSORBACKGROUND )
{
   x_guiBrowseColor( ctx, X_GUI_GRID_CURSORBACKGROUND );
}

// oBrowse:cursorBackground := nColor
X_FUNC_STATIC( GUI_BROWSE_SETCURSORBACKGROUND )
{
   x_guiBrowseSetColor( ctx, X_GUI_GRID_CURSORBACKGROUND );
}

// Text color of the cursor row (0xRRGGBB)
// oBrowse:cursorForeground --> nColor
X_FUNC_STATIC( GUI_BROWSE_GETCURSORFOREGROUND )
{
   x_guiBrowseColor( ctx, X_GUI_GRID_CURSORFOREGROUND );
}

// oBrowse:cursorForeground := nColor
X_FUNC_STATIC( GUI_BROWSE_SETCURSORFOREGROUND )
{
   x_guiBrowseSetColor( ctx, X_GUI_GRID_CURSORFOREGROUND );
}

// Color of the cursor row while the table has no focus (0xRRGGBB)
// oBrowse:inactiveCursor --> nColor
X_FUNC_STATIC( GUI_BROWSE_GETINACTIVECURSOR )
{
   x_guiBrowseColor( ctx, X_GUI_GRID_INACTIVECURSOR );
}

// oBrowse:inactiveCursor := nColor
X_FUNC_STATIC( GUI_BROWSE_SETINACTIVECURSOR )
{
   x_guiBrowseSetColor( ctx, X_GUI_GRID_INACTIVECURSOR );
}

// Color of the tick of a logical column (0xRRGGBB); on the cursor the tick has the color of the cursor text
// oBrowse:checkForeground --> nColor
X_FUNC_STATIC( GUI_BROWSE_GETCHECKFOREGROUND )
{
   x_guiBrowseColor( ctx, X_GUI_GRID_CHECKFOREGROUND );
}

// oBrowse:checkForeground := nColor
X_FUNC_STATIC( GUI_BROWSE_SETCHECKFOREGROUND )
{
   x_guiBrowseSetColor( ctx, X_GUI_GRID_CHECKFOREGROUND );
}

// Table of records read through goTopBlock, goBottomBlock and skipBlock { |n| nSkipped }, like TBrowse;
// positionBlock may return 0..1 for the scroll bar; emptyBlock { || lEmpty } gives .T. when there is no
// record, and the table then shows no row; OnChange runs when the cursor reaches another record,
// OnDoubleClick on a double click or Enter; both receive the control
// gui_Browse() --> oBrowse
X_FUNC( GUI_BROWSE )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 8, "GUI_BROWSE" );
      x_clsAddParent( s_class, x_clsRequire( "GUI_CONTROL", GUI_CONTROL ) );
      x_clsAddData( s_class, "GOTOPBLOCK", 1 );
      x_clsSetDataType( s_class, "GOTOPBLOCK", X_OO_VALUE_CODEBLOCK );
      x_clsAddData( s_class, "GOBOTTOMBLOCK", 2 );
      x_clsSetDataType( s_class, "GOBOTTOMBLOCK", X_OO_VALUE_CODEBLOCK );
      x_clsAddData( s_class, "SKIPBLOCK", 3 );
      x_clsSetDataType( s_class, "SKIPBLOCK", X_OO_VALUE_CODEBLOCK );
      x_clsAddData( s_class, "POSITIONBLOCK", 4 );
      x_clsSetDataType( s_class, "POSITIONBLOCK", X_OO_VALUE_CODEBLOCK );
      x_clsAddData( s_class, "ONCHANGE", 5 );
      x_clsSetDataType( s_class, "ONCHANGE", X_OO_VALUE_CODEBLOCK );
      x_clsAddData( s_class, "ONDOUBLECLICK", 6 );
      x_clsSetDataType( s_class, "ONDOUBLECLICK", X_OO_VALUE_CODEBLOCK );
      x_clsAddData( s_class, "COLUMNS", 7 );
      x_clsSetScope( s_class, "COLUMNS", X_OO_CLSTP_HIDDEN );
      x_clsAddData( s_class, "EMPTYBLOCK", 8 );
      x_clsSetDataType( s_class, "EMPTYBLOCK", X_OO_VALUE_CODEBLOCK );

      x_clsAdd( s_class, "NEW", X_FUNCNAME( GUI_BROWSE_NEW ) );
      x_clsAdd( s_class, "ADDCOLUMN", X_FUNCNAME( GUI_BROWSE_ADDCOLUMN ) );
      x_clsAdd( s_class, "REFRESH", X_FUNCNAME( GUI_BROWSE_REFRESH ) );
      x_clsAddAccess( s_class, "FREEZE", X_FUNCNAME( GUI_BROWSE_GETFREEZE ) );
      x_clsAddAssign( s_class, "FREEZE", X_FUNCNAME( GUI_BROWSE_SETFREEZE ) );
      x_clsAddAccess( s_class, "COLPOS", X_FUNCNAME( GUI_BROWSE_GETCOLPOS ) );
      x_clsAddAssign( s_class, "COLPOS", X_FUNCNAME( GUI_BROWSE_SETCOLPOS ) );
      x_clsAddAccess( s_class, "HEADERBACKGROUND", X_FUNCNAME( GUI_BROWSE_GETHEADERBACKGROUND ) );
      x_clsAddAssign( s_class, "HEADERBACKGROUND", X_FUNCNAME( GUI_BROWSE_SETHEADERBACKGROUND ) );
      x_clsAddAccess( s_class, "HEADERFOREGROUND", X_FUNCNAME( GUI_BROWSE_GETHEADERFOREGROUND ) );
      x_clsAddAssign( s_class, "HEADERFOREGROUND", X_FUNCNAME( GUI_BROWSE_SETHEADERFOREGROUND ) );
      x_clsAddAccess( s_class, "HEADERLINE", X_FUNCNAME( GUI_BROWSE_GETHEADERLINE ) );
      x_clsAddAssign( s_class, "HEADERLINE", X_FUNCNAME( GUI_BROWSE_SETHEADERLINE ) );
      x_clsAddAccess( s_class, "BACKGROUND", X_FUNCNAME( GUI_BROWSE_GETBACKGROUND ) );
      x_clsAddAssign( s_class, "BACKGROUND", X_FUNCNAME( GUI_BROWSE_SETBACKGROUND ) );
      x_clsAddAccess( s_class, "ALTERNATEBACKGROUND", X_FUNCNAME( GUI_BROWSE_GETALTERNATEBACKGROUND ) );
      x_clsAddAssign( s_class, "ALTERNATEBACKGROUND", X_FUNCNAME( GUI_BROWSE_SETALTERNATEBACKGROUND ) );
      x_clsAddAccess( s_class, "FOREGROUND", X_FUNCNAME( GUI_BROWSE_GETFOREGROUND ) );
      x_clsAddAssign( s_class, "FOREGROUND", X_FUNCNAME( GUI_BROWSE_SETFOREGROUND ) );
      x_clsAddAccess( s_class, "GRIDLINE", X_FUNCNAME( GUI_BROWSE_GETGRIDLINE ) );
      x_clsAddAssign( s_class, "GRIDLINE", X_FUNCNAME( GUI_BROWSE_SETGRIDLINE ) );
      x_clsAddAccess( s_class, "CURSORBACKGROUND", X_FUNCNAME( GUI_BROWSE_GETCURSORBACKGROUND ) );
      x_clsAddAssign( s_class, "CURSORBACKGROUND", X_FUNCNAME( GUI_BROWSE_SETCURSORBACKGROUND ) );
      x_clsAddAccess( s_class, "CURSORFOREGROUND", X_FUNCNAME( GUI_BROWSE_GETCURSORFOREGROUND ) );
      x_clsAddAssign( s_class, "CURSORFOREGROUND", X_FUNCNAME( GUI_BROWSE_SETCURSORFOREGROUND ) );
      x_clsAddAccess( s_class, "INACTIVECURSOR", X_FUNCNAME( GUI_BROWSE_GETINACTIVECURSOR ) );
      x_clsAddAssign( s_class, "INACTIVECURSOR", X_FUNCNAME( GUI_BROWSE_SETINACTIVECURSOR ) );
      x_clsAddAccess( s_class, "CHECKFOREGROUND", X_FUNCNAME( GUI_BROWSE_GETCHECKFOREGROUND ) );
      x_clsAddAssign( s_class, "CHECKFOREGROUND", X_FUNCNAME( GUI_BROWSE_SETCHECKFOREGROUND ) );
      x_clsAdd( s_class, "__GUINAVIGATE", X_FUNCNAME( GUI_BROWSE_NAVIGATE ) );
      x_clsAdd( s_class, "__GUIROW", X_FUNCNAME( GUI_BROWSE_ROW ) );
      x_clsAdd( s_class, "__GUIEVENT", X_FUNCNAME( GUI_BROWSE_EVENT ) );
      x_clsAdd( s_class, "__GUITOGGLE", X_FUNCNAME( GUI_BROWSE_TOGGLE ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   gui_Toolbar
   --------------------------------------------------------------------------------------------- */

// Create a toolbar at the side cSide of the window: "Top" (default), "Bottom" (above the status bar, also .T.),
// the buttons sharing the whole width of the window in one row, "Left" or "Right" (between the top and bottom
// bars, the buttons one under another with the text under the picture)
// gui_Toolbar():New( oWindow, [cSide] ) --> self
X_FUNC_STATIC( GUI_TOOLBAR_NEW )
{
   X_ITEM *actions = x_objDataSlot( ctx, 1 );

   x_guiToolbarNew( ctx );
   if( actions != NULL && !X_IS_ARRAY( actions ) )
      x_arrayNew( actions, 0 );
}

// Add a button with the text and an optional BMP picture; bAction runs on click and receives the toolbar;
// cShortcut is written as in gui_Menu, e.g. "Ctrl+N", "Del", "F2", and without cToolTip it is shown in the tip;
// returns the number of the button, counted from 1 without separators
// oToolbar:Add( cText, [cBitmap], [bAction], [cToolTip], [cShortcut] ) --> nButton
X_FUNC_STATIC( GUI_TOOLBAR_ADD )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_TOOLBAR:ADD" );
   X_ITEM *actions = x_objDataSlot( ctx, 1 );
   const X_ITEM *toolTip = x_paritem( ctx, 4 );
   const X_ITEM *shortcut = x_paritem( ctx, 5 );
   X_ITEM nil = X_ITEM_NIL_INIT;
   int button;

   if( window == NULL || actions == NULL || !X_IS_ARRAY( actions ) )
      return;

   if( !X_ISNIL( ctx, 3 ) && !X_ISBLOCK( ctx, 3 ) )
   {
      x_guiRaise( ctx, "GUI_TOOLBAR:ADD" );
      return;
   }

   button = x_guiToolbarAdd( ctx, window, x_paritem( ctx, 1 ), x_paritem( ctx, 2 ), toolTip, shortcut, 0 );
   if( button != 0 )
   {
      x_arrayAdd( actions, IIF( x_paritem( ctx, 3 ) != NULL, x_paritem( ctx, 3 ), &nil ) );
      x_retni( ctx, button );
   }
}

// Add a button with an arrow that opens oMenu, a gui_Menu():New( .T. ); a click or cShortcut opens the menu
// under the button, or above it on a bar at the bottom; the commands of the menu run as those of the menu bar
// and their bWhen makes them active or grey each time the menu opens
// oToolbar:AddMenu( cText, oMenu, [cBitmap], [cToolTip], [cShortcut] ) --> nButton
X_FUNC_STATIC( GUI_TOOLBAR_ADDMENU )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_TOOLBAR:ADDMENU" );
   X_ITEM *actions = x_objDataSlot( ctx, 1 );
   const X_ITEM *toolTip = x_paritem( ctx, 4 );
   const X_ITEM *shortcut = x_paritem( ctx, 5 );
   int button;

   if( window == NULL || actions == NULL || !X_IS_ARRAY( actions ) )
      return;

   if( x_guiObjectMenu( ctx, x_paritem( ctx, 2 ), "GUI_TOOLBAR:ADDMENU" ) == NULL )
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
X_FUNC_STATIC( GUI_TOOLBAR_ADDSEPARATOR )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_TOOLBAR:ADDSEPARATOR" );

   if( window != NULL )
   {
      x_guiToolbarSeparator( ctx, window );
      x_guiReturnSelf( ctx );
   }
}

// Remove all buttons, e.g. when another part of the program takes the bar
// oToolbar:Clear() --> self
X_FUNC_STATIC( GUI_TOOLBAR_CLEAR )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_TOOLBAR:CLEAR" );
   X_ITEM *actions = x_objDataSlot( ctx, 1 );

   if( window == NULL || actions == NULL )
      return;

   x_guiToolbarClear( window );
   x_arrayNew( actions, 0 );
   x_guiReturnSelf( ctx );
}

// Make button nButton active or grey; a grey button and its cShortcut do nothing
// oToolbar:Enable( nButton, lEnable ) --> self
X_FUNC_STATIC( GUI_TOOLBAR_ENABLE )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_TOOLBAR:ENABLE" );

   if( window == NULL )
      return;

   if( !X_ISNUM( ctx, 1 ) || !X_ISLOG( ctx, 2 ) || !x_guiToolbarEnable( window, x_parni( ctx, 1 ), x_parl( ctx, 2 ) ) )
   {
      x_guiRaise( ctx, "GUI_TOOLBAR:ENABLE" );
      return;
   }

   x_guiReturnSelf( ctx );
}

// Text color of the buttons (0xRRGGBB), NIL for the system color
// oToolbar:foreground --> nColor
X_FUNC_STATIC( GUI_TOOLBAR_GETFOREGROUND )
{
   x_guiToolbarColor( ctx, X_GUI_COLOR_FOREGROUND );
}

// oToolbar:foreground := nColor, NIL returns to the system color
X_FUNC_STATIC( GUI_TOOLBAR_SETFOREGROUND )
{
   x_guiToolbarSetColor( ctx, X_GUI_COLOR_FOREGROUND );
}

// Color of the bar and of the buttons, NIL for the system color
// oToolbar:background --> nColor
X_FUNC_STATIC( GUI_TOOLBAR_GETBACKGROUND )
{
   x_guiToolbarColor( ctx, X_GUI_COLOR_BACKGROUND );
}

// oToolbar:background := nColor, NIL returns to the system color
X_FUNC_STATIC( GUI_TOOLBAR_SETBACKGROUND )
{
   x_guiToolbarSetColor( ctx, X_GUI_COLOR_BACKGROUND );
}

// Text color of the button under the mouse, NIL for the system color
// oToolbar:hoverForeground --> nColor
X_FUNC_STATIC( GUI_TOOLBAR_GETHOVERFOREGROUND )
{
   x_guiToolbarColor( ctx, X_GUI_COLOR_HOVERFOREGROUND );
}

// oToolbar:hoverForeground := nColor, NIL returns to the system color
X_FUNC_STATIC( GUI_TOOLBAR_SETHOVERFOREGROUND )
{
   x_guiToolbarSetColor( ctx, X_GUI_COLOR_HOVERFOREGROUND );
}

// Color of the button under the mouse, NIL for the system color
// oToolbar:hoverBackground --> nColor
X_FUNC_STATIC( GUI_TOOLBAR_GETHOVERBACKGROUND )
{
   x_guiToolbarColor( ctx, X_GUI_COLOR_HOVERBACKGROUND );
}

// oToolbar:hoverBackground := nColor, NIL returns to the system color
X_FUNC_STATIC( GUI_TOOLBAR_SETHOVERBACKGROUND )
{
   x_guiToolbarSetColor( ctx, X_GUI_COLOR_HOVERBACKGROUND );
}

// Text color of the button held down by the mouse, NIL for the system color
// oToolbar:pressForeground --> nColor
X_FUNC_STATIC( GUI_TOOLBAR_GETPRESSFOREGROUND )
{
   x_guiToolbarColor( ctx, X_GUI_COLOR_PRESSFOREGROUND );
}

// oToolbar:pressForeground := nColor, NIL returns to the system color
X_FUNC_STATIC( GUI_TOOLBAR_SETPRESSFOREGROUND )
{
   x_guiToolbarSetColor( ctx, X_GUI_COLOR_PRESSFOREGROUND );
}

// Color of the button held down by the mouse, NIL for the system color
// oToolbar:pressBackground --> nColor
X_FUNC_STATIC( GUI_TOOLBAR_GETPRESSBACKGROUND )
{
   x_guiToolbarColor( ctx, X_GUI_COLOR_PRESSBACKGROUND );
}

// oToolbar:pressBackground := nColor, NIL returns to the system color
X_FUNC_STATIC( GUI_TOOLBAR_SETPRESSBACKGROUND )
{
   x_guiToolbarSetColor( ctx, X_GUI_COLOR_PRESSBACKGROUND );
}

// Receive a click of button nButton (internal)
// oToolbar:__GuiCommand( nCode, nButton ) --> NIL
X_FUNC_STATIC( GUI_TOOLBAR_COMMAND )
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
      HWND toolbar = x_guiSelfHandle( ctx, "GUI_TOOLBAR:ADDMENU" );
      HMENU menu = x_guiObjectMenu( ctx, &block, "GUI_TOOLBAR:ADDMENU" );

      x_guiToolbarPopup( toolbar, x_parni( ctx, 2 ), menu );
      return;
   }

   if( !X_IS_BLOCK( &block ) )
      return;

   args[0] = ( X_ARG ){ ctx->self, 0 };
   x_evalBlock( ctx, &result, &block, 1, args );
}

// Bar of buttons with a picture and the text on the right, drawn by the system or in the colors set
// gui_Toolbar() --> oToolbar
X_FUNC( GUI_TOOLBAR )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 1, "GUI_TOOLBAR" );
      x_clsAddParent( s_class, x_clsRequire( "GUI_CONTROL", GUI_CONTROL ) );
      x_clsAddData( s_class, "ACTIONS", 1 );
      x_clsSetScope( s_class, "ACTIONS", X_OO_CLSTP_HIDDEN );

      x_clsAdd( s_class, "NEW", X_FUNCNAME( GUI_TOOLBAR_NEW ) );
      x_clsAdd( s_class, "ADD", X_FUNCNAME( GUI_TOOLBAR_ADD ) );
      x_clsAdd( s_class, "ADDMENU", X_FUNCNAME( GUI_TOOLBAR_ADDMENU ) );
      x_clsAdd( s_class, "ADDSEPARATOR", X_FUNCNAME( GUI_TOOLBAR_ADDSEPARATOR ) );
      x_clsAdd( s_class, "CLEAR", X_FUNCNAME( GUI_TOOLBAR_CLEAR ) );
      x_clsAdd( s_class, "ENABLE", X_FUNCNAME( GUI_TOOLBAR_ENABLE ) );

      x_clsAddAccess( s_class, "FOREGROUND", X_FUNCNAME( GUI_TOOLBAR_GETFOREGROUND ) );
      x_clsAddAssign( s_class, "FOREGROUND", X_FUNCNAME( GUI_TOOLBAR_SETFOREGROUND ) );
      x_clsAddAccess( s_class, "BACKGROUND", X_FUNCNAME( GUI_TOOLBAR_GETBACKGROUND ) );
      x_clsAddAssign( s_class, "BACKGROUND", X_FUNCNAME( GUI_TOOLBAR_SETBACKGROUND ) );
      x_clsAddAccess( s_class, "HOVERFOREGROUND", X_FUNCNAME( GUI_TOOLBAR_GETHOVERFOREGROUND ) );
      x_clsAddAssign( s_class, "HOVERFOREGROUND", X_FUNCNAME( GUI_TOOLBAR_SETHOVERFOREGROUND ) );
      x_clsAddAccess( s_class, "HOVERBACKGROUND", X_FUNCNAME( GUI_TOOLBAR_GETHOVERBACKGROUND ) );
      x_clsAddAssign( s_class, "HOVERBACKGROUND", X_FUNCNAME( GUI_TOOLBAR_SETHOVERBACKGROUND ) );
      x_clsAddAccess( s_class, "PRESSFOREGROUND", X_FUNCNAME( GUI_TOOLBAR_GETPRESSFOREGROUND ) );
      x_clsAddAssign( s_class, "PRESSFOREGROUND", X_FUNCNAME( GUI_TOOLBAR_SETPRESSFOREGROUND ) );
      x_clsAddAccess( s_class, "PRESSBACKGROUND", X_FUNCNAME( GUI_TOOLBAR_GETPRESSBACKGROUND ) );
      x_clsAddAssign( s_class, "PRESSBACKGROUND", X_FUNCNAME( GUI_TOOLBAR_SETPRESSBACKGROUND ) );
      x_clsAdd( s_class, "__GUICOMMAND", X_FUNCNAME( GUI_TOOLBAR_COMMAND ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   gui_DatePicker
   --------------------------------------------------------------------------------------------- */

// Create a date field with a drop-down calendar; default size 120 x 23
// gui_DatePicker():New( oWindow, nX, nY, [nWidth], [nHeight] ) --> self
X_FUNC_STATIC( GUI_DATEPICKER_NEW )
{
   x_guiControlNew( ctx, X_GUI_DATEPICKER, 120, 23, 0 );
}

// Return the selected date
// oDate:value --> dDate
X_FUNC_STATIC( GUI_DATEPICKER_GETVALUE )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_DATEPICKER:VALUE" );

   if( window != NULL )
      x_retdl( ctx, x_guiDateValue( window ) );
}

// Select a date; an empty date is not allowed
// oDate:value := dDate
X_FUNC_STATIC( GUI_DATEPICKER_SETVALUE )
{
   const X_ITEM *value = x_paritem( ctx, 1 );
   HWND window = x_guiSelfHandle( ctx, "GUI_DATEPICKER:VALUE" );

   if( window == NULL )
      return;

   if( value == NULL || !X_IS_DATE( value ) || !x_guiDateSetValue( window, x_pardl( ctx, 1 ) ) )
   {
      x_guiRaise( ctx, "GUI_DATEPICKER:VALUE" );
      return;
   }

   x_itemReturn( ctx, value );
}

// Run OnChange when the user changes the date (internal)
// oDate:__GuiNotify( nCode ) --> NIL
X_FUNC_STATIC( GUI_DATEPICKER_NOTIFY )
{
   if( x_parni( ctx, 1 ) == ( int )DTN_DATETIMECHANGE )
      x_guiEvent( ctx, 1 );
}

// Date field; :value is a DATE, OnChange runs when the user changes it and receives the control
// gui_DatePicker() --> oDate
X_FUNC( GUI_DATEPICKER )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 1, "GUI_DATEPICKER" );
      x_clsAddParent( s_class, x_clsRequire( "GUI_CONTROL", GUI_CONTROL ) );
      x_clsAddData( s_class, "ONCHANGE", 1 );
      x_clsSetDataType( s_class, "ONCHANGE", X_OO_VALUE_CODEBLOCK );

      x_clsAdd( s_class, "NEW", X_FUNCNAME( GUI_DATEPICKER_NEW ) );

      x_clsAddAccess( s_class, "VALUE", X_FUNCNAME( GUI_DATEPICKER_GETVALUE ) );
      x_clsAddAssign( s_class, "VALUE", X_FUNCNAME( GUI_DATEPICKER_SETVALUE ) );
      x_clsAdd( s_class, "__GUINOTIFY", X_FUNCNAME( GUI_DATEPICKER_NOTIFY ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   gui_Slider
   --------------------------------------------------------------------------------------------- */

// Create a horizontal slider with the range 0 to 100; default size 200 x 30
// gui_Slider():New( oWindow, nX, nY, [nWidth], [nHeight] ) --> self
X_FUNC_STATIC( GUI_SLIDER_NEW )
{
   x_guiControlNew( ctx, X_GUI_SLIDER, 200, 30, 0 );
}

// Return the position of the slider
// oSlider:value --> nValue
X_FUNC_STATIC( GUI_SLIDER_GETVALUE )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_SLIDER:VALUE" );

   if( window != NULL )
      x_retni( ctx, x_guiSliderGet( window, X_GUI_RANGE_VALUE ) );
}

// Move the slider to a position between the minimum and the maximum
// oSlider:value := nValue
X_FUNC_STATIC( GUI_SLIDER_SETVALUE )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_SLIDER:VALUE" );
   int minimum, maximum;

   if( window == NULL )
      return;

   minimum = x_guiSliderGet( window, X_GUI_RANGE_MINIMUM );
   maximum = x_guiSliderGet( window, X_GUI_RANGE_MAXIMUM );
   if( !X_ISNUM( ctx, 1 ) || x_parni( ctx, 1 ) < minimum || x_parni( ctx, 1 ) > maximum )
   {
      x_guiRaise( ctx, "GUI_SLIDER:VALUE" );
      return;
   }

   x_guiSliderSet( window, X_GUI_RANGE_VALUE, x_parni( ctx, 1 ) );
   x_retni( ctx, x_parni( ctx, 1 ) );
}

// Return the lowest position
// oSlider:minimum --> nMinimum
X_FUNC_STATIC( GUI_SLIDER_GETMINIMUM )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_SLIDER:MINIMUM" );

   if( window != NULL )
      x_retni( ctx, x_guiSliderGet( window, X_GUI_RANGE_MINIMUM ) );
}

// Set the lowest position; it must not be above the maximum
// oSlider:minimum := nMinimum
X_FUNC_STATIC( GUI_SLIDER_SETMINIMUM )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_SLIDER:MINIMUM" );

   if( window == NULL )
      return;

   if( !X_ISNUM( ctx, 1 ) || x_parni( ctx, 1 ) > x_guiSliderGet( window, X_GUI_RANGE_MAXIMUM ) )
   {
      x_guiRaise( ctx, "GUI_SLIDER:MINIMUM" );
      return;
   }

   x_guiSliderSet( window, X_GUI_RANGE_MINIMUM, x_parni( ctx, 1 ) );
   x_retni( ctx, x_parni( ctx, 1 ) );
}

// Return the highest position
// oSlider:maximum --> nMaximum
X_FUNC_STATIC( GUI_SLIDER_GETMAXIMUM )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_SLIDER:MAXIMUM" );

   if( window != NULL )
      x_retni( ctx, x_guiSliderGet( window, X_GUI_RANGE_MAXIMUM ) );
}

// Set the highest position; it must not be below the minimum
// oSlider:maximum := nMaximum
X_FUNC_STATIC( GUI_SLIDER_SETMAXIMUM )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_SLIDER:MAXIMUM" );

   if( window == NULL )
      return;

   if( !X_ISNUM( ctx, 1 ) || x_parni( ctx, 1 ) < x_guiSliderGet( window, X_GUI_RANGE_MINIMUM ) )
   {
      x_guiRaise( ctx, "GUI_SLIDER:MAXIMUM" );
      return;
   }

   x_guiSliderSet( window, X_GUI_RANGE_MAXIMUM, x_parni( ctx, 1 ) );
   x_retni( ctx, x_parni( ctx, 1 ) );
}

// Run OnChange while the user moves the slider (internal)
// oSlider:__GuiCommand( nCode ) --> NIL
X_FUNC_STATIC( GUI_SLIDER_COMMAND )
{
   if( x_parni( ctx, 1 ) != TB_ENDTRACK )
      x_guiEvent( ctx, 1 );
}

// Slider; OnChange runs while the user moves it and receives the control
// gui_Slider() --> oSlider
X_FUNC( GUI_SLIDER )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 1, "GUI_SLIDER" );
      x_clsAddParent( s_class, x_clsRequire( "GUI_CONTROL", GUI_CONTROL ) );
      x_clsAddData( s_class, "ONCHANGE", 1 );
      x_clsSetDataType( s_class, "ONCHANGE", X_OO_VALUE_CODEBLOCK );

      x_clsAdd( s_class, "NEW", X_FUNCNAME( GUI_SLIDER_NEW ) );

      x_clsAddAccess( s_class, "VALUE", X_FUNCNAME( GUI_SLIDER_GETVALUE ) );
      x_clsAddAssign( s_class, "VALUE", X_FUNCNAME( GUI_SLIDER_SETVALUE ) );
      x_clsAddAccess( s_class, "MINIMUM", X_FUNCNAME( GUI_SLIDER_GETMINIMUM ) );
      x_clsAddAssign( s_class, "MINIMUM", X_FUNCNAME( GUI_SLIDER_SETMINIMUM ) );
      x_clsAddAccess( s_class, "MAXIMUM", X_FUNCNAME( GUI_SLIDER_GETMAXIMUM ) );
      x_clsAddAssign( s_class, "MAXIMUM", X_FUNCNAME( GUI_SLIDER_SETMAXIMUM ) );
      x_clsAdd( s_class, "__GUICOMMAND", X_FUNCNAME( GUI_SLIDER_COMMAND ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   gui_ListView
   --------------------------------------------------------------------------------------------- */

// Create a table with columns and rows of text; default size 300 x 200
// gui_ListView():New( oWindow, nX, nY, [nWidth], [nHeight] ) --> self
X_FUNC_STATIC( GUI_LISTVIEW_NEW )
{
   x_guiControlNew( ctx, X_GUI_LISTVIEW, 300, 200, 0 );
}

// Add a column; nAlign is 0 left, 1 right, 2 center (the first column is always left)
// oList:AddColumn( cTitle, [nWidth], [nAlign] ) --> nColumn
X_FUNC_STATIC( GUI_LISTVIEW_ADDCOLUMN )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_LISTVIEW:ADDCOLUMN" );
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
X_FUNC_STATIC( GUI_LISTVIEW_ADDROW )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_LISTVIEW:ADDROW" );
   int row;

   if( window != NULL && ( row = x_guiListViewAddRow( ctx, window, x_paritem( ctx, 1 ) ) ) != 0 )
      x_retni( ctx, row );
}

// Return the text of a cell
// oList:Cell( nRow, nColumn ) --> cText
X_FUNC_STATIC( GUI_LISTVIEW_CELL )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_LISTVIEW:CELL" );

   if( window != NULL )
      x_guiListViewCell( ctx, window, x_parni( ctx, 1 ), x_parni( ctx, 2 ) );
}

// Change the text of a cell
// oList:SetCell( nRow, nColumn, cText ) --> self
X_FUNC_STATIC( GUI_LISTVIEW_SETCELL )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_LISTVIEW:SETCELL" );

   if( window != NULL )
   {
      x_guiListViewSetCell( ctx, window, x_parni( ctx, 1 ), x_parni( ctx, 2 ), x_paritem( ctx, 3 ) );
      x_guiReturnSelf( ctx );
   }
}

// Delete a row
// oList:Delete( nRow ) --> self
X_FUNC_STATIC( GUI_LISTVIEW_DELETE )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_LISTVIEW:DELETE" );

   if( window != NULL )
   {
      x_guiListViewDelete( ctx, window, x_parni( ctx, 1 ) );
      x_guiReturnSelf( ctx );
   }
}

// Delete all rows; the columns stay
// oList:Clear() --> self
X_FUNC_STATIC( GUI_LISTVIEW_CLEAR )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_LISTVIEW:CLEAR" );

   if( window != NULL )
   {
      x_guiListViewClear( window );
      x_guiReturnSelf( ctx );
   }
}

// Return the number of rows
// oList:count --> nCount
X_FUNC_STATIC( GUI_LISTVIEW_COUNT )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_LISTVIEW:COUNT" );

   if( window != NULL )
      x_retni( ctx, x_guiListViewCount( window ) );
}

// Return the number of columns
// oList:columnCount --> nCount
X_FUNC_STATIC( GUI_LISTVIEW_COLUMNCOUNT )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_LISTVIEW:COLUMNCOUNT" );

   if( window != NULL )
      x_retni( ctx, x_guiListViewColumnCount( window ) );
}

// Return the selected row, 0 when nothing is selected
// oList:selected --> nRow
X_FUNC_STATIC( GUI_LISTVIEW_GETSELECTED )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_LISTVIEW:SELECTED" );

   if( window != NULL )
      x_retni( ctx, x_guiListViewSelected( window ) );
}

// Select a row and scroll it into view; 0 clears the selection
// oList:selected := nRow
X_FUNC_STATIC( GUI_LISTVIEW_SETSELECTED )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_LISTVIEW:SELECTED" );

   if( window == NULL )
      return;

   if( !X_ISNUM( ctx, 1 ) )
   {
      x_guiRaise( ctx, "GUI_LISTVIEW:SELECTED" );
      return;
   }

   x_guiListViewSetSelected( ctx, window, x_parni( ctx, 1 ) );
   x_retni( ctx, x_parni( ctx, 1 ) );
}

// Run OnSelect or OnDoubleClick for the user action (internal)
// oList:__GuiNotify( nCode, nRow ) --> NIL
X_FUNC_STATIC( GUI_LISTVIEW_NOTIFY )
{
   if( x_parni( ctx, 1 ) == ( int )LVN_ITEMCHANGED && x_parni( ctx, 2 ) > 0 )
      x_guiEvent( ctx, 1 );
   else if( x_parni( ctx, 1 ) == ( int )NM_DBLCLK )
      x_guiEvent( ctx, 2 );
}

// Table of text rows; OnSelect and OnDoubleClick receive the control
// gui_ListView() --> oList
X_FUNC( GUI_LISTVIEW )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 2, "GUI_LISTVIEW" );
      x_clsAddParent( s_class, x_clsRequire( "GUI_CONTROL", GUI_CONTROL ) );
      x_clsAddData( s_class, "ONSELECT", 1 );
      x_clsSetDataType( s_class, "ONSELECT", X_OO_VALUE_CODEBLOCK );
      x_clsAddData( s_class, "ONDOUBLECLICK", 2 );
      x_clsSetDataType( s_class, "ONDOUBLECLICK", X_OO_VALUE_CODEBLOCK );

      x_clsAdd( s_class, "NEW", X_FUNCNAME( GUI_LISTVIEW_NEW ) );
      x_clsAdd( s_class, "ADDCOLUMN", X_FUNCNAME( GUI_LISTVIEW_ADDCOLUMN ) );
      x_clsAdd( s_class, "ADDROW", X_FUNCNAME( GUI_LISTVIEW_ADDROW ) );
      x_clsAdd( s_class, "CELL", X_FUNCNAME( GUI_LISTVIEW_CELL ) );
      x_clsAdd( s_class, "SETCELL", X_FUNCNAME( GUI_LISTVIEW_SETCELL ) );
      x_clsAdd( s_class, "DELETE", X_FUNCNAME( GUI_LISTVIEW_DELETE ) );
      x_clsAdd( s_class, "CLEAR", X_FUNCNAME( GUI_LISTVIEW_CLEAR ) );

      x_clsAddAccess( s_class, "COUNT", X_FUNCNAME( GUI_LISTVIEW_COUNT ) );
      x_clsAddAccess( s_class, "COLUMNCOUNT", X_FUNCNAME( GUI_LISTVIEW_COLUMNCOUNT ) );
      x_clsAddAccess( s_class, "SELECTED", X_FUNCNAME( GUI_LISTVIEW_GETSELECTED ) );
      x_clsAddAssign( s_class, "SELECTED", X_FUNCNAME( GUI_LISTVIEW_SETSELECTED ) );
      x_clsAddAccess( s_class, "VALUE", X_FUNCNAME( GUI_LISTVIEW_GETSELECTED ) );
      x_clsAddAssign( s_class, "VALUE", X_FUNCNAME( GUI_LISTVIEW_SETSELECTED ) );
      x_clsAdd( s_class, "__GUINOTIFY", X_FUNCNAME( GUI_LISTVIEW_NOTIFY ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   gui_TreeView
   --------------------------------------------------------------------------------------------- */

// Create a tree; default size 200 x 200
// gui_TreeView():New( oWindow, nX, nY, [nWidth], [nHeight] ) --> self
X_FUNC_STATIC( GUI_TREEVIEW_NEW )
{
   x_guiControlNew( ctx, X_GUI_TREEVIEW, 200, 200, 0 );
}

// Add an item at the end of the root or of a parent item; the handle stays valid until Clear()
// oTree:Add( cText, [hParent] ) --> hItem
X_FUNC_STATIC( GUI_TREEVIEW_ADD )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_TREEVIEW:ADD" );

   if( window != NULL )
      x_guiTreeAdd( ctx, window, x_paritem( ctx, 1 ), x_paritem( ctx, 2 ) );
}

// Return the text of an item
// oTree:ItemText( hItem ) --> cText
X_FUNC_STATIC( GUI_TREEVIEW_ITEMTEXT )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_TREEVIEW:ITEMTEXT" );

   if( window != NULL )
      x_guiTreeText( ctx, window, x_paritem( ctx, 1 ) );
}

// Change the text of an item
// oTree:SetItemText( hItem, cText ) --> self
X_FUNC_STATIC( GUI_TREEVIEW_SETITEMTEXT )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_TREEVIEW:SETITEMTEXT" );

   if( window != NULL )
   {
      x_guiTreeSetText( ctx, window, x_paritem( ctx, 1 ), x_paritem( ctx, 2 ) );
      x_guiReturnSelf( ctx );
   }
}

// Show the children of an item
// oTree:Expand( hItem ) --> self
X_FUNC_STATIC( GUI_TREEVIEW_EXPAND )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_TREEVIEW:EXPAND" );

   if( window != NULL )
   {
      x_guiTreeExpand( ctx, window, x_paritem( ctx, 1 ), 1 );
      x_guiReturnSelf( ctx );
   }
}

// Hide the children of an item
// oTree:Collapse( hItem ) --> self
X_FUNC_STATIC( GUI_TREEVIEW_COLLAPSE )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_TREEVIEW:COLLAPSE" );

   if( window != NULL )
   {
      x_guiTreeExpand( ctx, window, x_paritem( ctx, 1 ), 0 );
      x_guiReturnSelf( ctx );
   }
}

// Delete all items
// oTree:Clear() --> self
X_FUNC_STATIC( GUI_TREEVIEW_CLEAR )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_TREEVIEW:CLEAR" );

   if( window != NULL )
   {
      x_guiTreeClear( window );
      x_guiReturnSelf( ctx );
   }
}

// Return the number of items
// oTree:count --> nCount
X_FUNC_STATIC( GUI_TREEVIEW_COUNT )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_TREEVIEW:COUNT" );

   if( window != NULL )
      x_retni( ctx, x_guiTreeCount( window ) );
}

// Return the selected item, NIL when nothing is selected
// oTree:selected --> hItem
X_FUNC_STATIC( GUI_TREEVIEW_GETSELECTED )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_TREEVIEW:SELECTED" );

   if( window != NULL )
      x_guiTreeSelected( ctx, window );
}

// Select an item; NIL clears the selection
// oTree:selected := hItem
X_FUNC_STATIC( GUI_TREEVIEW_SETSELECTED )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_TREEVIEW:SELECTED" );

   if( window != NULL )
   {
      x_guiTreeSelect( ctx, window, x_paritem( ctx, 1 ) );
      x_itemReturn( ctx, x_paritem( ctx, 1 ) );
   }
}

// Run OnSelect or OnDoubleClick for the user action (internal)
// oTree:__GuiNotify( nCode, lUser ) --> NIL
X_FUNC_STATIC( GUI_TREEVIEW_NOTIFY )
{
   if( x_parni( ctx, 1 ) == ( int )TVN_SELCHANGEDW && x_parni( ctx, 2 ) != 0 )
      x_guiEvent( ctx, 1 );
   else if( x_parni( ctx, 1 ) == ( int )NM_DBLCLK )
      x_guiEvent( ctx, 2 );
}

// Tree of text items; OnSelect and OnDoubleClick receive the control
// gui_TreeView() --> oTree
X_FUNC( GUI_TREEVIEW )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 2, "GUI_TREEVIEW" );
      x_clsAddParent( s_class, x_clsRequire( "GUI_CONTROL", GUI_CONTROL ) );
      x_clsAddData( s_class, "ONSELECT", 1 );
      x_clsSetDataType( s_class, "ONSELECT", X_OO_VALUE_CODEBLOCK );
      x_clsAddData( s_class, "ONDOUBLECLICK", 2 );
      x_clsSetDataType( s_class, "ONDOUBLECLICK", X_OO_VALUE_CODEBLOCK );

      x_clsAdd( s_class, "NEW", X_FUNCNAME( GUI_TREEVIEW_NEW ) );
      x_clsAdd( s_class, "ADD", X_FUNCNAME( GUI_TREEVIEW_ADD ) );
      x_clsAdd( s_class, "ITEMTEXT", X_FUNCNAME( GUI_TREEVIEW_ITEMTEXT ) );
      x_clsAdd( s_class, "SETITEMTEXT", X_FUNCNAME( GUI_TREEVIEW_SETITEMTEXT ) );
      x_clsAdd( s_class, "EXPAND", X_FUNCNAME( GUI_TREEVIEW_EXPAND ) );
      x_clsAdd( s_class, "COLLAPSE", X_FUNCNAME( GUI_TREEVIEW_COLLAPSE ) );
      x_clsAdd( s_class, "CLEAR", X_FUNCNAME( GUI_TREEVIEW_CLEAR ) );

      x_clsAddAccess( s_class, "COUNT", X_FUNCNAME( GUI_TREEVIEW_COUNT ) );
      x_clsAddAccess( s_class, "SELECTED", X_FUNCNAME( GUI_TREEVIEW_GETSELECTED ) );
      x_clsAddAssign( s_class, "SELECTED", X_FUNCNAME( GUI_TREEVIEW_SETSELECTED ) );
      x_clsAddAccess( s_class, "VALUE", X_FUNCNAME( GUI_TREEVIEW_GETSELECTED ) );
      x_clsAddAssign( s_class, "VALUE", X_FUNCNAME( GUI_TREEVIEW_SETSELECTED ) );
      x_clsAdd( s_class, "__GUINOTIFY", X_FUNCNAME( GUI_TREEVIEW_NOTIFY ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   gui_TabPage
   --------------------------------------------------------------------------------------------- */

// Pages are created by gui_TabControl:AddPage(), not by New()
// gui_TabPage():New() --> NIL
X_FUNC_STATIC( GUI_TABPAGE_NEW )
{
   x_guiRaise( ctx, "GUI_TABPAGE:NEW" );
}

// Page of a tab control; controls are created on it like on a window
// gui_TabPage() --> oPage
X_FUNC( GUI_TABPAGE )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 0, "GUI_TABPAGE" );
      x_clsAddParent( s_class, x_clsRequire( "GUI_WINDOW", GUI_WINDOW ) );

      x_clsAdd( s_class, "NEW", X_FUNCNAME( GUI_TABPAGE_NEW ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   gui_TabControl
   --------------------------------------------------------------------------------------------- */

// Create a tab control without pages; default size 300 x 200
// gui_TabControl():New( oWindow, nX, nY, [nWidth], [nHeight] ) --> self
X_FUNC_STATIC( GUI_TABCONTROL_NEW )
{
   x_guiControlNew( ctx, X_GUI_TABCONTROL, 300, 200, 0 );
}

// Add a tab with a page; the page follows the size of the tab control
// oTab:AddPage( cTitle ) --> oPage
X_FUNC_STATIC( GUI_TABCONTROL_ADDPAGE )
{
   X_ITEM page __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   HWND window = x_guiSelfHandle( ctx, "GUI_TABCONTROL:ADDPAGE" );

   if( window != NULL && x_objCreate( ctx, &page, x_clsRequire( "GUI_TABPAGE", GUI_TABPAGE ) ) )
      x_guiTabAddPage( ctx, window, x_paritem( ctx, 1 ), &page );
}

// Return the page at the position
// oTab:Page( nPosition ) --> oPage
X_FUNC_STATIC( GUI_TABCONTROL_PAGE )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_TABCONTROL:PAGE" );

   if( window != NULL )
      x_guiTabPage( ctx, window, x_parni( ctx, 1 ) );
}

// Return the number of pages
// oTab:count --> nCount
X_FUNC_STATIC( GUI_TABCONTROL_COUNT )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_TABCONTROL:COUNT" );

   if( window != NULL )
      x_retni( ctx, x_guiTabCount( window ) );
}

// Return the position of the selected page, 0 when there are no pages
// oTab:selected --> nPosition
X_FUNC_STATIC( GUI_TABCONTROL_GETSELECTED )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_TABCONTROL:SELECTED" );

   if( window != NULL )
      x_retni( ctx, x_guiTabSelected( window ) );
}

// Select the page at the position and show it
// oTab:selected := nPosition
X_FUNC_STATIC( GUI_TABCONTROL_SETSELECTED )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_TABCONTROL:SELECTED" );

   if( window == NULL )
      return;

   if( !X_ISNUM( ctx, 1 ) )
   {
      x_guiRaise( ctx, "GUI_TABCONTROL:SELECTED" );
      return;
   }

   x_guiTabSelect( ctx, window, x_parni( ctx, 1 ) );
   x_retni( ctx, x_parni( ctx, 1 ) );
}

// Show the selected page and run OnSelect when the user selects a tab (internal)
// oTab:__GuiNotify( nCode ) --> NIL
X_FUNC_STATIC( GUI_TABCONTROL_NOTIFY )
{
   HWND window;

   if( x_parni( ctx, 1 ) != ( int )TCN_SELCHANGE || ( window = x_guiSelfHandle( ctx, "GUI_TABCONTROL" ) ) == NULL )
      return;

   x_guiTabRefresh( window );
   x_guiEvent( ctx, 1 );
}

// Tab control with pages; :value is the position of the selected page, OnSelect receives the control
// gui_TabControl() --> oTab
X_FUNC( GUI_TABCONTROL )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 1, "GUI_TABCONTROL" );
      x_clsAddParent( s_class, x_clsRequire( "GUI_CONTROL", GUI_CONTROL ) );
      x_clsAddData( s_class, "ONSELECT", 1 );
      x_clsSetDataType( s_class, "ONSELECT", X_OO_VALUE_CODEBLOCK );

      x_clsAdd( s_class, "NEW", X_FUNCNAME( GUI_TABCONTROL_NEW ) );
      x_clsAdd( s_class, "ADDPAGE", X_FUNCNAME( GUI_TABCONTROL_ADDPAGE ) );
      x_clsAdd( s_class, "PAGE", X_FUNCNAME( GUI_TABCONTROL_PAGE ) );

      x_clsAddAccess( s_class, "COUNT", X_FUNCNAME( GUI_TABCONTROL_COUNT ) );
      x_clsAddAccess( s_class, "SELECTED", X_FUNCNAME( GUI_TABCONTROL_GETSELECTED ) );
      x_clsAddAssign( s_class, "SELECTED", X_FUNCNAME( GUI_TABCONTROL_SETSELECTED ) );
      x_clsAddAccess( s_class, "VALUE", X_FUNCNAME( GUI_TABCONTROL_GETSELECTED ) );
      x_clsAddAssign( s_class, "VALUE", X_FUNCNAME( GUI_TABCONTROL_SETSELECTED ) );
      x_clsAdd( s_class, "__GUINOTIFY", X_FUNCNAME( GUI_TABCONTROL_NOTIFY ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   gui_Timer
   --------------------------------------------------------------------------------------------- */

// Create a stopped timer of the window; the action runs every nInterval milliseconds
// gui_Timer():New( oWindow, nInterval, [bAction] ) --> self
X_FUNC_STATIC( GUI_TIMER_NEW )
{
   const X_ITEM *parent = x_paritem( ctx, 1 );
   const X_ITEM *action = x_paritem( ctx, 3 );
   X_ITEM *onTimer = x_objDataSlot( ctx, 1 );
   X_ITEM *owner = x_objDataSlot( ctx, 2 );
   X_ITEM *interval = x_objDataSlot( ctx, 3 );

   if( onTimer == NULL || owner == NULL || interval == NULL )
      return;

   if( x_guiObjectWindow( ctx, parent, "GUI_TIMER:NEW" ) == NULL )
      return;

   if( x_parni( ctx, 2 ) <= 0 || ( action != NULL && !X_IS_NIL( action ) && !X_IS_BLOCK( action ) ) )
   {
      x_guiRaise( ctx, "GUI_TIMER:NEW" );
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
X_FUNC_STATIC( GUI_TIMER_START )
{
   const X_ITEM *interval = x_objDataSlot( ctx, 3 );
   X_ITEM *id = x_objDataSlot( ctx, 4 );
   HWND window;
   UINT_PTR timer;

   if( interval == NULL || id == NULL )
      return;

   window = x_guiObjectWindow( ctx, x_objDataSlot( ctx, 2 ), "GUI_TIMER:START" );
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
X_FUNC_STATIC( GUI_TIMER_STOP )
{
   const X_ITEM *id = x_objDataSlot( ctx, 4 );

   if( id != NULL )
      x_guiTimerStop( ( UINT_PTR )x_itemGetNInt( id ) );

   x_guiReturnSelf( ctx );
}

// Return whether the timer is running
// oTimer:active --> lActive
X_FUNC_STATIC( GUI_TIMER_ACTIVE )
{
   const X_ITEM *id = x_objDataSlot( ctx, 4 );

   x_retl( ctx, id != NULL && x_guiTimerActive( ( UINT_PTR )x_itemGetNInt( id ) ) );
}

// Return the interval in milliseconds
// oTimer:interval --> nInterval
X_FUNC_STATIC( GUI_TIMER_GETINTERVAL )
{
   const X_ITEM *interval = x_objDataSlot( ctx, 3 );

   if( interval != NULL )
      x_retni( ctx, x_itemGetNI( interval ) );
}

// Set the interval in milliseconds; a running timer continues with the new interval
// oTimer:interval := nInterval
X_FUNC_STATIC( GUI_TIMER_SETINTERVAL )
{
   X_ITEM *interval = x_objDataSlot( ctx, 3 );
   const X_ITEM *id = x_objDataSlot( ctx, 4 );
   HWND window;

   if( interval == NULL || id == NULL )
      return;

   if( !X_ISNUM( ctx, 1 ) || x_parni( ctx, 1 ) <= 0 )
   {
      x_guiRaise( ctx, "GUI_TIMER:INTERVAL" );
      return;
   }

   x_itemPutNI( interval, x_parni( ctx, 1 ) );

   /* A running timer starts again with the new interval */
   if( x_guiTimerActive( ( UINT_PTR )x_itemGetNInt( id ) ) )
   {
      window = x_guiObjectWindow( ctx, x_objDataSlot( ctx, 2 ), "GUI_TIMER:INTERVAL" );
      if( window != NULL )
         x_guiTimerStart( ctx, window, ( UINT_PTR )x_itemGetNInt( id ), x_parni( ctx, 1 ) );
   }

   x_retni( ctx, x_parni( ctx, 1 ) );
}

// Run OnTimer for a tick of the timer (internal)
// oTimer:__GuiTimer() --> NIL
X_FUNC_STATIC( GUI_TIMER_EVENT )
{
   x_guiEvent( ctx, 1 );
}

// Timer of a window; OnTimer runs for every tick and receives the timer
// gui_Timer() --> oTimer
X_FUNC( GUI_TIMER )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 4, "GUI_TIMER" );
      x_clsAddData( s_class, "ONTIMER", 1 );
      x_clsSetDataType( s_class, "ONTIMER", X_OO_VALUE_CODEBLOCK );
      x_clsAddData( s_class, "PARENT", 2 );
      x_clsSetScope( s_class, "PARENT", X_OO_CLSTP_EXPORTED | X_OO_CLSTP_READONLY );
      x_clsAddData( s_class, "INTERVALMS", 3 );
      x_clsSetScope( s_class, "INTERVALMS", X_OO_CLSTP_HIDDEN );
      x_clsAddData( s_class, "TIMERID", 4 );
      x_clsSetScope( s_class, "TIMERID", X_OO_CLSTP_HIDDEN );

      x_clsAdd( s_class, "NEW", X_FUNCNAME( GUI_TIMER_NEW ) );
      x_clsAdd( s_class, "START", X_FUNCNAME( GUI_TIMER_START ) );
      x_clsAdd( s_class, "STOP", X_FUNCNAME( GUI_TIMER_STOP ) );

      x_clsAddAccess( s_class, "ACTIVE", X_FUNCNAME( GUI_TIMER_ACTIVE ) );
      x_clsAddAccess( s_class, "INTERVAL", X_FUNCNAME( GUI_TIMER_GETINTERVAL ) );
      x_clsAddAssign( s_class, "INTERVAL", X_FUNCNAME( GUI_TIMER_SETINTERVAL ) );
      x_clsAdd( s_class, "__GUITIMER", X_FUNCNAME( GUI_TIMER_EVENT ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   gui_Application
   --------------------------------------------------------------------------------------------- */

// Show the window and process messages until it is closed; an error in an event is raised again
// gui_Application():Run( oWindow ) --> .T.
X_FUNC_STATIC( GUI_APPLICATION_RUN )
{
   HWND window = x_guiObjectWindow( ctx, x_paritem( ctx, 1 ), "GUI_APPLICATION:RUN" );

   if( window != NULL )
      x_guiRun( ctx, window );
}

// Message loop of the program
// gui_Application() --> oApplication
X_FUNC( GUI_APPLICATION )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 0, "GUI_APPLICATION" );

      x_clsAdd( s_class, "RUN", X_FUNCNAME( GUI_APPLICATION_RUN ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   Dialogs
   --------------------------------------------------------------------------------------------- */

// Show an information box with an OK button; without a title the active window title is used
// gui_MsgInfo( cText, [cTitle] ) --> NIL
X_FUNC( GUI_MSGINFO )
{
   x_guiMessage( ctx, X_GUI_MESSAGE_INFO );
}

// Show a warning box with an OK button
// gui_MsgWarning( cText, [cTitle] ) --> NIL
X_FUNC( GUI_MSGWARNING )
{
   x_guiMessage( ctx, X_GUI_MESSAGE_WARNING );
}

// Show an error box with an OK button
// gui_MsgError( cText, [cTitle] ) --> NIL
X_FUNC( GUI_MSGERROR )
{
   x_guiMessage( ctx, X_GUI_MESSAGE_ERROR );
}

// Ask a question with Yes and No buttons
// gui_MsgYesNo( cText, [cTitle] ) --> lYes
X_FUNC( GUI_MSGYESNO )
{
   x_guiMessage( ctx, X_GUI_MESSAGE_YESNO );
}

// Select an existing file; aFilters is { { "Text files", "*.txt" }, ... }, "" when cancelled
// gui_FileOpenDialog( [cTitle], [aFilters], [cFile] ) --> cPath
X_FUNC( GUI_FILEOPENDIALOG )
{
   x_guiFileDialog( ctx, 0 );
}

// Select a file to save; adds the extension of the first filter, "" when cancelled
// gui_FileSaveDialog( [cTitle], [aFilters], [cFile] ) --> cPath
X_FUNC( GUI_FILESAVEDIALOG )
{
   x_guiFileDialog( ctx, 1 );
}

// Open the file in the program the system has for its type, e.g. a PDF in the PDF viewer
// gui_OpenFile( cFile ) --> lOpened
X_FUNC( GUI_OPENFILE )
{
   x_guiOpenFile( ctx );
}
