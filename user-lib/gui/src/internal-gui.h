/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library: xDev GUI
 * File: internal-gui.h
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef INTERNAL_GUI_H
#define INTERNAL_GUI_H

#ifndef UNICODE
   #define UNICODE
#endif
#ifndef _UNICODE
   #define _UNICODE
#endif

#include <windows.h>
#include <commctrl.h>

#include "platform.h"
#include "internal.h"
#include "classes.h"

#if !X_OS_WINDOWS
   #error "xDev GUI requires Windows"
#endif

/* ---------------------------------------------------------------------------------------------
   Types
   --------------------------------------------------------------------------------------------- */

/* Kinds of controls */
typedef enum
{
   X_GUI_LABEL = 1,
   X_GUI_EDIT,
   X_GUI_MEMO,
   X_GUI_BUTTON,
   X_GUI_CHECKBOX,
   X_GUI_RADIO,
   X_GUI_GROUPBOX,
   X_GUI_LISTBOX,
   X_GUI_COMBOBOX,
   X_GUI_PROGRESSBAR,
   X_GUI_STATUSBAR,
   X_GUI_DATEPICKER,
   X_GUI_SLIDER,
   X_GUI_TABCONTROL,
   X_GUI_LISTVIEW,
   X_GUI_TREEVIEW,
   X_GUI_TOOLBAR,
   X_GUI_BROWSE,
   X_GUI_GETMEMO
} X_GUI_KIND;

/* Colors of a window or a control */
typedef enum
{
   X_GUI_COLOR_FOREGROUND,
   X_GUI_COLOR_BACKGROUND,
   X_GUI_COLOR_HOVERFOREGROUND,
   X_GUI_COLOR_HOVERBACKGROUND,
   X_GUI_COLOR_PRESSFOREGROUND,
   X_GUI_COLOR_PRESSBACKGROUND,
   X_GUI_COLOR_COUNT
} X_GUI_COLOR;

/* Colors of a browse */
typedef enum
{
   X_GUI_GRID_HEADBACKGROUND,
   X_GUI_GRID_HEADFOREGROUND,
   X_GUI_GRID_HEADLINE,
   X_GUI_GRID_BACKGROUND,
   X_GUI_GRID_ALTERNATEBACKGROUND,
   X_GUI_GRID_FOREGROUND,
   X_GUI_GRID_LINE,
   X_GUI_GRID_CURSORBACKGROUND,
   X_GUI_GRID_CURSORFOREGROUND,
   X_GUI_GRID_INACTIVECURSOR,
   X_GUI_GRID_CHECKFOREGROUND,
   X_GUI_GRID_COLORS
} X_GUI_GRID_COLOR;

/* Kinds of windows */
typedef enum
{
   X_GUI_WINDOW_TOP,
   X_GUI_WINDOW_DIALOG,
   X_GUI_WINDOW_PAGE,
   X_GUI_WINDOW_PANEL
} X_GUI_WINDOW_KIND;

/* Values of a slider */
typedef enum
{
   X_GUI_RANGE_VALUE,
   X_GUI_RANGE_MINIMUM,
   X_GUI_RANGE_MAXIMUM
} X_GUI_RANGE;

/* Position and size */
typedef enum
{
   X_GUI_X,
   X_GUI_Y,
   X_GUI_WIDTH,
   X_GUI_HEIGHT
} X_GUI_BOUND;

/* Kinds of message boxes */
typedef enum
{
   X_GUI_MESSAGE_INFO = 1,
   X_GUI_MESSAGE_WARNING,
   X_GUI_MESSAGE_ERROR,
   X_GUI_MESSAGE_YESNO
} X_GUI_MESSAGE;

/* ---------------------------------------------------------------------------------------------
   Arguments, results and errors
   --------------------------------------------------------------------------------------------- */

void x_guiRaise( X_CALL_CTX *ctx, const char *operation );
void x_guiRaiseWin32( X_CALL_CTX *ctx, const char *operation, DWORD error );
wchar_t *x_guiText( X_CALL_CTX *ctx, const X_ITEM *item, const char *operation );
void x_guiReturnSelf( X_CALL_CTX *ctx );
void x_guiReturnWindowText( X_CALL_CTX *ctx, HWND window );
HWND x_guiSelfHandle( X_CALL_CTX *ctx, const char *operation );
HWND x_guiObjectWindow( X_CALL_CTX *ctx, const X_ITEM *object, const char *operation );
HWND x_guiObjectControl( X_CALL_CTX *ctx, const X_ITEM *object, const char *operation );
HMENU x_guiObjectMenu( X_CALL_CTX *ctx, const X_ITEM *object, const char *operation );
void x_guiEvent( X_CALL_CTX *ctx, size_t slot );

/* ---------------------------------------------------------------------------------------------
   Windows
   --------------------------------------------------------------------------------------------- */

void x_guiWindowNew( X_CALL_CTX *ctx );
void x_guiWindowCreate( X_CALL_CTX *ctx );
void x_guiDialogNew( X_CALL_CTX *ctx );
void x_guiPanelNew( X_CALL_CTX *ctx );
void x_guiWindowSetKey( X_CALL_CTX *ctx, HWND window, X_ITEM *blocks );
void x_guiWindowFocusNext( HWND window, int previous );
int x_guiWindowRead( HWND window, const HWND *gets, size_t count, int first );
void x_guiWindowEndRead( int save );
int x_guiWindowControl( X_CALL_CTX *ctx, HWND window, const char *name );
void x_guiDialogRun( X_CALL_CTX *ctx, HWND dialog );
void x_guiWindowSetMenu( X_CALL_CTX *ctx, HWND window, HMENU menu );
int x_guiWindowSizable( HWND window );
void x_guiWindowSetSizable( HWND window, int sizable );
void x_guiWindowRunMenu( X_CALL_CTX *ctx, int id );
int x_guiClientSize( HWND window, X_GUI_BOUND part );
void x_guiSetClientSize( X_CALL_CTX *ctx, HWND window, X_GUI_BOUND part, int value );
void x_guiWindowColor( X_CALL_CTX *ctx, X_GUI_COLOR part );
void x_guiWindowSetColor( X_CALL_CTX *ctx, X_GUI_COLOR part );
void x_guiCenter( HWND window );
void x_guiShow( HWND window, int visible );
void x_guiDestroy( HWND window );
void x_guiRun( X_CALL_CTX *ctx, HWND window );

/* ---------------------------------------------------------------------------------------------
   Menus
   --------------------------------------------------------------------------------------------- */

HMENU x_guiMenuCreate( X_CALL_CTX *ctx, int popup );
int x_guiMenuAdd( X_CALL_CTX *ctx, HMENU menu, const X_ITEM *text, const X_ITEM *action, const X_ITEM *shortcut,
                  const X_ITEM *image, const X_ITEM *when );
void x_guiWindowMenuWhen( X_CALL_CTX *ctx, HMENU menu );
void x_guiMenuAddMenu( X_CALL_CTX *ctx, HMENU menu, const X_ITEM *text, HMENU submenu, const X_ITEM *image );
void x_guiMenuAddSubmenu( X_CALL_CTX *ctx, HMENU menu, const X_ITEM *text, const X_ITEM *image );
int x_guiMenuStyle( int extended, int imageSize );
void x_guiMenuSeparator( X_CALL_CTX *ctx, HMENU menu );
void x_guiSetContextMenu( X_CALL_CTX *ctx, size_t slot, const char *operation );

/* ---------------------------------------------------------------------------------------------
   Controls
   --------------------------------------------------------------------------------------------- */

void x_guiControlNew( X_CALL_CTX *ctx, X_GUI_KIND kind, int width, int height, int hasText );
void x_guiGetNew( X_CALL_CTX *ctx );
void x_guiSayNew( X_CALL_CTX *ctx );
void x_guiStatusBarNew( X_CALL_CTX *ctx );
int x_guiStatusBarAdd( X_CALL_CTX *ctx, HWND status, int width, const X_ITEM *text, int raised );
void x_guiStatusBarSetText( X_CALL_CTX *ctx, HWND status, int panel, const X_ITEM *text );
void x_guiControlCreate( X_CALL_CTX *ctx );
void x_guiControlDestroy( HWND control );
int x_guiLabelBorder( HWND label );
void x_guiLabelSetBorder( HWND label, int border );
void x_guiLabelAddLineAfter( HWND label, int color );
int x_guiLabelAlign( HWND label );
void x_guiLabelSetAlign( HWND label, int align );
void x_guiControlDetach( X_CALL_CTX *ctx );
void x_guiWindowDetach( X_CALL_CTX *ctx );
void x_guiSetText( X_CALL_CTX *ctx, HWND window, const X_ITEM *text );
void x_guiSetFont( X_CALL_CTX *ctx, HWND window, const X_ITEM *font );
int x_guiFocused( HWND window );
void x_guiSetVisible( HWND window, int visible );
void x_guiSetEnabled( HWND window, int enabled );
void x_guiSetFocus( HWND window );
int x_guiBounds( HWND window, X_GUI_BOUND part );
void x_guiSetBounds( X_CALL_CTX *ctx, HWND window, X_GUI_BOUND part, int value );
void x_guiSetToolTip( X_CALL_CTX *ctx, HWND control, const X_ITEM *text );
int x_guiReadOnly( HWND window );
void x_guiSetReadOnly( HWND window, int readOnly );
int x_guiMaxLength( HWND window );
void x_guiSetMaxLength( HWND window, int length );
void x_guiGetAnchor( X_CALL_CTX *ctx, HWND window );
void x_guiSetAnchor( X_CALL_CTX *ctx, HWND window, const X_ITEM *anchor );
void x_guiEditPicture( X_CALL_CTX *ctx, HWND edit );
void x_guiEditSetPicture( X_CALL_CTX *ctx, HWND edit, const X_ITEM *picture );
void x_guiEditValue( X_CALL_CTX *ctx, HWND edit );
void x_guiEditSetValue( X_CALL_CTX *ctx, HWND edit, const X_ITEM *value );
int x_guiGetCheck( HWND window );
void x_guiSetCheck( HWND window, int checked );
int x_guiButtonRole( HWND button, int cancel );
void x_guiSetButtonRole( HWND button, int cancel, int enabled );

/* ---------------------------------------------------------------------------------------------
   Lists
   --------------------------------------------------------------------------------------------- */

int x_guiListAdd( X_CALL_CTX *ctx, HWND window, const X_ITEM *text );
void x_guiListClear( HWND window );
int x_guiListCount( HWND window );
int x_guiListSelected( HWND window );
void x_guiListSetSelected( X_CALL_CTX *ctx, HWND window, int index );
void x_guiListItem( X_CALL_CTX *ctx, HWND window, int index );

/* ---------------------------------------------------------------------------------------------
   Progress bars
   --------------------------------------------------------------------------------------------- */

int x_guiProgressValue( HWND window );
void x_guiProgressSetValue( HWND window, int value );
int x_guiProgressMaximum( HWND window );
void x_guiProgressSetMaximum( HWND window, int maximum );

/* ---------------------------------------------------------------------------------------------
   Date pickers and sliders
   --------------------------------------------------------------------------------------------- */

long x_guiDateValue( HWND window );
int x_guiDateSetValue( HWND window, long julian );
int x_guiSliderGet( HWND window, X_GUI_RANGE part );
void x_guiSliderSet( HWND window, X_GUI_RANGE part, int value );

/* ---------------------------------------------------------------------------------------------
   Tab controls
   --------------------------------------------------------------------------------------------- */

void x_guiTabAddPage( X_CALL_CTX *ctx, HWND tab, const X_ITEM *title, X_ITEM *page );
void x_guiTabRefresh( HWND tab );
void x_guiTabPage( X_CALL_CTX *ctx, HWND tab, int index );
int x_guiTabCount( HWND tab );
int x_guiTabSelected( HWND tab );
void x_guiTabSelect( X_CALL_CTX *ctx, HWND tab, int index );

/* ---------------------------------------------------------------------------------------------
   List views
   --------------------------------------------------------------------------------------------- */

int x_guiListViewAddColumn( X_CALL_CTX *ctx, HWND window, const X_ITEM *title, int width, int align );
int x_guiListViewColumnCount( HWND window );
int x_guiListViewAddRow( X_CALL_CTX *ctx, HWND window, const X_ITEM *values );
void x_guiListViewCell( X_CALL_CTX *ctx, HWND window, int row, int column );
void x_guiListViewSetCell( X_CALL_CTX *ctx, HWND window, int row, int column, const X_ITEM *text );
void x_guiListViewDelete( X_CALL_CTX *ctx, HWND window, int row );
void x_guiListViewClear( HWND window );
int x_guiListViewCount( HWND window );
int x_guiListViewSelected( HWND window );
void x_guiListViewSetSelected( X_CALL_CTX *ctx, HWND window, int row );

/* ---------------------------------------------------------------------------------------------
   Tree views
   --------------------------------------------------------------------------------------------- */

void x_guiTreeAdd( X_CALL_CTX *ctx, HWND window, const X_ITEM *text, const X_ITEM *parent );
void x_guiTreeText( X_CALL_CTX *ctx, HWND window, const X_ITEM *item );
void x_guiTreeSetText( X_CALL_CTX *ctx, HWND window, const X_ITEM *item, const X_ITEM *text );
void x_guiTreeExpand( X_CALL_CTX *ctx, HWND window, const X_ITEM *item, int expand );
void x_guiTreeSelected( X_CALL_CTX *ctx, HWND window );
void x_guiTreeSelect( X_CALL_CTX *ctx, HWND window, const X_ITEM *item );
void x_guiTreeClear( HWND window );
int x_guiTreeCount( HWND window );

/* ---------------------------------------------------------------------------------------------
   Pictures
   --------------------------------------------------------------------------------------------- */

HBITMAP x_guiPicture( X_CALL_CTX *ctx, const X_ITEM *file, const char *operation );

/* ---------------------------------------------------------------------------------------------
   Toolbars
   --------------------------------------------------------------------------------------------- */

void x_guiToolbarNew( X_CALL_CTX *ctx );
int x_guiToolbarAdd( X_CALL_CTX *ctx, HWND toolbar, const X_ITEM *text, const X_ITEM *bitmap, const X_ITEM *toolTip,
                     const X_ITEM *shortcut, int dropDown );
void x_guiToolbarPopup( HWND toolbar, int button, HMENU menu );
void x_guiToolbarSeparator( X_CALL_CTX *ctx, HWND toolbar );
void x_guiToolbarClear( HWND toolbar );
int x_guiToolbarEnable( HWND toolbar, int button, int enable );
void x_guiStatusBarScoreboard( X_CALL_CTX *ctx );
void x_guiStatusBarSetScoreboard( X_CALL_CTX *ctx );
void x_guiStatusBarColor( X_CALL_CTX *ctx, X_GUI_COLOR part );
void x_guiStatusBarSetColor( X_CALL_CTX *ctx, X_GUI_COLOR part );
void x_guiToolbarColor( X_CALL_CTX *ctx, X_GUI_COLOR part );
void x_guiToolbarSetColor( X_CALL_CTX *ctx, X_GUI_COLOR part );

/* ---------------------------------------------------------------------------------------------
   Browses
   --------------------------------------------------------------------------------------------- */

int x_guiBrowseAddColumn( X_CALL_CTX *ctx, HWND browse, const X_ITEM *title, int width, int align );
int x_guiBrowseTextWidth( const X_ITEM *title, const X_ITEM *value, const char *picture );
void x_guiBrowseColor( X_CALL_CTX *ctx, X_GUI_GRID_COLOR part );
void x_guiBrowseSetColor( X_CALL_CTX *ctx, X_GUI_GRID_COLOR part );
void x_guiBrowseToggle( X_CALL_CTX *ctx, const X_ITEM *columns, int number );
void x_guiBrowseFreeze( X_CALL_CTX *ctx );
void x_guiBrowseSetFreeze( X_CALL_CTX *ctx );
void x_guiBrowseColPos( X_CALL_CTX *ctx );
void x_guiBrowseSetColPos( X_CALL_CTX *ctx );
void x_guiBrowseRefresh( HWND browse );
void x_guiBrowseNavigate( X_CALL_CTX *ctx, const X_ITEM *goTop, const X_ITEM *goBottom, const X_ITEM *skip,
                          const X_ITEM *position, const X_ITEM *empty );
void x_guiBrowseRow( X_CALL_CTX *ctx, const X_ITEM *columns );

/* ---------------------------------------------------------------------------------------------
   Timers
   --------------------------------------------------------------------------------------------- */

UINT_PTR x_guiTimerStart( X_CALL_CTX *ctx, HWND window, UINT_PTR id, int interval );
void x_guiTimerStop( UINT_PTR id );
int x_guiTimerActive( UINT_PTR id );

/* ---------------------------------------------------------------------------------------------
   Dialogs
   --------------------------------------------------------------------------------------------- */

void x_guiMessage( X_CALL_CTX *ctx, X_GUI_MESSAGE kind );
void x_guiFileDialog( X_CALL_CTX *ctx, int save );
void x_guiOpenFile( X_CALL_CTX *ctx );

#endif
