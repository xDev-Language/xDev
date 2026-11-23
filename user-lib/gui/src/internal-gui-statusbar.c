/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev GUI
 * File......: internal-gui-statusbar.c
 *
 */

#include "internal-gui.h"

/* ---------------------------------------------------------------------------------------------
   Status bars
   --------------------------------------------------------------------------------------------- */

/* gui_StatusBar():New(): a status bar at the bottom of the window */
void x_guiStatusBarNew( X_CALL_CTX *ctx )
{
   x_guiControlSend( ctx, X_GUI_STATUSBAR, 2, 0, 0, 0, 0 );
}

/* The text of a panel with its border; NULL keeps the text the panel has */
void x_guiStatusSetText( HWND status, int index, int style, const wchar_t *text )
{
   wchar_t *current = NULL;

   if( text == NULL )
   {
      size_t length = LOWORD( SendMessageW( status, SB_GETTEXTLENGTHW, ( WPARAM )index, 0 ) );

      current = ( wchar_t * )x_xgrab( ( length + 1 ) * sizeof( wchar_t ) );
      current[0] = L'\0';
      SendMessageW( status, SB_GETTEXTW, ( WPARAM )index, ( LPARAM )current );
   }

   SendMessageW( status, SB_SETTEXTW, ( WPARAM )( index | style ), ( LPARAM )IIF( text != NULL, text, current ) );
   x_xfree( current );
}

/* Add a panel; width 0 shares the rest of the bar; returns the number of panels, 0 with an error raised */
int x_guiStatusBarAdd( X_CALL_CTX *ctx, HWND status, int width, const X_ITEM *text, int raised )
{
   X_GUI_STATUS *parts = x_guiStatusOf( status );
   wchar_t *label = NULL;

   if( width < 0 || ( parts != NULL && parts->count >= 255 ) )
   {
      x_guiRaise( ctx, "GUISTATUSBAR:ADD" );
      return 0;
   }

   if( text != NULL && !X_IS_NIL( text ) )
   {
      label = x_guiText( ctx, text, "GUISTATUSBAR:ADD" );
      if( label == NULL )
         return 0;
   }

   parts = x_guiStatusParts( status );
   parts->widths = ( int * )x_xrealloc( parts->widths, ( parts->count + 1 ) * sizeof( int ) );
   parts->styles = ( int * )x_xrealloc( parts->styles, ( parts->count + 1 ) * sizeof( int ) );
   parts->widths[parts->count] = width;
   parts->styles[parts->count] = IIF( raised, SBT_POPOUT, 0 );
   ++parts->count;
   x_callbackEnterDll();
   x_guiStatusLayout( status );
   x_guiStatusSetText( status, ( int )parts->count - 1, parts->styles[parts->count - 1], label );
   x_callbackLeaveDll();
   x_xfree( label );
   x_callbackRaisePending();

   return ( int )parts->count;
}

/* Set the text of a panel, counted from 1 */
void x_guiStatusBarSetText( X_CALL_CTX *ctx, HWND status, int panel, const X_ITEM *text )
{
   const X_GUI_STATUS *parts = x_guiStatusOf( status );
   int own = parts != NULL && parts->count > 0;
   size_t panels = IIF( own, parts->count, 1 );
   wchar_t *label;
   int style;

   if( panel < 1 || ( size_t )panel > panels )
   {
      x_guiRaise( ctx, "GUISTATUSBAR:SETTEXT" );
      return;
   }

   label = x_guiText( ctx, text, "GUISTATUSBAR:SETTEXT" );
   if( label == NULL )
      return;

   style = IIF( own, parts->styles[panel - 1], 0 );
   x_callbackEnterDll();
   x_guiStatusSetText( status, panel - 1, style, label );
   x_callbackLeaveDll();
   x_xfree( label );
   x_callbackRaisePending();
}
