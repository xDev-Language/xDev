/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev GUI
 * File......: internal-gui.c
 *
 */

#include <string.h>
#include "internal-gui.h"

#pragma comment( lib, "comctl32" )
#pragma comment( lib, "comdlg32" )
#pragma comment( lib, "uxtheme" )

/* The system font made for one DPI */
typedef struct
{
   UINT dpi;
   HFONT font;
} X_GUI_FONT;

/* The C functions of the events of one class */
typedef struct
{
   X_CLASS_HANDLE handle;
   X_METHODPTR functions[X_GUI_EVENT_COUNT];
} X_GUI_HANDLERS;

const wchar_t x_guiBrowseClassName[] = L"xDev.GUI.Browse";
const wchar_t x_guiAnchorProperty[] = L"xDev.GUI.Anchor";
const wchar_t x_guiDefaultProperty[] = L"xDev.GUI.Default";
const wchar_t x_guiCancelProperty[] = L"xDev.GUI.Cancel";
const wchar_t x_guiToolTipProperty[] = L"xDev.GUI.ToolTip";
const wchar_t x_guiSidebarProperty[] = L"xDev.GUI.Sidebar";
const wchar_t x_guiPaintProperty[] = L"xDev.GUI.Paint";
const wchar_t x_guiFontProperty[] = L"xDev.GUI.Font";
/* The text color of a label + 1, so that black is not 0 */
const wchar_t x_guiColorProperty[] = L"xDev.GUI.Color";
const wchar_t x_guiKeysProperty[] = L"xDev.GUI.Keys";
const wchar_t x_guiVarProperty[] = L"xDev.GUI.Var";
const wchar_t x_guiGetProperty[] = L"xDev.GUI.Get";
/* The number of rows a GET of several rows grows to while it has the focus */
const wchar_t x_guiRowsProperty[] = L"xDev.GUI.Rows";

ATOM x_guiWindowClass;
static X_GUI_FONT s_fonts[16];
static size_t s_fontCount;
int x_guiRunning;
X_GUI_ACTION *x_guiActions;
size_t x_guiActionCount;
size_t x_guiActionCapacity;
int x_guiQuiet;
int x_guiValidating;
int x_guiReading;
/* Set by EndRead() and by closing the window: 1 ends Read() with saving, 0 without; -1 lets Read() go on */
int x_guiReadEnd = -1;
/* The window of the running Read() and the window closed during it, which closes after Read() */
HWND x_guiReadWindow;
HWND x_guiReadClose;
static X_GUI_HANDLERS *s_handlers;
static size_t s_handlerCount;
static const char *s_message;
static X_GUI_EVENT s_event;
/* Names of the events in the error of a class without the function of an event */
static const char *s_eventNames[X_GUI_EVENT_COUNT] = { "GUI CREATE", "GUI DETACH", "GUI COMMAND", "GUI NOTIFY",
                                                       "GUI VALID", "GUI KEY", "GUI MENUWHEN", "GUI POPUP",
                                                       "GUI NAVIGATE", "GUI ROW", "GUI BROWSE", "GUI TOGGLE",
                                                       "GUI CHANGE", "GUI TIMER", "GUI ITEM" };

/* The extended style of menus, see x_guiMenuStyle() */
int x_guiMenuExtended;

/* Class description read by the compiler for typed code (LOCAL oWin AS gui_Window). */
#define X_GUI_INTERFACE \
   "XDEVCLS 1\n" \
   "C GUIWINDOW -\n" \
   "M DATA HANDLE 17 ANY -\n" \
   "M METHOD NEW 1 ANY - TITLE WIDTH HEIGHT\n" \
   "M METHOD SHOW 1 ANY -\n" \
   "M METHOD HIDE 1 ANY -\n" \
   "M METHOD CLOSE 1 ANY -\n" \
   "M METHOD ONCLOSE 1 ANY -\n" \
   "M METHOD ONRESIZE 1 ANY - WIDTH HEIGHT\n" \
   "M METHOD ONKEY 1 ANY - KEY\n" \
   "M METHOD ONMENU 1 ANY - ID\n" \
   "M ACCESS MENU 1 ANY -\n" \
   "M ASSIGN MENU 1 ANY - VALUE\n" \
   "M ACCESS TITLE 1 ANY -\n" \
   "M ASSIGN TITLE 1 ANY - VALUE\n" \
   "M ACCESS SIZABLE 1 ANY -\n" \
   "M ASSIGN SIZABLE 1 ANY - VALUE\n" \
   "M ACCESS WIDTH 1 ANY -\n" \
   "M ASSIGN WIDTH 1 ANY - VALUE\n" \
   "M ACCESS HEIGHT 1 ANY -\n" \
   "M ASSIGN HEIGHT 1 ANY - VALUE\n" \
   "M ACCESS CONTEXTMENU 1 ANY -\n" \
   "M ASSIGN CONTEXTMENU 1 ANY - VALUE\n" \
   "M METHOD CENTER 1 ANY -\n" \
   "M METHOD SETKEY 1 ANY - KEY ACTION\n" \
   "M METHOD FOCUSNEXT 1 ANY - PREVIOUS\n" \
   "M METHOD READMODAL 1 ANY - GETLIST GET\n" \
   "M METHOD ENDREAD 1 ANY - SAVE\n" \
   "M ACCESS BACKGROUND 1 ANY -\n" \
   "M ASSIGN BACKGROUND 1 ANY - VALUE\n" \
   "M ACCESS FOREGROUND 1 ANY -\n" \
   "M ASSIGN FOREGROUND 1 ANY - VALUE\n" \
   "C GUIPANEL GUIWINDOW\n" \
   "M METHOD NEW 1 ANY - PARENT X Y WIDTH HEIGHT\n" \
   "M ACCESS ANCHOR 1 ANY -\n" \
   "M ASSIGN ANCHOR 1 ANY - VALUE\n" \
   "C GUIDIALOG GUIWINDOW\n" \
   "M METHOD NEW 1 ANY - OWNER TITLE WIDTH HEIGHT\n" \
   "M METHOD SHOWMODAL 1 ANY -\n" \
   "M METHOD ENDDIALOG 1 ANY - RESULT\n" \
   "M ACCESS RESULT 1 ANY -\n" \
   "C GUIMENU -\n" \
   "M DATA HANDLE 17 ANY -\n" \
   "M METHOD NEW 1 ANY - POPUP\n" \
   "M METHOD ADDMENU 1 ANY - TEXT BITMAP\n" \
   "M METHOD ADD 1 ANY - TEXT ACTION SHORTCUT BITMAP WHEN\n" \
   "M METHOD ADDSEPARATOR 1 ANY -\n" \
   "C GUICONTROL -\n" \
   "M DATA HANDLE 17 ANY -\n" \
   "M DATA PARENT 17 ANY -\n" \
   "M DATA NAME 1 ANY -\n" \
   "M METHOD SETFOCUS 1 ANY -\n" \
   "M METHOD DESTROY 1 ANY -\n" \
   "M ACCESS TEXT 1 ANY -\n" \
   "M ASSIGN TEXT 1 ANY - VALUE\n" \
   "M ACCESS VISIBLE 1 ANY -\n" \
   "M ASSIGN VISIBLE 1 ANY - VALUE\n" \
   "M ACCESS ENABLED 1 ANY -\n" \
   "M ASSIGN ENABLED 1 ANY - VALUE\n" \
   "M ACCESS ANCHOR 1 ANY -\n" \
   "M ASSIGN ANCHOR 1 ANY - VALUE\n" \
   "M ACCESS X 1 ANY -\n" \
   "M ASSIGN X 1 ANY - VALUE\n" \
   "M ACCESS Y 1 ANY -\n" \
   "M ASSIGN Y 1 ANY - VALUE\n" \
   "M ACCESS WIDTH 1 ANY -\n" \
   "M ASSIGN WIDTH 1 ANY - VALUE\n" \
   "M ACCESS HEIGHT 1 ANY -\n" \
   "M ASSIGN HEIGHT 1 ANY - VALUE\n" \
   "M ACCESS TOOLTIP 1 ANY -\n" \
   "M ASSIGN TOOLTIP 1 ANY - VALUE\n" \
   "M ACCESS CONTEXTMENU 1 ANY -\n" \
   "M ASSIGN CONTEXTMENU 1 ANY - VALUE\n" \
   "M ACCESS VALUE 1 ANY -\n" \
   "M ASSIGN VALUE 1 ANY - VALUE\n" \
   "M ACCESS FONT 1 ANY -\n" \
   "M ASSIGN FONT 1 ANY - VALUE\n" \
   "M ACCESS FOCUSED 1 ANY -\n" \
   "C GUIFONT -\n" \
   "M DATA SIZE 1 ANY -\n" \
   "M DATA BOLD 1 ANY -\n" \
   "M DATA ITALIC 1 ANY -\n" \
   "M DATA NAME 1 ANY -\n" \
   "M METHOD NEW 1 ANY - SIZE BOLD ITALIC NAME\n" \
   "C GUIFONTICON -\n" \
   "M DATA FILE 1 ANY -\n" \
   "M DATA CODE 1 ANY -\n" \
   "M DATA COLOR 1 ANY -\n" \
   "M DATA SIZE 1 ANY -\n" \
   "M METHOD NEW 1 ANY - FILE CODE COLOR SIZE\n" \
   "C GUILABEL GUICONTROL\n" \
   "M METHOD NEW 1 ANY - PARENT X Y WIDTH HEIGHT TEXT\n" \
   "M ACCESS BORDER 1 ANY -\n" \
   "M ASSIGN BORDER 1 ANY - VALUE\n" \
   "M ACCESS ALIGN 1 ANY -\n" \
   "M ASSIGN ALIGN 1 ANY - VALUE\n" \
   "C GUISAY GUILABEL\n" \
   "M METHOD NEW 1 ANY - PARENT X Y LEN HEIGHT TEXT\n" \
   "C GUIEDIT GUICONTROL\n" \
   "M DATA ONCHANGE 1 CODEBLOCK -\n" \
   "M DATA ONVALID 1 CODEBLOCK -\n" \
   "M ACCESS PICTURE 1 ANY -\n" \
   "M ASSIGN PICTURE 1 ANY - VALUE\n" \
   "M METHOD NEW 1 ANY - PARENT X Y WIDTH HEIGHT TEXT\n" \
   "M ACCESS READONLY 1 ANY -\n" \
   "M ASSIGN READONLY 1 ANY - VALUE\n" \
   "M ACCESS MAXLENGTH 1 ANY -\n" \
   "M ASSIGN MAXLENGTH 1 ANY - VALUE\n" \
   "C GUIGET GUIEDIT\n" \
   "M METHOD NEW 1 ANY - PARENT X Y LEN HEIGHT VAR PICTURE ROWS MAX\n" \
   "C GUIMEMO GUIEDIT\n" \
   "M METHOD NEW 1 ANY - PARENT X Y WIDTH HEIGHT TEXT\n" \
   "C GUIBUTTON GUICONTROL\n" \
   "M DATA ONCLICK 1 CODEBLOCK -\n" \
   "M METHOD NEW 1 ANY - PARENT X Y WIDTH HEIGHT TEXT\n" \
   "M ACCESS ISDEFAULT 1 ANY -\n" \
   "M ASSIGN ISDEFAULT 1 ANY - VALUE\n" \
   "M ACCESS ISCANCEL 1 ANY -\n" \
   "M ASSIGN ISCANCEL 1 ANY - VALUE\n" \
   "C GUICHECKBOX GUICONTROL\n" \
   "M DATA ONCHANGE 1 CODEBLOCK -\n" \
   "M METHOD NEW 1 ANY - PARENT X Y WIDTH HEIGHT TEXT\n" \
   "M ACCESS CHECKED 1 ANY -\n" \
   "M ASSIGN CHECKED 1 ANY - VALUE\n" \
   "C GUIRADIOBUTTON GUICHECKBOX\n" \
   "M METHOD NEW 1 ANY - PARENT X Y WIDTH HEIGHT TEXT\n" \
   "C GUIGROUPBOX GUICONTROL\n" \
   "M METHOD NEW 1 ANY - PARENT X Y WIDTH HEIGHT TEXT\n" \
   "C GUILISTBOX GUICONTROL\n" \
   "M DATA ONSELECT 1 CODEBLOCK -\n" \
   "M DATA ONDOUBLECLICK 1 CODEBLOCK -\n" \
   "M METHOD NEW 1 ANY - PARENT X Y WIDTH HEIGHT\n" \
   "M METHOD ADD 1 ANY - TEXT\n" \
   "M METHOD CLEAR 1 ANY -\n" \
   "M METHOD ITEM 1 ANY - INDEX\n" \
   "M ACCESS COUNT 1 ANY -\n" \
   "M ACCESS SELECTED 1 ANY -\n" \
   "M ASSIGN SELECTED 1 ANY - VALUE\n" \
   "M ACCESS SELECTEDTEXT 1 ANY -\n" \
   "C GUICOMBOBOX GUILISTBOX\n" \
   "M METHOD NEW 1 ANY - PARENT X Y WIDTH HEIGHT\n" \
   "C GUIPROGRESSBAR GUICONTROL\n" \
   "M METHOD NEW 1 ANY - PARENT X Y WIDTH HEIGHT\n" \
   "M ACCESS VALUE 1 ANY -\n" \
   "M ASSIGN VALUE 1 ANY - VALUE\n" \
   "M ACCESS MAXIMUM 1 ANY -\n" \
   "M ASSIGN MAXIMUM 1 ANY - VALUE\n" \
   "C GUISTATUSBAR GUICONTROL\n" \
   "M METHOD NEW 1 ANY - PARENT TEXT\n" \
   "M METHOD ADD 1 ANY - WIDTH TEXT RAISED\n" \
   "M METHOD SETTEXT 1 ANY - PANEL TEXT\n" \
   "M ACCESS SCOREBOARD 1 ANY -\n" \
   "M ASSIGN SCOREBOARD 1 ANY - VALUE\n" \
   "M ACCESS FOREGROUND 1 ANY -\n" \
   "M ASSIGN FOREGROUND 1 ANY - VALUE\n" \
   "M ACCESS BACKGROUND 1 ANY -\n" \
   "M ASSIGN BACKGROUND 1 ANY - VALUE\n" \
   "C GUIDATEPICKER GUICONTROL\n" \
   "M DATA ONCHANGE 1 CODEBLOCK -\n" \
   "M METHOD NEW 1 ANY - PARENT X Y WIDTH HEIGHT\n" \
   "C GUISLIDER GUICONTROL\n" \
   "M DATA ONCHANGE 1 CODEBLOCK -\n" \
   "M METHOD NEW 1 ANY - PARENT X Y WIDTH HEIGHT\n" \
   "M ACCESS MINIMUM 1 ANY -\n" \
   "M ASSIGN MINIMUM 1 ANY - VALUE\n" \
   "M ACCESS MAXIMUM 1 ANY -\n" \
   "M ASSIGN MAXIMUM 1 ANY - VALUE\n" \
   "C GUILISTVIEW GUICONTROL\n" \
   "M DATA ONSELECT 1 CODEBLOCK -\n" \
   "M DATA ONDOUBLECLICK 1 CODEBLOCK -\n" \
   "M METHOD NEW 1 ANY - PARENT X Y WIDTH HEIGHT\n" \
   "M METHOD ADDCOLUMN 1 ANY - TITLE WIDTH ALIGN\n" \
   "M METHOD ADDROW 1 ANY - VALUES\n" \
   "M METHOD CELL 1 ANY - ROW COLUMN\n" \
   "M METHOD SETCELL 1 ANY - ROW COLUMN TEXT\n" \
   "M METHOD DELETE 1 ANY - ROW\n" \
   "M METHOD CLEAR 1 ANY -\n" \
   "M ACCESS COUNT 1 ANY -\n" \
   "M ACCESS COLUMNCOUNT 1 ANY -\n" \
   "M ACCESS SELECTED 1 ANY -\n" \
   "M ASSIGN SELECTED 1 ANY - VALUE\n" \
   "C GUITREEVIEW GUICONTROL\n" \
   "M DATA ONSELECT 1 CODEBLOCK -\n" \
   "M DATA ONDOUBLECLICK 1 CODEBLOCK -\n" \
   "M METHOD NEW 1 ANY - PARENT X Y WIDTH HEIGHT\n" \
   "M METHOD ADD 1 ANY - TEXT PARENT\n" \
   "M METHOD ITEMTEXT 1 ANY - ITEM\n" \
   "M METHOD SETITEMTEXT 1 ANY - ITEM TEXT\n" \
   "M METHOD EXPAND 1 ANY - ITEM\n" \
   "M METHOD COLLAPSE 1 ANY - ITEM\n" \
   "M METHOD CLEAR 1 ANY -\n" \
   "M ACCESS COUNT 1 ANY -\n" \
   "M ACCESS SELECTED 1 ANY -\n" \
   "M ASSIGN SELECTED 1 ANY - VALUE\n" \
   "C GUITABPAGE GUIWINDOW\n" \
   "C GUITABCONTROL GUICONTROL\n" \
   "M DATA ONSELECT 1 CODEBLOCK -\n" \
   "M METHOD NEW 1 ANY - PARENT X Y WIDTH HEIGHT\n" \
   "M METHOD ADDPAGE 1 ANY - TITLE\n" \
   "M METHOD PAGE 1 ANY - INDEX\n" \
   "M ACCESS COUNT 1 ANY -\n" \
   "M ACCESS SELECTED 1 ANY -\n" \
   "M ASSIGN SELECTED 1 ANY - VALUE\n" \
   "C GUIBROWSE GUICONTROL\n" \
   "M ACCESS FREEZE 1 ANY -\n" \
   "M ASSIGN FREEZE 1 ANY - VALUE\n" \
   "M ACCESS COLPOS 1 ANY -\n" \
   "M ASSIGN COLPOS 1 ANY - VALUE\n" \
   "M ACCESS HEADERBACKGROUND 1 ANY -\n" \
   "M ASSIGN HEADERBACKGROUND 1 ANY - VALUE\n" \
   "M ACCESS HEADERFOREGROUND 1 ANY -\n" \
   "M ASSIGN HEADERFOREGROUND 1 ANY - VALUE\n" \
   "M ACCESS HEADERLINE 1 ANY -\n" \
   "M ASSIGN HEADERLINE 1 ANY - VALUE\n" \
   "M ACCESS BACKGROUND 1 ANY -\n" \
   "M ASSIGN BACKGROUND 1 ANY - VALUE\n" \
   "M ACCESS ALTERNATEBACKGROUND 1 ANY -\n" \
   "M ASSIGN ALTERNATEBACKGROUND 1 ANY - VALUE\n" \
   "M ACCESS FOREGROUND 1 ANY -\n" \
   "M ASSIGN FOREGROUND 1 ANY - VALUE\n" \
   "M ACCESS GRIDLINE 1 ANY -\n" \
   "M ASSIGN GRIDLINE 1 ANY - VALUE\n" \
   "M ACCESS CURSORBACKGROUND 1 ANY -\n" \
   "M ASSIGN CURSORBACKGROUND 1 ANY - VALUE\n" \
   "M ACCESS CURSORFOREGROUND 1 ANY -\n" \
   "M ASSIGN CURSORFOREGROUND 1 ANY - VALUE\n" \
   "M ACCESS INACTIVECURSOR 1 ANY -\n" \
   "M ASSIGN INACTIVECURSOR 1 ANY - VALUE\n" \
   "M ACCESS CHECKFOREGROUND 1 ANY -\n" \
   "M ASSIGN CHECKFOREGROUND 1 ANY - VALUE\n" \
   "M DATA GOTOPBLOCK 1 CODEBLOCK -\n" \
   "M DATA GOBOTTOMBLOCK 1 CODEBLOCK -\n" \
   "M DATA SKIPBLOCK 1 CODEBLOCK -\n" \
   "M DATA POSITIONBLOCK 1 CODEBLOCK -\n" \
   "M DATA ONCHANGE 1 CODEBLOCK -\n" \
   "M DATA ONDOUBLECLICK 1 CODEBLOCK -\n" \
   "M METHOD NEW 1 ANY - PARENT X Y WIDTH HEIGHT\n" \
   "M METHOD ADDCOLUMN 1 ANY - HEADING BLOCK WIDTH PICTURE ALIGN\n" \
   "M METHOD REFRESH 1 ANY -\n" \
   "C GUITOOLBAR GUICONTROL\n" \
   "M METHOD NEW 1 ANY - PARENT SIDE\n" \
   "M METHOD ADD 1 ANY - TEXT BITMAP ACTION TOOLTIP SHORTCUT\n" \
   "M METHOD ADDMENU 1 ANY - TEXT MENU BITMAP TOOLTIP SHORTCUT\n" \
   "M METHOD ADDSEPARATOR 1 ANY -\n" \
   "M METHOD CLEAR 1 ANY -\n" \
   "M ACCESS FOREGROUND 1 ANY -\n" \
   "M ASSIGN FOREGROUND 1 ANY - VALUE\n" \
   "M ACCESS BACKGROUND 1 ANY -\n" \
   "M ASSIGN BACKGROUND 1 ANY - VALUE\n" \
   "M ACCESS HOVERFOREGROUND 1 ANY -\n" \
   "M ASSIGN HOVERFOREGROUND 1 ANY - VALUE\n" \
   "M ACCESS HOVERBACKGROUND 1 ANY -\n" \
   "M ASSIGN HOVERBACKGROUND 1 ANY - VALUE\n" \
   "M ACCESS PRESSFOREGROUND 1 ANY -\n" \
   "M ASSIGN PRESSFOREGROUND 1 ANY - VALUE\n" \
   "M ACCESS PRESSBACKGROUND 1 ANY -\n" \
   "M ASSIGN PRESSBACKGROUND 1 ANY - VALUE\n" \
   "C GUISIDEBAR GUIWINDOW\n" \
   "M DATA ONCHANGE 1 CODEBLOCK -\n" \
   "M METHOD NEW 1 ANY - PARENT\n" \
   "M METHOD ADDBUTTON 1 ANY - BITMAP TOOLTIP SHORTCUT MENU\n" \
   "M METHOD PANEL 1 ANY - PAGE\n" \
   "M METHOD ADDACTION 1 ANY - PAGE BITMAP ACTION TOOLTIP\n" \
   "M METHOD ADDGLOBAL 1 ANY - BITMAP ACTION TOOLTIP\n" \
   "M METHOD BADGE 1 ANY - PAGE COUNT\n" \
   "M METHOD ADDSECTION 1 ANY - PAGE TITLE\n" \
   "M ACCESS PAGE 1 ANY -\n" \
   "M ASSIGN PAGE 1 ANY - VALUE\n" \
   "M ACCESS WIDTH 1 ANY -\n" \
   "M ASSIGN WIDTH 1 ANY - VALUE\n" \
   "M ACCESS BARBACKGROUND 1 ANY -\n" \
   "M ASSIGN BARBACKGROUND 1 ANY - VALUE\n" \
   "M ACCESS ACCENT 1 ANY -\n" \
   "M ASSIGN ACCENT 1 ANY - VALUE\n" \
   "C GUITIMER -\n" \
   "M DATA ONTIMER 1 CODEBLOCK -\n" \
   "M DATA PARENT 17 ANY -\n" \
   "M METHOD NEW 1 ANY - WINDOW INTERVAL ACTION\n" \
   "M METHOD START 1 ANY -\n" \
   "M METHOD STOP 1 ANY -\n" \
   "M ACCESS ACTIVE 1 ANY -\n" \
   "M ACCESS INTERVAL 1 ANY -\n" \
   "M ASSIGN INTERVAL 1 ANY - VALUE\n" \
   "C GUIAPPLICATION -\n" \
   "M METHOD RUN 1 ANY - WINDOW\n"

/* The class description in its own section of the object file, where the compiler finds it */
static const char s_interface[sizeof( X_GUI_INTERFACE ) - 1]
   __attribute__( ( section( ".xdevcls" ), used, nonstring ) ) = X_GUI_INTERFACE;

/* ---------------------------------------------------------------------------------------------
   Arguments, results and errors
   --------------------------------------------------------------------------------------------- */

/* Raise the argument error of an operation */
void x_guiRaise( X_CALL_CTX *ctx, const char *operation )
{
   x_errorRaiseNamed( ctx->result, 1102, operation );
}

/* Raise the error of a failed Windows API call */
void x_guiRaiseWin32( X_CALL_CTX *ctx, const char *operation, DWORD error )
{
   X_ERROR_DEF definition = { 2, 20, ( int )error, 0, "GUI", "Windows API call failed", operation };

   x_errorRaiseDefinition( ctx->result, &definition, NULL, NULL );
}

/* An argument as UTF-16 text; an error for a value that is not a string or holds a zero byte */
wchar_t *x_guiText( X_CALL_CTX *ctx, const X_ITEM *item, const char *operation )
{
   void *handle = NULL;
   size_t length = 0;
   const char *text = IIF( item != NULL && X_IS_STRING( item ), x_itemGetStrUTF8( item, &handle, &length ), NULL );
   wchar_t *wide = NULL;

   if( text == NULL || memchr( text, '\0', length ) != NULL )
      x_guiRaise( ctx, operation );
   else
      wide = ( wchar_t * )x_utf8ToUtf16Alloc( text, length, NULL );

   x_strfree( handle );

   return wide;
}

/* Line ends as Windows controls want them: every lone LF becomes CR LF; the text is replaced */
wchar_t *x_guiCrlf( wchar_t *text )
{
   size_t length = 0, extra = 0, i, j;
   wchar_t *result;

   for( ; text[length] != L'\0'; ++length )
      extra += text[length] == L'\n' && ( length == 0 || text[length - 1] != L'\r' );

   if( extra == 0 )
      return text;

   result = ( wchar_t * )x_xgrabArray( length + extra + 1, sizeof( wchar_t ) );
   for( i = j = 0; i < length; ++i )
   {
      if( text[i] == L'\n' && ( i == 0 || text[i - 1] != L'\r' ) )
         result[j++] = L'\r';

      result[j++] = text[i];
   }

   result[j] = L'\0';
   x_xfree( text );

   return result;
}

/* Whether a window is of the given window class */
int x_guiHasClass( HWND window, const wchar_t *name )
{
   wchar_t className[32];

   return GetClassNameW( window, className, 32 ) && lstrcmpiW( className, name ) == 0;
}

/* Whether a window is an edit field of several lines */
int x_guiIsMultiline( HWND window )
{
   return x_guiHasClass( window, L"Edit" ) && ( GetWindowLongPtrW( window, GWL_STYLE ) & ES_MULTILINE ) != 0;
}

/* Whether a window is a radio button */
int x_guiIsRadio( HWND window )
{
   if( window == NULL || !x_guiHasClass( window, L"Button" ) )
      return 0;

   return ( GetWindowLongPtrW( window, GWL_STYLE ) & BS_TYPEMASK ) == BS_AUTORADIOBUTTON;
}

/* A check box or a radio button: in GetList of ReadModal() it is a field as an edit field, Space changes it */
int x_guiIsChoice( HWND window )
{
   LONG_PTR type;

   if( window == NULL || !x_guiHasClass( window, L"Button" ) )
      return 0;

   type = GetWindowLongPtrW( window, GWL_STYLE ) & BS_TYPEMASK;

   return type == BS_AUTORADIOBUTTON || type == BS_AUTOCHECKBOX;
}

/* Return self, so calls can be chained */
void x_guiReturnSelf( X_CALL_CTX *ctx )
{
   x_itemCopy( ctx->result, ctx->self );
}

/* Return the text of a window as UTF-8 */
void x_guiReturnWindowText( X_CALL_CTX *ctx, HWND window )
{
   int length = GetWindowTextLengthW( window );
   wchar_t *wide = ( wchar_t * )x_xgrabArray( ( size_t )length + 1, sizeof( wchar_t ) );
   size_t size;
   char *text;

   length = GetWindowTextW( window, wide, length + 1 );
   text = x_utf16ToUtf8Alloc( ( const uint16_t * )wide, ( size_t )length, &size );
   x_xfree( wide );
   x_retclen_buffer( ctx, text, size );
}

/* The window handle of an xDev object, read through its HANDLE message */
HWND x_guiObjectHandle( X_CALL_CTX *ctx, X_ITEM *object )
{
   X_ITEM handle __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;

   if( object == NULL || !X_IS_OBJECT( object ) || !x_objSend( ctx, &handle, object, "HANDLE", 0, NULL ) )
      return NULL;

   return ( HWND )x_itemGetPtr( &handle );
}

/* The window of self; an error when it is closed */
HWND x_guiSelfHandle( X_CALL_CTX *ctx, const char *operation )
{
   HWND window = x_guiObjectHandle( ctx, ctx->self );

   if( window == NULL || !IsWindow( window ) )
   {
      x_guiRaise( ctx, operation );
      return NULL;
   }

   return window;
}

/* The window of a gui_Window object; an error for another value or a closed window */
HWND x_guiObjectWindow( X_CALL_CTX *ctx, const X_ITEM *object, const char *operation )
{
   HWND window = NULL;

   if( object != NULL && x_objIsKindOf( object, "GUIWINDOW" ) )
      window = x_guiObjectHandle( ctx, ( X_ITEM * )object );

   if( window == NULL || !IsWindow( window ) )
   {
      x_guiRaise( ctx, operation );
      return NULL;
   }

   return window;
}

/* The window of a gui_Control object; an error for another value or a closed control */
HWND x_guiObjectControl( X_CALL_CTX *ctx, const X_ITEM *object, const char *operation )
{
   HWND control = NULL;

   if( object != NULL && x_objIsKindOf( object, "GUICONTROL" ) )
      control = x_guiObjectHandle( ctx, ( X_ITEM * )object );

   if( control == NULL || !IsWindow( control ) )
   {
      x_guiRaise( ctx, operation );
      return NULL;
   }

   return control;
}

/* The menu of a gui_Menu object; an error for another value */
HMENU x_guiObjectMenu( X_CALL_CTX *ctx, const X_ITEM *object, const char *operation )
{
   HMENU menu = NULL;

   if( object != NULL && x_objIsKindOf( object, "GUIMENU" ) )
      menu = ( HMENU )x_guiObjectHandle( ctx, ( X_ITEM * )object );

   if( menu == NULL || !IsMenu( menu ) )
   {
      x_guiRaise( ctx, operation );
      return NULL;
   }

   return menu;
}

/* Evaluate the block in a data slot of self with self as its parameter; nothing without a block */
void x_guiEvent( X_CALL_CTX *ctx, size_t slot )
{
   const X_ITEM *data = x_objDataSlot( ctx, slot );
   X_ITEM block __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ARG args[1];

   if( data == NULL || !X_IS_BLOCK( data ) )
      return;

   x_itemCopy( &block, data );
   args[0] = ( X_ARG ){ ctx->self, 0 };
   x_evalBlock( ctx, &result, &block, 1, args );
}

/* ---------------------------------------------------------------------------------------------
   DPI, fonts and layout
   --------------------------------------------------------------------------------------------- */

/* A size at 96 DPI scaled to dpi */
int x_guiScale( int value, UINT dpi )
{
   return MulDiv( value, ( int )dpi, USER_DEFAULT_SCREEN_DPI );
}

/* The system message font for dpi, made once for each DPI */
HFONT x_guiFont( UINT dpi )
{
   NONCLIENTMETRICSW metrics;
   HFONT font = NULL;
   size_t i;

   for( i = 0; i < s_fontCount; ++i )
   {
      if( s_fonts[i].dpi == dpi )
         return s_fonts[i].font;
   }

   memset( &metrics, 0, sizeof( metrics ) );
   metrics.cbSize = sizeof( metrics );
   if( s_fontCount < sizeof( s_fonts ) / sizeof( s_fonts[0] ) )
   {
      if( SystemParametersInfoForDpi( SPI_GETNONCLIENTMETRICS, sizeof( metrics ), &metrics, 0, dpi ) )
         font = CreateFontIndirectW( &metrics.lfMessageFont );
   }

   if( font == NULL )
      return ( HFONT )GetStockObject( DEFAULT_GUI_FONT );

   s_fonts[s_fontCount++] = ( X_GUI_FONT ){ dpi, font };

   return font;
}

/* ---------------------------------------------------------------------------------------------
   Events and object lifetime
   --------------------------------------------------------------------------------------------- */

/* Set the C function of an event of a class */
void x_guiHandlerAdd( X_CLASS_HANDLE classHandle, X_GUI_EVENT event, X_METHODPTR function )
{
   size_t i = 0;

   while( i < s_handlerCount && s_handlers[i].handle != classHandle )
      ++i;

   if( i == s_handlerCount )
   {
      s_handlers = ( X_GUI_HANDLERS * )x_xreallocArray( s_handlers, s_handlerCount + 1, sizeof( X_GUI_HANDLERS ) );
      s_handlers[i] = ( X_GUI_HANDLERS ){ classHandle, { NULL } };
      ++s_handlerCount;
   }

   s_handlers[i].functions[event] = function;
}

/* The C function of an event of a class, NULL when the class has none */
static X_METHODPTR s_handlerFind( X_CLASS_HANDLE classHandle, X_GUI_EVENT event )
{
   size_t i;

   for( i = 0; i < s_handlerCount; ++i )
   {
      if( s_handlers[i].handle == classHandle )
         return s_handlers[i].functions[event];
   }

   return NULL;
}

/* Run the function of an event for an object: that of its own class or of the nearest ancestor which has one */
int x_guiHandlerCall( X_ITEM *result, X_ITEM *object, X_GUI_EVENT event, int32_t argc, const X_ARG *args )
{
   const X_CLASS *classInfo = x_clsGet( x_objGetClass( object ) );
   size_t i;

   if( classInfo == NULL )
      return x_errorRaiseNamed( result, 1004, s_eventNames[event] );

   for( i = classInfo->lineageCount; i > 0; --i )
   {
      X_METHODPTR function = s_handlerFind( classInfo->lineage[i - 1], event );

      if( function != NULL )
      {
         x_objCall( result, object, classInfo->lineage[i - 1], function, argc, args );
         return 1;
      }
   }

   return x_errorRaiseNamed( result, 1004, s_eventNames[event] );
}

/* Send s_message to the object in the first argument with the other arguments */
static void s_dispatch( X_CALL_CTX *ctx )
{
   x_objSend( ctx, ctx->result, ( X_ITEM * )ctx->args[0].value, s_message, ctx->argc - 1, ctx->args + 1 );
}

/* Run s_event for the object in the first argument with the other arguments */
static void s_dispatchEvent( X_CALL_CTX *ctx )
{
   x_guiHandlerCall( ctx->result, ( X_ITEM * )ctx->args[0].value, s_event, ctx->argc - 1, ctx->args + 1 );
}

/* Run dispatch for an xDev object with up to two numbers; an error in it ends the running message loop */
static int s_notify( X_FUNCPTR dispatch, X_ITEM *object, X_ITEM *result, int argc, const int64_t *values )
{
   X_ITEM params[2] = { X_ITEM_NIL_INIT, X_ITEM_NIL_INIT };
   X_ARG args[3] = { { object, 0 } };
   int i, success;

   for( i = 0; i < argc; ++i )
   {
      x_itemPutNInt( &params[i], values[i] );
      args[i + 1] = ( X_ARG ){ &params[i], 0 };
   }

   success = x_callbackInvoke( dispatch, result, argc + 1, args );
   for( i = 0; i < argc; ++i )
      x_itemClear( &params[i] );

   if( !success && x_guiRunning )
      PostQuitMessage( 1 );

   return success;
}

/* Send a message to an xDev object with up to two numbers */
int x_guiNotify( X_ITEM *object, const char *message, X_ITEM *result, int argc, const int64_t *values )
{
   s_message = message;

   return s_notify( s_dispatch, object, result, argc, values );
}

/* Give an event of the library to an xDev object with up to two numbers */
int x_guiNotifyEvent( X_ITEM *object, X_GUI_EVENT event, X_ITEM *result, int argc, const int64_t *values )
{
   s_event = event;

   return s_notify( s_dispatchEvent, object, result, argc, values );
}

/* Release what a window keeps and tell its xDev object that the window is gone */
void x_guiDetach( HWND window )
{
   X_ITEM *object = ( X_ITEM * )GetWindowLongPtrW( window, GWLP_USERDATA );
   X_GUI_ANCHOR *anchor = ( X_GUI_ANCHOR * )RemovePropW( window, x_guiAnchorProperty );
   X_ITEM result = X_ITEM_NIL_INIT;
   X_ARG args[1];
   X_CALL_CTX call;

   x_xfree( anchor );
   x_guiToolbarRelease( window );
   x_guiStatusRelease( window );
   RemovePropW( window, x_guiVarProperty );
   RemovePropW( window, x_guiGetProperty );
   RemovePropW( window, x_guiRowsProperty );
   RemovePropW( window, x_guiColorProperty );
   x_guiFontRelease( window );
   if( object == NULL )
      return;

   SetWindowLongPtrW( window, GWLP_USERDATA, 0 );
   args[0] = ( X_ARG ){ object, 0 };
   call = ( X_CALL_CTX ){ args, 1, &result, NULL, NULL };
   s_event = X_GUI_EVENT_DETACH;
   x_callCleanup( s_dispatchEvent, &call );
   x_itemClear( &result );
   x_itemRelease( object );
}

/* The control leaves its window, as RestScreen() takes a GET away */
void x_guiControlDestroy( HWND control )
{
   x_callbackEnterDll();
   x_guiDetach( control );
   DestroyWindow( control );
   x_callbackLeaveDll();
   x_callbackRaisePending();
}

/* A label with a sunken frame looks like a field, as SAY on its own color */
int x_guiLabelBorder( HWND label )
{
   return ( GetWindowLongPtrW( label, GWL_EXSTYLE ) & WS_EX_CLIENTEDGE ) != 0;
}

/* Give a label the sunken frame of a field or take it away */
void x_guiLabelSetBorder( HWND label, int border )
{
   LONG_PTR exStyle = GetWindowLongPtrW( label, GWL_EXSTYLE );
   LONG_PTR style = GetWindowLongPtrW( label, GWL_STYLE );

   x_callbackEnterDll();
   /* A value is shown in the fixed font 9x18 of the GETs */
   if( border )
   {
      x_guiXfontAttach( label );
      SendMessageW( label, WM_SETFONT, ( WPARAM )x_guiFontOf( label, GetDpiForWindow( label ) ), FALSE );
   }

   exStyle = IIF( border, exStyle | WS_EX_CLIENTEDGE, exStyle & ~( LONG_PTR )WS_EX_CLIENTEDGE );
   style = IIF( border, style | SS_CENTERIMAGE, style & ~( LONG_PTR )SS_CENTERIMAGE );
   SetWindowLongPtrW( label, GWL_EXSTYLE, exStyle );
   SetWindowLongPtrW( label, GWL_STYLE, style );
   SetWindowPos( label, NULL, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE );
   x_callbackLeaveDll();
   x_callbackRaisePending();
}

/* The line of AddLineAfter(): one pixel from the end of the text to the right edge, in the middle of the
   text line; the text is measured with the font of the label at every paint */
static void s_lineAfterPaint( HWND label, COLORREF color )
{
   wchar_t text[512];
   int len = GetWindowTextW( label, text, ( int )( sizeof( text ) / sizeof( text[0] ) ) );
   HFONT font = ( HFONT )SendMessageW( label, WM_GETFONT, 0, 0 );
   HDC dc = GetDC( label );
   HGDIOBJ oldFont, oldPen;
   HPEN pen;
   SIZE size = { 0, 0 };
   RECT client;
   int x, y;

   if( dc == NULL )
      return;

   GetClientRect( label, &client );
   oldFont = SelectObject( dc, IIF( font != NULL, ( HGDIOBJ )font, GetStockObject( DEFAULT_GUI_FONT ) ) );
   if( len > 0 )
      GetTextExtentPoint32W( dc, text, len, &size );
   else
      GetTextExtentPoint32W( dc, L"X", 1, &size );

   x = IIF( len > 0, size.cx + x_guiScale( 4, GetDpiForWindow( label ) ), 0 );
   y = size.cy / 2;
   if( x < client.right )
   {
      pen = CreatePen( PS_SOLID, 1, color );
      oldPen = SelectObject( dc, pen );
      MoveToEx( dc, x, y, NULL );
      LineTo( dc, client.right, y );
      SelectObject( dc, oldPen );
      DeleteObject( pen );
   }

   SelectObject( dc, oldFont );
   ReleaseDC( label, dc );
}

/* The label paints itself, then the line after its text */
static LRESULT CALLBACK s_lineAfterProc( HWND label, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR id,
                                         DWORD_PTR data )
{
   LRESULT result;

   switch( message )
   {
   case WM_PAINT:
      result = DefSubclassProc( label, message, wParam, lParam );
      s_lineAfterPaint( label, ( COLORREF )data );
      return result;

   case WM_SETTEXT:
   case WM_SETFONT:
   case WM_SIZE:
      result = DefSubclassProc( label, message, wParam, lParam );
      InvalidateRect( label, NULL, TRUE );
      return result;

   case WM_NCDESTROY:
      RemoveWindowSubclass( label, s_lineAfterProc, id );
      break;
   }

   return DefSubclassProc( label, message, wParam, lParam );
}

/* nColor 0xRRGGBB, -1 the color of the frames of the system */
void x_guiLabelAddLineAfter( HWND label, int color )
{
   COLORREF line = IIF( color >= 0, x_guiColorRef( color ), GetSysColor( COLOR_BTNSHADOW ) );

   x_callbackEnterDll();
   SetWindowSubclass( label, s_lineAfterProc, 1, ( DWORD_PTR )line );
   InvalidateRect( label, NULL, TRUE );
   x_callbackLeaveDll();
}

/* 0 left, 1 right, 2 center, as nAlign of the browse columns */
int x_guiLabelAlign( HWND label )
{
   LONG_PTR type = GetWindowLongPtrW( label, GWL_STYLE ) & SS_TYPEMASK;

   if( type == SS_RIGHT )
      return 1;

   if( type == SS_CENTER )
      return 2;

   return 0;
}

/* Align the text of a label: 0 left, 1 right, 2 center */
void x_guiLabelSetAlign( HWND label, int align )
{
   static const LONG_PTR types[] = { SS_LEFT, SS_RIGHT, SS_CENTER };
   LONG_PTR style = GetWindowLongPtrW( label, GWL_STYLE );

   x_callbackEnterDll();
   SetWindowLongPtrW( label, GWL_STYLE, ( style & ~( LONG_PTR )SS_TYPEMASK ) | types[align] );
   InvalidateRect( label, NULL, TRUE );
   x_callbackLeaveDll();
   x_callbackRaisePending();
}

/* The own text color of a control 0xRRGGBB, -1 when it takes the color of its window */
int x_guiControlColor( HWND control )
{
   return ( int )( ( INT_PTR )GetPropW( control, x_guiColorProperty ) - 1 );
}

/* oControl:color --> nColor: the own text color of a label, frame or check box, NIL for the color of the window */
void x_guiGetColor( X_CALL_CTX *ctx, const char *operation )
{
   HWND control = x_guiSelfHandle( ctx, operation );

   if( control != NULL && x_guiControlColor( control ) >= 0 )
      x_retni( ctx, x_guiControlColor( control ) );
}

/* oControl:color := nColor: the own text color 0xRRGGBB; NIL gives the control the color of its window again */
void x_guiSetColor( X_CALL_CTX *ctx, const char *operation )
{
   HWND control = x_guiSelfHandle( ctx, operation );

   if( control == NULL )
      return;

   if( !X_ISNIL( ctx, 1 ) && ( !X_ISNUM( ctx, 1 ) || x_parni( ctx, 1 ) < 0 || x_parni( ctx, 1 ) > 0xFFFFFF ) )
   {
      x_guiRaise( ctx, operation );
      return;
   }

   x_callbackEnterDll();
   if( X_ISNIL( ctx, 1 ) )
      RemovePropW( control, x_guiColorProperty );
   else
      SetPropW( control, x_guiColorProperty, ( HANDLE )( INT_PTR )( x_parni( ctx, 1 ) + 1 ) );

   x_guiPaintTheme( control );
   InvalidateRect( control, NULL, TRUE );
   x_callbackLeaveDll();
   x_callbackRaisePending();
   x_itemReturn( ctx, x_paritem( ctx, 1 ) );
}

/* Detach a child window of a window that is destroyed */
BOOL CALLBACK x_guiDetachChild( HWND window, LPARAM unused )
{
   UNUSED( unused );
   x_guiDetach( window );

   return TRUE;
}

/* Clear the data of a window object whose window is gone */
void x_guiWindowDetach( X_CALL_CTX *ctx )
{
   X_ITEM *handle = x_objDataSlot( ctx, 1 );
   X_ITEM *menu = x_objDataSlot( ctx, 2 );
   X_ITEM *context = x_objDataSlot( ctx, 3 );
   X_ITEM *keys = x_objDataSlot( ctx, 4 );

   if( handle != NULL )
      x_itemClear( handle );

   if( menu != NULL )
      x_itemClear( menu );

   if( context != NULL )
      x_itemClear( context );

   if( keys != NULL )
      x_itemClear( keys );
}

/* Clear the data of a control object whose control is gone */
void x_guiControlDetach( X_CALL_CTX *ctx )
{
   X_ITEM *handle = x_objDataSlot( ctx, 1 );
   X_ITEM *parent = x_objDataSlot( ctx, 2 );
   X_ITEM *context = x_objDataSlot( ctx, 4 );
   X_ITEM *font = x_objDataSlot( ctx, 5 );

   if( handle != NULL )
      x_itemClear( handle );

   if( parent != NULL )
      x_itemClear( parent );

   if( context != NULL )
      x_itemClear( context );

   if( font != NULL )
      x_itemClear( font );
}

/* ---------------------------------------------------------------------------------------------
   Module registration and shutdown
   --------------------------------------------------------------------------------------------- */

/* Collect a window of the library (EnumThreadWindows) */
static BOOL CALLBACK s_collectWindow( HWND window, LPARAM list )
{
   HWND **cursor = ( HWND ** )list;

   if( ( ATOM )GetClassLongPtrW( window, GCW_ATOM ) == x_guiWindowClass )
      *( *cursor )++ = window;

   return TRUE;
}

/* Count a window of the thread (EnumThreadWindows) */
static BOOL CALLBACK s_countWindow( HWND window, LPARAM count )
{
   UNUSED( window );
   ++*( size_t * )count;

   return TRUE;
}

/* Nothing to do at this stage of the program */
static void s_moduleNone( X_CALL_CTX *ctx )
{
   UNUSED( ctx );
}

/* At the end of the program: close the windows, stop the timers, release the commands and the fonts */
static void s_moduleExit( X_CALL_CTX *ctx )
{
   size_t count = 0;
   HWND *windows, *cursor;

   UNUSED( ctx );
   if( x_guiWindowClass != 0 )
   {
      EnumThreadWindows( GetCurrentThreadId(), s_countWindow, ( LPARAM )&count );
      windows = cursor = ( HWND * )x_xgrabArray( count + 1, sizeof( HWND ) );
      EnumThreadWindows( GetCurrentThreadId(), s_collectWindow, ( LPARAM )&cursor );
      x_callbackEnterDll();
      while( cursor > windows )
         DestroyWindow( *--cursor );

      x_callbackLeaveDll();
      x_xfree( windows );
   }

   x_guiTimersShutdown();

   while( x_guiActionCount > 0 )
   {
      --x_guiActionCount;
      x_itemRelease( x_guiActions[x_guiActionCount].action );
      x_itemRelease( x_guiActions[x_guiActionCount].when );
   }

   if( x_guiActions != NULL )
   {
      x_xfree( x_guiActions );
      x_guiActions = NULL;
      x_guiActionCapacity = 0;
   }

   while( s_fontCount > 0 )
      DeleteObject( s_fonts[--s_fontCount].font );

   x_xfree( s_handlers );
   s_handlers = NULL;
   s_handlerCount = 0;
}

/* Register the module of the library before Main() runs */
__attribute__( ( constructor( 65534 ) ) ) static void s_moduleRegister( void )
{
   x_moduleRegister( s_moduleNone, s_moduleNone, s_moduleExit, s_moduleNone );
}
