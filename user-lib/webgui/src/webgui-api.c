/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev WebGUI
 * File......: webgui-api.c
 *
 */

#include <string.h>

#include "internal-webgui.h"

/* ---------------------------------------------------------------------------------------------
   wgWindow
   --------------------------------------------------------------------------------------------- */

// Create a hidden window with a browser, the client area nWidth x nHeight (default 640 x 480); Run() shows it
// wgWindow():New( [cTitle], [nWidth], [nHeight] ) --> self
X_FUNC_STATIC( WGWINDOW_NEW )
{
   int valid = X_ISNIL( ctx, 1 ) || X_ISCHAR( ctx, 1 );

   valid = valid && ( X_ISNIL( ctx, 2 ) || ( X_ISNUM( ctx, 2 ) && x_parni( ctx, 2 ) > 0 ) );
   valid = valid && ( X_ISNIL( ctx, 3 ) || ( X_ISNUM( ctx, 3 ) && x_parni( ctx, 3 ) > 0 ) );
   if( !valid )
   {
      x_webguiRaise( ctx, "WGWINDOW:NEW" );
      return;
   }

   x_webguiWindowNew( ctx );
}

// Show the HTML file cFile in the window; a relative path counts from the folder of the program; the title of the
// page becomes the title of the window; returns .F. when there is no such file
// oWindow:Open( cFile ) --> lSuccess
X_FUNC_STATIC( WGWINDOW_OPEN )
{
   HWND window = x_webguiSelfHandle( ctx, "WGWINDOW:OPEN" );

   if( window == NULL )
      return;

   if( !X_ISCHAR( ctx, 1 ) )
   {
      x_webguiRaise( ctx, "WGWINDOW:OPEN" );
      return;
   }

   x_retl( ctx, x_webguiWindowOpen( ctx, window, x_paritem( ctx, 1 ) ) );
}

// Send cJson to the page; the page gets it in window.chrome.webview.addEventListener( "message", e => e.data ) as
// an object; before the page is ready the text waits, so Send() works also before Run()
// oWindow:Send( cJson ) --> self
X_FUNC_STATIC( WGWINDOW_SEND )
{
   HWND window = x_webguiSelfHandle( ctx, "WGWINDOW:SEND" );

   if( window == NULL )
      return;

   if( !X_ISCHAR( ctx, 1 ) )
   {
      x_webguiRaise( ctx, "WGWINDOW:SEND" );
      return;
   }

   x_webguiWindowSend( ctx, window, x_paritem( ctx, 1 ) );
   x_itemCopy( ctx->result, ctx->self );
}

// Run OnMessage with the window and the JSON text of a message of the page (internal)
// oWindow:__WgMessage( cJson ) --> NIL
X_FUNC_STATIC( WGWINDOW_MESSAGE )
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
X_FUNC_STATIC( WGWINDOW_RUN )
{
   HWND window = x_webguiSelfHandle( ctx, "WGWINDOW:RUN" );

   if( window != NULL )
      x_webguiWindowRun( ctx, window );
}

// Close the window with its browser
// oWindow:Close() --> NIL
X_FUNC_STATIC( WGWINDOW_CLOSE )
{
   HWND window = x_webguiSelfHandle( ctx, "WGWINDOW:CLOSE" );

   if( window != NULL )
      x_webguiWindowClose( window );
}

// Window of the system with a browser showing an HTML page; the program keeps the logic, the page only draws:
// the page sends events with window.chrome.webview.postMessage(), OnMessage receives the window and their JSON
// text, Send() answers the page
// wgWindow() --> oWindow
X_FUNC( WGWINDOW )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 2, "WGWINDOW" );

      x_clsAddData( s_class, "HANDLE", 1 );
      x_clsSetScope( s_class, "HANDLE", X_OO_CLSTP_EXPORTED | X_OO_CLSTP_READONLY );
      x_clsAddData( s_class, "ONMESSAGE", 2 );
      x_clsSetDataType( s_class, "ONMESSAGE", X_OO_VALUE_CODEBLOCK );

      x_clsAdd( s_class, "NEW", X_FUNCNAME( WGWINDOW_NEW ) );
      x_clsAdd( s_class, "OPEN", X_FUNCNAME( WGWINDOW_OPEN ) );
      x_clsAdd( s_class, "SEND", X_FUNCNAME( WGWINDOW_SEND ) );
      x_clsAdd( s_class, "RUN", X_FUNCNAME( WGWINDOW_RUN ) );
      x_clsAdd( s_class, "CLOSE", X_FUNCNAME( WGWINDOW_CLOSE ) );
      x_clsAdd( s_class, "__WGMESSAGE", X_FUNCNAME( WGWINDOW_MESSAGE ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   wgControl
   --------------------------------------------------------------------------------------------- */

// Create the control on the page (internal); New() of a kind of control sends it
// oControl:__WgCreate( cKind, oWindow, nX, nY, nWidth, nHeight, cText, xValue, aItems ) --> self
X_FUNC_STATIC( WGCONTROL_CREATE )
{
   x_webguiControlCreate( ctx );
}

// Return the text of a label, of a button or of a check box, the label of an edit field, of a get or of a combo box,
// the title of a menu
// oControl:text --> cText
X_FUNC_STATIC( WGCONTROL_GETTEXT )
{
   x_itemReturn( ctx, x_objDataSlot( ctx, X_WEBGUI_TEXT ) );
}

// Set the text of the control
// oControl:text := cText
X_FUNC_STATIC( WGCONTROL_SETTEXT )
{
   if( !X_ISCHAR( ctx, 1 ) )
   {
      x_webguiRaise( ctx, "WGCONTROL:TEXT" );
      return;
   }

   x_webguiControlSet( ctx, "text", X_WEBGUI_TEXT, "WGCONTROL:TEXT" );
}

// Return the value the user gave: the text of an edit field, .T. for a checked check box, the number of the item
// of a combo box
// oControl:value --> xValue
X_FUNC_STATIC( WGCONTROL_GETVALUE )
{
   x_itemReturn( ctx, x_objDataSlot( ctx, X_WEBGUI_VALUE ) );
}

// Set the value: text for an edit field, a logical value for a check box, the number of the item for a combo box,
// the number of the current row for a browse, a value of the same type for a get, the number of the shown page for
// a sidebar, 0 to hide its panel
// oControl:value := xValue
X_FUNC_STATIC( WGCONTROL_SETVALUE )
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
      x_webguiRaise( ctx, "WGCONTROL:VALUE" );
      return;
   }

   x_webguiControlSet( ctx, "value", X_WEBGUI_VALUE, "WGCONTROL:VALUE" );
}

// Return whether the control is active
// oControl:enabled --> lEnabled
X_FUNC_STATIC( WGCONTROL_GETENABLED )
{
   x_itemReturn( ctx, x_objDataSlot( ctx, X_WEBGUI_ENABLED ) );
}

// Make the control active or grey
// oControl:enabled := lEnabled
X_FUNC_STATIC( WGCONTROL_SETENABLED )
{
   if( !X_ISLOG( ctx, 1 ) )
   {
      x_webguiRaise( ctx, "WGCONTROL:ENABLED" );
      return;
   }

   x_webguiControlSet( ctx, "enabled", X_WEBGUI_ENABLED, "WGCONTROL:ENABLED" );
}

// Return whether the control is shown
// oControl:visible --> lVisible
X_FUNC_STATIC( WGCONTROL_GETVISIBLE )
{
   x_itemReturn( ctx, x_objDataSlot( ctx, X_WEBGUI_VISIBLE ) );
}

// Show or hide the control
// oControl:visible := lVisible
X_FUNC_STATIC( WGCONTROL_SETVISIBLE )
{
   if( !X_ISLOG( ctx, 1 ) )
   {
      x_webguiRaise( ctx, "WGCONTROL:VISIBLE" );
      return;
   }

   x_webguiControlSet( ctx, "visible", X_WEBGUI_VISIBLE, "WGCONTROL:VISIBLE" );
}

// Return the left edge of a control placed in pixels, NIL when its parent places it
// oControl:x --> nX
X_FUNC_STATIC( WGCONTROL_GETX )
{
   x_itemReturn( ctx, x_objDataSlot( ctx, X_WEBGUI_X ) );
}

// Place the control at the left edge nX of its parent in logical pixels, out of the order of the parent; NIL puts
// it back in the order
// oControl:x := nX
X_FUNC_STATIC( WGCONTROL_SETX )
{
   if( !X_ISNIL( ctx, 1 ) && !( X_ISNUM( ctx, 1 ) && x_parni( ctx, 1 ) >= 0 ) )
   {
      x_webguiRaise( ctx, "WGCONTROL:X" );
      return;
   }

   x_webguiControlSet( ctx, "x", X_WEBGUI_X, "WGCONTROL:X" );
}

// Return the top edge of a control placed in pixels, NIL when its parent places it
// oControl:y --> nY
X_FUNC_STATIC( WGCONTROL_GETY )
{
   x_itemReturn( ctx, x_objDataSlot( ctx, X_WEBGUI_Y ) );
}

// Place the control at the top edge nY of its parent in logical pixels, out of the order of the parent; NIL puts
// it back in the order
// oControl:y := nY
X_FUNC_STATIC( WGCONTROL_SETY )
{
   if( !X_ISNIL( ctx, 1 ) && !( X_ISNUM( ctx, 1 ) && x_parni( ctx, 1 ) >= 0 ) )
   {
      x_webguiRaise( ctx, "WGCONTROL:Y" );
      return;
   }

   x_webguiControlSet( ctx, "y", X_WEBGUI_Y, "WGCONTROL:Y" );
}

// Return the width given to the control, NIL for its own width
// oControl:width --> nWidth
X_FUNC_STATIC( WGCONTROL_GETWIDTH )
{
   x_itemReturn( ctx, x_objDataSlot( ctx, X_WEBGUI_WIDTH ) );
}

// Give the control a width in logical pixels, the width of the panel for a sidebar; NIL for its own width
// oControl:width := nWidth
X_FUNC_STATIC( WGCONTROL_SETWIDTH )
{
   if( !X_ISNIL( ctx, 1 ) && !( X_ISNUM( ctx, 1 ) && x_parni( ctx, 1 ) > 0 ) )
   {
      x_webguiRaise( ctx, "WGCONTROL:WIDTH" );
      return;
   }

   x_webguiControlSet( ctx, "width", X_WEBGUI_WIDTH, "WGCONTROL:WIDTH" );
}

// Return the height given to the control, NIL for its own height
// oControl:height --> nHeight
X_FUNC_STATIC( WGCONTROL_GETHEIGHT )
{
   x_itemReturn( ctx, x_objDataSlot( ctx, X_WEBGUI_HEIGHT ) );
}

// Give the control a height in logical pixels; NIL for its own height
// oControl:height := nHeight
X_FUNC_STATIC( WGCONTROL_SETHEIGHT )
{
   if( !X_ISNIL( ctx, 1 ) && !( X_ISNUM( ctx, 1 ) && x_parni( ctx, 1 ) > 0 ) )
   {
      x_webguiRaise( ctx, "WGCONTROL:HEIGHT" );
      return;
   }

   x_webguiControlSet( ctx, "height", X_WEBGUI_HEIGHT, "WGCONTROL:HEIGHT" );
}

// Return whether the control takes the room its parent leaves
// oControl:grow --> lGrow
X_FUNC_STATIC( WGCONTROL_GETGROW )
{
   x_itemReturn( ctx, x_objDataSlot( ctx, X_WEBGUI_GROW ) );
}

// .T. lets the control take the room its parent leaves: the rest of the height in a column, of the width in a row;
// controls that grow share the room
// oControl:grow := lGrow
X_FUNC_STATIC( WGCONTROL_SETGROW )
{
   if( !X_ISLOG( ctx, 1 ) )
   {
      x_webguiRaise( ctx, "WGCONTROL:GROW" );
      return;
   }

   x_webguiControlSet( ctx, "grow", X_WEBGUI_GROW, "WGCONTROL:GROW" );
}

// Give the focus to the control
// oControl:SetFocus() --> self
X_FUNC_STATIC( WGCONTROL_SETFOCUS )
{
   x_webguiControlCommand( ctx, 0 );
}

// Remove the control from the page
// oControl:Destroy() --> self
X_FUNC_STATIC( WGCONTROL_DESTROY )
{
   x_webguiControlCommand( ctx, 1 );
}

// Receive an event of the page: keep the new value and run OnChange or OnClick (internal)
// oControl:__WgEvent( cEvent, cValue ) --> NIL
X_FUNC_STATIC( WGCONTROL_EVENT )
{
   x_webguiControlEvent( ctx );
}

// Add a column of a browse (internal); wgBrowse:AddColumn() sends it
// oControl:__WgColumn( cHeading, bBlock, nWidth, cPicture, nAlign ) --> nColumn
X_FUNC_STATIC( WGCONTROL_COLUMN )
{
   x_webguiBrowseColumn( ctx );
}

// Set the rows of a browse (internal); wgBrowse:SetArray() sends it
// oControl:__WgRows( aRows ) --> self
X_FUNC_STATIC( WGCONTROL_ROWS )
{
   x_webguiBrowseRows( ctx );
}

// Show the rows of a browse again (internal); wgBrowse:Refresh() sends it
// oControl:__WgRefresh() --> self
X_FUNC_STATIC( WGCONTROL_REFRESH )
{
   x_webguiBrowseRefresh( ctx );
}

// Add an item to a menu (internal); wgMenu:Add() and AddSeparator() send it
// oControl:__WgItem( cText, bAction, cShortcut ) --> self
X_FUNC_STATIC( WGCONTROL_ITEM )
{
   x_webguiMenuItem( ctx );
}

// Add an action of a page of a sidebar or a button at the bottom of its strip (internal); wgSidebar:AddAction()
// and AddButton() send it
// oControl:__WgAction( nPage, cPicture, bAction, cToolTip ) --> self
X_FUNC_STATIC( WGCONTROL_ACTION )
{
   x_webguiSidebarAction( ctx );
}

// Control drawn by the page of its wgWindow; the program keeps its text and value; OnClick and OnChange receive
// the control
// wgControl() --> oControl
X_FUNC( WGCONTROL )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( X_WEBGUI_CONTROL_DATA, "WGCONTROL" );

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

      x_clsAdd( s_class, "SETFOCUS", X_FUNCNAME( WGCONTROL_SETFOCUS ) );
      x_clsAdd( s_class, "DESTROY", X_FUNCNAME( WGCONTROL_DESTROY ) );
      x_clsAddAccess( s_class, "TEXT", X_FUNCNAME( WGCONTROL_GETTEXT ) );
      x_clsAddAssign( s_class, "TEXT", X_FUNCNAME( WGCONTROL_SETTEXT ) );
      x_clsAddAccess( s_class, "VALUE", X_FUNCNAME( WGCONTROL_GETVALUE ) );
      x_clsAddAssign( s_class, "VALUE", X_FUNCNAME( WGCONTROL_SETVALUE ) );
      x_clsAddAccess( s_class, "ENABLED", X_FUNCNAME( WGCONTROL_GETENABLED ) );
      x_clsAddAssign( s_class, "ENABLED", X_FUNCNAME( WGCONTROL_SETENABLED ) );
      x_clsAddAccess( s_class, "VISIBLE", X_FUNCNAME( WGCONTROL_GETVISIBLE ) );
      x_clsAddAssign( s_class, "VISIBLE", X_FUNCNAME( WGCONTROL_SETVISIBLE ) );
      x_clsAddAccess( s_class, "X", X_FUNCNAME( WGCONTROL_GETX ) );
      x_clsAddAssign( s_class, "X", X_FUNCNAME( WGCONTROL_SETX ) );
      x_clsAddAccess( s_class, "Y", X_FUNCNAME( WGCONTROL_GETY ) );
      x_clsAddAssign( s_class, "Y", X_FUNCNAME( WGCONTROL_SETY ) );
      x_clsAddAccess( s_class, "WIDTH", X_FUNCNAME( WGCONTROL_GETWIDTH ) );
      x_clsAddAssign( s_class, "WIDTH", X_FUNCNAME( WGCONTROL_SETWIDTH ) );
      x_clsAddAccess( s_class, "HEIGHT", X_FUNCNAME( WGCONTROL_GETHEIGHT ) );
      x_clsAddAssign( s_class, "HEIGHT", X_FUNCNAME( WGCONTROL_SETHEIGHT ) );
      x_clsAddAccess( s_class, "GROW", X_FUNCNAME( WGCONTROL_GETGROW ) );
      x_clsAddAssign( s_class, "GROW", X_FUNCNAME( WGCONTROL_SETGROW ) );

      x_clsAdd( s_class, "__WGCREATE", X_FUNCNAME( WGCONTROL_CREATE ) );
      x_clsAdd( s_class, "__WGEVENT", X_FUNCNAME( WGCONTROL_EVENT ) );
      x_clsAdd( s_class, "__WGCOLUMN", X_FUNCNAME( WGCONTROL_COLUMN ) );
      x_clsAdd( s_class, "__WGROWS", X_FUNCNAME( WGCONTROL_ROWS ) );
      x_clsAdd( s_class, "__WGREFRESH", X_FUNCNAME( WGCONTROL_REFRESH ) );
      x_clsAdd( s_class, "__WGITEM", X_FUNCNAME( WGCONTROL_ITEM ) );
      x_clsAdd( s_class, "__WGACTION", X_FUNCNAME( WGCONTROL_ACTION ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   wgLabel
   --------------------------------------------------------------------------------------------- */

// Create a label in its parent, a window or a box
// wgLabel():New( oParent, [cText] ) --> self
X_FUNC_STATIC( WGLABEL_NEW )
{
   if( !x_webguiParentValid( ctx ) || ( !X_ISNIL( ctx, 2 ) && !X_ISCHAR( ctx, 2 ) ) )
   {
      x_webguiRaise( ctx, "WGLABEL:NEW" );
      return;
   }

   x_webguiControlSend( ctx, "label", x_paritem( ctx, 2 ), NULL, NULL );
}

// Text on the page
// wgLabel() --> oLabel
X_FUNC( WGLABEL )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 0, "WGLABEL" );
      x_clsAddParent( s_class, x_clsRequire( "WGCONTROL", WGCONTROL ) );

      x_clsAdd( s_class, "NEW", X_FUNCNAME( WGLABEL_NEW ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   wgButton
   --------------------------------------------------------------------------------------------- */

// Create a button in its parent; OnClick runs when the user clicks it
// wgButton():New( oParent, [cText] ) --> self
X_FUNC_STATIC( WGBUTTON_NEW )
{
   if( !x_webguiParentValid( ctx ) || ( !X_ISNIL( ctx, 2 ) && !X_ISCHAR( ctx, 2 ) ) )
   {
      x_webguiRaise( ctx, "WGBUTTON:NEW" );
      return;
   }

   x_webguiControlSend( ctx, "button", x_paritem( ctx, 2 ), NULL, NULL );
}

// Push button
// wgButton() --> oButton
X_FUNC( WGBUTTON )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 0, "WGBUTTON" );
      x_clsAddParent( s_class, x_clsRequire( "WGCONTROL", WGCONTROL ) );

      x_clsAdd( s_class, "NEW", X_FUNCNAME( WGBUTTON_NEW ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   wgEdit
   --------------------------------------------------------------------------------------------- */

// Create an edit field of one line in its parent with cLabel above it, its :text; :value is the text of the field,
// OnChange runs at every change by the user
// wgEdit():New( oParent, [cValue], [cLabel] ) --> self
X_FUNC_STATIC( WGEDIT_NEW )
{
   X_ITEM empty __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   int valid = x_webguiParentValid( ctx ) && ( X_ISNIL( ctx, 2 ) || X_ISCHAR( ctx, 2 ) );

   if( !valid || ( !X_ISNIL( ctx, 3 ) && !X_ISCHAR( ctx, 3 ) ) )
   {
      x_webguiRaise( ctx, "WGEDIT:NEW" );
      return;
   }

   x_itemPutC( &empty, "" );
   x_webguiControlSend( ctx, "edit", x_paritem( ctx, 3 ), IIF( X_ISNIL( ctx, 2 ), &empty, x_paritem( ctx, 2 ) ), NULL );
}

// Edit field of one line
// wgEdit() --> oEdit
X_FUNC( WGEDIT )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 0, "WGEDIT" );
      x_clsAddParent( s_class, x_clsRequire( "WGCONTROL", WGCONTROL ) );

      x_clsAdd( s_class, "NEW", X_FUNCNAME( WGEDIT_NEW ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   wgCheckBox
   --------------------------------------------------------------------------------------------- */

// Create a check box in its parent with cText at its right; :value is .T. when it is checked, OnChange runs when
// the user changes it
// wgCheckBox():New( oParent, [cText], [lChecked] ) --> self
X_FUNC_STATIC( WGCHECKBOX_NEW )
{
   X_ITEM checked __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   int valid = x_webguiParentValid( ctx ) && ( X_ISNIL( ctx, 2 ) || X_ISCHAR( ctx, 2 ) );

   if( !valid || ( !X_ISNIL( ctx, 3 ) && !X_ISLOG( ctx, 3 ) ) )
   {
      x_webguiRaise( ctx, "WGCHECKBOX:NEW" );
      return;
   }

   x_itemPutL( &checked, X_ISLOG( ctx, 3 ) && x_parl( ctx, 3 ) );
   x_webguiControlSend( ctx, "check", x_paritem( ctx, 2 ), &checked, NULL );
}

// Check box
// wgCheckBox() --> oCheckBox
X_FUNC( WGCHECKBOX )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 0, "WGCHECKBOX" );
      x_clsAddParent( s_class, x_clsRequire( "WGCONTROL", WGCONTROL ) );

      x_clsAdd( s_class, "NEW", X_FUNCNAME( WGCHECKBOX_NEW ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   wgComboBox
   --------------------------------------------------------------------------------------------- */

// Create a drop-down list of the texts of aItems in its parent with cLabel above it, its :text; :value is the
// number of the chosen item, nSelected or 1 at the start; OnChange runs when the user chooses another item
// wgComboBox():New( oParent, aItems, [cLabel], [nSelected] ) --> self
X_FUNC_STATIC( WGCOMBOBOX_NEW )
{
   X_ITEM selected __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   int valid = x_webguiParentValid( ctx ) && X_ISARRAY( ctx, 2 ) && ( X_ISNIL( ctx, 3 ) || X_ISCHAR( ctx, 3 ) );

   if( !valid || ( !X_ISNIL( ctx, 4 ) && !X_ISNUM( ctx, 4 ) ) )
   {
      x_webguiRaise( ctx, "WGCOMBOBOX:NEW" );
      return;
   }

   x_itemPutNI( &selected, x_parnidef( ctx, 4, 1 ) );
   x_webguiControlSend( ctx, "combo", x_paritem( ctx, 3 ), &selected, x_paritem( ctx, 2 ) );
}

// Drop-down list
// wgComboBox() --> oComboBox
X_FUNC( WGCOMBOBOX )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 0, "WGCOMBOBOX" );
      x_clsAddParent( s_class, x_clsRequire( "WGCONTROL", WGCONTROL ) );

      x_clsAdd( s_class, "NEW", X_FUNCNAME( WGCOMBOBOX_NEW ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   wgBrowse
   --------------------------------------------------------------------------------------------- */

// Create a browse in its parent, a table of rows with a heading that stays at the top, 200 high unless :grow or
// :height says otherwise; :value is the number of the current row; OnChange runs when the user moves to another
// row, OnClick on a double click or Enter; a click on a heading sorts the rows
// wgBrowse():New( oParent ) --> self
X_FUNC_STATIC( WGBROWSE_NEW )
{
   X_ITEM none __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;

   if( !x_webguiParentValid( ctx ) )
   {
      x_webguiRaise( ctx, "WGBROWSE:NEW" );
      return;
   }

   x_itemPutNI( &none, 0 );
   x_webguiControlSend( ctx, "browse", NULL, &none, NULL );
}

// Add a column; bBlock receives the row, an element of the array of SetArray(), and its number and returns the
// value of the cell, shown through Transform() with cPicture; nWidth in logical pixels; nAlign 0 left, 1 right,
// 2 center, without it a number stands on the right; returns the number of the column
// oBrowse:AddColumn( cHeading, bBlock, [nWidth], [cPicture], [nAlign] ) --> nColumn
X_FUNC_STATIC( WGBROWSE_ADDCOLUMN )
{
   int valid = X_ISCHAR( ctx, 1 ) && X_ISBLOCK( ctx, 2 );

   valid = valid && ( X_ISNIL( ctx, 3 ) || ( X_ISNUM( ctx, 3 ) && x_parni( ctx, 3 ) > 0 ) );
   valid = valid && ( X_ISNIL( ctx, 4 ) || X_ISCHAR( ctx, 4 ) );
   valid = valid && ( X_ISNIL( ctx, 5 ) || ( X_ISNUM( ctx, 5 ) && x_parni( ctx, 5 ) >= 0 && x_parni( ctx, 5 ) <= 2 ) );
   if( !valid )
   {
      x_webguiRaise( ctx, "WGBROWSE:ADDCOLUMN" );
      return;
   }

   x_webguiForward( ctx, "__WGCOLUMN", 5 );
}

// Show the rows of aRows, any values the blocks of the columns read; the first row becomes the current one; the
// browse keeps the array itself, Refresh() shows its later changes
// oBrowse:SetArray( aRows ) --> self
X_FUNC_STATIC( WGBROWSE_SETARRAY )
{
   if( !X_ISARRAY( ctx, 1 ) )
   {
      x_webguiRaise( ctx, "WGBROWSE:SETARRAY" );
      return;
   }

   x_webguiForward( ctx, "__WGROWS", 1 );
}

// Show the rows again, e.g. after the array changed
// oBrowse:Refresh() --> self
X_FUNC_STATIC( WGBROWSE_REFRESH )
{
   x_webguiForward( ctx, "__WGREFRESH", 0 );
}

// Table of rows drawn by the page; the program keeps the rows and makes the texts of the cells
// wgBrowse() --> oBrowse
X_FUNC( WGBROWSE )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 0, "WGBROWSE" );
      x_clsAddParent( s_class, x_clsRequire( "WGCONTROL", WGCONTROL ) );

      x_clsAdd( s_class, "NEW", X_FUNCNAME( WGBROWSE_NEW ) );
      x_clsAdd( s_class, "ADDCOLUMN", X_FUNCNAME( WGBROWSE_ADDCOLUMN ) );
      x_clsAdd( s_class, "SETARRAY", X_FUNCNAME( WGBROWSE_SETARRAY ) );
      x_clsAdd( s_class, "REFRESH", X_FUNCNAME( WGBROWSE_REFRESH ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   wgMenu
   --------------------------------------------------------------------------------------------- */

// Create a menu of the menu bar at the top of the window; a click on cTitle opens its items; a menu without items
// is a command of the bar: a click runs bAction, its OnClick
// wgMenu():New( oWindow, cTitle, [bAction] ) --> self
X_FUNC_STATIC( WGMENU_NEW )
{
   const X_ITEM *parent = x_paritem( ctx, 1 );
   int valid = parent != NULL && X_IS_OBJECT( parent ) && x_objIsKindOf( parent, "WGWINDOW" );

   if( !valid || !X_ISCHAR( ctx, 2 ) || ( !X_ISNIL( ctx, 3 ) && !X_ISBLOCK( ctx, 3 ) ) )
   {
      x_webguiRaise( ctx, "WGMENU:NEW" );
      return;
   }

   x_webguiControlSend( ctx, "menu", x_paritem( ctx, 2 ), x_paritem( ctx, 3 ), NULL );
}

// Add an item; bAction runs on its click and receives the menu; cShortcut, e.g. "Ctrl+N", "F2", "Del", stands at
// the right of the item and runs it from the keyboard
// oMenu:Add( cText, bAction, [cShortcut] ) --> self
X_FUNC_STATIC( WGMENU_ADD )
{
   if( !X_ISCHAR( ctx, 1 ) || !X_ISBLOCK( ctx, 2 ) || ( !X_ISNIL( ctx, 3 ) && !X_ISCHAR( ctx, 3 ) ) )
   {
      x_webguiRaise( ctx, "WGMENU:ADD" );
      return;
   }

   x_webguiForward( ctx, "__WGITEM", 3 );
}

// Add a line between items
// oMenu:AddSeparator() --> self
X_FUNC_STATIC( WGMENU_ADDSEPARATOR )
{
   x_webguiForward( ctx, "__WGITEM", 3 );
}

// Menu of the menu bar of a wgWindow; the arrows, Esc, Home and End move in the menus
// wgMenu() --> oMenu
X_FUNC( WGMENU )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 0, "WGMENU" );
      x_clsAddParent( s_class, x_clsRequire( "WGCONTROL", WGCONTROL ) );

      x_clsAdd( s_class, "NEW", X_FUNCNAME( WGMENU_NEW ) );
      x_clsAdd( s_class, "ADD", X_FUNCNAME( WGMENU_ADD ) );
      x_clsAdd( s_class, "ADDSEPARATOR", X_FUNCNAME( WGMENU_ADDSEPARATOR ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   wgGet
   --------------------------------------------------------------------------------------------- */

// Create a GET for xValue, text, a number, a date or a logical value, edited through cPicture: for text 9 a digit,
// A a letter, ! a letter in capitals, X any character, other characters stay, "@!" makes all letters capitals; for
// a number e.g. "999,999.99" with text before and after; for a date DD, MM and YYYY, "DD.MM.YYYY" by default; for a
// logical value "Yes/No", ".T./.F." by default; cLabel stands above the field, its :text; Enter and leaving the
// field keep the value in :value and run OnChange, Enter goes to the next field; a value that does not fit the
// picture is marked and not kept
// wgGet():New( oParent, xValue, [cPicture], [cLabel] ) --> self
X_FUNC_STATIC( WGGET_NEW )
{
   int valid = x_webguiParentValid( ctx ) && ( X_ISNIL( ctx, 3 ) || X_ISCHAR( ctx, 3 ) );

   valid = valid && ( X_ISCHAR( ctx, 2 ) || X_ISNUM( ctx, 2 ) || X_ISDATE( ctx, 2 ) || X_ISLOG( ctx, 2 ) );
   if( !valid || ( !X_ISNIL( ctx, 4 ) && !X_ISCHAR( ctx, 4 ) ) )
   {
      x_webguiRaise( ctx, "WGGET:NEW" );
      return;
   }

   x_webguiControlSend( ctx, "get", x_paritem( ctx, 4 ), x_paritem( ctx, 2 ), x_paritem( ctx, 3 ) );
}

// Field with a picture, as GET of xBase; VALID {| oGet, xValue | lValid } checks a new value before :value keeps it,
// a refused value stays marked in the field, which gets the focus back; WHEN {| oGet | lWhen } decides whether the
// field can be entered, otherwise the focus goes on to the next field
// wgGet() --> oGet
X_FUNC( WGGET )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 2, "WGGET" );
      x_clsAddParent( s_class, x_clsRequire( "WGCONTROL", WGCONTROL ) );
      x_clsAddData( s_class, "VALID", 1 );
      x_clsSetDataType( s_class, "VALID", X_OO_VALUE_CODEBLOCK );
      x_clsAddData( s_class, "WHEN", 2 );
      x_clsSetDataType( s_class, "WHEN", X_OO_VALUE_CODEBLOCK );

      x_clsAdd( s_class, "NEW", X_FUNCNAME( WGGET_NEW ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   wgPanel
   --------------------------------------------------------------------------------------------- */

// A box: a control holding controls, the parent given to their New(); it places them one after another
// wgBox() --> oBox
X_FUNC( WGBOX )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 0, "WGBOX" );
      x_clsAddParent( s_class, x_clsRequire( "WGCONTROL", WGCONTROL ) );
   }

   x_clsReturn( ctx, s_class );
}

// Panel of a page of a sidebar, made by wgSidebar:Add(); it places its controls one under another, as a column
// wgPanel() --> oPanel
X_FUNC( WGPANEL )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 0, "WGPANEL" );
      x_clsAddParent( s_class, x_clsRequire( "WGBOX", WGBOX ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   wgColumn, wgRow
   --------------------------------------------------------------------------------------------- */

// Create a column in its parent: its controls stand one under another, as wide as the column; a window, a panel
// and a part of a split are columns too
// wgColumn():New( oParent ) --> self
X_FUNC_STATIC( WGCOLUMN_NEW )
{
   if( !x_webguiParentValid( ctx ) )
   {
      x_webguiRaise( ctx, "WGCOLUMN:NEW" );
      return;
   }

   x_webguiControlSend( ctx, "column", NULL, NULL, NULL );
}

// Box of controls one under another
// wgColumn() --> oColumn
X_FUNC( WGCOLUMN )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 0, "WGCOLUMN" );
      x_clsAddParent( s_class, x_clsRequire( "WGBOX", WGBOX ) );

      x_clsAdd( s_class, "NEW", X_FUNCNAME( WGCOLUMN_NEW ) );
   }

   x_clsReturn( ctx, s_class );
}

// Create a row in its parent: its controls stand one beside another, on a common bottom line, so fields with a
// label above them and buttons stand in line
// wgRow():New( oParent ) --> self
X_FUNC_STATIC( WGROW_NEW )
{
   if( !x_webguiParentValid( ctx ) )
   {
      x_webguiRaise( ctx, "WGROW:NEW" );
      return;
   }

   x_webguiControlSend( ctx, "row", NULL, NULL, NULL );
}

// Box of controls one beside another
// wgRow() --> oRow
X_FUNC( WGROW )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 0, "WGROW" );
      x_clsAddParent( s_class, x_clsRequire( "WGBOX", WGBOX ) );

      x_clsAdd( s_class, "NEW", X_FUNCNAME( WGROW_NEW ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   wgSplit
   --------------------------------------------------------------------------------------------- */

// Create a split in its parent: two parts, columns, side by side with a line between them, which the mouse drags;
// the split takes the room its parent leaves; :value is the width of the left part in logical pixels, nPosition or
// 300 at the start; OnChange runs when the user drags the line
// wgSplit():New( oParent, [nPosition] ) --> self
X_FUNC_STATIC( WGSPLIT_NEW )
{
   X_ITEM position __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM first    __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM second   __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_CLASS_HANDLE column = x_clsRequire( "WGCOLUMN", WGCOLUMN );

   if( !x_webguiParentValid( ctx ) || ( !X_ISNIL( ctx, 2 ) && !( X_ISNUM( ctx, 2 ) && x_parni( ctx, 2 ) >= 0 ) ) )
   {
      x_webguiRaise( ctx, "WGSPLIT:NEW" );
      return;
   }

   x_itemPutNI( &position, x_parnidef( ctx, 2, 300 ) );
   x_webguiControlSend( ctx, "split", NULL, &position, NULL );
   if( x_objCreate( ctx, &first, column ) && x_objCreate( ctx, &second, column ) )
      x_webguiSplitParts( ctx, &first, &second );
}

// Return part nPart of the split, a column: 1 at the left, 2 at the right
// oSplit:Panel( nPart ) --> oColumn
X_FUNC_STATIC( WGSPLIT_PANEL )
{
   x_webguiSplitPanel( ctx, x_parni( ctx, 1 ) );
}

// Two columns with a line between them that the mouse drags
// wgSplit() --> oSplit
X_FUNC( WGSPLIT )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 1, "WGSPLIT" );
      x_clsAddParent( s_class, x_clsRequire( "WGCONTROL", WGCONTROL ) );
      x_clsAddData( s_class, "APARTS", 1 );
      x_clsSetScope( s_class, "APARTS", X_OO_CLSTP_HIDDEN );

      x_clsAdd( s_class, "NEW", X_FUNCNAME( WGSPLIT_NEW ) );
      x_clsAdd( s_class, "PANEL", X_FUNCNAME( WGSPLIT_PANEL ) );
   }

   x_clsReturn( ctx, s_class );
}

/* ---------------------------------------------------------------------------------------------
   wgSidebar
   --------------------------------------------------------------------------------------------- */

// Create a sidebar at the left edge of the window: a strip of page pictures with buttons at its bottom and the panel
// of the shown page, nWidth wide (260 by default) and wider or narrower when the mouse drags its right edge; the
// controls of the window stand at its right; :value is the number of the shown page, 0 when the panel is hidden
// wgSidebar():New( oWindow, [nWidth] ) --> self
X_FUNC_STATIC( WGSIDEBAR_NEW )
{
   X_ITEM none __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   const X_ITEM *parent = x_paritem( ctx, 1 );
   int valid = parent != NULL && X_IS_OBJECT( parent ) && x_objIsKindOf( parent, "WGWINDOW" );

   if( !valid || ( !X_ISNIL( ctx, 2 ) && !( X_ISNUM( ctx, 2 ) && x_parni( ctx, 2 ) > 0 ) ) )
   {
      x_webguiRaise( ctx, "WGSIDEBAR:NEW" );
      return;
   }

   x_itemPutNI( &none, 0 );
   x_webguiControlSend( ctx, "sidebar", NULL, &none, NULL );
}

// Add a page: cPicture, a PNG, SVG, JPEG, GIF or BMP file, in the strip, cToolTip its tip (cTitle without it) and a
// panel under the header with cTitle; a click on the picture shows the page, a click on the picture of the shown
// page hides the panel; OnChange runs after both; returns the number of the page, counted from 1
// oSidebar:Add( cTitle, cPicture, [cToolTip] ) --> nPage
X_FUNC_STATIC( WGSIDEBAR_ADD )
{
   X_ITEM panel __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   int number;

   if( !X_ISCHAR( ctx, 1 ) || !X_ISCHAR( ctx, 2 ) || ( !X_ISNIL( ctx, 3 ) && !X_ISCHAR( ctx, 3 ) ) )
   {
      x_webguiRaise( ctx, "WGSIDEBAR:ADD" );
      return;
   }

   if( !x_objCreate( ctx, &panel, x_clsRequire( "WGPANEL", WGPANEL ) ) )
      return;

   number = x_webguiSidebarAdd( ctx, &panel );
   if( number != 0 )
      x_retni( ctx, number );
}

// Return the panel of page nPage; controls are created on it as on a window
// oSidebar:Panel( nPage ) --> oPanel
X_FUNC_STATIC( WGSIDEBAR_PANEL )
{
   x_webguiSidebarPanel( ctx, x_parni( ctx, 1 ) );
}

// Add a small button with cPicture at the right of the header of page nPage, shown with the page; bAction runs on
// its click and receives the sidebar
// oSidebar:AddAction( nPage, cPicture, bAction, [cToolTip] ) --> self
X_FUNC_STATIC( WGSIDEBAR_ADDACTION )
{
   const X_ITEM *pages = x_objDataSlot( ctx, 1 );
   int count = IIF( pages != NULL && X_IS_ARRAY( pages ), ( int )x_arrayLen( pages ), 0 );
   int valid = X_ISNUM( ctx, 1 ) && x_parni( ctx, 1 ) >= 1 && x_parni( ctx, 1 ) <= count;

   valid = valid && X_ISCHAR( ctx, 2 ) && X_ISBLOCK( ctx, 3 ) && ( X_ISNIL( ctx, 4 ) || X_ISCHAR( ctx, 4 ) );
   if( !valid )
   {
      x_webguiRaise( ctx, "WGSIDEBAR:ADDACTION" );
      return;
   }

   x_webguiSidebarButton( ctx, 2 );
}

// Add a button with cPicture at the bottom of the strip, e.g. for the settings; bAction runs on its click and
// receives the sidebar
// oSidebar:AddButton( cPicture, bAction, [cToolTip] ) --> self
X_FUNC_STATIC( WGSIDEBAR_ADDBUTTON )
{
   if( !X_ISCHAR( ctx, 1 ) || !X_ISBLOCK( ctx, 2 ) || ( !X_ISNIL( ctx, 3 ) && !X_ISCHAR( ctx, 3 ) ) )
   {
      x_webguiRaise( ctx, "WGSIDEBAR:ADDBUTTON" );
      return;
   }

   x_webguiSidebarButton( ctx, 1 );
}

// Sidebar as in code editors, drawn by the page; the program keeps the pages and the shown one
// wgSidebar() --> oSidebar
X_FUNC( WGSIDEBAR )
{
   static X_CLASS_HANDLE s_class = 0;

   if( s_class == 0 )
   {
      s_class = x_clsCreate( 1, "WGSIDEBAR" );
      x_clsAddParent( s_class, x_clsRequire( "WGCONTROL", WGCONTROL ) );
      x_clsAddData( s_class, "APAGES", 1 );
      x_clsSetScope( s_class, "APAGES", X_OO_CLSTP_HIDDEN );

      x_clsAdd( s_class, "NEW",       X_FUNCNAME( WGSIDEBAR_NEW ) );
      x_clsAdd( s_class, "ADD",       X_FUNCNAME( WGSIDEBAR_ADD ) );
      x_clsAdd( s_class, "PANEL",     X_FUNCNAME( WGSIDEBAR_PANEL ) );
      x_clsAdd( s_class, "ADDACTION", X_FUNCNAME( WGSIDEBAR_ADDACTION ) );
      x_clsAdd( s_class, "ADDBUTTON", X_FUNCNAME( WGSIDEBAR_ADDBUTTON ) );
   }

   x_clsReturn( ctx, s_class );
}
