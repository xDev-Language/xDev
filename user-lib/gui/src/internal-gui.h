/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev GUI
 * File......: internal-gui.h
 *
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

/* Colors of a sidebar */
typedef enum
{
   X_GUI_SIDEBAR_COLOR_BAR,
   X_GUI_SIDEBAR_COLOR_ACCENT,
   X_GUI_SIDEBAR_COLOR_COUNT
} X_GUI_SIDEBAR_COLOR;

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

/* Menus drawn in the extended style: a picture strip at the left of drop-down menus and a gradient under the
   commands of the menu bar; the colors of the selection are those of Office XP */
typedef struct
{
   wchar_t *text;
   wchar_t *shortcut;
   HIMAGELIST image;
   int bar;
   int separator;
} X_GUI_MENUITEM;

/* A running timer and its xDev object */
typedef struct
{
   HWND window;
   UINT_PTR id;
   X_ITEM *timer;
} X_GUI_TIMER;

/* Events the library gives to the C functions of its classes; they are not messages of the classes */
typedef enum
{
   X_GUI_EVENT_CREATE,
   X_GUI_EVENT_DETACH,
   X_GUI_EVENT_COMMAND,
   X_GUI_EVENT_NOTIFY,
   X_GUI_EVENT_VALID,
   X_GUI_EVENT_KEY,
   X_GUI_EVENT_MENUWHEN,
   X_GUI_EVENT_POPUP,
   X_GUI_EVENT_NAVIGATE,
   X_GUI_EVENT_ROW,
   X_GUI_EVENT_BROWSE,
   X_GUI_EVENT_TOGGLE,
   X_GUI_EVENT_CHANGE,
   X_GUI_EVENT_TIMER,
   X_GUI_EVENT_ITEM,
   X_GUI_EVENT_COUNT
} X_GUI_EVENT;

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
void x_guiHandlerAdd( X_CLASS_HANDLE classHandle, X_GUI_EVENT event, X_METHODPTR function );
int x_guiHandlerCall( X_ITEM *result, X_ITEM *object, X_GUI_EVENT event, int32_t argc, const X_ARG *args );
int x_guiNotify( X_ITEM *object, const char *message, X_ITEM *result, int argc, const int64_t *values );
int x_guiNotifyEvent( X_ITEM *object, X_GUI_EVENT event, X_ITEM *result, int argc, const int64_t *values );

/* ---------------------------------------------------------------------------------------------
   DPI, fonts and colors
   --------------------------------------------------------------------------------------------- */

int x_guiScale( int value, UINT dpi );
HFONT x_guiFont( UINT dpi );
COLORREF x_guiMix( COLORREF first, COLORREF second, int amount );

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
void x_guiWindowSetIcon( X_CALL_CTX *ctx, HWND window, const X_ITEM *file );
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

LRESULT x_guiSend( HWND window, UINT message, WPARAM wParam, LPARAM lParam );
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
int x_guiControlColor( HWND control );
void x_guiGetColor( X_CALL_CTX *ctx, const char *operation );
void x_guiSetColor( X_CALL_CTX *ctx, const char *operation );
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

int x_guiIsPicture( const X_ITEM *item );
HBITMAP x_guiPicture( X_CALL_CTX *ctx, const X_ITEM *file, const char *operation );
int x_guiHasAlpha( const BITMAP *info );
COLORREF x_guiCornerColor( HBITMAP picture );

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
   Sidebars
   --------------------------------------------------------------------------------------------- */

void x_guiSidebarNew( X_CALL_CTX *ctx );
int x_guiSidebarAdd( X_CALL_CTX *ctx, HWND sidebar, X_ITEM *page, HMENU menu );
void x_guiSidebarPanel( X_CALL_CTX *ctx, HWND sidebar, int page );
int x_guiSidebarAddButton( X_CALL_CTX *ctx, HWND sidebar, int page, const X_ITEM *bitmap, const X_ITEM *toolTip );
int x_guiSidebarBadge( HWND sidebar, int page, int count );
void x_guiSidebarAddSection( X_CALL_CTX *ctx, HWND sidebar, X_ITEM *section );
int x_guiSidebarToggle( HWND sidebar );
int x_guiSidebarPage( HWND sidebar );
void x_guiSidebarSelect( X_CALL_CTX *ctx, HWND sidebar, int page );
int x_guiSidebarWidth( HWND sidebar );
void x_guiSidebarSetWidth( HWND sidebar, int width );
void x_guiSidebarColor( X_CALL_CTX *ctx, X_GUI_SIDEBAR_COLOR part );
void x_guiSidebarSetColor( X_CALL_CTX *ctx, X_GUI_SIDEBAR_COLOR part );
void x_guiSidebarColors( HWND sidebar, COLORREF *back, COLORREF *fore );

/* ---------------------------------------------------------------------------------------------
   Menus in a panel of a sidebar
   --------------------------------------------------------------------------------------------- */

void x_guiSideMenu( HWND panel, HMENU menu );
HIMAGELIST x_guiSidePicture( HBITMAP picture );

/* ---------------------------------------------------------------------------------------------
   Lists of rows in a panel of a sidebar
   --------------------------------------------------------------------------------------------- */

int x_guiSidebarAddItem( X_CALL_CTX *ctx, HWND sidebar, int page );
int x_guiSidebarClearItems( HWND sidebar, int page );

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

X_GUI_TIMER *x_guiTimerFind( UINT_PTR id );
void x_guiTimersRelease( HWND window );
UINT_PTR x_guiTimerStart( X_CALL_CTX *ctx, HWND window, UINT_PTR id, int interval );
void x_guiTimerStop( UINT_PTR id );
int x_guiTimerActive( UINT_PTR id );
void x_guiTimersShutdown( void );

/* ---------------------------------------------------------------------------------------------
   Dialogs
   --------------------------------------------------------------------------------------------- */

void x_guiMessage( X_CALL_CTX *ctx, X_GUI_MESSAGE kind );
void x_guiFileDialog( X_CALL_CTX *ctx, int save );
void x_guiOpenFile( X_CALL_CTX *ctx );

/* ---------------------------------------------------------------------------------------------
   Types shared by the files of the library
   --------------------------------------------------------------------------------------------- */

/* Ranges of the identifiers of controls and of menu commands */
enum
{
   X_GUI_CONTROL_FIRST = 100,
   X_GUI_CONTROL_LAST = 0x7FFF,
   X_GUI_MENU_FIRST = 0x8000,
   X_GUI_MENU_LAST = 0xFFFF
};

/* Sides a control is anchored to */
enum
{
   X_GUI_ANCHOR_LEFT = 1,
   X_GUI_ANCHOR_TOP = 2,
   X_GUI_ANCHOR_RIGHT = 4,
   X_GUI_ANCHOR_BOTTOM = 8
};

/* The anchored sides of a control and its distances from them */
typedef struct
{
   int flags, left, top, right, bottom;
} X_GUI_ANCHOR;

/* A command of a menu; when is NIL or a block telling whether the command can be chosen now */
typedef struct
{
   int id;
   UINT key;
   UINT modifiers;
   X_ITEM *action;
   X_ITEM *when;
} X_GUI_ACTION;

/* Modifier keys of a shortcut */
enum
{
   X_GUI_SHIFT = 1,
   X_GUI_CTRL = 2,
   X_GUI_ALT = 4
};

/* Side of the parent a toolbar stands at; a bar of a sidebar is placed by the sidebar */
typedef enum
{
   X_GUI_SIDE_TOP,
   X_GUI_SIDE_BOTTOM,
   X_GUI_SIDE_LEFT,
   X_GUI_SIDE_RIGHT,
   X_GUI_SIDE_NONE
} X_GUI_SIDE;

/* The picture of a button as a character of a font, from a gui_Icon: the family of the font, the character, its
   color 0xRRGGBB or -1 for the color of the text and its size in pixels at 96 DPI; an empty family for a picture
   from a file */
typedef struct
{
   wchar_t family[LF_FACESIZE];
   unsigned code;
   int color;
   int size;
} X_GUI_GLYPH;

/* A toolbar stands right in its window, at a side of it; glyphs holds the character of every button with a
   gui_Icon, which a sidebar draws as text */
typedef struct
{
   X_GUI_SIDE side;
   int colors[X_GUI_COLOR_COUNT];
   HIMAGELIST images;
   HWND grip;
   wchar_t **tips;
   UINT *keys;
   UINT *modifiers;
   X_GUI_GLYPH *glyphs;
   size_t count;
} X_GUI_BAR;

/* Sizes of a sidebar at 96 DPI: the strip of pictures, the header of the panel with actions,
   the gap at the right of the actions, the buttons of the actions, the mark of the shown page, the edge dragged
   by the mouse, the first and the smallest width of the panel; the opacity
   of the pictures of pages not shown; the header of a section, the place of its arrow and of its title; the place of
   the title of the shown page in the header */
enum
{
   X_GUI_SIDEBAR_STRIP = 48,
   X_GUI_SIDEBAR_HEADER = 35,
   X_GUI_SIDEBAR_GAP = 8,
   X_GUI_SIDEBAR_ACTION = 22,
   X_GUI_SIDEBAR_MARK = 2,
   X_GUI_SIDEBAR_EDGE = 4,
   X_GUI_SIDEBAR_WIDTH = 260,
   X_GUI_SIDEBAR_MINWIDTH = 170,
   X_GUI_SIDEBAR_PALE = 150,
   X_GUI_SIDEBAR_SECTION = 22,
   X_GUI_SIDEBAR_ARROW = 10,
   X_GUI_SIDEBAR_CAPTION = 20,
   X_GUI_SIDEBAR_TITLE = 20
};

/* A section of a page of a sidebar: its panel under its header with the title, open while the panel is shown */
typedef struct
{
   HWND panel;
   wchar_t *title;
   int open;
} X_GUI_SIDESECTION;

/* A row of the list of a page: its picture, its title, the text under it, the line at the bottom and the button of
   the settings at the right of that line when the row has bSettings */
typedef struct
{
   HIMAGELIST image;
   wchar_t *title;
   wchar_t *text;
   wchar_t *info;
   int settings;
} X_GUI_SIDEROW;

/* The list of rows of a page: the row under the mouse and its part (1 the row, 2 the button of the settings), the
   row clicked last, -1 for none; scroll is the distance of the list scrolled up in pixels, tracking set while the
   panel waits for the mouse to leave it */
typedef struct
{
   X_GUI_SIDEROW *rows;
   size_t count;
   int hot;
   int part;
   int chosen;
   int scroll;
   int tracking;
} X_GUI_SIDELIST;

/* A page of a sidebar: its panel, the bar of its actions in the header, its title, the count on its picture, its
   sections and its list of rows, NULL without rows */
typedef struct
{
   HWND panel;
   HWND actions;
   wchar_t *title;
   int badge;
   X_GUI_SIDESECTION *sections;
   size_t sectionCount;
   X_GUI_SIDELIST *list;
} X_GUI_SIDEPAGE;

/* A sidebar: the bar of pages at the top of the strip, the bar of buttons at its bottom and the panel of the shown
   page; width of the panel at 96 DPI, page 0 when the panel is hidden, last the page shown last, colors -1 for the
   system ones, drag set while the mouse drags the edge, before the width before the drag; grab the place of the
   picture under the pressed mouse, -1 without it, start the point of the press and moving set while the mouse
   moves the picture */
typedef struct
{
   HWND activity;
   HWND buttons;
   X_GUI_SIDEPAGE *pages;
   size_t count;
   int page;
   int last;
   int width;
   int colors[X_GUI_SIDEBAR_COLOR_COUNT];
   int drag;
   int before;
   int grab;
   POINT start;
   int moving;
} X_GUI_SIDEBAR;

/* Panels of a status bar: width in pixels at 96 DPI, 0 shares the rest of the bar; SBT_POPOUT is raised;
   colors of X_GUI_COLOR_FOREGROUND and X_GUI_COLOR_BACKGROUND, -1 for the system color; scoreboard is the
   panel showing the insert mode during ReadModal(), 0 none */
typedef struct
{
   int *widths;
   int *styles;
   size_t count;
   int colors[2];
   int scoreboard;
} X_GUI_STATUS;

/* Picture of an edit field: type 'C', 'N' or 'D' of :value, the functions after @ and the template */
typedef struct
{
   char type;
   int upper;
   int clear;
   int remove;
   int typed;
   char *picture;
   wchar_t *mask;
   size_t length;
   int digits;
   int decimals;
   double number;
} X_GUI_MASK;

/* Column of a browse; its block and picture stay in the xDev object; a column of logical values shows
   a tick */
typedef struct
{
   wchar_t *title;
   int width;
   int align;
   int shown;
   int logical;
} X_GUI_COLUMN;

/* The rows on the screen of a browse; the record of the cursor row is the current record of the data */
typedef struct
{
   X_GUI_COLUMN *columns;
   size_t columnCount;
   wchar_t **cells;
   int cellRows;
   int rows;
   int rowPos;
   int offset;
   int valid;
   int hitTop;
   int hitBottom;
   int position;
   int colPos;
   int freeze;
   int colors[X_GUI_GRID_COLORS];
} X_GUI_GRID;

/* Font of a control given by a gui_Font: size in points (0 = system size), name ("" = system font) */
typedef struct
{
   int size;
   int bold;
   int italic;
   wchar_t name[LF_FACESIZE];
   UINT dpi;
   HFONT font;
} X_GUI_OWNFONT;

/* Keys of oWindow:SetKey(); their blocks stay in the xDev object, in the same order */
typedef struct
{
   UINT *keys;
   UINT *modifiers;
   size_t count;
} X_GUI_KEYS;

/* Colors of a window chosen by the program; -1 keeps the system color */
typedef struct
{
   int colors[X_GUI_COLOR_BACKGROUND + 1];
   HBRUSH brush;
} X_GUI_PAINT;

/* The pages of a tab control: placed in area and shown by selected (1-based), or with find only looked up */
typedef struct
{
   HWND tab;
   RECT area;
   int selected;
   int find;
   HWND found;
} X_GUI_TABS;

/* ---------------------------------------------------------------------------------------------
   State shared by the files of the library
   --------------------------------------------------------------------------------------------- */

extern const wchar_t x_guiBrowseClassName[];
extern const wchar_t x_guiAnchorProperty[];
extern const wchar_t x_guiDefaultProperty[];
extern const wchar_t x_guiCancelProperty[];
extern const wchar_t x_guiToolTipProperty[];
extern const wchar_t x_guiSidebarProperty[];
extern const wchar_t x_guiPaintProperty[];
extern const wchar_t x_guiFontProperty[];
extern const wchar_t x_guiColorProperty[];
extern const wchar_t x_guiKeysProperty[];
extern const wchar_t x_guiVarProperty[];
extern const wchar_t x_guiGetProperty[];
extern const wchar_t x_guiRowsProperty[];
extern ATOM x_guiWindowClass;
extern int x_guiRunning;
extern X_GUI_ACTION *x_guiActions;
extern size_t x_guiActionCount;
extern size_t x_guiActionCapacity;
extern int x_guiQuiet;
extern int x_guiValidating;
extern int x_guiReading;
extern int x_guiReadEnd;
extern HWND x_guiReadWindow;
extern HWND x_guiReadClose;
extern int x_guiMenuExtended;
extern const wchar_t x_guiXfontName[];

/* ---------------------------------------------------------------------------------------------
   Functions shared by the files of the library
   --------------------------------------------------------------------------------------------- */

wchar_t *x_guiCrlf( wchar_t *text );
int x_guiHasClass( HWND window, const wchar_t *name );
int x_guiIsMultiline( HWND window );
int x_guiIsRadio( HWND window );
int x_guiIsChoice( HWND window );
HWND x_guiObjectHandle( X_CALL_CTX *ctx, X_ITEM *object );
void x_guiXfontAttach( HWND control );
HFONT x_guiOwnFontCreate( const X_GUI_OWNFONT *own, UINT dpi );
HFONT x_guiFontOf( HWND window, UINT dpi );
void x_guiFontRelease( HWND window );
void x_guiChildFrame( HWND child, RECT *frame );
BOOL CALLBACK x_guiRescaleChild( HWND child, LPARAM data );
void x_guiLayout( HWND window, int width, int height );
COLORREF x_guiColorRef( int rgb );
X_GUI_BAR *x_guiToolbarOf( HWND window );
X_GUI_SIDEBAR *x_guiSidebarOf( HWND window );
void x_guiSideListRelease( X_GUI_SIDELIST *list );
int x_guiFontIconGlyph( X_CALL_CTX *ctx, const X_ITEM *icon, X_GUI_GLYPH *glyph );
int x_guiGlyphText( unsigned code, wchar_t *text );
void x_guiToolbarInit( HWND toolbar );
void x_guiToolbarRelease( HWND toolbar );
X_GUI_STATUS *x_guiStatusOf( HWND window );
X_GUI_STATUS *x_guiStatusParts( HWND status );
void x_guiStatusRelease( HWND status );
void x_guiStatusLayout( HWND status );
LRESULT CALLBACK x_guiStatusProc( HWND status, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR id,
                                  DWORD_PTR data );
int x_guiShown( HWND window );
void x_guiBarsArea( HWND parent, RECT *area, int horizontal );
BOOL CALLBACK x_guiLayoutToolbar( HWND child, LPARAM parent );
BOOL CALLBACK x_guiLayoutSideToolbar( HWND child, LPARAM parent );
LRESULT x_guiToolbarDraw( NMTBCUSTOMDRAW *draw );
void x_guiToolbarTip( NMTBGETINFOTIPW *tip );
X_GUI_PAINT *x_guiPaintOf( HWND window );
void x_guiPaintRelease( HWND window );
void x_guiPaintTheme( HWND control );
LRESULT CALLBACK x_guiGroupProc( HWND group, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR id, DWORD_PTR data );
BOOL CALLBACK x_guiPaintThemeChild( HWND child, LPARAM parent );
LRESULT x_guiPaintControl( HWND window, HDC dc, HWND control );
void x_guiDetach( HWND window );
BOOL CALLBACK x_guiDetachChild( HWND window, LPARAM unused );
BOOL CALLBACK x_guiTabPageEnum( HWND child, LPARAM data );
void x_guiTabLayout( HWND tab, int show );
BOOL CALLBACK x_guiLayoutPanel( HWND child, LPARAM parent );
void x_guiSidebarPlace( HWND sidebar );
BOOL CALLBACK x_guiLayoutSidebar( HWND child, LPARAM parent );
void x_guiDestroyWindow( HWND window );
void x_guiWindowSend( X_CALL_CTX *ctx, X_ITEM *target, const X_ITEM *owner, const X_ITEM *title, int width, int height,
                      X_GUI_WINDOW_KIND kind, int id );
void x_guiDispatchMessage( MSG *message );
void x_guiMenuMeasure( HWND window, MEASUREITEMSTRUCT *measure );
void x_guiMenuDraw( HWND window, const DRAWITEMSTRUCT *draw );
BOOL x_guiMenuAppend( HMENU menu, UINT flags, UINT_PTR id, const wchar_t *text );
int x_guiParseShortcut( const char *text, size_t length, UINT *key, UINT *modifiers, char *display, size_t size );
X_GUI_MASK *x_guiMaskOf( HWND edit );
void x_guiMaskParse( X_GUI_MASK *mask, char type );
void x_guiMaskStyle( HWND edit, const X_GUI_MASK *mask );
X_GUI_MASK *x_guiMaskEnsure( HWND edit, char type );
void x_guiEditCurrent( HWND edit, X_GUI_MASK *mask, X_ITEM *value );
void x_guiEditShow( HWND edit, X_GUI_MASK *mask, const X_ITEM *value );
int x_guiEditValid( HWND edit, HWND next, X_GUI_MASK *mask );
void x_guiReadScoreboard( HWND root, int reading );
void x_guiGetFit( HWND edit, int focused );
LRESULT CALLBACK x_guiEditProc( HWND edit, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR id, DWORD_PTR data );
X_GUI_GRID *x_guiGridOf( HWND browse );
BOOL x_guiRegisterBrowseClass( HINSTANCE instance );
void x_guiInitCommonControls( void );
void x_guiControlSend( X_CALL_CTX *ctx, X_GUI_KIND kind, int textParam, int x, int y, int width, int height );
void x_guiStatusSetText( HWND status, int index, int style, const wchar_t *text );

#endif
