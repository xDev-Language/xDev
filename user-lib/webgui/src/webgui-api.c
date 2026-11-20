/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library: xDev WebGUI
 * File: webgui-api.c
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include <string.h>

#include "internal-webgui.h"

/* ---------------------------------------------------------------------------------------------
   wg_Window
   --------------------------------------------------------------------------------------------- */

// Create a hidden window with a browser, the client area nWidth x nHeight (default 640 x 480); Run() shows it
// wg_Window():New( [cTitle], [nWidth], [nHeight] ) --> self
X_FUNC_STATIC( WG_WINDOW_NEW )
{
   int valid = X_ISNIL( ctx, 1 ) || X_ISCHAR( ctx, 1 );

   valid = valid && ( X_ISNIL( ctx, 2 ) || ( X_ISNUM( ctx, 2 ) && x_parni( ctx, 2 ) > 0 ) );
   valid = valid && ( X_ISNIL( ctx, 3 ) || ( X_ISNUM( ctx, 3 ) && x_parni( ctx, 3 ) > 0 ) );
   if( !valid )
   {
      x_webguiRaise( ctx, "WG_WINDOW:NEW" );
      return;
   }

   x_webguiWindowNew( ctx );
}

// Show the HTML file cFile in the window; a relative path counts from the folder of the program; the title of the
// page becomes the title of the window; returns .F. when there is no such file
// oWindow:Open( cFile ) --> lSuccess
X_FUNC_STATIC( WG_WINDOW_OPEN )
{
   HWND window = x_webguiSelfHandle( ctx, "WG_WINDOW:OPEN" );

   if( window == NULL )
      return;

   if( !X_ISCHAR( ctx, 1 ) )
   {
      x_webguiRaise( ctx, "WG_WINDOW:OPEN" );
      return;
   }

   x_retl( ctx, x_webguiWindowOpen( ctx, window, x_paritem( ctx, 1 ) ) );
}

// Send cJson to the page; the page gets it in window.chrome.webview.addEventListener( "message", e => e.data ) as
// an object; before the page is ready the text waits, so Send() works also before Run()
// oWindow:Send( cJson ) --> self
X_FUNC_STATIC( WG_WINDOW_SEND )
{
   HWND window = x_webguiSelfHandle( ctx, "WG_WINDOW:SEND" );

   if( window == NULL )
      return;

   if( !X_ISCHAR( ctx, 1 ) )
   {
      x_webguiRaise( ctx, "WG_WINDOW:SEND" );
      return;
   }

   x_webguiWindowSend( ctx, window, x_paritem( ctx, 1 ) );
   x_itemCopy( ctx->result, ctx->self );
}

// Run OnMessage with the window and the JSON text of a message of the page (internal)
// oWindow:__WgMessage( cJson ) --> NIL
X_FUNC_STATIC( WG_WINDOW_MESSAGE )
{
   const X_ITEM *data = x_objDataSlot( ctx, 2 );
   X_ITEM block __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ARG args[2];

   if( data == NULL || !X_IS_BLOCK( data ) )
      return;

   x_itemCopy( &block, data );
   args[0] = ( X_ARG ){ ctx->self, 0 };
   args[1] = ( X_ARG ){ ( X_ITEM * )x_paritem( ctx, 1 ), 0 };
   x_evalBlock( ctx, &result, &block, 2, args );
}

// Show the window and run it until it closes; window.close() of the page closes it too; an error when the browser
// cannot start, e.g. without WebView2Loader.dll or without the WebView2 runtime
// oWindow:Run() --> NIL
X_FUNC_STATIC( WG_WINDOW_RUN )
{
   HWND window = x_webguiSelfHandle( ctx, "WG_WINDOW:RUN" );

   if( window != NULL )
      x_webguiWindowRun( ctx, window );
}

// Close the window with its browser
// oWindow:Close() --> NIL
X_FUNC_STATIC( WG_WINDOW_CLOSE )
{
   HWND window = x_webguiSelfHandle( ctx, "WG_WINDOW:CLOSE" );

   if( window != NULL )
      x_webguiWindowClose( window );
}

// Window of the system with a browser showing an HTML page; the program keeps the logic, the page only draws:
// the page sends events with window.chrome.webview.postMessage(), OnMessage receives the window and their JSON
// text, Send() answers the page
// wg_Window() --> oWindow
X_FUNC( WG_WINDOW )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 2, "WG_WINDOW" );

      x_clsAddData( s_class, "HANDLE", 1 );
      x_clsSetScope( s_class, "HANDLE", X_OO_CLSTP_EXPORTED | X_OO_CLSTP_READONLY );
      x_clsAddData( s_class, "ONMESSAGE", 2 );
      x_clsSetDataType( s_class, "ONMESSAGE", X_OO_VALUE_CODEBLOCK );

      x_clsAdd( s_class, "NEW", X_FUNCNAME( WG_WINDOW_NEW ) );
      x_clsAdd( s_class, "OPEN", X_FUNCNAME( WG_WINDOW_OPEN ) );
      x_clsAdd( s_class, "SEND", X_FUNCNAME( WG_WINDOW_SEND ) );
      x_clsAdd( s_class, "RUN", X_FUNCNAME( WG_WINDOW_RUN ) );
      x_clsAdd( s_class, "CLOSE", X_FUNCNAME( WG_WINDOW_CLOSE ) );
      x_clsAdd( s_class, "__WGMESSAGE", X_FUNCNAME( WG_WINDOW_MESSAGE ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   wg_Control
   --------------------------------------------------------------------------------------------- */

// Create the control on the page (internal); New() of a kind of control sends it
// oControl:__WgCreate( cKind, oWindow, nX, nY, nWidth, nHeight, cText, xValue, aItems ) --> self
X_FUNC_STATIC( WG_CONTROL_CREATE )
{
   x_webguiControlCreate( ctx );
}

// Return the text of a label, of a button or of a check box, the label of an edit field, of a get or of a combo box,
// the title of a menu
// oControl:text --> cText
X_FUNC_STATIC( WG_CONTROL_GETTEXT )
{
   x_itemReturn( ctx, x_objDataSlot( ctx, X_WEBGUI_TEXT ) );
}

// Set the text of the control
// oControl:text := cText
X_FUNC_STATIC( WG_CONTROL_SETTEXT )
{
   if( !X_ISCHAR( ctx, 1 ) )
   {
      x_webguiRaise( ctx, "WG_CONTROL:TEXT" );
      return;
   }

   x_webguiControlSet( ctx, "text", X_WEBGUI_TEXT, "WG_CONTROL:TEXT" );
}

// Return the value the user gave: the text of an edit field, .T. for a checked check box, the number of the item
// of a combo box
// oControl:value --> xValue
X_FUNC_STATIC( WG_CONTROL_GETVALUE )
{
   x_itemReturn( ctx, x_objDataSlot( ctx, X_WEBGUI_VALUE ) );
}

// Set the value: text for an edit field, a logical value for a check box, the number of the item for a combo box,
// the number of the current row for a browse, a value of the same type for a get, the number of the shown page for
// a sidebar, 0 to hide its panel
// oControl:value := xValue
X_FUNC_STATIC( WG_CONTROL_SETVALUE )
{
   const X_ITEM *slot = x_objDataSlot( ctx, X_WEBGUI_KIND );
   const X_ITEM *old = x_objDataSlot( ctx, X_WEBGUI_VALUE );
   const char *kind = IIF( X_IS_STRING( slot ), x_itemGetCPtr( slot ), "" );
   int valid = strcmp( kind, "edit" ) == 0 && X_ISCHAR( ctx, 1 );
   int same = ( X_IS_STRING( old ) && X_ISCHAR( ctx, 1 ) ) || ( X_IS_NUMERIC( old ) && X_ISNUM( ctx, 1 ) );

   same = same || ( X_IS_DATE( old ) && X_ISDATE( ctx, 1 ) ) || ( X_IS_LOGICAL( old ) && X_ISLOG( ctx, 1 ) );
   valid = valid || ( strcmp( kind, "check" ) == 0 && X_ISLOG( ctx, 1 ) );
   valid = valid || ( strcmp( kind, "combo" ) == 0 && X_ISNUM( ctx, 1 ) );
   valid = valid || ( strcmp( kind, "browse" ) == 0 && X_ISNUM( ctx, 1 ) );
   valid = valid || ( strcmp( kind, "sidebar" ) == 0 && X_ISNUM( ctx, 1 ) && x_parni( ctx, 1 ) >= 0 );
   valid = valid || ( strcmp( kind, "split" ) == 0 && X_ISNUM( ctx, 1 ) && x_parni( ctx, 1 ) >= 0 );
   valid = valid || ( strcmp( kind, "get" ) == 0 && same );
   if( !valid )
   {
      x_webguiRaise( ctx, "WG_CONTROL:VALUE" );
      return;
   }

   x_webguiControlSet( ctx, "value", X_WEBGUI_VALUE, "WG_CONTROL:VALUE" );
}

// Return whether the control is active
// oControl:enabled --> lEnabled
X_FUNC_STATIC( WG_CONTROL_GETENABLED )
{
   x_itemReturn( ctx, x_objDataSlot( ctx, X_WEBGUI_ENABLED ) );
}

// Make the control active or grey
// oControl:enabled := lEnabled
X_FUNC_STATIC( WG_CONTROL_SETENABLED )
{
   if( !X_ISLOG( ctx, 1 ) )
   {
      x_webguiRaise( ctx, "WG_CONTROL:ENABLED" );
      return;
   }

   x_webguiControlSet( ctx, "enabled", X_WEBGUI_ENABLED, "WG_CONTROL:ENABLED" );
}

// Return whether the control is shown
// oControl:visible --> lVisible
X_FUNC_STATIC( WG_CONTROL_GETVISIBLE )
{
   x_itemReturn( ctx, x_objDataSlot( ctx, X_WEBGUI_VISIBLE ) );
}

// Show or hide the control
// oControl:visible := lVisible
X_FUNC_STATIC( WG_CONTROL_SETVISIBLE )
{
   if( !X_ISLOG( ctx, 1 ) )
   {
      x_webguiRaise( ctx, "WG_CONTROL:VISIBLE" );
      return;
   }

   x_webguiControlSet( ctx, "visible", X_WEBGUI_VISIBLE, "WG_CONTROL:VISIBLE" );
}

// Return the left edge of a control placed in pixels, NIL when its parent places it
// oControl:x --> nX
X_FUNC_STATIC( WG_CONTROL_GETX )
{
   x_itemReturn( ctx, x_objDataSlot( ctx, X_WEBGUI_X ) );
}

// Place the control at the left edge nX of its parent in logical pixels, out of the order of the parent; NIL puts
// it back in the order
// oControl:x := nX
X_FUNC_STATIC( WG_CONTROL_SETX )
{
   if( !X_ISNIL( ctx, 1 ) && !( X_ISNUM( ctx, 1 ) && x_parni( ctx, 1 ) >= 0 ) )
   {
      x_webguiRaise( ctx, "WG_CONTROL:X" );
      return;
   }

   x_webguiControlSet( ctx, "x", X_WEBGUI_X, "WG_CONTROL:X" );
}

// Return the top edge of a control placed in pixels, NIL when its parent places it
// oControl:y --> nY
X_FUNC_STATIC( WG_CONTROL_GETY )
{
   x_itemReturn( ctx, x_objDataSlot( ctx, X_WEBGUI_Y ) );
}

// Place the control at the top edge nY of its parent in logical pixels, out of the order of the parent; NIL puts
// it back in the order
// oControl:y := nY
X_FUNC_STATIC( WG_CONTROL_SETY )
{
   if( !X_ISNIL( ctx, 1 ) && !( X_ISNUM( ctx, 1 ) && x_parni( ctx, 1 ) >= 0 ) )
   {
      x_webguiRaise( ctx, "WG_CONTROL:Y" );
      return;
   }

   x_webguiControlSet( ctx, "y", X_WEBGUI_Y, "WG_CONTROL:Y" );
}

// Return the width given to the control, NIL for its own width
// oControl:width --> nWidth
X_FUNC_STATIC( WG_CONTROL_GETWIDTH )
{
   x_itemReturn( ctx, x_objDataSlot( ctx, X_WEBGUI_WIDTH ) );
}

// Give the control a width in logical pixels, the width of the panel for a sidebar; NIL for its own width
// oControl:width := nWidth
X_FUNC_STATIC( WG_CONTROL_SETWIDTH )
{
   if( !X_ISNIL( ctx, 1 ) && !( X_ISNUM( ctx, 1 ) && x_parni( ctx, 1 ) > 0 ) )
   {
      x_webguiRaise( ctx, "WG_CONTROL:WIDTH" );
      return;
   }

   x_webguiControlSet( ctx, "width", X_WEBGUI_WIDTH, "WG_CONTROL:WIDTH" );
}

// Return the height given to the control, NIL for its own height
// oControl:height --> nHeight
X_FUNC_STATIC( WG_CONTROL_GETHEIGHT )
{
   x_itemReturn( ctx, x_objDataSlot( ctx, X_WEBGUI_HEIGHT ) );
}

// Give the control a height in logical pixels; NIL for its own height
// oControl:height := nHeight
X_FUNC_STATIC( WG_CONTROL_SETHEIGHT )
{
   if( !X_ISNIL( ctx, 1 ) && !( X_ISNUM( ctx, 1 ) && x_parni( ctx, 1 ) > 0 ) )
   {
      x_webguiRaise( ctx, "WG_CONTROL:HEIGHT" );
      return;
   }

   x_webguiControlSet( ctx, "height", X_WEBGUI_HEIGHT, "WG_CONTROL:HEIGHT" );
}

// Return whether the control takes the room its parent leaves
// oControl:grow --> lGrow
X_FUNC_STATIC( WG_CONTROL_GETGROW )
{
   x_itemReturn( ctx, x_objDataSlot( ctx, X_WEBGUI_GROW ) );
}

// .T. lets the control take the room its parent leaves: the rest of the height in a column, of the width in a row;
// controls that grow share the room
// oControl:grow := lGrow
X_FUNC_STATIC( WG_CONTROL_SETGROW )
{
   if( !X_ISLOG( ctx, 1 ) )
   {
      x_webguiRaise( ctx, "WG_CONTROL:GROW" );
      return;
   }

   x_webguiControlSet( ctx, "grow", X_WEBGUI_GROW, "WG_CONTROL:GROW" );
}

// Give the focus to the control
// oControl:SetFocus() --> self
X_FUNC_STATIC( WG_CONTROL_SETFOCUS )
{
   x_webguiControlCommand( ctx, 0 );
}

// Remove the control from the page
// oControl:Destroy() --> self
X_FUNC_STATIC( WG_CONTROL_DESTROY )
{
   x_webguiControlCommand( ctx, 1 );
}

// Receive an event of the page: keep the new value and run OnChange or OnClick (internal)
// oControl:__WgEvent( cEvent, cValue ) --> NIL
X_FUNC_STATIC( WG_CONTROL_EVENT )
{
   x_webguiControlEvent( ctx );
}

// Add a column of a browse (internal); wg_Browse:AddColumn() sends it
// oControl:__WgColumn( cHeading, bBlock, nWidth, cPicture, nAlign ) --> nColumn
X_FUNC_STATIC( WG_CONTROL_COLUMN )
{
   x_webguiBrowseColumn( ctx );
}

// Set the rows of a browse (internal); wg_Browse:SetArray() sends it
// oControl:__WgRows( aRows ) --> self
X_FUNC_STATIC( WG_CONTROL_ROWS )
{
   x_webguiBrowseRows( ctx );
}

// Show the rows of a browse again (internal); wg_Browse:Refresh() sends it
// oControl:__WgRefresh() --> self
X_FUNC_STATIC( WG_CONTROL_REFRESH )
{
   x_webguiBrowseRefresh( ctx );
}

// Add an item to a menu (internal); wg_Menu:Add() and AddSeparator() send it
// oControl:__WgItem( cText, bAction, cShortcut ) --> self
X_FUNC_STATIC( WG_CONTROL_ITEM )
{
   x_webguiMenuItem( ctx );
}

// Add an action of a page of a sidebar or a button at the bottom of its strip (internal); wg_Sidebar:AddAction()
// and AddButton() send it
// oControl:__WgAction( nPage, cPicture, bAction, cToolTip ) --> self
X_FUNC_STATIC( WG_CONTROL_ACTION )
{
   x_webguiSidebarAction( ctx );
}

// Control drawn by the page of its wg_Window; the program keeps its text and value; OnClick and OnChange receive
// the control
// wg_Control() --> oControl
X_FUNC( WG_CONTROL )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( X_WEBGUI_CONTROL_DATA, "WG_CONTROL" );

      x_clsAddData( s_class, "PARENT", X_WEBGUI_PARENT );
      x_clsSetScope( s_class, "PARENT", X_OO_CLSTP_EXPORTED | X_OO_CLSTP_READONLY );
      x_clsAddData( s_class, "ID", X_WEBGUI_ID );
      x_clsSetScope( s_class, "ID", X_OO_CLSTP_EXPORTED | X_OO_CLSTP_READONLY );
      x_clsAddData( s_class, "KIND", X_WEBGUI_KIND );
      x_clsSetScope( s_class, "KIND", X_OO_CLSTP_HIDDEN );
      x_clsAddData( s_class, "CTEXT", X_WEBGUI_TEXT );
      x_clsSetScope( s_class, "CTEXT", X_OO_CLSTP_HIDDEN );
      x_clsAddData( s_class, "XVALUE", X_WEBGUI_VALUE );
      x_clsSetScope( s_class, "XVALUE", X_OO_CLSTP_HIDDEN );
      x_clsAddData( s_class, "LENABLED", X_WEBGUI_ENABLED );
      x_clsSetScope( s_class, "LENABLED", X_OO_CLSTP_HIDDEN );
      x_clsAddData( s_class, "LVISIBLE", X_WEBGUI_VISIBLE );
      x_clsSetScope( s_class, "LVISIBLE", X_OO_CLSTP_HIDDEN );
      x_clsAddData( s_class, "ONCLICK", X_WEBGUI_ONCLICK );
      x_clsSetDataType( s_class, "ONCLICK", X_OO_VALUE_CODEBLOCK );
      x_clsAddData( s_class, "ONCHANGE", X_WEBGUI_ONCHANGE );
      x_clsSetDataType( s_class, "ONCHANGE", X_OO_VALUE_CODEBLOCK );
      x_clsAddData( s_class, "ACOLUMNS", X_WEBGUI_COLUMNS );
      x_clsSetScope( s_class, "ACOLUMNS", X_OO_CLSTP_HIDDEN );
      x_clsAddData( s_class, "AROWS", X_WEBGUI_ROWS );
      x_clsSetScope( s_class, "AROWS", X_OO_CLSTP_HIDDEN );
      x_clsAddData( s_class, "AACTIONS", X_WEBGUI_ACTIONS );
      x_clsSetScope( s_class, "AACTIONS", X_OO_CLSTP_HIDDEN );
      x_clsAddData( s_class, "NX", X_WEBGUI_X );
      x_clsSetScope( s_class, "NX", X_OO_CLSTP_HIDDEN );
      x_clsAddData( s_class, "NY", X_WEBGUI_Y );
      x_clsSetScope( s_class, "NY", X_OO_CLSTP_HIDDEN );
      x_clsAddData( s_class, "NWIDTH", X_WEBGUI_WIDTH );
      x_clsSetScope( s_class, "NWIDTH", X_OO_CLSTP_HIDDEN );
      x_clsAddData( s_class, "NHEIGHT", X_WEBGUI_HEIGHT );
      x_clsSetScope( s_class, "NHEIGHT", X_OO_CLSTP_HIDDEN );
      x_clsAddData( s_class, "LGROW", X_WEBGUI_GROW );
      x_clsSetScope( s_class, "LGROW", X_OO_CLSTP_HIDDEN );

      x_clsAdd( s_class, "SETFOCUS", X_FUNCNAME( WG_CONTROL_SETFOCUS ) );
      x_clsAdd( s_class, "DESTROY", X_FUNCNAME( WG_CONTROL_DESTROY ) );
      x_clsAddAccess( s_class, "TEXT", X_FUNCNAME( WG_CONTROL_GETTEXT ) );
      x_clsAddAssign( s_class, "TEXT", X_FUNCNAME( WG_CONTROL_SETTEXT ) );
      x_clsAddAccess( s_class, "VALUE", X_FUNCNAME( WG_CONTROL_GETVALUE ) );
      x_clsAddAssign( s_class, "VALUE", X_FUNCNAME( WG_CONTROL_SETVALUE ) );
      x_clsAddAccess( s_class, "ENABLED", X_FUNCNAME( WG_CONTROL_GETENABLED ) );
      x_clsAddAssign( s_class, "ENABLED", X_FUNCNAME( WG_CONTROL_SETENABLED ) );
      x_clsAddAccess( s_class, "VISIBLE", X_FUNCNAME( WG_CONTROL_GETVISIBLE ) );
      x_clsAddAssign( s_class, "VISIBLE", X_FUNCNAME( WG_CONTROL_SETVISIBLE ) );
      x_clsAddAccess( s_class, "X", X_FUNCNAME( WG_CONTROL_GETX ) );
      x_clsAddAssign( s_class, "X", X_FUNCNAME( WG_CONTROL_SETX ) );
      x_clsAddAccess( s_class, "Y", X_FUNCNAME( WG_CONTROL_GETY ) );
      x_clsAddAssign( s_class, "Y", X_FUNCNAME( WG_CONTROL_SETY ) );
      x_clsAddAccess( s_class, "WIDTH", X_FUNCNAME( WG_CONTROL_GETWIDTH ) );
      x_clsAddAssign( s_class, "WIDTH", X_FUNCNAME( WG_CONTROL_SETWIDTH ) );
      x_clsAddAccess( s_class, "HEIGHT", X_FUNCNAME( WG_CONTROL_GETHEIGHT ) );
      x_clsAddAssign( s_class, "HEIGHT", X_FUNCNAME( WG_CONTROL_SETHEIGHT ) );
      x_clsAddAccess( s_class, "GROW", X_FUNCNAME( WG_CONTROL_GETGROW ) );
      x_clsAddAssign( s_class, "GROW", X_FUNCNAME( WG_CONTROL_SETGROW ) );
      x_clsAdd( s_class, "__WGCREATE", X_FUNCNAME( WG_CONTROL_CREATE ) );
      x_clsAdd( s_class, "__WGEVENT", X_FUNCNAME( WG_CONTROL_EVENT ) );
      x_clsAdd( s_class, "__WGCOLUMN", X_FUNCNAME( WG_CONTROL_COLUMN ) );
      x_clsAdd( s_class, "__WGROWS", X_FUNCNAME( WG_CONTROL_ROWS ) );
      x_clsAdd( s_class, "__WGREFRESH", X_FUNCNAME( WG_CONTROL_REFRESH ) );
      x_clsAdd( s_class, "__WGITEM", X_FUNCNAME( WG_CONTROL_ITEM ) );
      x_clsAdd( s_class, "__WGACTION", X_FUNCNAME( WG_CONTROL_ACTION ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   wg_Label
   --------------------------------------------------------------------------------------------- */

// Create a label in its parent, a window or a box
// wg_Label():New( oParent, [cText] ) --> self
X_FUNC_STATIC( WG_LABEL_NEW )
{
   if( !x_webguiParentValid( ctx ) || ( !X_ISNIL( ctx, 2 ) && !X_ISCHAR( ctx, 2 ) ) )
   {
      x_webguiRaise( ctx, "WG_LABEL:NEW" );
      return;
   }

   x_webguiControlSend( ctx, "label", x_paritem( ctx, 2 ), NULL, NULL );
}

// Text on the page
// wg_Label() --> oLabel
X_FUNC( WG_LABEL )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 0, "WG_LABEL" );
      x_clsAddParent( s_class, x_clsRequire( "WG_CONTROL", WG_CONTROL ) );

      x_clsAdd( s_class, "NEW", X_FUNCNAME( WG_LABEL_NEW ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   wg_Button
   --------------------------------------------------------------------------------------------- */

// Create a button in its parent; OnClick runs when the user clicks it
// wg_Button():New( oParent, [cText] ) --> self
X_FUNC_STATIC( WG_BUTTON_NEW )
{
   if( !x_webguiParentValid( ctx ) || ( !X_ISNIL( ctx, 2 ) && !X_ISCHAR( ctx, 2 ) ) )
   {
      x_webguiRaise( ctx, "WG_BUTTON:NEW" );
      return;
   }

   x_webguiControlSend( ctx, "button", x_paritem( ctx, 2 ), NULL, NULL );
}

// Push button
// wg_Button() --> oButton
X_FUNC( WG_BUTTON )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 0, "WG_BUTTON" );
      x_clsAddParent( s_class, x_clsRequire( "WG_CONTROL", WG_CONTROL ) );

      x_clsAdd( s_class, "NEW", X_FUNCNAME( WG_BUTTON_NEW ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   wg_Edit
   --------------------------------------------------------------------------------------------- */

// Create an edit field of one line in its parent with cLabel above it, its :text; :value is the text of the field,
// OnChange runs at every change by the user
// wg_Edit():New( oParent, [cValue], [cLabel] ) --> self
X_FUNC_STATIC( WG_EDIT_NEW )
{
   X_ITEM empty __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   int valid = x_webguiParentValid( ctx ) && ( X_ISNIL( ctx, 2 ) || X_ISCHAR( ctx, 2 ) );

   if( !valid || ( !X_ISNIL( ctx, 3 ) && !X_ISCHAR( ctx, 3 ) ) )
   {
      x_webguiRaise( ctx, "WG_EDIT:NEW" );
      return;
   }

   x_itemPutC( &empty, "" );
   x_webguiControlSend( ctx, "edit", x_paritem( ctx, 3 ), IIF( X_ISNIL( ctx, 2 ), &empty, x_paritem( ctx, 2 ) ), NULL );
}

// Edit field of one line
// wg_Edit() --> oEdit
X_FUNC( WG_EDIT )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 0, "WG_EDIT" );
      x_clsAddParent( s_class, x_clsRequire( "WG_CONTROL", WG_CONTROL ) );

      x_clsAdd( s_class, "NEW", X_FUNCNAME( WG_EDIT_NEW ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   wg_CheckBox
   --------------------------------------------------------------------------------------------- */

// Create a check box in its parent with cText at its right; :value is .T. when it is checked, OnChange runs when
// the user changes it
// wg_CheckBox():New( oParent, [cText], [lChecked] ) --> self
X_FUNC_STATIC( WG_CHECKBOX_NEW )
{
   X_ITEM checked __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   int valid = x_webguiParentValid( ctx ) && ( X_ISNIL( ctx, 2 ) || X_ISCHAR( ctx, 2 ) );

   if( !valid || ( !X_ISNIL( ctx, 3 ) && !X_ISLOG( ctx, 3 ) ) )
   {
      x_webguiRaise( ctx, "WG_CHECKBOX:NEW" );
      return;
   }

   x_itemPutL( &checked, X_ISLOG( ctx, 3 ) && x_parl( ctx, 3 ) );
   x_webguiControlSend( ctx, "check", x_paritem( ctx, 2 ), &checked, NULL );
}

// Check box
// wg_CheckBox() --> oCheckBox
X_FUNC( WG_CHECKBOX )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 0, "WG_CHECKBOX" );
      x_clsAddParent( s_class, x_clsRequire( "WG_CONTROL", WG_CONTROL ) );

      x_clsAdd( s_class, "NEW", X_FUNCNAME( WG_CHECKBOX_NEW ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   wg_ComboBox
   --------------------------------------------------------------------------------------------- */

// Create a drop-down list of the texts of aItems in its parent with cLabel above it, its :text; :value is the
// number of the chosen item, nSelected or 1 at the start; OnChange runs when the user chooses another item
// wg_ComboBox():New( oParent, aItems, [cLabel], [nSelected] ) --> self
X_FUNC_STATIC( WG_COMBOBOX_NEW )
{
   X_ITEM selected __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   int valid = x_webguiParentValid( ctx ) && X_ISARRAY( ctx, 2 ) && ( X_ISNIL( ctx, 3 ) || X_ISCHAR( ctx, 3 ) );

   if( !valid || ( !X_ISNIL( ctx, 4 ) && !X_ISNUM( ctx, 4 ) ) )
   {
      x_webguiRaise( ctx, "WG_COMBOBOX:NEW" );
      return;
   }

   x_itemPutNI( &selected, x_parnidef( ctx, 4, 1 ) );
   x_webguiControlSend( ctx, "combo", x_paritem( ctx, 3 ), &selected, x_paritem( ctx, 2 ) );
}

// Drop-down list
// wg_ComboBox() --> oComboBox
X_FUNC( WG_COMBOBOX )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 0, "WG_COMBOBOX" );
      x_clsAddParent( s_class, x_clsRequire( "WG_CONTROL", WG_CONTROL ) );

      x_clsAdd( s_class, "NEW", X_FUNCNAME( WG_COMBOBOX_NEW ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   wg_Browse
   --------------------------------------------------------------------------------------------- */

// Create a browse in its parent, a table of rows with a heading that stays at the top, 200 high unless :grow or
// :height says otherwise; :value is the number of the current row; OnChange runs when the user moves to another
// row, OnClick on a double click or Enter; a click on a heading sorts the rows
// wg_Browse():New( oParent ) --> self
X_FUNC_STATIC( WG_BROWSE_NEW )
{
   X_ITEM none __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;

   if( !x_webguiParentValid( ctx ) )
   {
      x_webguiRaise( ctx, "WG_BROWSE:NEW" );
      return;
   }

   x_itemPutNI( &none, 0 );
   x_webguiControlSend( ctx, "browse", NULL, &none, NULL );
}

// Add a column; bBlock receives the row, an element of the array of SetArray(), and its number and returns the
// value of the cell, shown through Transform() with cPicture; nWidth in logical pixels; nAlign 0 left, 1 right,
// 2 center, without it a number stands on the right; returns the number of the column
// oBrowse:AddColumn( cHeading, bBlock, [nWidth], [cPicture], [nAlign] ) --> nColumn
X_FUNC_STATIC( WG_BROWSE_ADDCOLUMN )
{
   int valid = X_ISCHAR( ctx, 1 ) && X_ISBLOCK( ctx, 2 );

   valid = valid && ( X_ISNIL( ctx, 3 ) || ( X_ISNUM( ctx, 3 ) && x_parni( ctx, 3 ) > 0 ) );
   valid = valid && ( X_ISNIL( ctx, 4 ) || X_ISCHAR( ctx, 4 ) );
   valid = valid && ( X_ISNIL( ctx, 5 ) || ( X_ISNUM( ctx, 5 ) && x_parni( ctx, 5 ) >= 0 && x_parni( ctx, 5 ) <= 2 ) );
   if( !valid )
   {
      x_webguiRaise( ctx, "WG_BROWSE:ADDCOLUMN" );
      return;
   }

   x_webguiForward( ctx, "__WGCOLUMN", 5 );
}

// Show the rows of aRows, any values the blocks of the columns read; the first row becomes the current one; the
// browse keeps the array itself, Refresh() shows its later changes
// oBrowse:SetArray( aRows ) --> self
X_FUNC_STATIC( WG_BROWSE_SETARRAY )
{
   if( !X_ISARRAY( ctx, 1 ) )
   {
      x_webguiRaise( ctx, "WG_BROWSE:SETARRAY" );
      return;
   }

   x_webguiForward( ctx, "__WGROWS", 1 );
}

// Show the rows again, e.g. after the array changed
// oBrowse:Refresh() --> self
X_FUNC_STATIC( WG_BROWSE_REFRESH )
{
   x_webguiForward( ctx, "__WGREFRESH", 0 );
}

// Table of rows drawn by the page; the program keeps the rows and makes the texts of the cells
// wg_Browse() --> oBrowse
X_FUNC( WG_BROWSE )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 0, "WG_BROWSE" );
      x_clsAddParent( s_class, x_clsRequire( "WG_CONTROL", WG_CONTROL ) );

      x_clsAdd( s_class, "NEW", X_FUNCNAME( WG_BROWSE_NEW ) );
      x_clsAdd( s_class, "ADDCOLUMN", X_FUNCNAME( WG_BROWSE_ADDCOLUMN ) );
      x_clsAdd( s_class, "SETARRAY", X_FUNCNAME( WG_BROWSE_SETARRAY ) );
      x_clsAdd( s_class, "REFRESH", X_FUNCNAME( WG_BROWSE_REFRESH ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   wg_Menu
   --------------------------------------------------------------------------------------------- */

// Create a menu of the menu bar at the top of the window; a click on cTitle opens its items; a menu without items
// is a command of the bar: a click runs bAction, its OnClick
// wg_Menu():New( oWindow, cTitle, [bAction] ) --> self
X_FUNC_STATIC( WG_MENU_NEW )
{
   const X_ITEM *parent = x_paritem( ctx, 1 );
   int valid = parent != NULL && X_IS_OBJECT( parent ) && x_objIsKindOf( parent, "WG_WINDOW" );

   if( !valid || !X_ISCHAR( ctx, 2 ) || ( !X_ISNIL( ctx, 3 ) && !X_ISBLOCK( ctx, 3 ) ) )
   {
      x_webguiRaise( ctx, "WG_MENU:NEW" );
      return;
   }

   x_webguiControlSend( ctx, "menu", x_paritem( ctx, 2 ), x_paritem( ctx, 3 ), NULL );
}

// Add an item; bAction runs on its click and receives the menu; cShortcut, e.g. "Ctrl+N", "F2", "Del", stands at
// the right of the item and runs it from the keyboard
// oMenu:Add( cText, bAction, [cShortcut] ) --> self
X_FUNC_STATIC( WG_MENU_ADD )
{
   if( !X_ISCHAR( ctx, 1 ) || !X_ISBLOCK( ctx, 2 ) || ( !X_ISNIL( ctx, 3 ) && !X_ISCHAR( ctx, 3 ) ) )
   {
      x_webguiRaise( ctx, "WG_MENU:ADD" );
      return;
   }

   x_webguiForward( ctx, "__WGITEM", 3 );
}

// Add a line between items
// oMenu:AddSeparator() --> self
X_FUNC_STATIC( WG_MENU_ADDSEPARATOR )
{
   x_webguiForward( ctx, "__WGITEM", 3 );
}

// Menu of the menu bar of a wg_Window; the arrows, Esc, Home and End move in the menus
// wg_Menu() --> oMenu
X_FUNC( WG_MENU )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 0, "WG_MENU" );
      x_clsAddParent( s_class, x_clsRequire( "WG_CONTROL", WG_CONTROL ) );

      x_clsAdd( s_class, "NEW", X_FUNCNAME( WG_MENU_NEW ) );
      x_clsAdd( s_class, "ADD", X_FUNCNAME( WG_MENU_ADD ) );
      x_clsAdd( s_class, "ADDSEPARATOR", X_FUNCNAME( WG_MENU_ADDSEPARATOR ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   wg_Get
   --------------------------------------------------------------------------------------------- */

// Create a GET for xValue, text, a number, a date or a logical value, edited through cPicture: for text 9 a digit,
// A a letter, ! a letter in capitals, X any character, other characters stay, "@!" makes all letters capitals; for
// a number e.g. "999,999.99" with text before and after; for a date DD, MM and YYYY, "DD.MM.YYYY" by default; for a
// logical value "Yes/No", ".T./.F." by default; cLabel stands above the field, its :text; Enter and leaving the
// field keep the value in :value and run OnChange, Enter goes to the next field; a value that does not fit the
// picture is marked and not kept
// wg_Get():New( oParent, xValue, [cPicture], [cLabel] ) --> self
X_FUNC_STATIC( WG_GET_NEW )
{
   int valid = x_webguiParentValid( ctx ) && ( X_ISNIL( ctx, 3 ) || X_ISCHAR( ctx, 3 ) );

   valid = valid && ( X_ISCHAR( ctx, 2 ) || X_ISNUM( ctx, 2 ) || X_ISDATE( ctx, 2 ) || X_ISLOG( ctx, 2 ) );
   if( !valid || ( !X_ISNIL( ctx, 4 ) && !X_ISCHAR( ctx, 4 ) ) )
   {
      x_webguiRaise( ctx, "WG_GET:NEW" );
      return;
   }

   x_webguiControlSend( ctx, "get", x_paritem( ctx, 4 ), x_paritem( ctx, 2 ), x_paritem( ctx, 3 ) );
}

// Field with a picture, as GET of xBase; VALID {| oGet, xValue | lValid } checks a new value before :value keeps it,
// a refused value stays marked in the field, which gets the focus back; WHEN {| oGet | lWhen } decides whether the
// field can be entered, otherwise the focus goes on to the next field
// wg_Get() --> oGet
X_FUNC( WG_GET )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 2, "WG_GET" );
      x_clsAddParent( s_class, x_clsRequire( "WG_CONTROL", WG_CONTROL ) );
      x_clsAddData( s_class, "VALID", 1 );
      x_clsSetDataType( s_class, "VALID", X_OO_VALUE_CODEBLOCK );
      x_clsAddData( s_class, "WHEN", 2 );
      x_clsSetDataType( s_class, "WHEN", X_OO_VALUE_CODEBLOCK );

      x_clsAdd( s_class, "NEW", X_FUNCNAME( WG_GET_NEW ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   wg_Panel
   --------------------------------------------------------------------------------------------- */

// A box: a control holding controls, the parent given to their New(); it places them one after another
// wg_Box() --> oBox
X_FUNC( WG_BOX )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 0, "WG_BOX" );
      x_clsAddParent( s_class, x_clsRequire( "WG_CONTROL", WG_CONTROL ) );
   }

   x_clsReturn( ctx, s_class );
}

// Panel of a page of a sidebar, made by wg_Sidebar:Add(); it places its controls one under another, as a column
// wg_Panel() --> oPanel
X_FUNC( WG_PANEL )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 0, "WG_PANEL" );
      x_clsAddParent( s_class, x_clsRequire( "WG_BOX", WG_BOX ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   wg_Column, wg_Row
   --------------------------------------------------------------------------------------------- */

// Create a column in its parent: its controls stand one under another, as wide as the column; a window, a panel
// and a part of a split are columns too
// wg_Column():New( oParent ) --> self
X_FUNC_STATIC( WG_COLUMN_NEW )
{
   if( !x_webguiParentValid( ctx ) )
   {
      x_webguiRaise( ctx, "WG_COLUMN:NEW" );
      return;
   }

   x_webguiControlSend( ctx, "column", NULL, NULL, NULL );
}

// Box of controls one under another
// wg_Column() --> oColumn
X_FUNC( WG_COLUMN )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 0, "WG_COLUMN" );
      x_clsAddParent( s_class, x_clsRequire( "WG_BOX", WG_BOX ) );

      x_clsAdd( s_class, "NEW", X_FUNCNAME( WG_COLUMN_NEW ) );
   }

   x_clsReturn( ctx, s_class );
}

// Create a row in its parent: its controls stand one beside another, on a common bottom line, so fields with a
// label above them and buttons stand in line
// wg_Row():New( oParent ) --> self
X_FUNC_STATIC( WG_ROW_NEW )
{
   if( !x_webguiParentValid( ctx ) )
   {
      x_webguiRaise( ctx, "WG_ROW:NEW" );
      return;
   }

   x_webguiControlSend( ctx, "row", NULL, NULL, NULL );
}

// Box of controls one beside another
// wg_Row() --> oRow
X_FUNC( WG_ROW )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 0, "WG_ROW" );
      x_clsAddParent( s_class, x_clsRequire( "WG_BOX", WG_BOX ) );

      x_clsAdd( s_class, "NEW", X_FUNCNAME( WG_ROW_NEW ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   wg_Split
   --------------------------------------------------------------------------------------------- */

// Create a split in its parent: two parts, columns, side by side with a line between them, which the mouse drags;
// the split takes the room its parent leaves; :value is the width of the left part in logical pixels, nPosition or
// 300 at the start; OnChange runs when the user drags the line
// wg_Split():New( oParent, [nPosition] ) --> self
X_FUNC_STATIC( WG_SPLIT_NEW )
{
   X_ITEM position __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM first __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM second __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_CLASS_HANDLE column = x_clsRequire( "WG_COLUMN", WG_COLUMN );

   if( !x_webguiParentValid( ctx ) || ( !X_ISNIL( ctx, 2 ) && !( X_ISNUM( ctx, 2 ) && x_parni( ctx, 2 ) >= 0 ) ) )
   {
      x_webguiRaise( ctx, "WG_SPLIT:NEW" );
      return;
   }

   x_itemPutNI( &position, x_parnidef( ctx, 2, 300 ) );
   x_webguiControlSend( ctx, "split", NULL, &position, NULL );
   if( x_objCreate( ctx, &first, column ) && x_objCreate( ctx, &second, column ) )
      x_webguiSplitParts( ctx, &first, &second );
}

// Return part nPart of the split, a column: 1 at the left, 2 at the right
// oSplit:Panel( nPart ) --> oColumn
X_FUNC_STATIC( WG_SPLIT_PANEL )
{
   x_webguiSplitPanel( ctx, x_parni( ctx, 1 ) );
}

// Two columns with a line between them that the mouse drags
// wg_Split() --> oSplit
X_FUNC( WG_SPLIT )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 1, "WG_SPLIT" );
      x_clsAddParent( s_class, x_clsRequire( "WG_CONTROL", WG_CONTROL ) );
      x_clsAddData( s_class, "APARTS", 1 );
      x_clsSetScope( s_class, "APARTS", X_OO_CLSTP_HIDDEN );

      x_clsAdd( s_class, "NEW", X_FUNCNAME( WG_SPLIT_NEW ) );
      x_clsAdd( s_class, "PANEL", X_FUNCNAME( WG_SPLIT_PANEL ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   wg_Sidebar
   --------------------------------------------------------------------------------------------- */

// Create a sidebar at the left edge of the window: a strip of page pictures with buttons at its bottom and the panel
// of the shown page, nWidth wide (260 by default) and wider or narrower when the mouse drags its right edge; the
// controls of the window stand at its right; :value is the number of the shown page, 0 when the panel is hidden
// wg_Sidebar():New( oWindow, [nWidth] ) --> self
X_FUNC_STATIC( WG_SIDEBAR_NEW )
{
   X_ITEM none __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   const X_ITEM *parent = x_paritem( ctx, 1 );
   int valid = parent != NULL && X_IS_OBJECT( parent ) && x_objIsKindOf( parent, "WG_WINDOW" );

   if( !valid || ( !X_ISNIL( ctx, 2 ) && !( X_ISNUM( ctx, 2 ) && x_parni( ctx, 2 ) > 0 ) ) )
   {
      x_webguiRaise( ctx, "WG_SIDEBAR:NEW" );
      return;
   }

   x_itemPutNI( &none, 0 );
   x_webguiControlSend( ctx, "sidebar", NULL, &none, NULL );
}

// Add a page: cPicture, a PNG, SVG, JPEG, GIF or BMP file, in the strip, cToolTip its tip (cTitle without it) and a
// panel under the header with cTitle; a click on the picture shows the page, a click on the picture of the shown
// page hides the panel; OnChange runs after both; returns the number of the page, counted from 1
// oSidebar:Add( cTitle, cPicture, [cToolTip] ) --> nPage
X_FUNC_STATIC( WG_SIDEBAR_ADD )
{
   X_ITEM panel __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   int number;

   if( !X_ISCHAR( ctx, 1 ) || !X_ISCHAR( ctx, 2 ) || ( !X_ISNIL( ctx, 3 ) && !X_ISCHAR( ctx, 3 ) ) )
   {
      x_webguiRaise( ctx, "WG_SIDEBAR:ADD" );
      return;
   }

   if( !x_objCreate( ctx, &panel, x_clsRequire( "WG_PANEL", WG_PANEL ) ) )
      return;

   number = x_webguiSidebarAdd( ctx, &panel );
   if( number != 0 )
      x_retni( ctx, number );
}

// Return the panel of page nPage; controls are created on it as on a window
// oSidebar:Panel( nPage ) --> oPanel
X_FUNC_STATIC( WG_SIDEBAR_PANEL )
{
   x_webguiSidebarPanel( ctx, x_parni( ctx, 1 ) );
}

// Add a small button with cPicture at the right of the header of page nPage, shown with the page; bAction runs on
// its click and receives the sidebar
// oSidebar:AddAction( nPage, cPicture, bAction, [cToolTip] ) --> self
X_FUNC_STATIC( WG_SIDEBAR_ADDACTION )
{
   const X_ITEM *pages = x_objDataSlot( ctx, 1 );
   int count = IIF( pages != NULL && X_IS_ARRAY( pages ), ( int )x_arrayLen( pages ), 0 );
   int valid = X_ISNUM( ctx, 1 ) && x_parni( ctx, 1 ) >= 1 && x_parni( ctx, 1 ) <= count;

   valid = valid && X_ISCHAR( ctx, 2 ) && X_ISBLOCK( ctx, 3 ) && ( X_ISNIL( ctx, 4 ) || X_ISCHAR( ctx, 4 ) );
   if( !valid )
   {
      x_webguiRaise( ctx, "WG_SIDEBAR:ADDACTION" );
      return;
   }

   x_webguiSidebarButton( ctx, 2 );
}

// Add a button with cPicture at the bottom of the strip, e.g. for the settings; bAction runs on its click and
// receives the sidebar
// oSidebar:AddButton( cPicture, bAction, [cToolTip] ) --> self
X_FUNC_STATIC( WG_SIDEBAR_ADDBUTTON )
{
   if( !X_ISCHAR( ctx, 1 ) || !X_ISBLOCK( ctx, 2 ) || ( !X_ISNIL( ctx, 3 ) && !X_ISCHAR( ctx, 3 ) ) )
   {
      x_webguiRaise( ctx, "WG_SIDEBAR:ADDBUTTON" );
      return;
   }

   x_webguiSidebarButton( ctx, 1 );
}

// Sidebar as in code editors, drawn by the page; the program keeps the pages and the shown one
// wg_Sidebar() --> oSidebar
X_FUNC( WG_SIDEBAR )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 1, "WG_SIDEBAR" );
      x_clsAddParent( s_class, x_clsRequire( "WG_CONTROL", WG_CONTROL ) );
      x_clsAddData( s_class, "APAGES", 1 );
      x_clsSetScope( s_class, "APAGES", X_OO_CLSTP_HIDDEN );

      x_clsAdd( s_class, "NEW", X_FUNCNAME( WG_SIDEBAR_NEW ) );
      x_clsAdd( s_class, "ADD", X_FUNCNAME( WG_SIDEBAR_ADD ) );
      x_clsAdd( s_class, "PANEL", X_FUNCNAME( WG_SIDEBAR_PANEL ) );
      x_clsAdd( s_class, "ADDACTION", X_FUNCNAME( WG_SIDEBAR_ADDACTION ) );
      x_clsAdd( s_class, "ADDBUTTON", X_FUNCNAME( WG_SIDEBAR_ADDBUTTON ) );
   }

   x_clsReturn( ctx, s_class );
}
