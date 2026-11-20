/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library: xDev GUI
 * File: internal-gui.c
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wctype.h>

#include "internal-gui.h"
#include "terminal.h"

#include <commdlg.h>
#include <shellapi.h>
#include <uxtheme.h>
#include <zlib.h>

#include "../../fonts/xfont9x18.h"

#pragma comment( lib, "comctl32" )
#pragma comment( lib, "comdlg32" )
#pragma comment( lib, "uxtheme" )

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

/* The system font made for one DPI */
typedef struct
{
   UINT dpi;
   HFONT font;
} X_GUI_FONT;

/* The anchored sides of a control and its distances from them */
typedef struct
{
   int flags, left, top, right, bottom;
} X_GUI_ANCHOR;

/* One layout of the children of a window */
typedef struct
{
   HWND parent;
   HDWP defer;
   int width, height;
} X_GUI_LAYOUT;

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

/* A key name of a shortcut, its virtual key and the text shown in menus */
typedef struct
{
   const char *name;
   UINT key;
   const char *display;
} X_GUI_KEYNAME;

static const X_GUI_KEYNAME s_keyNames[] = {
   { "DEL", VK_DELETE, "Del" }, { "DELETE", VK_DELETE, "Del" }, { "INS", VK_INSERT, "Ins" },
   { "INSERT", VK_INSERT, "Ins" }, { "HOME", VK_HOME, "Home" }, { "END", VK_END, "End" },
   { "PGUP", VK_PRIOR, "PgUp" }, { "PAGEUP", VK_PRIOR, "PgUp" }, { "PGDN", VK_NEXT, "PgDn" },
   { "PAGEDOWN", VK_NEXT, "PgDn" }, { "ESC", VK_ESCAPE, "Esc" }, { "ESCAPE", VK_ESCAPE, "Esc" },
   { "ENTER", VK_RETURN, "Enter" }, { "RETURN", VK_RETURN, "Enter" }, { "TAB", VK_TAB, "Tab" },
   { "SPACE", VK_SPACE, "Space" }, { "BACKSPACE", VK_BACK, "Backspace" }, { "UP", VK_UP, "Up" },
   { "DOWN", VK_DOWN, "Down" }, { "LEFT", VK_LEFT, "Left" }, { "RIGHT", VK_RIGHT, "Right" }
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

/* A toolbar stands right in its window, at a side of it */
typedef struct
{
   X_GUI_SIDE side;
   int colors[X_GUI_COLOR_COUNT];
   HIMAGELIST images;
   HWND grip;
   wchar_t **tips;
   UINT *keys;
   UINT *modifiers;
   size_t count;
} X_GUI_BAR;

/* Sizes of a sidebar at 96 DPI: the strip of pictures, the header of the panel, the left margin of the title,
   the gap at the right of the actions, the buttons of the actions, the mark of the shown page, the edge dragged
   by the mouse, the first and the smallest width of the panel and the room it leaves for the window; the opacity
   of the pictures of pages not shown */
enum
{
   X_GUI_SIDEBAR_STRIP = 48,
   X_GUI_SIDEBAR_HEADER = 35,
   X_GUI_SIDEBAR_TITLE = 20,
   X_GUI_SIDEBAR_GAP = 8,
   X_GUI_SIDEBAR_ACTION = 22,
   X_GUI_SIDEBAR_MARK = 2,
   X_GUI_SIDEBAR_EDGE = 4,
   X_GUI_SIDEBAR_WIDTH = 260,
   X_GUI_SIDEBAR_MINWIDTH = 170,
   X_GUI_SIDEBAR_REST = 100,
   X_GUI_SIDEBAR_PALE = 150
};

/* Colors of a sidebar as 0xRRGGBB: the strip of pictures without barBackground and the line of one pixel at the
   right edge of the strip */
enum
{
   X_GUI_SIDEBAR_BAR = 0x181818,
   X_GUI_SIDEBAR_BORDER = 0x2B2B2B
};

/* A page of a sidebar: its panel, the bar of its actions in the header and its title */
typedef struct
{
   HWND panel;
   HWND actions;
   wchar_t *title;
} X_GUI_SIDEPAGE;

/* A sidebar: the bar of pages at the top of the strip, the bar of buttons at its bottom and the panel of the shown
   page; width of the panel at 96 DPI, page 0 when the panel is hidden, colors -1 for the system ones, drag set
   while the mouse drags the edge */
typedef struct
{
   HWND activity;
   HWND buttons;
   X_GUI_SIDEPAGE *pages;
   size_t count;
   int page;
   int width;
   int colors[X_GUI_SIDEBAR_COLOR_COUNT];
   int drag;
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

/* The messages of a list box or of a combo box for the same operations */
typedef struct
{
   UINT add, reset, count, getSelection, setSelection, textLength, text;
} X_GUI_LIST_MESSAGES;

static const X_GUI_LIST_MESSAGES s_listBoxMessages = { LB_ADDSTRING, LB_RESETCONTENT, LB_GETCOUNT, LB_GETCURSEL,
                                                        LB_SETCURSEL, LB_GETTEXTLEN, LB_GETTEXT };
static const X_GUI_LIST_MESSAGES s_comboBoxMessages = { CB_ADDSTRING, CB_RESETCONTENT, CB_GETCOUNT, CB_GETCURSEL,
                                                        CB_SETCURSEL, CB_GETLBTEXTLEN, CB_GETLBTEXT };

static const wchar_t s_windowClassName[] = L"xDev.GUI.Window";
static const wchar_t s_browseClassName[] = L"xDev.GUI.Browse";
static const wchar_t s_dpiProperty[] = L"xDev.GUI.Dpi";
static const wchar_t s_anchorProperty[] = L"xDev.GUI.Anchor";
static const wchar_t s_defaultProperty[] = L"xDev.GUI.Default";
static const wchar_t s_cancelProperty[] = L"xDev.GUI.Cancel";
static const wchar_t s_modalProperty[] = L"xDev.GUI.Modal";
static const wchar_t s_toolTipProperty[] = L"xDev.GUI.ToolTip";
static const wchar_t s_toolbarProperty[] = L"xDev.GUI.Toolbar";
static const wchar_t s_sidebarProperty[] = L"xDev.GUI.Sidebar";
static const wchar_t s_paintProperty[] = L"xDev.GUI.Paint";
static const wchar_t s_maskProperty[] = L"xDev.GUI.Mask";
static const wchar_t s_focusProperty[] = L"xDev.GUI.Focus";
static const wchar_t s_gridProperty[] = L"xDev.GUI.Grid";
static const wchar_t s_fontProperty[] = L"xDev.GUI.Font";
static const wchar_t s_fillProperty[] = L"xDev.GUI.Fill";
static const wchar_t s_keysProperty[] = L"xDev.GUI.Keys";
static const wchar_t s_statusProperty[] = L"xDev.GUI.Status";
static const wchar_t s_varProperty[] = L"xDev.GUI.Var";
static const wchar_t s_getProperty[] = L"xDev.GUI.Get";
/* The number of rows a GET of several rows grows to while it has the focus */
static const wchar_t s_rowsProperty[] = L"xDev.GUI.Rows";

static ATOM s_windowClass;
static ATOM s_browseClass;
static X_GUI_FONT s_fonts[16];
static size_t s_fontCount;
static int s_dpiAware;
static HWND s_mainWindow;
static int s_running;
static int s_controlId = X_GUI_CONTROL_FIRST;
static int s_menuId = X_GUI_MENU_FIRST;
static X_GUI_ACTION *s_actions;
static size_t s_actionCount;
static size_t s_actionCapacity;
static int s_commonControls;
static int s_quiet;
static int s_validating;
static int s_reading;
/* Set by EndRead() and by closing the window: 1 ends Read() with saving, 0 without; -1 lets Read() go on */
static int s_readEnd = -1;
/* The window of the running Read() and the window closed during it, which closes after Read() */
static HWND s_readWindow;
static HWND s_readClose;
/* The fields of Read( GetList ) in their order; without them all one-line fields of the window */
static const HWND *s_readGets;
static size_t s_readGetCount;
static const char *s_message;

/* The extended style of menus, see x_guiMenuStyle() */
static int s_menuExtended;
static int s_menuImageSize = 16;
static HFONT s_menuFontHandle;
static UINT s_menuFontDpi;

static void s_menuMeasure( HWND window, MEASUREITEMSTRUCT *measure );
static void s_menuDraw( HWND window, const DRAWITEMSTRUCT *draw );
static LRESULT s_menuChar( HMENU menu, wchar_t key );

/* Class description read by the compiler for typed code (LOCAL oWin AS gui_Window). */
#define X_GUI_INTERFACE \
   "XDEVCLS 1\n" \
   "C GUI_WINDOW -\n" \
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
   "C GUI_PANEL GUI_WINDOW\n" \
   "M METHOD NEW 1 ANY - PARENT X Y WIDTH HEIGHT\n" \
   "M ACCESS ANCHOR 1 ANY -\n" \
   "M ASSIGN ANCHOR 1 ANY - VALUE\n" \
   "C GUI_DIALOG GUI_WINDOW\n" \
   "M METHOD NEW 1 ANY - OWNER TITLE WIDTH HEIGHT\n" \
   "M METHOD SHOWMODAL 1 ANY -\n" \
   "M METHOD ENDDIALOG 1 ANY - RESULT\n" \
   "M ACCESS RESULT 1 ANY -\n" \
   "C GUI_MENU -\n" \
   "M DATA HANDLE 17 ANY -\n" \
   "M METHOD NEW 1 ANY - POPUP\n" \
   "M METHOD ADDMENU 1 ANY - TEXT BITMAP\n" \
   "M METHOD ADD 1 ANY - TEXT ACTION SHORTCUT BITMAP WHEN\n" \
   "M METHOD ADDSEPARATOR 1 ANY -\n" \
   "C GUI_CONTROL -\n" \
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
   "C GUI_FONT -\n" \
   "M DATA SIZE 1 ANY -\n" \
   "M DATA BOLD 1 ANY -\n" \
   "M DATA ITALIC 1 ANY -\n" \
   "M DATA NAME 1 ANY -\n" \
   "M METHOD NEW 1 ANY - SIZE BOLD ITALIC NAME\n" \
   "C GUI_LABEL GUI_CONTROL\n" \
   "M METHOD NEW 1 ANY - PARENT X Y WIDTH HEIGHT TEXT\n" \
   "M ACCESS BORDER 1 ANY -\n" \
   "M ASSIGN BORDER 1 ANY - VALUE\n" \
   "M ACCESS ALIGN 1 ANY -\n" \
   "M ASSIGN ALIGN 1 ANY - VALUE\n" \
   "C GUI_SAY GUI_LABEL\n" \
   "M METHOD NEW 1 ANY - PARENT X Y LEN HEIGHT TEXT\n" \
   "C GUI_EDIT GUI_CONTROL\n" \
   "M DATA ONCHANGE 1 CODEBLOCK -\n" \
   "M DATA ONVALID 1 CODEBLOCK -\n" \
   "M ACCESS PICTURE 1 ANY -\n" \
   "M ASSIGN PICTURE 1 ANY - VALUE\n" \
   "M METHOD NEW 1 ANY - PARENT X Y WIDTH HEIGHT TEXT\n" \
   "M ACCESS READONLY 1 ANY -\n" \
   "M ASSIGN READONLY 1 ANY - VALUE\n" \
   "M ACCESS MAXLENGTH 1 ANY -\n" \
   "M ASSIGN MAXLENGTH 1 ANY - VALUE\n" \
   "C GUI_GET GUI_EDIT\n" \
   "M METHOD NEW 1 ANY - PARENT X Y LEN HEIGHT VAR PICTURE ROWS MAX\n" \
   "C GUI_MEMO GUI_EDIT\n" \
   "M METHOD NEW 1 ANY - PARENT X Y WIDTH HEIGHT TEXT\n" \
   "C GUI_BUTTON GUI_CONTROL\n" \
   "M DATA ONCLICK 1 CODEBLOCK -\n" \
   "M METHOD NEW 1 ANY - PARENT X Y WIDTH HEIGHT TEXT\n" \
   "M ACCESS ISDEFAULT 1 ANY -\n" \
   "M ASSIGN ISDEFAULT 1 ANY - VALUE\n" \
   "M ACCESS ISCANCEL 1 ANY -\n" \
   "M ASSIGN ISCANCEL 1 ANY - VALUE\n" \
   "C GUI_CHECKBOX GUI_CONTROL\n" \
   "M DATA ONCHANGE 1 CODEBLOCK -\n" \
   "M METHOD NEW 1 ANY - PARENT X Y WIDTH HEIGHT TEXT\n" \
   "M ACCESS CHECKED 1 ANY -\n" \
   "M ASSIGN CHECKED 1 ANY - VALUE\n" \
   "C GUI_RADIOBUTTON GUI_CHECKBOX\n" \
   "M METHOD NEW 1 ANY - PARENT X Y WIDTH HEIGHT TEXT\n" \
   "C GUI_GROUPBOX GUI_CONTROL\n" \
   "M METHOD NEW 1 ANY - PARENT X Y WIDTH HEIGHT TEXT\n" \
   "C GUI_LISTBOX GUI_CONTROL\n" \
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
   "C GUI_COMBOBOX GUI_LISTBOX\n" \
   "M METHOD NEW 1 ANY - PARENT X Y WIDTH HEIGHT\n" \
   "C GUI_PROGRESSBAR GUI_CONTROL\n" \
   "M METHOD NEW 1 ANY - PARENT X Y WIDTH HEIGHT\n" \
   "M ACCESS VALUE 1 ANY -\n" \
   "M ASSIGN VALUE 1 ANY - VALUE\n" \
   "M ACCESS MAXIMUM 1 ANY -\n" \
   "M ASSIGN MAXIMUM 1 ANY - VALUE\n" \
   "C GUI_STATUSBAR GUI_CONTROL\n" \
   "M METHOD NEW 1 ANY - PARENT TEXT\n" \
   "M METHOD ADD 1 ANY - WIDTH TEXT RAISED\n" \
   "M METHOD SETTEXT 1 ANY - PANEL TEXT\n" \
   "M ACCESS SCOREBOARD 1 ANY -\n" \
   "M ASSIGN SCOREBOARD 1 ANY - VALUE\n" \
   "M ACCESS FOREGROUND 1 ANY -\n" \
   "M ASSIGN FOREGROUND 1 ANY - VALUE\n" \
   "M ACCESS BACKGROUND 1 ANY -\n" \
   "M ASSIGN BACKGROUND 1 ANY - VALUE\n" \
   "C GUI_DATEPICKER GUI_CONTROL\n" \
   "M DATA ONCHANGE 1 CODEBLOCK -\n" \
   "M METHOD NEW 1 ANY - PARENT X Y WIDTH HEIGHT\n" \
   "C GUI_SLIDER GUI_CONTROL\n" \
   "M DATA ONCHANGE 1 CODEBLOCK -\n" \
   "M METHOD NEW 1 ANY - PARENT X Y WIDTH HEIGHT\n" \
   "M ACCESS MINIMUM 1 ANY -\n" \
   "M ASSIGN MINIMUM 1 ANY - VALUE\n" \
   "M ACCESS MAXIMUM 1 ANY -\n" \
   "M ASSIGN MAXIMUM 1 ANY - VALUE\n" \
   "C GUI_LISTVIEW GUI_CONTROL\n" \
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
   "C GUI_TREEVIEW GUI_CONTROL\n" \
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
   "C GUI_TABPAGE GUI_WINDOW\n" \
   "C GUI_TABCONTROL GUI_CONTROL\n" \
   "M DATA ONSELECT 1 CODEBLOCK -\n" \
   "M METHOD NEW 1 ANY - PARENT X Y WIDTH HEIGHT\n" \
   "M METHOD ADDPAGE 1 ANY - TITLE\n" \
   "M METHOD PAGE 1 ANY - INDEX\n" \
   "M ACCESS COUNT 1 ANY -\n" \
   "M ACCESS SELECTED 1 ANY -\n" \
   "M ASSIGN SELECTED 1 ANY - VALUE\n" \
   "C GUI_BROWSE GUI_CONTROL\n" \
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
   "C GUI_TOOLBAR GUI_CONTROL\n" \
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
   "C GUI_SIDEBAR GUI_WINDOW\n" \
   "M DATA ONCHANGE 1 CODEBLOCK -\n" \
   "M METHOD NEW 1 ANY - PARENT\n" \
   "M METHOD ADD 1 ANY - TITLE BITMAP TOOLTIP SHORTCUT MENU\n" \
   "M METHOD PANEL 1 ANY - PAGE\n" \
   "M METHOD ADDACTION 1 ANY - PAGE BITMAP ACTION TOOLTIP\n" \
   "M METHOD ADDBUTTON 1 ANY - BITMAP ACTION TOOLTIP\n" \
   "M ACCESS PAGE 1 ANY -\n" \
   "M ASSIGN PAGE 1 ANY - VALUE\n" \
   "M ACCESS WIDTH 1 ANY -\n" \
   "M ASSIGN WIDTH 1 ANY - VALUE\n" \
   "M ACCESS BARBACKGROUND 1 ANY -\n" \
   "M ASSIGN BARBACKGROUND 1 ANY - VALUE\n" \
   "M ACCESS ACCENT 1 ANY -\n" \
   "M ASSIGN ACCENT 1 ANY - VALUE\n" \
   "C GUI_TIMER -\n" \
   "M DATA ONTIMER 1 CODEBLOCK -\n" \
   "M DATA PARENT 17 ANY -\n" \
   "M METHOD NEW 1 ANY - WINDOW INTERVAL ACTION\n" \
   "M METHOD START 1 ANY -\n" \
   "M METHOD STOP 1 ANY -\n" \
   "M ACCESS ACTIVE 1 ANY -\n" \
   "M ACCESS INTERVAL 1 ANY -\n" \
   "M ASSIGN INTERVAL 1 ANY - VALUE\n" \
   "C GUI_APPLICATION -\n" \
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
static wchar_t *s_crlf( wchar_t *text )
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
static int s_hasClass( HWND window, const wchar_t *name )
{
   wchar_t className[32];

   return GetClassNameW( window, className, 32 ) && lstrcmpiW( className, name ) == 0;
}

/* Whether a window is an edit field of several lines */
static int s_isMultiline( HWND window )
{
   return s_hasClass( window, L"Edit" ) && ( GetWindowLongPtrW( window, GWL_STYLE ) & ES_MULTILINE ) != 0;
}

/* Whether a window is a radio button */
static int s_isRadio( HWND window )
{
   if( window == NULL || !s_hasClass( window, L"Button" ) )
      return 0;

   return ( GetWindowLongPtrW( window, GWL_STYLE ) & BS_TYPEMASK ) == BS_AUTORADIOBUTTON;
}

/* A check box or a radio button: in GetList of ReadModal() it is a field as an edit field, Space changes it */
static int s_isChoice( HWND window )
{
   LONG_PTR type;

   if( window == NULL || !s_hasClass( window, L"Button" ) )
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
static HWND s_objectHandle( X_CALL_CTX *ctx, X_ITEM *object )
{
   X_ITEM handle __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;

   if( object == NULL || !X_IS_OBJECT( object ) || !x_objSend( ctx, &handle, object, "HANDLE", 0, NULL ) )
      return NULL;

   return ( HWND )x_itemGetPtr( &handle );
}

/* The window of self; an error when it is closed */
HWND x_guiSelfHandle( X_CALL_CTX *ctx, const char *operation )
{
   HWND window = s_objectHandle( ctx, ctx->self );

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

   if( object != NULL && x_objIsKindOf( object, "GUI_WINDOW" ) )
      window = s_objectHandle( ctx, ( X_ITEM * )object );

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

   if( object != NULL && x_objIsKindOf( object, "GUI_CONTROL" ) )
      control = s_objectHandle( ctx, ( X_ITEM * )object );

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

   if( object != NULL && x_objIsKindOf( object, "GUI_MENU" ) )
      menu = ( HMENU )s_objectHandle( ctx, ( X_ITEM * )object );

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
   The fixed font 9x18 of the GT drivers for GETs and tables: its bitmaps are made once into a TrueType
   font in memory, every pixel a square of 64 x 64 units, and given to this program only
   --------------------------------------------------------------------------------------------- */

/* Font units of one pixel and the pixels above the baseline */
#define X_XFONT_UNIT 64
#define X_XFONT_ASCENT 14

static const wchar_t s_xfontName[] = L"xFont 9x18";

/* A growing buffer of the font file */
typedef struct
{
   unsigned char *data;
   size_t size;
   size_t capacity;
} X_XFONT_BUF;

/* Append bytes to the buffer */
static void s_xbufPut( X_XFONT_BUF *buf, const void *data, size_t size )
{
   if( buf->size + size > buf->capacity )
   {
      buf->capacity = ( buf->size + size ) * 2 + 256;
      buf->data = ( unsigned char * )x_xrealloc( buf->data, buf->capacity );
   }

   memcpy( buf->data + buf->size, data, size );
   buf->size += size;
}

/* Append a big-endian 16-bit word */
static void s_xbufU16( X_XFONT_BUF *buf, unsigned value )
{
   const unsigned char bytes[2] = { ( unsigned char )( value >> 8 ), ( unsigned char )value };

   s_xbufPut( buf, bytes, 2 );
}

/* Append a big-endian 32-bit word */
static void s_xbufU32( X_XFONT_BUF *buf, uint32_t value )
{
   s_xbufU16( buf, ( unsigned )( value >> 16 ) & 0xFFFF );
   s_xbufU16( buf, ( unsigned )value & 0xFFFF );
}

/* Fill the buffer with zeros up to a 4-byte boundary */
static void s_xbufAlign( X_XFONT_BUF *buf )
{
   static const unsigned char zero[4] = { 0, 0, 0, 0 };

   if( buf->size % 4 != 0 )
      s_xbufPut( buf, zero, 4 - buf->size % 4 );
}

/* The checksum of TrueType: the sum of big-endian 32-bit words, the last one filled with zeros */
static uint32_t s_xbufSum( const unsigned char *data, size_t size )
{
   uint32_t sum = 0;
   size_t i, j;

   for( i = 0; i < size; i += 4 )
   {
      uint32_t word = 0;

      for( j = 0; j < 4; ++j )
         word = ( word << 8 ) | IIF( i + j < size, data[i + j], 0 );

      sum += word;
   }

   return sum;
}

/* One glyph: every run of pixels in a row is a rectangle drawn clockwise; returns its left side */
static int s_xfontGlyph( X_XFONT_BUF *glyf, size_t index, unsigned *points, unsigned *contours )
{
   int runs[X_FONT9X18_HEIGHT * 5][3];
   int count = 0, row, x, start, i, j;
   int xMin = X_FONT9X18_WIDTH, xMax = 0, yMin = X_XFONT_ASCENT, yMax = X_XFONT_ASCENT - X_FONT9X18_HEIGHT;
   int previousX = 0, previousY = 0;

   for( row = 0; row < X_FONT9X18_HEIGHT; ++row )
   {
      for( x = 0; x < X_FONT9X18_WIDTH; )
      {
         if( !( s_font9x18Bitmaps[index][row] & ( 0x8000 >> x ) ) )
         {
            ++x;
            continue;
         }

         for( start = x; x < X_FONT9X18_WIDTH && ( s_font9x18Bitmaps[index][row] & ( 0x8000 >> x ) ); ++x )
            ;

         runs[count][0] = start;
         runs[count][1] = x;
         runs[count][2] = row;
         xMin = IIF( start < xMin, start, xMin );
         xMax = IIF( x > xMax, x, xMax );
         yMax = IIF( X_XFONT_ASCENT - row > yMax, X_XFONT_ASCENT - row, yMax );
         yMin = IIF( X_XFONT_ASCENT - row - 1 < yMin, X_XFONT_ASCENT - row - 1, yMin );
         ++count;
      }
   }

   if( count == 0 )
      return 0;

   s_xbufU16( glyf, ( unsigned )count );
   s_xbufU16( glyf, ( unsigned )( xMin * X_XFONT_UNIT ) & 0xFFFF );
   s_xbufU16( glyf, ( unsigned )( yMin * X_XFONT_UNIT ) & 0xFFFF );
   s_xbufU16( glyf, ( unsigned )( xMax * X_XFONT_UNIT ) & 0xFFFF );
   s_xbufU16( glyf, ( unsigned )( yMax * X_XFONT_UNIT ) & 0xFFFF );
   for( i = 0; i < count; ++i )
      s_xbufU16( glyf, ( unsigned )( i * 4 + 3 ) );

   s_xbufU16( glyf, 0 );
   for( i = 0; i < count * 4; ++i )
      s_xbufPut( glyf, "\x01", 1 );

   for( i = 0; i < count; ++i )
   {
      const int xs[4] = { runs[i][0], runs[i][0], runs[i][1], runs[i][1] };

      for( j = 0; j < 4; ++j )
      {
         s_xbufU16( glyf, ( unsigned )( ( xs[j] - previousX ) * X_XFONT_UNIT ) & 0xFFFF );
         previousX = xs[j];
      }
   }

   for( i = 0; i < count; ++i )
   {
      const int bottom = X_XFONT_ASCENT - runs[i][2] - 1;
      const int ys[4] = { bottom, bottom + 1, bottom + 1, bottom };

      for( j = 0; j < 4; ++j )
      {
         s_xbufU16( glyf, ( unsigned )( ( ys[j] - previousY ) * X_XFONT_UNIT ) & 0xFFFF );
         previousY = ys[j];
      }
   }

   *points = IIF( ( unsigned )count * 4 > *points, ( unsigned )count * 4, *points );
   *contours = IIF( ( unsigned )count > *contours, ( unsigned )count, *contours );

   return xMin * X_XFONT_UNIT;
}

/* A negative number of font units as an unsigned 16-bit word */
static unsigned s_xfontNegative( unsigned value )
{
   return ( 0x10000 - value ) & 0xFFFF;
}

/* The whole TrueType font: OS/2, cmap, glyf, head, hhea, hmtx, loca, maxp, name and post */
static X_XFONT_BUF s_xfontBuild( void )
{
   static const char tags[10][5] = { "OS/2", "cmap", "glyf", "head", "hhea", "hmtx", "loca", "maxp", "name", "post" };
   static const char *names[5] = { "xFont 9x18", "Regular", "xFont 9x18", "xFont 9x18", "xFont9x18" };
   static const int ids[5] = { 1, 2, 3, 4, 6 };
   X_XFONT_BUF table[10], font, strings;
   const unsigned glyphs = X_FONT9X18_GLYPH_COUNT;
   const unsigned u = X_XFONT_UNIT;
   const unsigned descent = ( X_FONT9X18_HEIGHT - X_XFONT_ASCENT ) * X_XFONT_UNIT;
   unsigned points = 0, contours = 0, segments = 1, search = 1, entry = 0, i;
   uint32_t *loca = ( uint32_t * )x_xgrabArray( glyphs + 1, sizeof( uint32_t ) );
   int *lsb = ( int * )x_xgrabArray( glyphs, sizeof( int ) );
   size_t offset, headAt = 0;
   const char *name;
   uint32_t adjust;

   memset( table, 0, sizeof( table ) );
   memset( &font, 0, sizeof( font ) );
   memset( &strings, 0, sizeof( strings ) );
   for( i = 0; i < glyphs; ++i )
   {
      loca[i] = ( uint32_t )table[2].size;
      lsb[i] = s_xfontGlyph( &table[2], i, &points, &contours );
      s_xbufAlign( &table[2] );
   }

   loca[glyphs] = ( uint32_t )table[2].size;

   /* OS/2 version 3: regular, monospaced, Latin 1 and Latin 2 */
   s_xbufU16( &table[0], 3 );
   s_xbufU16( &table[0], 9 * u );
   s_xbufU16( &table[0], 400 );
   s_xbufU16( &table[0], 5 );
   s_xbufU16( &table[0], 0 );
   for( i = 0; i < 8; ++i )
      s_xbufU16( &table[0], IIF( i % 4 < 2, 9 * u, 0 ) );

   s_xbufU16( &table[0], u );
   s_xbufU16( &table[0], 5 * u );
   s_xbufU16( &table[0], 0 );
   s_xbufPut( &table[0], "\x02\x00\x05\x09\x00\x00\x00\x00\x00\x00", 10 );
   s_xbufU32( &table[0], 0x7 );
   s_xbufU32( &table[0], 0 );
   s_xbufU32( &table[0], 0 );
   s_xbufU32( &table[0], 0 );
   s_xbufPut( &table[0], "XDEV", 4 );
   s_xbufU16( &table[0], 0x40 );
   s_xbufU16( &table[0], 0x20 );
   s_xbufU16( &table[0], 0xFFFD );
   s_xbufU16( &table[0], X_XFONT_ASCENT * u );
   s_xbufU16( &table[0], s_xfontNegative( descent ) );
   s_xbufU16( &table[0], 0 );
   s_xbufU16( &table[0], X_XFONT_ASCENT * u );
   s_xbufU16( &table[0], descent );
   s_xbufU32( &table[0], 0x3 );
   s_xbufU32( &table[0], 0 );
   s_xbufU16( &table[0], 7 * u );
   s_xbufU16( &table[0], 10 * u );
   s_xbufU16( &table[0], 0 );
   s_xbufU16( &table[0], 32 );
   s_xbufU16( &table[0], 1 );

   /* cmap format 4: a segment for every run of following code points; glyph 0 is .notdef */
   for( i = 2; i < glyphs; ++i )
   {
      if( s_font9x18Codepoints[i] != s_font9x18Codepoints[i - 1] + 1 )
         ++segments;
   }

   ++segments;
   for( ; search * 2 <= segments; search *= 2, ++entry )
      ;

   s_xbufU16( &table[1], 0 );
   s_xbufU16( &table[1], 1 );
   s_xbufU16( &table[1], 3 );
   s_xbufU16( &table[1], 1 );
   s_xbufU32( &table[1], 12 );
   s_xbufU16( &table[1], 4 );
   s_xbufU16( &table[1], 16 + 8 * segments );
   s_xbufU16( &table[1], 0 );
   s_xbufU16( &table[1], segments * 2 );
   s_xbufU16( &table[1], search * 2 );
   s_xbufU16( &table[1], entry );
   s_xbufU16( &table[1], segments * 2 - search * 2 );
   for( i = 1; i < glyphs; ++i )
   {
      if( i + 1 == glyphs || s_font9x18Codepoints[i + 1] != s_font9x18Codepoints[i] + 1 )
         s_xbufU16( &table[1], s_font9x18Codepoints[i] );
   }

   s_xbufU16( &table[1], 0xFFFF );
   s_xbufU16( &table[1], 0 );
   for( i = 1; i < glyphs; ++i )
   {
      if( i == 1 || s_font9x18Codepoints[i] != s_font9x18Codepoints[i - 1] + 1 )
         s_xbufU16( &table[1], s_font9x18Codepoints[i] );
   }

   s_xbufU16( &table[1], 0xFFFF );
   for( i = 1; i < glyphs; ++i )
   {
      if( i == 1 || s_font9x18Codepoints[i] != s_font9x18Codepoints[i - 1] + 1 )
         s_xbufU16( &table[1], ( i - s_font9x18Codepoints[i] ) & 0xFFFF );
   }

   s_xbufU16( &table[1], 1 );
   for( i = 0; i < segments; ++i )
      s_xbufU16( &table[1], 0 );

   /* head: 18 pixels to the em, long offsets in loca */
   s_xbufU32( &table[3], 0x00010000 );
   s_xbufU32( &table[3], 0x00010000 );
   s_xbufU32( &table[3], 0 );
   s_xbufU32( &table[3], 0x5F0F3CF5 );
   s_xbufU16( &table[3], 0x000B );
   s_xbufU16( &table[3], X_FONT9X18_HEIGHT * u );
   for( i = 0; i < 4; ++i )
      s_xbufU32( &table[3], 0 );

   s_xbufU16( &table[3], 0 );
   s_xbufU16( &table[3], s_xfontNegative( descent ) );
   s_xbufU16( &table[3], 9 * u );
   s_xbufU16( &table[3], X_XFONT_ASCENT * u );
   s_xbufU16( &table[3], 0 );
   s_xbufU16( &table[3], 8 );
   s_xbufU16( &table[3], 2 );
   s_xbufU16( &table[3], 1 );
   s_xbufU16( &table[3], 0 );

   /* hhea and hmtx: one advance of 9 pixels for all glyphs */
   s_xbufU32( &table[4], 0x00010000 );
   s_xbufU16( &table[4], X_XFONT_ASCENT * u );
   s_xbufU16( &table[4], s_xfontNegative( descent ) );
   s_xbufU16( &table[4], 0 );
   s_xbufU16( &table[4], 9 * u );
   s_xbufU16( &table[4], 0 );
   s_xbufU16( &table[4], 0 );
   s_xbufU16( &table[4], 9 * u );
   s_xbufU16( &table[4], 1 );
   for( i = 0; i < 7; ++i )
      s_xbufU16( &table[4], 0 );

   s_xbufU16( &table[4], 1 );
   s_xbufU16( &table[5], 9 * u );
   for( i = 0; i < glyphs; ++i )
      s_xbufU16( &table[5], ( unsigned )lsb[i] );

   /* loca and maxp */
   for( i = 0; i <= glyphs; ++i )
      s_xbufU32( &table[6], loca[i] );

   s_xbufU32( &table[7], 0x00010000 );
   s_xbufU16( &table[7], glyphs );
   s_xbufU16( &table[7], points );
   s_xbufU16( &table[7], contours );
   s_xbufU16( &table[7], 0 );
   s_xbufU16( &table[7], 0 );
   s_xbufU16( &table[7], 2 );
   for( i = 0; i < 8; ++i )
      s_xbufU16( &table[7], 0 );

   /* name: Windows, Unicode, English */
   s_xbufU16( &table[8], 0 );
   s_xbufU16( &table[8], 5 );
   s_xbufU16( &table[8], 6 + 12 * 5 );
   for( i = 0; i < 5; ++i )
   {
      s_xbufU16( &table[8], 3 );
      s_xbufU16( &table[8], 1 );
      s_xbufU16( &table[8], 0x409 );
      s_xbufU16( &table[8], ( unsigned )ids[i] );
      s_xbufU16( &table[8], ( unsigned )strlen( names[i] ) * 2 );
      s_xbufU16( &table[8], ( unsigned )strings.size );
      for( name = names[i]; *name != '\0'; ++name )
         s_xbufU16( &strings, ( unsigned char )*name );
   }

   s_xbufPut( &table[8], strings.data, strings.size );
   x_xfree( strings.data );

   /* post version 3: fixed pitch, no glyph names */
   s_xbufU32( &table[9], 0x00030000 );
   s_xbufU32( &table[9], 0 );
   s_xbufU16( &table[9], s_xfontNegative( 2 * u ) );
   s_xbufU16( &table[9], u );
   s_xbufU32( &table[9], 1 );
   for( i = 0; i < 4; ++i )
      s_xbufU32( &table[9], 0 );

   /* The file: the table directory, then the tables on 4-byte boundaries */
   s_xbufU32( &font, 0x00010000 );
   s_xbufU16( &font, 10 );
   s_xbufU16( &font, 8 * 16 );
   s_xbufU16( &font, 3 );
   s_xbufU16( &font, 10 * 16 - 8 * 16 );
   offset = 12 + 16 * 10;
   for( i = 0; i < 10; ++i )
   {
      s_xbufPut( &font, tags[i], 4 );
      s_xbufU32( &font, s_xbufSum( table[i].data, table[i].size ) );
      s_xbufU32( &font, ( uint32_t )offset );
      s_xbufU32( &font, ( uint32_t )table[i].size );
      offset += ( table[i].size + 3 ) & ~( size_t )3;
   }

   for( i = 0; i < 10; ++i )
   {
      if( i == 3 )
         headAt = font.size;

      s_xbufPut( &font, table[i].data, table[i].size );
      s_xbufAlign( &font );
      x_xfree( table[i].data );
   }

   adjust = 0xB1B0AFBA - s_xbufSum( font.data, font.size );
   font.data[headAt + 8] = ( unsigned char )( adjust >> 24 );
   font.data[headAt + 9] = ( unsigned char )( adjust >> 16 );
   font.data[headAt + 10] = ( unsigned char )( adjust >> 8 );
   font.data[headAt + 11] = ( unsigned char )adjust;
   x_xfree( loca );
   x_xfree( lsb );

   return font;
}

/* The font is made and given to this program once, at its first use */
static int s_xfontReady( void )
{
   static int state;
   static X_XFONT_BUF font;
   DWORD count = 0;

   if( state == 0 )
   {
      font = s_xfontBuild();
      state = IIF( AddFontMemResourceEx( font.data, ( DWORD )font.size, NULL, &count ) != NULL, 1, -1 );
   }

   return state == 1;
}

/* GETs and tables write with the fixed font 9x18 unless the program gives them another one */
static void s_xfontAttach( HWND control )
{
   X_GUI_OWNFONT *own;

   if( GetPropW( control, s_fontProperty ) != NULL )
      return;

   own = ( X_GUI_OWNFONT * )x_xgrab( sizeof( X_GUI_OWNFONT ) );
   memset( own, 0, sizeof( *own ) );
   lstrcpynW( own->name, s_xfontName, LF_FACESIZE );
   SetPropW( control, s_fontProperty, own );
}

/* The system font changed by the gui_Font of the control and made for the DPI of its window */
static HFONT s_ownFontCreate( const X_GUI_OWNFONT *own, UINT dpi )
{
   NONCLIENTMETRICSW metrics;
   LOGFONTW font;

   memset( &metrics, 0, sizeof( metrics ) );
   metrics.cbSize = sizeof( metrics );
   if( SystemParametersInfoForDpi( SPI_GETNONCLIENTMETRICS, sizeof( metrics ), &metrics, 0, dpi ) )
      font = metrics.lfMessageFont;
   else
   {
      memset( &font, 0, sizeof( font ) );
      font.lfHeight = -MulDiv( 9, ( int )dpi, 72 );
      lstrcpynW( font.lfFaceName, L"Segoe UI", LF_FACESIZE );
   }

   if( own->size > 0 )
      font.lfHeight = -MulDiv( own->size, ( int )dpi, 72 );

   font.lfWeight = IIF( own->bold, FW_BOLD, FW_NORMAL );
   font.lfItalic = ( BYTE )IIF( own->italic, TRUE, FALSE );
   if( own->name[0] != L'\0' )
      lstrcpynW( font.lfFaceName, own->name, LF_FACESIZE );

   /* The fixed font 9x18 keeps its pixels: 18 pixels high at 96 DPI, without smoothing */
   if( lstrcmpiW( own->name, s_xfontName ) == 0 && s_xfontReady() )
   {
      if( own->size == 0 )
         font.lfHeight = -x_guiScale( X_FONT9X18_HEIGHT, dpi );

      font.lfWeight = FW_NORMAL;
      font.lfQuality = NONANTIALIASED_QUALITY;
      font.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
   }

   return CreateFontIndirectW( &font );
}

/* The font of a control at dpi: its own one, otherwise the system font */
static HFONT s_fontOf( HWND window, UINT dpi )
{
   X_GUI_OWNFONT *own = ( X_GUI_OWNFONT * )GetPropW( window, s_fontProperty );
   HFONT font;

   if( own == NULL )
      return x_guiFont( dpi );

   if( own->font == NULL || own->dpi != dpi )
   {
      font = s_ownFontCreate( own, dpi );
      if( font == NULL )
         return x_guiFont( dpi );

      if( own->font != NULL )
         DeleteObject( own->font );

      own->font = font;
      own->dpi = dpi;
   }

   return own->font;
}

/* Release the own font of a control */
static void s_fontRelease( HWND window )
{
   X_GUI_OWNFONT *own = ( X_GUI_OWNFONT * )RemovePropW( window, s_fontProperty );

   if( own == NULL )
      return;

   if( own->font != NULL )
      DeleteObject( own->font );

   x_xfree( own );
}

/* The rectangle of a child window in the client coordinates of its parent */
static void s_childFrame( HWND child, RECT *frame )
{
   GetWindowRect( child, frame );
   MapWindowPoints( HWND_DESKTOP, GetParent( child ), ( POINT * )frame, 2 );
}

/* Move, size and anchor a child for a new DPI; data points to the old and the new DPI */
static BOOL CALLBACK s_rescaleChild( HWND child, LPARAM data )
{
   const UINT *dpi = ( const UINT * )data;
   X_GUI_ANCHOR *anchor = ( X_GUI_ANCHOR * )GetPropW( child, s_anchorProperty );
   int from = ( int )dpi[0];
   int to = ( int )dpi[1];
   int x, y, width, height;
   RECT frame;

   s_childFrame( child, &frame );
   x = MulDiv( frame.left, to, from );
   y = MulDiv( frame.top, to, from );
   width = MulDiv( frame.right - frame.left, to, from );
   height = MulDiv( frame.bottom - frame.top, to, from );
   SetWindowPos( child, NULL, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE );
   SendMessageW( child, WM_SETFONT, ( WPARAM )s_fontOf( child, dpi[1] ), TRUE );
   if( anchor != NULL )
   {
      anchor->left = MulDiv( anchor->left, to, from );
      anchor->top = MulDiv( anchor->top, to, from );
      anchor->right = MulDiv( anchor->right, to, from );
      anchor->bottom = MulDiv( anchor->bottom, to, from );
   }

   return TRUE;
}

/* Move and size an anchored child for the new size of its parent */
static BOOL CALLBACK s_layoutChild( HWND child, LPARAM data )
{
   X_GUI_LAYOUT *layout = ( X_GUI_LAYOUT * )data;
   const X_GUI_ANCHOR *anchor = ( const X_GUI_ANCHOR * )GetPropW( child, s_anchorProperty );
   RECT frame;
   int x, y, width, height;

   if( anchor == NULL || layout->defer == NULL || GetParent( child ) != layout->parent )
      return TRUE;

   s_childFrame( child, &frame );
   x = frame.left;
   y = frame.top;
   width = frame.right - frame.left;
   height = frame.bottom - frame.top;
   if( ( anchor->flags & X_GUI_ANCHOR_LEFT ) && ( anchor->flags & X_GUI_ANCHOR_RIGHT ) )
   {
      x = anchor->left;
      width = layout->width - anchor->left - anchor->right;
   }
   else if( anchor->flags & X_GUI_ANCHOR_RIGHT )
      x = layout->width - anchor->right - width;

   if( ( anchor->flags & X_GUI_ANCHOR_TOP ) && ( anchor->flags & X_GUI_ANCHOR_BOTTOM ) )
   {
      y = anchor->top;
      height = layout->height - anchor->top - anchor->bottom;
   }
   else if( anchor->flags & X_GUI_ANCHOR_BOTTOM )
      y = layout->height - anchor->bottom - height;

   if( width < 0 )
      width = 0;

   if( height < 0 )
      height = 0;

   layout->defer = DeferWindowPos( layout->defer, child, NULL, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE );

   return TRUE;
}

/* Lay out the anchored children of a window for its new client size */
static void s_layout( HWND window, int width, int height )
{
   X_GUI_LAYOUT layout = { window, BeginDeferWindowPos( 8 ), width, height };

   if( layout.defer == NULL )
      return;

   EnumChildWindows( window, s_layoutChild, ( LPARAM )&layout );
   if( layout.defer != NULL )
      EndDeferWindowPos( layout.defer );
}

/* ---------------------------------------------------------------------------------------------
   Toolbar drawing and layout
   --------------------------------------------------------------------------------------------- */

/* xDev colors are numbers 0xRRGGBB, Windows keeps them as 0x00BBGGRR */
static COLORREF s_colorRef( int rgb )
{
   return RGB( ( rgb >> 16 ) & 0xFF, ( rgb >> 8 ) & 0xFF, rgb & 0xFF );
}

/* The toolbar data of a window, or NULL when it is not a toolbar */
static X_GUI_BAR *s_toolbarOf( HWND window )
{
   return ( X_GUI_BAR * )GetPropW( window, s_toolbarProperty );
}

/* The sidebar data of a window, or NULL when it is not a sidebar */
static X_GUI_SIDEBAR *s_sidebarOf( HWND window )
{
   return ( X_GUI_SIDEBAR * )GetPropW( window, s_sidebarProperty );
}

/* The official xDev colors: blue bar, dark blue under the mouse, light while pressed */
static void s_toolbarInit( HWND toolbar )
{
   X_GUI_BAR *bar = ( X_GUI_BAR * )x_xgrab( sizeof( X_GUI_BAR ) );
   UINT dpi = GetDpiForWindow( toolbar );

   memset( bar, 0, sizeof( *bar ) );
   /* -1: the system draws the bar */
   bar->colors[X_GUI_COLOR_FOREGROUND] = -1;
   bar->colors[X_GUI_COLOR_BACKGROUND] = -1;
   bar->colors[X_GUI_COLOR_HOVERFOREGROUND] = -1;
   bar->colors[X_GUI_COLOR_HOVERBACKGROUND] = -1;
   bar->colors[X_GUI_COLOR_PRESSFOREGROUND] = -1;
   bar->colors[X_GUI_COLOR_PRESSBACKGROUND] = -1;
   SendMessageW( toolbar, TB_BUTTONSTRUCTSIZE, sizeof( TBBUTTON ), 0 );
   /* A button with a menu shows an arrow beside its text */
   SendMessageW( toolbar, TB_SETEXTENDEDSTYLE, 0, TBSTYLE_EX_DRAWDDARROWS );
   /* Larger buttons of one width, so they stand in an even row */
   SendMessageW( toolbar, TB_SETPADDING, 0, MAKELPARAM( x_guiScale( 16, dpi ), x_guiScale( 14, dpi ) ) );
   SendMessageW( toolbar, TB_SETBUTTONWIDTH, 0, MAKELPARAM( x_guiScale( 110, dpi ), x_guiScale( 200, dpi ) ) );
   SetPropW( toolbar, s_toolbarProperty, bar );
}

/* Release the toolbar data: its pictures, size grip, tips and shortcuts */
static void s_toolbarRelease( HWND toolbar )
{
   X_GUI_BAR *bar = ( X_GUI_BAR * )RemovePropW( toolbar, s_toolbarProperty );
   size_t i;

   if( bar == NULL )
      return;

   if( bar->images != NULL )
   {
      SendMessageW( toolbar, TB_SETIMAGELIST, 0, 0 );
      ImageList_Destroy( bar->images );
   }

   if( bar->grip != NULL && IsWindow( bar->grip ) )
      DestroyWindow( bar->grip );

   for( i = 0; i < bar->count; ++i )
      x_xfree( bar->tips[i] );

   x_xfree( bar->tips );
   x_xfree( bar->keys );
   x_xfree( bar->modifiers );
   x_xfree( bar );
}

/* The panels of a status bar, or NULL before the first one */
static X_GUI_STATUS *s_statusOf( HWND window )
{
   return ( X_GUI_STATUS * )GetPropW( window, s_statusProperty );
}

/* The panels of the status bar, made at the first panel or color */
static X_GUI_STATUS *s_statusParts( HWND status )
{
   X_GUI_STATUS *parts = s_statusOf( status );

   if( parts == NULL )
   {
      parts = ( X_GUI_STATUS * )x_xgrab( sizeof( X_GUI_STATUS ) );
      memset( parts, 0, sizeof( *parts ) );
      parts->colors[0] = -1;
      parts->colors[1] = -1;
      SetPropW( status, s_statusProperty, parts );
   }

   return parts;
}

/* Release the panels of a status bar */
static void s_statusRelease( HWND status )
{
   X_GUI_STATUS *parts = ( X_GUI_STATUS * )RemovePropW( status, s_statusProperty );

   if( parts == NULL )
      return;

   x_xfree( parts->widths );
   x_xfree( parts->styles );
   x_xfree( parts );
}

/* The panels without their own width share what the others leave of the bar; the last one reaches the end */
static void s_statusLayout( HWND status )
{
   const X_GUI_STATUS *parts = s_statusOf( status );
   UINT dpi = GetDpiForWindow( status );
   int fixed = 0, shared = 0, right = 0;
   int *edges;
   RECT client;
   size_t i;

   if( parts == NULL || parts->count == 0 )
      return;

   GetClientRect( status, &client );
   for( i = 0; i < parts->count; ++i )
   {
      if( parts->widths[i] > 0 )
         fixed += x_guiScale( parts->widths[i], dpi );
      else
         ++shared;
   }

   edges = ( int * )x_xgrab( parts->count * sizeof( int ) );
   for( i = 0; i < parts->count; ++i )
   {
      if( parts->widths[i] > 0 )
         right += x_guiScale( parts->widths[i], dpi );
      else if( client.right > fixed )
         right += ( client.right - fixed ) / shared;

      edges[i] = right;
   }

   edges[parts->count - 1] = -1;
   SendMessageW( status, SB_SETPARTS, ( WPARAM )parts->count, ( LPARAM )edges );
   x_xfree( edges );
}

/* A status bar with its own background draws itself: the panels with their texts, a line between them and
   the dots of the size grip in the color of the text */
static void s_statusPaint( HWND status, const X_GUI_STATUS *parts )
{
   PAINTSTRUCT state;
   HDC dc = BeginPaint( status, &state );
   UINT dpi = GetDpiForWindow( status );
   HFONT font = ( HFONT )SendMessageW( status, WM_GETFONT, 0, 0 );
   HGDIOBJ old = SelectObject( dc, IIF( font != NULL, ( HGDIOBJ )font, GetStockObject( DEFAULT_GUI_FONT ) ) );
   COLORREF text = GetSysColor( COLOR_BTNTEXT );
   int count = ( int )SendMessageW( status, SB_GETPARTS, 0, 0 );
   int step = x_guiScale( 4, dpi );
   int size = x_guiScale( 2, dpi );
   RECT client, part, mark;
   wchar_t *label;
   int i, j;

   if( parts->colors[X_GUI_COLOR_FOREGROUND] >= 0 )
      text = s_colorRef( parts->colors[X_GUI_COLOR_FOREGROUND] );

   GetClientRect( status, &client );
   SetDCBrushColor( dc, s_colorRef( parts->colors[X_GUI_COLOR_BACKGROUND] ) );
   FillRect( dc, &client, ( HBRUSH )GetStockObject( DC_BRUSH ) );
   SetBkMode( dc, TRANSPARENT );
   SetTextColor( dc, text );
   SetDCBrushColor( dc, text );
   for( i = 0; i < count; ++i )
   {
      size_t length = LOWORD( SendMessageW( status, SB_GETTEXTLENGTHW, ( WPARAM )i, 0 ) );

      label = ( wchar_t * )x_xgrabArray( length + 1, sizeof( wchar_t ) );
      label[0] = L'\0';
      SendMessageW( status, SB_GETTEXTW, ( WPARAM )i, ( LPARAM )label );
      SendMessageW( status, SB_GETRECT, ( WPARAM )i, ( LPARAM )&part );
      if( i + 1 < count )
      {
         mark = ( RECT ){ part.right - 1, part.top + size, part.right, part.bottom - size };
         FillRect( dc, &mark, ( HBRUSH )GetStockObject( DC_BRUSH ) );
      }

      part.left += step;
      DrawTextW( dc, label, -1, &part, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS );
      x_xfree( label );
   }

   if( ( GetWindowLongPtrW( status, GWL_STYLE ) & SBARS_SIZEGRIP ) && !IsZoomed( GetAncestor( status, GA_ROOT ) ) )
   {
      for( i = 1; i <= 3; ++i )
      {
         for( j = 1; i + j <= 4; ++j )
         {
            int left = client.right - step * i;
            int top = client.bottom - step * j;

            mark = ( RECT ){ left, top, left + size, top + size };
            FillRect( dc, &mark, ( HBRUSH )GetStockObject( DC_BRUSH ) );
         }
      }
   }

   SelectObject( dc, old );
   EndPaint( status, &state );
}

/* A status bar with its own background paints itself */
static LRESULT CALLBACK s_statusProc( HWND status, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR id,
                                      DWORD_PTR data )
{
   const X_GUI_STATUS *parts = s_statusOf( status );

   UNUSED( id );
   UNUSED( data );
   if( parts != NULL && parts->colors[X_GUI_COLOR_BACKGROUND] >= 0 )
   {
      if( message == WM_ERASEBKGND )
         return 1;

      if( message == WM_PAINT )
      {
         s_statusPaint( status, parts );
         return 0;
      }
   }

   return DefSubclassProc( status, message, wParam, lParam );
}

/* A bar shown by its own style, also before its window appears on the screen */
static int s_shown( HWND window )
{
   return window != NULL && ( GetWindowLongPtrW( window, GWL_STYLE ) & WS_VISIBLE ) != 0;
}

/* What the toolbars, the sidebar and the status bar leave of the parent; with lHorizontal only the bars at the top
   and at the bottom count, the side bars and the sidebar stand between them */
static void s_barsArea( HWND parent, RECT *area, int horizontal )
{
   HWND child;
   RECT frame;

   GetClientRect( parent, area );
   for( child = GetWindow( parent, GW_CHILD ); child != NULL; child = GetWindow( child, GW_HWNDNEXT ) )
   {
      const X_GUI_BAR *bar = s_toolbarOf( child );

      if( !s_shown( child ) || !GetWindowRect( child, &frame ) )
         continue;

      if( s_sidebarOf( child ) != NULL )
      {
         if( !horizontal )
            area->left += frame.right - frame.left;
      }
      else if( bar == NULL )
      {
         if( s_hasClass( child, STATUSCLASSNAMEW ) )
            area->bottom -= frame.bottom - frame.top;
      }
      else if( bar->side == X_GUI_SIDE_TOP )
         area->top += frame.bottom - frame.top;
      else if( bar->side == X_GUI_SIDE_BOTTOM )
         area->bottom -= frame.bottom - frame.top;
      else if( !horizontal && bar->side == X_GUI_SIDE_LEFT )
         area->left += frame.right - frame.left;
      else if( !horizontal && bar->side == X_GUI_SIDE_RIGHT )
         area->right -= frame.right - frame.left;
   }
}

/* The buttons of a bar at the top or at the bottom share the whole width in one row, as the keys at the bottom
   of a commander, the separators keeping their width; the height of the row */
static int s_toolbarStretch( HWND toolbar, int width )
{
   int count = ( int )SendMessageW( toolbar, TB_BUTTONCOUNT, 0, 0 );
   int buttons = 0, rest = width, share, extra, buttonWidth, i, k;
   TBBUTTON button;
   TBBUTTONINFOW info;
   RECT rect;

   for( i = 0; i < count; ++i )
   {
      if( SendMessageW( toolbar, TB_GETBUTTON, ( WPARAM )i, ( LPARAM )&button ) )
      {
         if( !( button.fsStyle & BTNS_SEP ) )
            ++buttons;
         else if( SendMessageW( toolbar, TB_GETITEMRECT, ( WPARAM )i, ( LPARAM )&rect ) )
            rest -= rect.right - rect.left;
      }
   }

   if( buttons > 0 )
   {
      share = IIF( rest > 0, rest / buttons, 0 );
      extra = IIF( rest > 0, rest % buttons, 0 );
      for( i = 0, k = 0; i < count; ++i )
      {
         if( SendMessageW( toolbar, TB_GETBUTTON, ( WPARAM )i, ( LPARAM )&button ) && !( button.fsStyle & BTNS_SEP ) )
         {
            memset( &info, 0, sizeof( info ) );
            info.cbSize = sizeof( info );
            info.dwMask = TBIF_SIZE | TBIF_BYINDEX;
            buttonWidth = share + IIF( k < extra, 1, 0 );
            info.cx = ( WORD )IIF( buttonWidth > 1, buttonWidth, 1 );
            SendMessageW( toolbar, TB_SETBUTTONINFOW, ( WPARAM )i, ( LPARAM )&info );
            ++k;
         }
      }
   }

   return HIWORD( SendMessageW( toolbar, TB_GETBUTTONSIZE, 0, 0 ) ) + x_guiScale( 4, GetDpiForWindow( toolbar ) );
}

/* A toolbar at the top or at the bottom goes across the whole parent, a bar at the left or right side between
   them takes the width of its buttons; the sidebar places its own bars */
static void s_toolbarPlace( HWND toolbar )
{
   X_GUI_BAR *bar = s_toolbarOf( toolbar );
   HWND frame = toolbar;
   HWND parent = GetParent( toolbar );
   HWND status = FindWindowExW( parent, NULL, STATUSCLASSNAMEW, NULL );
   int size = GetSystemMetricsForDpi( SM_CXVSCROLL, GetDpiForWindow( toolbar ) );
   int thickness, x, y, height, grip;
   SIZE buttons = { 0, 0 };
   RECT client, area, line;

   if( bar == NULL || bar->side == X_GUI_SIDE_NONE )
      return;

   GetClientRect( parent, &client );
   if( bar->side == X_GUI_SIDE_TOP || bar->side == X_GUI_SIDE_BOTTOM )
      thickness = s_toolbarStretch( toolbar, client.right );
   else
   {
      SendMessageW( toolbar, TB_GETMAXSIZE, 0, ( LPARAM )&buttons );
      if( buttons.cx == 0 )
         buttons.cx = LOWORD( SendMessageW( toolbar, TB_GETBUTTONSIZE, 0, 0 ) );

      thickness = buttons.cx + x_guiScale( 4, GetDpiForWindow( toolbar ) );
   }

   switch( bar->side )
   {
   case X_GUI_SIDE_TOP:
      SetWindowPos( frame, NULL, 0, 0, client.right, thickness, SWP_NOZORDER | SWP_NOACTIVATE );
      break;
   case X_GUI_SIDE_BOTTOM:
      area = client;
      if( s_shown( status ) && GetWindowRect( status, &line ) )
         area.bottom -= line.bottom - line.top;

      SetWindowPos( frame, NULL, 0, area.bottom - thickness, client.right, thickness, SWP_NOZORDER | SWP_NOACTIVATE );
      break;
   default:
      s_barsArea( parent, &area, 1 );
      x = IIF( bar->side == X_GUI_SIDE_LEFT, 0, client.right - thickness );
      height = IIF( area.bottom > area.top, area.bottom - area.top, 0 );
      SetWindowPos( frame, NULL, x, area.top, thickness, height, SWP_NOZORDER | SWP_NOACTIVATE );
      break;
   }

   /* The lowest bar of a window with a sizing frame and without a status bar carries the dots for
      dragging the size of the window in its right corner */
   grip = bar->side == X_GUI_SIDE_BOTTOM && !s_shown( status );
   grip = grip && ( GetWindowLongPtrW( parent, GWL_STYLE ) & WS_THICKFRAME ) && !IsZoomed( parent );
   if( grip )
   {
      DWORD style = WS_CHILD | WS_VISIBLE | SBS_SIZEGRIP;
      HINSTANCE instance = GetModuleHandleW( NULL );

      if( bar->grip == NULL || !IsWindow( bar->grip ) )
         bar->grip = CreateWindowExW( 0, L"SCROLLBAR", NULL, style, 0, 0, size, size, parent, NULL, instance, NULL );

      x = client.right - size;
      y = client.bottom - size;
      if( bar->grip != NULL )
         SetWindowPos( bar->grip, HWND_TOP, x, y, size, size, SWP_NOACTIVATE | SWP_SHOWWINDOW );
   }
   else if( bar->grip != NULL && IsWindow( bar->grip ) )
      ShowWindow( bar->grip, SW_HIDE );
}

/* The bars at the top and at the bottom first, the side bars stand between them */
static BOOL CALLBACK s_layoutToolbar( HWND child, LPARAM parent )
{
   const X_GUI_BAR *bar = s_toolbarOf( child );

   if( bar == NULL || GetParent( child ) != ( HWND )parent )
      return TRUE;

   if( bar->side == X_GUI_SIDE_TOP || bar->side == X_GUI_SIDE_BOTTOM )
      s_toolbarPlace( child );

   return TRUE;
}

/* The side bars after the bars at the top and at the bottom */
static BOOL CALLBACK s_layoutSideToolbar( HWND child, LPARAM parent )
{
   const X_GUI_BAR *bar = s_toolbarOf( child );

   if( bar == NULL || GetParent( child ) != ( HWND )parent )
      return TRUE;

   if( bar->side == X_GUI_SIDE_LEFT || bar->side == X_GUI_SIDE_RIGHT )
      s_toolbarPlace( child );

   return TRUE;
}

/* A color of the toolbar; a part not set by the program has the color of Windows: the system colors of
   buttons, light blue under the mouse and a deeper blue while pressed, as in the Explorer of Windows 10 */
static COLORREF s_toolbarColor( const X_GUI_BAR *bar, X_GUI_COLOR part )
{
   if( bar->colors[part] >= 0 )
      return s_colorRef( bar->colors[part] );

   switch( part )
   {
   case X_GUI_COLOR_BACKGROUND:
      return GetSysColor( COLOR_BTNFACE );
   case X_GUI_COLOR_HOVERBACKGROUND:
      return RGB( 0xE5, 0xF3, 0xFF );
   case X_GUI_COLOR_PRESSBACKGROUND:
      return RGB( 0xCC, 0xE8, 0xFF );
   default:
      return GetSysColor( COLOR_BTNTEXT );
   }
}

/* The bar and every button are filled with the toolbar colors; without them the system draws the bar */
static LRESULT s_toolbarDraw( NMTBCUSTOMDRAW *draw )
{
   const X_GUI_BAR *bar = s_toolbarOf( draw->nmcd.hdr.hwndFrom );
   UINT dpi = GetDpiForWindow( draw->nmcd.hdr.hwndFrom );
   X_GUI_COLOR fore, back;
   RECT face;
   int part;

   if( bar == NULL )
      return CDRF_DODEFAULT;

   for( part = 0; part < X_GUI_COLOR_COUNT && bar->colors[part] < 0; part++ )
      ;

   switch( draw->nmcd.dwDrawStage )
   {
   case CDDS_PREPAINT:
      SetDCBrushColor( draw->nmcd.hdc, s_toolbarColor( bar, X_GUI_COLOR_BACKGROUND ) );
      FillRect( draw->nmcd.hdc, &draw->nmcd.rc, ( HBRUSH )GetStockObject( DC_BRUSH ) );
      return IIF( part == X_GUI_COLOR_COUNT, CDRF_DODEFAULT, CDRF_NOTIFYITEMDRAW );

   case CDDS_ITEMPREPAINT:
      if( draw->nmcd.uItemState & ( CDIS_DISABLED | CDIS_GRAYED ) )
      {
         fore = X_GUI_COLOR_FOREGROUND;
         back = X_GUI_COLOR_BACKGROUND;
      }
      else if( draw->nmcd.uItemState & CDIS_SELECTED )
      {
         fore = X_GUI_COLOR_PRESSFOREGROUND;
         back = X_GUI_COLOR_PRESSBACKGROUND;
      }
      else if( draw->nmcd.uItemState & CDIS_HOT )
      {
         fore = X_GUI_COLOR_HOVERFOREGROUND;
         back = X_GUI_COLOR_HOVERBACKGROUND;
      }
      else
      {
         fore = X_GUI_COLOR_FOREGROUND;
         back = X_GUI_COLOR_BACKGROUND;
      }

      /* The button under the mouse or pressed is a field with a gap to the next one and a frame of the press
         color; other buttons lie on the bar without a frame */
      face = draw->nmcd.rc;
      InflateRect( &face, -x_guiScale( 2, dpi ), -x_guiScale( 3, dpi ) );
      SetDCBrushColor( draw->nmcd.hdc, s_toolbarColor( bar, X_GUI_COLOR_BACKGROUND ) );
      FillRect( draw->nmcd.hdc, &draw->nmcd.rc, ( HBRUSH )GetStockObject( DC_BRUSH ) );
      SetDCBrushColor( draw->nmcd.hdc, s_toolbarColor( bar, back ) );
      FillRect( draw->nmcd.hdc, &face, ( HBRUSH )GetStockObject( DC_BRUSH ) );
      if( back != X_GUI_COLOR_BACKGROUND )
      {
         SetDCBrushColor( draw->nmcd.hdc, s_toolbarColor( bar, X_GUI_COLOR_PRESSBACKGROUND ) );
         FrameRect( draw->nmcd.hdc, &face, ( HBRUSH )GetStockObject( DC_BRUSH ) );
      }

      if( draw->nmcd.uItemState & ( CDIS_DISABLED | CDIS_GRAYED ) )
         draw->clrText = GetSysColor( COLOR_GRAYTEXT );
      else
         draw->clrText = s_toolbarColor( bar, fore );

      draw->clrTextHighlight = draw->clrText;
      return TBCDRF_USECDCOLORS | TBCDRF_NOBACKGROUND | TBCDRF_NOEDGES | TBCDRF_NOOFFSET;
   }

   return CDRF_DODEFAULT;
}

/* Buttons are numbered from 1 in the order of Add(), separators have number 0 */
static void s_toolbarTip( NMTBGETINFOTIPW *tip )
{
   const X_GUI_BAR *bar = s_toolbarOf( tip->hdr.hwndFrom );

   if( bar == NULL || tip->iItem < 1 || ( size_t )tip->iItem > bar->count || tip->cchTextMax <= 0 )
      return;

   if( bar->tips[tip->iItem - 1] != NULL )
      lstrcpynW( tip->pszText, bar->tips[tip->iItem - 1], tip->cchTextMax );
}

/* A panel or a page without colors of its own has the colors of the window it lies in */
static X_GUI_PAINT *s_paintOf( HWND window )
{
   X_GUI_PAINT *paint = ( X_GUI_PAINT * )GetPropW( window, s_paintProperty );
   int child;

   if( paint != NULL || window == NULL )
      return paint;

   child = ( GetWindowLongPtrW( window, GWL_STYLE ) & WS_CHILD ) != 0;
   if( child && ( ATOM )GetClassLongPtrW( window, GCW_ATOM ) == s_windowClass )
      paint = s_paintOf( GetParent( window ) );

   return paint;
}

/* Release the colors and the brush of a window */
static void s_paintRelease( HWND window )
{
   X_GUI_PAINT *paint = ( X_GUI_PAINT * )RemovePropW( window, s_paintProperty );

   if( paint == NULL )
      return;

   if( paint->brush != NULL )
      DeleteObject( paint->brush );

   x_xfree( paint );
}

/* Labels, frames, check boxes and sliders take the colors of the window; edit fields keep their own.
   The visual style draws frames, check boxes and radio buttons in its own colors; in a window with
   colors of the program they are drawn the classic way, which takes the colors of the window */
static void s_paintTheme( HWND control )
{
   const X_GUI_PAINT *paint = s_paintOf( GetParent( control ) );
   LONG_PTR type = GetWindowLongPtrW( control, GWL_STYLE ) & BS_TYPEMASK;
   int own = paint != NULL && ( paint->brush != NULL || paint->colors[X_GUI_COLOR_FOREGROUND] >= 0 );
   const wchar_t *theme = IIF( own, L"", NULL );

   if( !s_hasClass( control, L"Button" ) )
      return;

   if( type != BS_GROUPBOX && type != BS_AUTOCHECKBOX && type != BS_AUTORADIOBUTTON )
      return;

   SetWindowTheme( control, theme, theme );
}

/* The frame was made before the fields in it, so it lies above them in the order of the windows; painting its
   inside it leaves out the fields shown in it, or it covers them when it is drawn again after them */
static void s_groupClip( HWND group, HDC dc )
{
   HWND child;
   RECT frame, place, common;

   GetWindowRect( group, &frame );
   for( child = GetWindow( GetParent( group ), GW_CHILD ); child != NULL; child = GetWindow( child, GW_HWNDNEXT ) )
   {
      if( child == group || !IsWindowVisible( child ) || !GetWindowRect( child, &place ) )
         continue;

      if( IntersectRect( &common, &frame, &place ) )
      {
         MapWindowPoints( NULL, group, ( POINT * )&common, 2 );
         ExcludeClipRect( dc, common.left, common.top, common.right, common.bottom );
      }
   }
}

/* A frame in a window with colors of the program: its inside in the color of the window and a line
   of one pixel in the text color instead of the etched double line */
static void s_groupPaint( HWND group, const X_GUI_PAINT *paint )
{
   PAINTSTRUCT state;
   HDC dc = BeginPaint( group, &state );
   HFONT font = ( HFONT )SendMessageW( group, WM_GETFONT, 0, 0 );
   HGDIOBJ old = SelectObject( dc, IIF( font != NULL, ( HGDIOBJ )font, GetStockObject( DEFAULT_GUI_FONT ) ) );
   COLORREF fore = GetSysColor( COLOR_WINDOWTEXT );
   HBRUSH back = IIF( paint->brush != NULL, paint->brush, GetSysColorBrush( COLOR_BTNFACE ) );
   HPEN pen;
   int left = x_guiScale( 8, GetDpiForWindow( group ) );
   int space = x_guiScale( 2, GetDpiForWindow( group ) );
   wchar_t text[256];
   int length = GetWindowTextW( group, text, 256 );
   RECT client, label;
   SIZE size = { 0, 0 };
   HGDIOBJ oldPen, oldBrush;

   if( paint->colors[X_GUI_COLOR_FOREGROUND] >= 0 )
      fore = s_colorRef( paint->colors[X_GUI_COLOR_FOREGROUND] );

   pen = CreatePen( PS_SOLID, 1, fore );
   GetClientRect( group, &client );
   GetTextExtentPoint32W( dc, L"M", 1, &size );
   s_groupClip( group, dc );
   FillRect( dc, &client, back );
   oldPen = SelectObject( dc, pen );
   oldBrush = SelectObject( dc, GetStockObject( NULL_BRUSH ) );
   Rectangle( dc, 0, size.cy / 2, client.right, client.bottom );
   SelectObject( dc, oldBrush );
   SelectObject( dc, oldPen );
   if( length > 0 )
   {
      GetTextExtentPoint32W( dc, text, length, &size );
      label = ( RECT ){ left - space, 0, left + size.cx + space, size.cy };
      FillRect( dc, &label, back );
      SetBkMode( dc, TRANSPARENT );
      SetTextColor( dc, IIF( IsWindowEnabled( group ), fore, GetSysColor( COLOR_GRAYTEXT ) ) );
      TextOutW( dc, left, 0, text, length );
   }

   SelectObject( dc, old );
   DeleteObject( pen );
   EndPaint( group, &state );
}

/* A frame paints its inside in the color of the window and keeps the fields in it visible */
static LRESULT CALLBACK s_groupProc( HWND group, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR id,
                                     DWORD_PTR data )
{
   const X_GUI_PAINT *paint = s_paintOf( GetParent( group ) );
   int own = paint != NULL && ( paint->brush != NULL || paint->colors[X_GUI_COLOR_FOREGROUND] >= 0 );
   LRESULT result;

   UNUSED( data );
   switch( message )
   {
   /* The frame does not paint its inside; it gets the color of the window */
   case WM_ERASEBKGND:
      if( !own )
      {
         RECT client;
         int saved = SaveDC( ( HDC )wParam );

         GetClientRect( group, &client );
         s_groupClip( group, ( HDC )wParam );
         FillRect( ( HDC )wParam, &client, GetSysColorBrush( COLOR_BTNFACE ) );
         RestoreDC( ( HDC )wParam, saved );
      }
      return 1;

   case WM_PAINT:
      if( own )
      {
         s_groupPaint( group, paint );
         return 0;
      }
      break;

   /* The button redraws itself after these without WM_PAINT */
   case WM_SETTEXT:
   case WM_ENABLE:
   case WM_UPDATEUISTATE:
      result = DefSubclassProc( group, message, wParam, lParam );
      if( own )
         InvalidateRect( group, NULL, TRUE );
      return result;

   case WM_NCDESTROY:
      RemoveWindowSubclass( group, s_groupProc, id );
      break;
   }

   return DefSubclassProc( group, message, wParam, lParam );
}

/* Set the drawing style of a direct child of the window */
static BOOL CALLBACK s_paintThemeChild( HWND child, LPARAM parent )
{
   if( GetParent( child ) == ( HWND )parent )
      s_paintTheme( child );

   return TRUE;
}

/* WM_CTLCOLOR*: the text color and the brush of a control in a window with colors of the program */
static LRESULT s_paintControl( HWND window, HDC dc, HWND control )
{
   const X_GUI_PAINT *paint = s_paintOf( window );

   if( paint == NULL || s_hasClass( control, L"Edit" ) )
      return 0;

   if( paint->brush == NULL && paint->colors[X_GUI_COLOR_FOREGROUND] < 0 )
      return 0;

   if( paint->colors[X_GUI_COLOR_FOREGROUND] >= 0 )
      SetTextColor( dc, s_colorRef( paint->colors[X_GUI_COLOR_FOREGROUND] ) );

   if( paint->brush == NULL )
   {
      SetBkColor( dc, GetSysColor( COLOR_BTNFACE ) );
      return ( LRESULT )GetSysColorBrush( COLOR_BTNFACE );
   }

   SetBkColor( dc, s_colorRef( paint->colors[X_GUI_COLOR_BACKGROUND] ) );

   return ( LRESULT )paint->brush;
}

/* ---------------------------------------------------------------------------------------------
   Events and object lifetime
   --------------------------------------------------------------------------------------------- */

/* Send s_message to the object in the first argument with the other arguments */
static void s_dispatch( X_CALL_CTX *ctx )
{
   x_objSend( ctx, ctx->result, ( X_ITEM * )ctx->args[0].value, s_message, ctx->argc - 1, ctx->args + 1 );
}

/* Send an event to an xDev object with up to two numbers; an error in it ends the running message loop */
int x_guiNotify( X_ITEM *object, const char *message, X_ITEM *result, int argc, const int64_t *values )
{
   X_ITEM params[2] = { X_ITEM_NIL_INIT, X_ITEM_NIL_INIT };
   X_ARG args[3] = { { object, 0 } };
   int i, success;

   for( i = 0; i < argc; ++i )
   {
      x_itemPutNInt( &params[i], values[i] );
      args[i + 1] = ( X_ARG ){ &params[i], 0 };
   }

   s_message = message;
   success = x_callbackInvoke( s_dispatch, result, argc + 1, args );
   for( i = 0; i < argc; ++i )
      x_itemClear( &params[i] );

   if( !success && s_running )
      PostQuitMessage( 1 );

   return success;
}

/* Release what a window keeps and tell its xDev object that the window is gone */
static void s_detach( HWND window )
{
   X_ITEM *object = ( X_ITEM * )GetWindowLongPtrW( window, GWLP_USERDATA );
   X_GUI_ANCHOR *anchor = ( X_GUI_ANCHOR * )RemovePropW( window, s_anchorProperty );
   X_ITEM result = X_ITEM_NIL_INIT;
   X_ARG args[1];
   X_CALL_CTX call;

   if( anchor != NULL )
      x_xfree( anchor );

   s_toolbarRelease( window );
   s_statusRelease( window );
   RemovePropW( window, s_varProperty );
   RemovePropW( window, s_getProperty );
   RemovePropW( window, s_rowsProperty );
   s_fontRelease( window );
   if( object == NULL )
      return;

   SetWindowLongPtrW( window, GWLP_USERDATA, 0 );
   args[0] = ( X_ARG ){ object, 0 };
   call = ( X_CALL_CTX ){ args, 1, &result, NULL, NULL };
   s_message = "__GUIDETACH";
   x_callCleanup( s_dispatch, &call );
   x_itemClear( &result );
   x_itemRelease( object );
}

/* The control leaves its window, as RestScreen() takes a GET away */
void x_guiControlDestroy( HWND control )
{
   x_callbackEnterDll();
   s_detach( control );
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
      s_xfontAttach( label );
      SendMessageW( label, WM_SETFONT, ( WPARAM )s_fontOf( label, GetDpiForWindow( label ) ), FALSE );
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
   COLORREF line = IIF( color >= 0, s_colorRef( color ), GetSysColor( COLOR_BTNSHADOW ) );

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

/* Detach a child window of a window that is destroyed */
static BOOL CALLBACK s_detachChild( HWND window, LPARAM unused )
{
   UNUSED( unused );
   s_detach( window );

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
   Windows
   --------------------------------------------------------------------------------------------- */

/* The pages of a tab control: placed in area and shown by selected (1-based), or with find only looked up */
typedef struct
{
   HWND tab;
   RECT area;
   int selected;
   int find;
   HWND found;
} X_GUI_TABS;

/* Place and show one page of a tab control, or find the selected one */
static BOOL CALLBACK s_tabPage( HWND child, LPARAM data )
{
   X_GUI_TABS *tabs = ( X_GUI_TABS * )data;
   int width, height;

   if( GetParent( child ) != tabs->tab || ( ATOM )GetClassLongPtrW( child, GCW_ATOM ) != s_windowClass )
      return TRUE;

   if( tabs->find )
   {
      if( GetDlgCtrlID( child ) != tabs->selected )
         return TRUE;

      tabs->found = child;
      return FALSE;
   }

   width = tabs->area.right - tabs->area.left;
   height = tabs->area.bottom - tabs->area.top;
   SetWindowPos( child, NULL, tabs->area.left, tabs->area.top, width, height, SWP_NOZORDER | SWP_NOACTIVATE );
   if( tabs->selected > 0 )
      ShowWindow( child, IIF( GetDlgCtrlID( child ) == tabs->selected, SW_SHOW, SW_HIDE ) );

   return TRUE;
}

/* Place the pages of a tab control; with show only the selected page is visible */
static void s_tabLayout( HWND tab, int show )
{
   X_GUI_TABS tabs;

   memset( &tabs, 0, sizeof( tabs ) );
   tabs.tab = tab;
   GetClientRect( tab, &tabs.area );
   SendMessageW( tab, TCM_ADJUSTRECT, FALSE, ( LPARAM )&tabs.area );
   tabs.selected = IIF( show, ( int )SendMessageW( tab, TCM_GETCURSEL, 0, 0 ) + 1, 0 );
   EnumChildWindows( tab, s_tabPage, ( LPARAM )&tabs );
}

/* Place the pages of a tab control of the window */
static BOOL CALLBACK s_layoutTab( HWND child, LPARAM parent )
{
   if( GetParent( child ) == ( HWND )parent && s_hasClass( child, WC_TABCONTROLW ) )
      s_tabLayout( child, 0 );

   return TRUE;
}

/* A status bar of the window follows its new size */
static BOOL CALLBACK s_resizeStatusBar( HWND child, LPARAM parent )
{
   if( GetParent( child ) == ( HWND )parent && s_hasClass( child, STATUSCLASSNAMEW ) )
   {
      SendMessageW( child, WM_SIZE, 0, 0 );
      s_statusLayout( child );
   }

   return TRUE;
}

/* The context menu given to the object of a window, or NULL */
static HMENU s_contextMenuOf( HWND window )
{
   X_ITEM *object = ( X_ITEM * )GetWindowLongPtrW( window, GWLP_USERDATA );
   X_ITEM menu __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM handle __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   HMENU result;

   if( object == NULL || !x_guiNotify( object, "CONTEXTMENU", &menu, 0, NULL ) || !X_IS_OBJECT( &menu ) )
      return NULL;

   if( !x_guiNotify( &menu, "HANDLE", &handle, 0, NULL ) )
      return NULL;

   result = ( HMENU )x_itemGetPtr( &handle );

   return IIF( result != NULL && IsMenu( result ), result, NULL );
}

/* Open the context menu of the target or of the nearest window around it; position -1 is the keyboard */
static int s_contextMenu( HWND window, HWND target, LPARAM position )
{
   HWND current = target;
   HMENU menu = NULL;
   POINT point = { ( short )LOWORD( position ), ( short )HIWORD( position ) };
   RECT frame;

   while( menu == NULL && current != NULL )
   {
      menu = s_contextMenuOf( current );
      if( current == window )
         break;

      current = GetParent( current );
   }

   if( menu == NULL )
      return 0;

   if( position == -1 )
   {
      GetWindowRect( target, &frame );
      point = ( POINT ){ frame.left, frame.top };
   }

   TrackPopupMenuEx( menu, TPM_RIGHTBUTTON, point.x, point.y, GetAncestor( window, GA_ROOT ), NULL );

   return 1;
}

/* A panel without its own size takes the whole parent between its toolbars and the status bar */
static void s_panelPlace( HWND panel )
{
   RECT area;
   int width, height;

   s_barsArea( GetParent( panel ), &area, 0 );
   width = IIF( area.right > area.left, area.right - area.left, 0 );
   height = IIF( area.bottom > area.top, area.bottom - area.top, 0 );
   SetWindowPos( panel, NULL, area.left, area.top, width, height, SWP_NOZORDER | SWP_NOACTIVATE );
}

/* A panel that fills the window follows its new size */
static BOOL CALLBACK s_layoutPanel( HWND child, LPARAM parent )
{
   if( GetParent( child ) == ( HWND )parent && GetPropW( child, s_fillProperty ) != NULL )
      s_panelPlace( child );

   return TRUE;
}

/* The sidebar stands at the left edge of its window between the bars at the top and at the bottom; it is as wide
   as its strip of pictures, with the panel of the shown page and its edge */
static void s_sidebarPlace( HWND sidebar )
{
   const X_GUI_SIDEBAR *side = s_sidebarOf( sidebar );
   UINT dpi = GetDpiForWindow( sidebar );
   int width = x_guiScale( X_GUI_SIDEBAR_STRIP, dpi );
   int height;
   RECT area;

   if( side->page > 0 )
      width += x_guiScale( side->width + X_GUI_SIDEBAR_EDGE, dpi );

   s_barsArea( GetParent( sidebar ), &area, 1 );
   height = IIF( area.bottom > area.top, area.bottom - area.top, 0 );
   SetWindowPos( sidebar, NULL, 0, area.top, width, height, SWP_NOZORDER | SWP_NOACTIVATE );
}

/* A sidebar of the window follows its new size */
static BOOL CALLBACK s_layoutSidebar( HWND child, LPARAM parent )
{
   if( GetParent( child ) == ( HWND )parent && s_sidebarOf( child ) != NULL )
      s_sidebarPlace( child );

   return TRUE;
}

/* Release the keys of SetKey() of a window */
static void s_keysRelease( HWND window )
{
   X_GUI_KEYS *keys = ( X_GUI_KEYS * )RemovePropW( window, s_keysProperty );

   if( keys == NULL )
      return;

   x_xfree( keys->keys );
   x_xfree( keys->modifiers );
   x_xfree( keys );
}

/* A modal dialog gives its owner the input back before it disappears, so Windows activates the owner
   and not the window of another program */
static void s_destroyWindow( HWND window )
{
   HWND owner = GetWindow( window, GW_OWNER );

   if( owner != NULL && GetPropW( window, s_modalProperty ) != NULL )
      EnableWindow( owner, TRUE );

   DestroyWindow( window );
}

static void s_getFit( HWND edit, int focused );

/* The window procedure of windows, dialogs, panels and pages: events go to the xDev object */
static LRESULT CALLBACK s_windowProc( HWND window, UINT message, WPARAM wParam, LPARAM lParam )
{
   X_ITEM *object = ( X_ITEM * )GetWindowLongPtrW( window, GWLP_USERDATA );
   X_ITEM result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   int64_t values[2];

   switch( message )
   {
   case WM_NCCREATE:
   {
      const CREATESTRUCTW *create = ( const CREATESTRUCTW * )lParam;

      SetWindowLongPtrW( window, GWLP_USERDATA, ( LONG_PTR )x_itemNew( ( const X_ITEM * )create->lpCreateParams ) );
      break;
   }

   case WM_CLOSE:
      /* The user closes the window of a running Read(): Read() ends first as with Esc, then the window closes */
      if( s_reading > 0 && s_readWindow != NULL && ( window == s_readWindow || IsChild( window, s_readWindow ) ) )
      {
         s_readEnd = 0;
         s_readClose = window;
         return 0;
      }

      /* OnClose() returning .F. keeps the window open */
      if( object == NULL || !x_guiNotify( object, "ONCLOSE", &result, 0, NULL ) )
         s_destroyWindow( window );
      else if( !X_IS_LOGICAL( &result ) || result.u.logical )
         s_destroyWindow( window );
      return 0;

   /* The control with the focus is remembered when the window is left and gets it back on return;
      a window shown for the first time gives the focus to its first control */
   case WM_ACTIVATE:
      if( LOWORD( wParam ) == WA_INACTIVE )
      {
         HWND focus = GetFocus();

         SetPropW( window, s_focusProperty, IIF( focus != NULL && IsChild( window, focus ), focus, NULL ) );
      }
      break;

   case WM_SETFOCUS:
   {
      HWND focus = ( HWND )GetPropW( window, s_focusProperty );

      if( focus == NULL || !IsWindow( focus ) || !IsChild( window, focus ) )
         focus = GetNextDlgTabItem( window, NULL, FALSE );

      if( focus != NULL )
         SetFocus( focus );
      return 0;
   }

   case WM_SIZE:
      EnumChildWindows( window, s_resizeStatusBar, ( LPARAM )window );
      EnumChildWindows( window, s_layoutToolbar, ( LPARAM )window );
      EnumChildWindows( window, s_layoutSideToolbar, ( LPARAM )window );
      EnumChildWindows( window, s_layoutSidebar, ( LPARAM )window );
      EnumChildWindows( window, s_layoutPanel, ( LPARAM )window );
      if( wParam != SIZE_MINIMIZED )
         s_layout( window, LOWORD( lParam ), HIWORD( lParam ) );

      EnumChildWindows( window, s_layoutTab, ( LPARAM )window );
      if( object != NULL )
      {
         values[0] = MulDiv( LOWORD( lParam ), USER_DEFAULT_SCREEN_DPI, ( int )GetDpiForWindow( window ) );
         values[1] = MulDiv( HIWORD( lParam ), USER_DEFAULT_SCREEN_DPI, ( int )GetDpiForWindow( window ) );
         x_guiNotify( object, "ONRESIZE", &result, 2, values );
      }
      return 0;

   case WM_DPICHANGED:
   {
      const RECT *frame = ( const RECT * )lParam;
      UINT dpi[2] = { ( UINT )( UINT_PTR )GetPropW( window, s_dpiProperty ), HIWORD( wParam ) };
      int width = frame->right - frame->left;
      int height = frame->bottom - frame->top;

      SetPropW( window, s_dpiProperty, ( HANDLE )( UINT_PTR )dpi[1] );
      if( dpi[0] != 0 )
         EnumChildWindows( window, s_rescaleChild, ( LPARAM )dpi );

      SetWindowPos( window, NULL, frame->left, frame->top, width, height, SWP_NOZORDER | SWP_NOACTIVATE );
      return 0;
   }

   case WM_KEYDOWN:
      if( object != NULL )
      {
         values[0] = ( int64_t )wParam;
         x_guiNotify( object, "ONKEY", &result, 1, values );
      }
      return 0;

   case WM_MEASUREITEM:
   {
      MEASUREITEMSTRUCT *measure = ( MEASUREITEMSTRUCT * )lParam;

      if( wParam == 0 && measure->CtlType == ODT_MENU && measure->itemData != 0 )
      {
         s_menuMeasure( window, measure );
         return TRUE;
      }
      break;
   }

   case WM_DRAWITEM:
   {
      const DRAWITEMSTRUCT *draw = ( const DRAWITEMSTRUCT * )lParam;

      if( wParam == 0 && draw->CtlType == ODT_MENU && draw->itemData != 0 )
      {
         s_menuDraw( window, draw );
         return TRUE;
      }
      break;
   }

   case WM_INITMENUPOPUP:
      if( object != NULL )
      {
         values[0] = ( int64_t )( intptr_t )wParam;
         x_guiNotify( object, "__GUIMENUWHEN", &result, 1, values );
      }
      break;

   case WM_MENUCHAR:
      if( s_menuExtended )
         return s_menuChar( ( HMENU )lParam, ( wchar_t )LOWORD( wParam ) );

      break;

   case WM_NOTIFY:
   {
      const NMHDR *header = ( const NMHDR * )lParam;
      HWND from = IIF( header != NULL, header->hwndFrom, NULL );
      X_ITEM *control = IIF( from != NULL, ( X_ITEM * )GetWindowLongPtrW( from, GWLP_USERDATA ), NULL );

      if( from != NULL && s_toolbarOf( from ) != NULL )
      {
         if( header->code == NM_CUSTOMDRAW )
            return s_toolbarDraw( ( NMTBCUSTOMDRAW * )lParam );

         if( header->code == TBN_GETINFOTIPW )
            s_toolbarTip( ( NMTBGETINFOTIPW * )lParam );

         /* A click on a button with a menu goes the way of a click on a button and of its shortcut */
         if( header->code == TBN_DROPDOWN )
         {
            int item = ( ( const NMTOOLBARW * )lParam )->iItem;

            PostMessageW( window, WM_COMMAND, MAKEWPARAM( item, BN_CLICKED ), ( LPARAM )from );
            return TBDDRET_DEFAULT;
         }
         return 0;
      }

      if( control != NULL && GetParent( header->hwndFrom ) == window )
      {
         values[0] = ( int )header->code;
         values[1] = 0;
         if( header->code == LVN_ITEMCHANGED )
         {
            const NMLISTVIEW *change = ( const NMLISTVIEW * )lParam;
            int selected = ( change->uChanged & LVIF_STATE ) && ( change->uNewState & LVIS_SELECTED );

            if( !s_quiet && selected && !( change->uOldState & LVIS_SELECTED ) )
               values[1] = change->iItem + 1;
         }
         else if( header->code == TVN_SELCHANGEDW )
            values[1] = ( ( const NMTREEVIEWW * )lParam )->action != TVC_UNKNOWN;

         x_guiNotify( control, "__GUINOTIFY", &result, 2, values );
      }
      return 0;
   }

   case WM_HSCROLL:
   case WM_VSCROLL:
      if( lParam != 0 )
      {
         X_ITEM *control = ( X_ITEM * )GetWindowLongPtrW( ( HWND )lParam, GWLP_USERDATA );

         if( control != NULL )
         {
            values[0] = LOWORD( wParam );
            x_guiNotify( control, "__GUICOMMAND", &result, 1, values );
         }
         return 0;
      }
      break;

   case WM_COMMAND:
      if( lParam != 0 )
      {
         X_ITEM *control = ( X_ITEM * )GetWindowLongPtrW( ( HWND )lParam, GWLP_USERDATA );

         /* A GET of several rows follows its text while it is written */
         if( HIWORD( wParam ) == EN_CHANGE && GetFocus() == ( HWND )lParam )
            s_getFit( ( HWND )lParam, 1 );

         if( control != NULL )
         {
            values[0] = HIWORD( wParam );
            values[1] = LOWORD( wParam );
            x_guiNotify( control, "__GUICOMMAND", &result, 2, values );
         }
         return 0;
      }

      if( object != NULL && LOWORD( wParam ) >= X_GUI_MENU_FIRST )
      {
         values[0] = LOWORD( wParam );
         x_guiNotify( object, "ONMENU", &result, 1, values );
         return 0;
      }

      if( LOWORD( wParam ) == IDCANCEL )
      {
         HWND cancel = ( HWND )GetPropW( window, s_cancelProperty );

         if( cancel != NULL && IsWindow( cancel ) && IsWindowEnabled( cancel ) )
            SendMessageW( cancel, BM_CLICK, 0, 0 );
         return 0;
      }
      break;

   case WM_ERASEBKGND:
   {
      const X_GUI_PAINT *paint = s_paintOf( window );
      RECT client;

      if( paint != NULL && paint->brush != NULL )
      {
         GetClientRect( window, &client );
         FillRect( ( HDC )wParam, &client, paint->brush );
         return 1;
      }
      break;
   }

   case WM_CTLCOLORSTATIC:
   case WM_CTLCOLORBTN:
   {
      LRESULT brush = s_paintControl( window, ( HDC )wParam, ( HWND )lParam );

      if( brush != 0 )
         return brush;

      break;
   }

   case WM_CONTEXTMENU:
      if( s_contextMenu( window, ( HWND )wParam, lParam ) )
         return 0;

      break;

   case DM_GETDEFID:
   {
      HWND button = ( HWND )GetPropW( window, s_defaultProperty );

      if( button != NULL && IsWindow( button ) )
         return MAKELRESULT( GetDlgCtrlID( button ), DC_HASDEFID );

      break;
   }

   case WM_TIMER:
   {
      const X_GUI_TIMER *timer = x_guiTimerFind( ( UINT_PTR )wParam );

      if( timer != NULL && timer->window == window )
      {
         X_ITEM *target = x_itemNew( timer->timer );

         x_guiNotify( target, "__GUITIMER", &result, 0, NULL );
         x_itemRelease( target );
      }
      return 0;
   }

   case WM_DESTROY:
      if( RemovePropW( window, s_modalProperty ) != NULL && GetWindow( window, GW_OWNER ) != NULL )
         EnableWindow( GetWindow( window, GW_OWNER ), TRUE );

      x_guiTimersRelease( window );
      EnumChildWindows( window, s_detachChild, 0 );
      if( window == s_mainWindow )
      {
         s_mainWindow = NULL;
         if( s_running )
            PostQuitMessage( 0 );
      }
      return 0;

   case WM_NCDESTROY:
      RemovePropW( window, s_dpiProperty );
      RemovePropW( window, s_focusProperty );
      RemovePropW( window, s_fillProperty );
      s_keysRelease( window );
      RemovePropW( window, s_defaultProperty );
      RemovePropW( window, s_cancelProperty );
      RemovePropW( window, s_toolTipProperty );
      s_paintRelease( window );
      s_detach( window );
      break;
   }

   return DefWindowProcW( window, message, wParam, lParam );
}

/* Register the window class of windows, dialogs, panels and pages once */
static BOOL s_registerWindowClass( HINSTANCE instance )
{
   WNDCLASSEXW windowClass;
   int iconWidth = GetSystemMetrics( SM_CXSMICON );
   int iconHeight = GetSystemMetrics( SM_CYSMICON );
   LPCWSTR icon = MAKEINTRESOURCEW( 1 );

   if( s_windowClass != 0 )
      return TRUE;

   memset( &windowClass, 0, sizeof( windowClass ) );
   windowClass.cbSize = sizeof( windowClass );
   windowClass.style = CS_HREDRAW | CS_VREDRAW;
   windowClass.lpfnWndProc = s_windowProc;
   windowClass.hInstance = instance;
   /* The icon put into the program by xdev from the .ico of the .rmk, otherwise the one of Windows */
   windowClass.hIcon = LoadIconW( instance, icon );
   windowClass.hIconSm = ( HICON )LoadImageW( instance, icon, IMAGE_ICON, iconWidth, iconHeight, LR_DEFAULTCOLOR );
   windowClass.hCursor = LoadCursorW( NULL, IDC_ARROW );
   windowClass.hbrBackground = ( HBRUSH )( COLOR_BTNFACE + 1 );
   windowClass.lpszClassName = s_windowClassName;
   if( windowClass.hIcon == NULL )
      windowClass.hIcon = LoadIconW( NULL, IDI_APPLICATION );

   s_windowClass = RegisterClassExW( &windowClass );

   return s_windowClass != 0;
}

/* The frame of a window with a client area of width x height at 96 DPI, for dpi */
static void s_frameForDpi( RECT *frame, int width, int height, UINT dpi, DWORD style, DWORD exStyle )
{
   *frame = ( RECT ){ 0, 0, x_guiScale( width, dpi ), x_guiScale( height, dpi ) };
   AdjustWindowRectExForDpi( frame, style, FALSE, exStyle, dpi );
}

/* Center a window over its owner, or on its screen without one, inside the work area */
static void s_centerOnOwner( HWND window, HWND owner )
{
   MONITORINFO monitor;
   RECT frame, area;
   int x, y, width, height;
   HMONITOR screen;

   memset( &monitor, 0, sizeof( monitor ) );
   monitor.cbSize = sizeof( monitor );
   GetWindowRect( window, &frame );
   width = frame.right - frame.left;
   height = frame.bottom - frame.top;
   screen = MonitorFromWindow( IIF( owner != NULL, owner, window ), MONITOR_DEFAULTTONEAREST );
   if( !GetMonitorInfoW( screen, &monitor ) )
      return;

   area = monitor.rcWork;
   if( owner != NULL )
      GetWindowRect( owner, &frame );
   else
      frame = area;

   x = frame.left + ( frame.right - frame.left - width ) / 2;
   y = frame.top + ( frame.bottom - frame.top - height ) / 2;
   x = IIF( x + width > area.right, area.right - width, x );
   y = IIF( y + height > area.bottom, area.bottom - height, y );
   x = IIF( x < area.left, area.left, x );
   y = IIF( y < area.top, area.top, y );
   SetWindowPos( window, NULL, x, y, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE );
}

/* Create the window of a window object through its __GuiCreate() message */
static void s_windowSend( X_CALL_CTX *ctx, X_ITEM *target, const X_ITEM *owner, const X_ITEM *title, int width,
                          int height, X_GUI_WINDOW_KIND kind, int id )
{
   X_ITEM values[6];
   X_ARG args[6];
   int i;

   for( i = 0; i < 6; ++i )
      values[i] = ( X_ITEM )X_ITEM_NIL_INIT;

   if( owner != NULL )
      x_itemCopy( &values[0], owner );

   if( title != NULL )
      x_itemCopy( &values[1], title );

   x_itemPutNI( &values[2], width );
   x_itemPutNI( &values[3], height );
   x_itemPutNI( &values[4], ( int )kind );
   x_itemPutNI( &values[5], id );
   for( i = 0; i < 6; ++i )
      args[i] = ( X_ARG ){ &values[i], 0 };

   x_objSend( ctx, ctx->result, target, "__GUICREATE", 6, args );
   for( i = 0; i < 6; ++i )
      x_itemClear( &values[i] );
}

/* gui_Window():New(): a top-level window, 640 x 480 by default */
void x_guiWindowNew( X_CALL_CTX *ctx )
{
   int width = x_parnidef( ctx, 2, 640 );
   int height = x_parnidef( ctx, 3, 480 );

   s_windowSend( ctx, ctx->self, NULL, x_paritem( ctx, 1 ), width, height, X_GUI_WINDOW_TOP, 0 );
}

/* gui_Dialog():New(): a dialog over its owner, 400 x 300 by default */
void x_guiDialogNew( X_CALL_CTX *ctx )
{
   const X_ITEM *owner = x_paritem( ctx, 1 );
   int width = x_parnidef( ctx, 3, 400 );
   int height = x_parnidef( ctx, 4, 300 );

   if( owner != NULL && !X_IS_NIL( owner ) && x_guiObjectWindow( ctx, owner, "GUI_DIALOG:NEW" ) == NULL )
      return;

   s_windowSend( ctx, ctx->self, owner, x_paritem( ctx, 2 ), width, height, X_GUI_WINDOW_DIALOG, 0 );
}

/* A panel holds the controls of one part of a window; without nWidth and nHeight it fills the window */
void x_guiPanelNew( X_CALL_CTX *ctx )
{
   const X_ITEM *owner = x_paritem( ctx, 1 );
   const int fill = X_ISNIL( ctx, 4 ) && X_ISNIL( ctx, 5 );
   HWND parent, panel;
   RECT client;
   UINT dpi;
   int width, height, x, y;

   parent = x_guiObjectWindow( ctx, owner, "GUI_PANEL:NEW" );
   if( parent == NULL )
      return;

   dpi = GetDpiForWindow( parent );
   GetClientRect( parent, &client );
   width = x_parnidef( ctx, 4, 1 );
   height = x_parnidef( ctx, 5, 1 );
   if( fill )
   {
      width = IIF( client.right > 0, MulDiv( client.right, USER_DEFAULT_SCREEN_DPI, ( int )dpi ), 1 );
      height = IIF( client.bottom > 0, MulDiv( client.bottom, USER_DEFAULT_SCREEN_DPI, ( int )dpi ), 1 );
   }

   s_windowSend( ctx, ctx->self, owner, NULL, width, height, X_GUI_WINDOW_PANEL, 0 );
   panel = s_objectHandle( ctx, ctx->self );
   if( panel == NULL || !IsWindow( panel ) )
      return;

   x_callbackEnterDll();
   if( fill )
   {
      SetPropW( panel, s_fillProperty, ( HANDLE )1 );
      s_panelPlace( panel );
   }
   else
   {
      x = x_guiScale( x_parnidef( ctx, 2, 0 ), dpi );
      y = x_guiScale( x_parnidef( ctx, 3, 0 ), dpi );
      SetWindowPos( panel, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE );
   }

   x_callbackLeaveDll();
   x_callbackRaisePending();
}

/* __GuiCreate(): create the window of a window, dialog, page or panel object */
void x_guiWindowCreate( X_CALL_CTX *ctx )
{
   static const DWORD styles[] = { WS_OVERLAPPEDWINDOW, WS_POPUP | WS_CAPTION | WS_SYSMENU,
                                     WS_CHILD | WS_CLIPSIBLINGS, WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE };
   static const DWORD exStyles[] = { 0, WS_EX_DLGMODALFRAME, WS_EX_CONTROLPARENT, WS_EX_CONTROLPARENT };
   HINSTANCE instance = GetModuleHandleW( NULL );
   X_ITEM *slot = x_objDataSlot( ctx, 1 );
   const X_ITEM *ownerItem = x_paritem( ctx, 1 );
   const int width = x_parni( ctx, 3 );
   const int height = x_parni( ctx, 4 );
   const int kind = x_parni( ctx, 5 );
   const int dialog = kind == X_GUI_WINDOW_DIALOG;
   const int page = kind == X_GUI_WINDOW_PAGE;
   const int child = page || kind == X_GUI_WINDOW_PANEL;
   const char *operation = IIF( dialog, "GUI_DIALOG:NEW", "GUI_WINDOW:NEW" );
   LPVOID param = ( LPVOID )ctx->self;
   int valid, pos, cx, cy;
   HMENU menu;
   DWORD style;
   DWORD exStyle;
   HWND owner = NULL;
   wchar_t *title;
   HWND window;
   UINT dpi;
   RECT frame;
   DWORD error;

   valid = slot != NULL && kind >= X_GUI_WINDOW_TOP && kind <= X_GUI_WINDOW_PANEL && width > 0 && height > 0;
   valid = valid && ( X_ISNIL( ctx, 2 ) || X_ISCHAR( ctx, 2 ) );
   if( !valid )
   {
      x_guiRaise( ctx, operation );
      return;
   }

   style = styles[kind] | WS_CLIPCHILDREN;
   exStyle = exStyles[kind];
   if( page )
   {
      owner = s_objectHandle( ctx, ( X_ITEM * )ownerItem );
      if( owner == NULL || !IsWindow( owner ) )
      {
         x_guiRaise( ctx, "GUI_TABCONTROL:ADDPAGE" );
         return;
      }
   }
   else if( kind == X_GUI_WINDOW_PANEL )
   {
      owner = x_guiObjectWindow( ctx, ownerItem, "GUI_PANEL:NEW" );
      if( owner == NULL )
         return;
   }
   else if( ownerItem != NULL && !X_IS_NIL( ownerItem ) )
   {
      owner = x_guiObjectWindow( ctx, ownerItem, "GUI_DIALOG:NEW" );
      if( owner == NULL )
         return;
   }

   if( !s_dpiAware )
   {
      SetProcessDpiAwarenessContext( DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 );
      s_dpiAware = 1;
   }

   if( !s_registerWindowClass( instance ) )
   {
      x_guiRaiseWin32( ctx, "RegisterClassExW", GetLastError() );
      return;
   }

   if( X_ISNIL( ctx, 2 ) )
      title = ( wchar_t * )x_xgrabzArray( 1, sizeof( wchar_t ) );
   else
   {
      title = x_guiText( ctx, x_paritem( ctx, 2 ), operation );
      if( title == NULL )
         return;
   }

   dpi = IIF( owner != NULL, GetDpiForWindow( owner ), GetDpiForSystem() );
   s_frameForDpi( &frame, width, height, dpi, style, exStyle );
   x_callbackEnterDll();
   pos = IIF( child, 0, CW_USEDEFAULT );
   cx = frame.right - frame.left;
   cy = frame.bottom - frame.top;
   menu = IIF( page, ( HMENU )( INT_PTR )x_parni( ctx, 6 ), NULL );
   window = CreateWindowExW( exStyle, s_windowClassName, title, style, pos, pos, cx, cy, owner, menu, instance, param );
   error = GetLastError();
   if( window != NULL && !child && GetDpiForWindow( window ) != dpi )
   {
      dpi = GetDpiForWindow( window );
      s_frameForDpi( &frame, width, height, dpi, style, exStyle );
      cx = frame.right - frame.left;
      cy = frame.bottom - frame.top;
      SetWindowPos( window, NULL, 0, 0, cx, cy, SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE );
   }

   if( window != NULL )
   {
      SetPropW( window, s_dpiProperty, ( HANDLE )( UINT_PTR )dpi );
      if( owner != NULL && !child )
         s_centerOnOwner( window, owner );
   }

   x_callbackLeaveDll();
   x_xfree( title );
   x_callbackRaisePending();

   if( window == NULL )
   {
      x_guiRaiseWin32( ctx, "CreateWindowExW", error );
      return;
   }

   x_itemPutPtr( slot, window );
   x_guiReturnSelf( ctx );
}

/* A frame for dragging the size and the button for the full screen; the controls follow their anchors */
int x_guiWindowSizable( HWND window )
{
   return ( GetWindowLongPtrW( window, GWL_STYLE ) & WS_THICKFRAME ) != 0;
}

/* Turn the sizing frame and the maximize button of a window on or off */
void x_guiWindowSetSizable( HWND window, int sizable )
{
   LONG_PTR style = GetWindowLongPtrW( window, GWL_STYLE );

   if( sizable )
      style |= WS_THICKFRAME | WS_MAXIMIZEBOX;
   else
      style &= ~( LONG_PTR )( WS_THICKFRAME | WS_MAXIMIZEBOX );

   x_callbackEnterDll();
   SetWindowLongPtrW( window, GWL_STYLE, style );
   SetWindowPos( window, NULL, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE );
   x_callbackLeaveDll();
   x_callbackRaisePending();
}

/* Set the menu bar of a window; the window grows so its client area keeps its size */
void x_guiWindowSetMenu( X_CALL_CTX *ctx, HWND window, HMENU menu )
{
   RECT before, after, frame;
   BOOL done;
   DWORD error;

   GetClientRect( window, &before );
   x_callbackEnterDll();
   done = SetMenu( window, menu );
   error = GetLastError();
   if( done )
   {
      GetClientRect( window, &after );
      GetWindowRect( window, &frame );
      if( after.bottom < before.bottom )
      {
         int width = frame.right - frame.left;
         int height = frame.bottom - frame.top + before.bottom - after.bottom;

         SetWindowPos( window, NULL, 0, 0, width, height, SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE );
      }
   }

   x_callbackLeaveDll();
   x_callbackRaisePending();
   if( !done )
      x_guiRaiseWin32( ctx, "SetMenu", error );
}

/* The block bWhen of the command gives .F. when the command cannot be chosen now */
static int s_actionAllowed( X_CALL_CTX *ctx, const X_GUI_ACTION *action )
{
   X_ITEM result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM block __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;

   if( action->when == NULL || !X_IS_BLOCK( action->when ) )
      return 1;

   x_itemCopy( &block, action->when );
   x_evalBlock( ctx, &result, &block, 0, NULL );

   return !X_IS_LOGICAL( &result ) || result.u.logical;
}

/* Before a menu opens its commands with bWhen become active or grey */
void x_guiWindowMenuWhen( X_CALL_CTX *ctx, HMENU menu )
{
   int count = IIF( menu != NULL && IsMenu( menu ), GetMenuItemCount( menu ), 0 );
   int item;
   UINT id;
   size_t i;

   for( item = 0; item < count; ++item )
   {
      id = GetMenuItemID( menu, item );
      if( id == ( UINT )-1 )
         continue;

      for( i = 0; i < s_actionCount; ++i )
      {
         if( s_actions[i].id == ( int )id && s_actions[i].when != NULL && X_IS_BLOCK( s_actions[i].when ) )
         {
            UINT enable = IIF( s_actionAllowed( ctx, &s_actions[i] ), MF_ENABLED, MF_GRAYED );

            EnableMenuItem( menu, ( UINT )item, MF_BYPOSITION | enable );
            break;
         }
      }
   }
}

/* Run the action of a menu command with the window as its parameter, unless its bWhen refuses */
void x_guiWindowRunMenu( X_CALL_CTX *ctx, int id )
{
   X_ITEM result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM block __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ARG args[1];
   size_t i;

   for( i = 0; i < s_actionCount; ++i )
   {
      if( s_actions[i].id == id )
      {
         if( !X_IS_BLOCK( s_actions[i].action ) || !s_actionAllowed( ctx, &s_actions[i] ) )
            return;

         x_itemCopy( &block, s_actions[i].action );
         args[0] = ( X_ARG ){ ctx->self, 0 };
         x_evalBlock( ctx, &result, &block, 1, args );
         return;
      }
   }
}

/* The width or the height of the client area at 96 DPI */
int x_guiClientSize( HWND window, X_GUI_BOUND part )
{
   RECT client;
   int size;

   GetClientRect( window, &client );
   size = IIF( part == X_GUI_HEIGHT, client.bottom, client.right );

   return MulDiv( size, USER_DEFAULT_SCREEN_DPI, ( int )GetDpiForWindow( window ) );
}

/* Set the width or the height of the client area, given at 96 DPI */
void x_guiSetClientSize( X_CALL_CTX *ctx, HWND window, X_GUI_BOUND part, int value )
{
   const UINT dpi = GetDpiForWindow( window );
   RECT client, frame;
   DWORD style, exStyle;
   int width, height;

   if( value <= 0 )
   {
      x_guiRaise( ctx, "GUI_WINDOW:SIZE" );
      return;
   }

   GetClientRect( window, &client );
   if( part == X_GUI_HEIGHT )
      client.bottom = x_guiScale( value, dpi );
   else
      client.right = x_guiScale( value, dpi );

   frame = ( RECT ){ 0, 0, client.right, client.bottom };
   style = ( DWORD )GetWindowLongPtrW( window, GWL_STYLE );
   exStyle = ( DWORD )GetWindowLongPtrW( window, GWL_EXSTYLE );
   AdjustWindowRectExForDpi( &frame, style, GetMenu( window ) != NULL, exStyle, dpi );
   width = frame.right - frame.left;
   height = frame.bottom - frame.top;
   x_callbackEnterDll();
   SetWindowPos( window, NULL, 0, 0, width, height, SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE );
   x_callbackLeaveDll();
   x_callbackRaisePending();
}

/* Return a color of the window, NIL for the system color */
void x_guiWindowColor( X_CALL_CTX *ctx, X_GUI_COLOR part )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_WINDOW:COLOR" );
   const X_GUI_PAINT *paint = IIF( window != NULL, s_paintOf( window ), NULL );

   if( paint != NULL && paint->colors[part] >= 0 )
      x_retni( ctx, paint->colors[part] );
}

/* Set a color of the window, NIL for the system color; the controls are drawn again */
void x_guiWindowSetColor( X_CALL_CTX *ctx, X_GUI_COLOR part )
{
   HWND window = x_guiSelfHandle( ctx, "GUI_WINDOW:COLOR" );
   X_GUI_PAINT *paint;

   if( window == NULL )
      return;

   if( !X_ISNIL( ctx, 1 ) && ( !X_ISNUM( ctx, 1 ) || x_parnint( ctx, 1 ) < 0 || x_parnint( ctx, 1 ) > 0xFFFFFF ) )
   {
      x_guiRaise( ctx, "GUI_WINDOW:COLOR" );
      return;
   }

   paint = ( X_GUI_PAINT * )GetPropW( window, s_paintProperty );
   if( paint == NULL )
   {
      paint = ( X_GUI_PAINT * )x_xgrab( sizeof( X_GUI_PAINT ) );
      paint->colors[X_GUI_COLOR_FOREGROUND] = -1;
      paint->colors[X_GUI_COLOR_BACKGROUND] = -1;
      paint->brush = NULL;
      SetPropW( window, s_paintProperty, paint );
   }

   paint->colors[part] = IIF( X_ISNIL( ctx, 1 ), -1, ( int )x_parnint( ctx, 1 ) );
   if( part == X_GUI_COLOR_BACKGROUND )
   {
      if( paint->brush != NULL )
         DeleteObject( paint->brush );

      paint->brush = IIF( paint->colors[part] >= 0, CreateSolidBrush( s_colorRef( paint->colors[part] ) ), NULL );
   }

   x_callbackEnterDll();
   EnumChildWindows( window, s_paintThemeChild, ( LPARAM )window );
   RedrawWindow( window, NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN );
   x_callbackLeaveDll();
   x_callbackRaisePending();
   if( x_paritem( ctx, 1 ) != NULL )
      x_itemReturn( ctx, x_paritem( ctx, 1 ) );
}

/* Center a window over its owner */
void x_guiCenter( HWND window )
{
   x_callbackEnterDll();
   s_centerOnOwner( window, GetWindow( window, GW_OWNER ) );
   x_callbackLeaveDll();
   x_callbackRaisePending();
}

/* Show or hide a window */
void x_guiShow( HWND window, int visible )
{
   x_callbackEnterDll();
   ShowWindow( window, IIF( visible, SW_SHOW, SW_HIDE ) );
   x_callbackLeaveDll();
   x_callbackRaisePending();
}

/* Close a window */
void x_guiDestroy( HWND window )
{
   x_callbackEnterDll();
   s_destroyWindow( window );
   x_callbackLeaveDll();
   x_callbackRaisePending();
}

/* The shortcut of an active menu command runs the command */
static int s_shortcut( const MSG *message )
{
   HWND root;
   HMENU menu;
   UINT modifiers = 0, state;
   size_t i;

   if( s_reading || ( message->message != WM_KEYDOWN && message->message != WM_SYSKEYDOWN ) )
      return 0;

   root = GetAncestor( message->hwnd, GA_ROOT );
   if( root == NULL || ( ATOM )GetClassLongPtrW( root, GCW_ATOM ) != s_windowClass )
      return 0;

   menu = GetMenu( root );
   if( menu == NULL )
      return 0;

   modifiers |= IIF( GetKeyState( VK_CONTROL ) < 0, X_GUI_CTRL, 0 );
   modifiers |= IIF( GetKeyState( VK_SHIFT ) < 0, X_GUI_SHIFT, 0 );
   modifiers |= IIF( GetKeyState( VK_MENU ) < 0, X_GUI_ALT, 0 );
   for( i = 0; i < s_actionCount; ++i )
   {
      if( s_actions[i].key != 0 && s_actions[i].key == ( UINT )message->wParam && s_actions[i].modifiers == modifiers )
      {
         state = GetMenuState( menu, ( UINT )s_actions[i].id, MF_BYCOMMAND );
         if( state == ( UINT )-1 || ( state & ( MF_DISABLED | MF_GRAYED ) ) )
            continue;

         SendMessageW( root, WM_COMMAND, MAKEWPARAM( s_actions[i].id, 1 ), 0 );
         return 1;
      }
   }

   return 0;
}

/* A key of SetKey() of the window with the focus or of a window around it, the nearest first, runs
   its block; it comes before the shortcuts of the menu and of the toolbars */
static int s_windowKey( const MSG *message )
{
   X_ITEM result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   UINT modifiers = 0;
   int64_t values[1];
   HWND current;
   size_t i;

   if( s_reading || ( message->message != WM_KEYDOWN && message->message != WM_SYSKEYDOWN ) )
      return 0;

   modifiers |= IIF( GetKeyState( VK_CONTROL ) < 0, X_GUI_CTRL, 0 );
   modifiers |= IIF( GetKeyState( VK_SHIFT ) < 0, X_GUI_SHIFT, 0 );
   modifiers |= IIF( GetKeyState( VK_MENU ) < 0, X_GUI_ALT, 0 );
   for( current = message->hwnd; current != NULL; current = GetParent( current ) )
   {
      const X_GUI_KEYS *keys = ( const X_GUI_KEYS * )GetPropW( current, s_keysProperty );
      X_ITEM *object = ( X_ITEM * )GetWindowLongPtrW( current, GWLP_USERDATA );

      if( keys != NULL && object != NULL && ( ATOM )GetClassLongPtrW( current, GCW_ATOM ) == s_windowClass )
      {
         for( i = 0; i < keys->count; ++i )
         {
            if( keys->keys[i] == ( UINT )message->wParam && keys->modifiers[i] == modifiers )
            {
               values[0] = ( int64_t )i + 1;
               x_guiNotify( object, "__GUIKEY", &result, 1, values );
               return 1;
            }
         }
      }

      if( !( GetWindowLongPtrW( current, GWL_STYLE ) & WS_CHILD ) )
         break;
   }

   return 0;
}

/* A key looked up among the toolbar buttons of a window and the button found */
typedef struct
{
   UINT key;
   UINT modifiers;
   HWND toolbar;
   int button;
   int owned;
} X_GUI_KEYHIT;

/* Look for the key among the buttons of one toolbar */
static BOOL CALLBACK s_toolbarKeyChild( HWND child, LPARAM data )
{
   X_GUI_KEYHIT *hit = ( X_GUI_KEYHIT * )data;
   const X_GUI_BAR *bar = s_toolbarOf( child );
   size_t i;

   if( bar == NULL || !IsWindowVisible( child ) || !IsWindowEnabled( child ) )
      return TRUE;

   for( i = 0; i < bar->count; ++i )
   {
      if( bar->keys[i] != 0 && bar->keys[i] == hit->key && bar->modifiers[i] == hit->modifiers )
      {
         hit->owned = 1;
         if( SendMessageW( child, TB_ISBUTTONENABLED, ( WPARAM )( i + 1 ), 0 ) )
         {
            hit->toolbar = child;
            hit->button = ( int )i + 1;
            return FALSE;
         }
      }
   }

   return TRUE;
}

/* The shortcut of a toolbar button of a visible toolbar clicks the button; a key without Ctrl and Alt
   stays with an edit field, so Del there deletes a character; the key of a disabled button is taken
   too, so F10 of a disabled button does not open the menu of the window */
static int s_toolbarKey( const MSG *message )
{
   X_GUI_KEYHIT hit = { ( UINT )message->wParam, 0, NULL, 0, 0 };
   HWND root = GetAncestor( message->hwnd, GA_ROOT );
   int function;

   if( s_reading || ( message->message != WM_KEYDOWN && message->message != WM_SYSKEYDOWN ) )
      return 0;

   if( root == NULL || ( ATOM )GetClassLongPtrW( root, GCW_ATOM ) != s_windowClass )
      return 0;

   hit.modifiers |= IIF( GetKeyState( VK_CONTROL ) < 0, X_GUI_CTRL, 0 );
   hit.modifiers |= IIF( GetKeyState( VK_SHIFT ) < 0, X_GUI_SHIFT, 0 );
   hit.modifiers |= IIF( GetKeyState( VK_MENU ) < 0, X_GUI_ALT, 0 );
   function = hit.key >= VK_F1 && hit.key <= VK_F24;
   if( !( hit.modifiers & ( X_GUI_CTRL | X_GUI_ALT ) ) && !function && s_hasClass( message->hwnd, L"Edit" ) )
      return 0;

   EnumChildWindows( root, s_toolbarKeyChild, ( LPARAM )&hit );
   if( hit.toolbar == NULL )
      return hit.owned;

   SendMessageW( GetParent( hit.toolbar ), WM_COMMAND, MAKEWPARAM( hit.button, BN_CLICKED ), ( LPARAM )hit.toolbar );

   return 1;
}

/* The first visible tab control of the window */
static BOOL CALLBACK s_findTab( HWND child, LPARAM found )
{
   if( !s_hasClass( child, WC_TABCONTROLW ) || !IsWindowVisible( child ) )
      return TRUE;

   *( HWND * )found = child;

   return FALSE;
}

/* Ctrl+Tab and Ctrl+PgDn select the next page, with Shift or Ctrl+PgUp the previous one, as in property
   sheets: of the tab control holding the focus, otherwise of the first one in the window */
static int s_tabKey( const MSG *message )
{
   HWND tab = NULL;
   HWND root = GetAncestor( message->hwnd, GA_ROOT );
   HWND current, page;
   int count, index, back;
   int ctrl = GetKeyState( VK_CONTROL ) < 0 && GetKeyState( VK_MENU ) >= 0;
   int pageKey = message->wParam == VK_TAB || message->wParam == VK_PRIOR || message->wParam == VK_NEXT;

   if( message->message != WM_KEYDOWN || !ctrl || !pageKey )
      return 0;

   if( root == NULL || ( ATOM )GetClassLongPtrW( root, GCW_ATOM ) != s_windowClass )
      return 0;

   for( current = message->hwnd; current != NULL && current != root && tab == NULL; current = GetParent( current ) )
   {
      if( s_hasClass( current, WC_TABCONTROLW ) )
         tab = current;
   }

   if( tab == NULL )
      EnumChildWindows( root, s_findTab, ( LPARAM )&tab );

   if( tab == NULL )
      return 0;

   count = ( int )SendMessageW( tab, TCM_GETITEMCOUNT, 0, 0 );
   if( count < 2 )
      return 0;

   back = message->wParam == VK_PRIOR || ( message->wParam == VK_TAB && GetKeyState( VK_SHIFT ) < 0 );
   index = ( int )SendMessageW( tab, TCM_GETCURSEL, 0, 0 );
   index = ( index + IIF( back, count - 1, 1 ) ) % count;
   SendMessageW( tab, TCM_SETCURFOCUS, ( WPARAM )index, 0 );
   for( page = GetWindow( tab, GW_CHILD ); page != NULL; page = GetWindow( page, GW_HWNDNEXT ) )
   {
      if( IsWindowVisible( page ) && ( ATOM )GetClassLongPtrW( page, GCW_ATOM ) == s_windowClass )
         break;
   }

   current = IIF( page != NULL, GetNextDlgTabItem( page, NULL, FALSE ), NULL );
   SetFocus( IIF( current != NULL, current, tab ) );

   return 1;
}

/* Dispatch a message of the loop: keys of SetKey(), shortcuts and the Tab of dialogs first */
static void s_dispatchMessage( MSG *message )
{
   HWND root = GetAncestor( message->hwnd, GA_ROOT );
   int arrow, own;

   if( s_windowKey( message ) || s_shortcut( message ) || s_toolbarKey( message ) || s_tabKey( message ) )
      return;

   /* An arrow in the window itself or in its status bar goes to the menu bar, as F10 does */
   arrow = message->message == WM_KEYDOWN && ( message->wParam == VK_LEFT || message->wParam == VK_RIGHT );
   arrow = arrow && ( message->hwnd == root || s_hasClass( message->hwnd, STATUSCLASSNAMEW ) );
   arrow = arrow && GetMenu( root ) != NULL && GetKeyState( VK_CONTROL ) >= 0 && GetKeyState( VK_MENU ) >= 0;
   if( arrow )
   {
      PostMessageW( root, WM_SYSCOMMAND, SC_KEYMENU, 0 );
      return;
   }

   own = root != NULL && ( ATOM )GetClassLongPtrW( root, GCW_ATOM ) == s_windowClass;
   if( !own || !IsDialogMessageW( root, message ) )
   {
      TranslateMessage( message );
      DispatchMessageW( message );
   }
}

/* Show a dialog modal over its owner and run messages until it closes */
void x_guiDialogRun( X_CALL_CTX *ctx, HWND dialog )
{
   HWND owner = GetWindow( dialog, GW_OWNER );
   int previousRunning = s_running;
   MSG message;
   int status = 1;

   s_running = 1;
   x_callbackEnterDll();
   SetPropW( dialog, s_modalProperty, dialog );
   if( owner != NULL )
      EnableWindow( owner, FALSE );

   ShowWindow( dialog, SW_SHOW );
   UpdateWindow( dialog );
   while( IsWindow( dialog ) && ( status = ( int )GetMessageW( &message, NULL, 0, 0 ) ) > 0 )
      s_dispatchMessage( &message );

   if( IsWindow( dialog ) )
      s_destroyWindow( dialog );

   if( status == 0 && previousRunning )
      PostQuitMessage( ( int )message.wParam );

   s_running = previousRunning;
   x_callbackLeaveDll();
   x_callbackRaisePending();

   if( status == -1 )
      x_guiRaiseWin32( ctx, "GetMessageW", GetLastError() );
}

/* Show the main window and run messages until it closes */
void x_guiRun( X_CALL_CTX *ctx, HWND window )
{
   HWND previousWindow = s_mainWindow;
   int previousRunning = s_running;
   MSG message;
   int status;

   s_mainWindow = window;
   s_running = 1;
   x_callbackEnterDll();
   ShowWindow( window, SW_SHOW );
   UpdateWindow( window );
   while( ( status = ( int )GetMessageW( &message, NULL, 0, 0 ) ) > 0 )
      s_dispatchMessage( &message );

   s_running = 0;
   if( IsWindow( window ) )
      DestroyWindow( window );

   s_mainWindow = IIF( IsWindow( previousWindow ), previousWindow, NULL );
   s_running = previousRunning;
   x_callbackLeaveDll();
   x_callbackRaisePending();

   if( status == -1 )
      x_guiRaiseWin32( ctx, "GetMessageW", GetLastError() );
   else
      x_retl( ctx, 1 );
}

/* ---------------------------------------------------------------------------------------------
   Pictures
   --------------------------------------------------------------------------------------------- */

/* A big-endian 32-bit number of a PNG file */
static unsigned s_pngNumber( const unsigned char *bytes )
{
   return ( ( unsigned )bytes[0] << 24 ) | ( ( unsigned )bytes[1] << 16 ) | ( ( unsigned )bytes[2] << 8 ) | bytes[3];
}

/* Sample number index of a row of samples of depth bits */
static unsigned s_pngSample( const unsigned char *row, unsigned index, int depth )
{
   unsigned bit;

   if( depth == 8 )
      return row[index];

   if( depth == 16 )
      return ( ( unsigned )row[index * 2] << 8 ) | row[index * 2 + 1];

   bit = index * ( unsigned )depth;

   return ( row[bit >> 3] >> ( 8 - depth - ( int )( bit & 7 ) ) ) & ( ( 1u << depth ) - 1 );
}

/* A sample made 8-bit */
static unsigned char s_pngByte( unsigned sample, int depth )
{
   if( depth == 16 )
      return ( unsigned char )( sample >> 8 );

   return ( unsigned char )( sample * 255 / ( ( 1u << depth ) - 1 ) );
}

/* The Paeth predictor of PNG: the neighbor nearest to a + b - c */
static int s_pngPaeth( int a, int b, int c )
{
   int p = a + b - c, pa = abs( p - a ), pb = abs( p - b ), pc = abs( p - c );

   if( pa <= pb && pa <= pc )
      return a;

   return IIF( pb <= pc, b, c );
}

/* PNG without interlacing: the pixels as rows of BGRA from the top, alpha not premultiplied; NULL for a
   damaged or unsupported file */
static unsigned char *s_pngDecode( const unsigned char *data, size_t size, unsigned *width, unsigned *height )
{
   static const unsigned char signature[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
   static const int channels[7] = { 1, 0, 3, 1, 2, 0, 4 };

   unsigned char palette[256][4];
   unsigned char *packed = NULL, *raw = NULL, *pixels = NULL;
   size_t packedSize = 0, pos = 8, stride, step;
   unsigned colors = 0, x, y, i, length, transparent[3] = { 0x10000, 0x10000, 0x10000 };
   int depth = 0, type = -1, interlace = 0, bits, done = 0, valid, lowDepth;
   uLongf rawSize;

   *width = 0;
   *height = 0;
   if( size < 8 || memcmp( data, signature, 8 ) != 0 )
      return NULL;

   memset( palette, 0, sizeof( palette ) );
   for( i = 0; i < 256; ++i )
      palette[i][3] = 255;

   while( !done && pos + 12 <= size )
   {
      const unsigned char *kind = data + pos + 4;
      const unsigned char *body = data + pos + 8;

      length = s_pngNumber( data + pos );
      if( length > size - pos - 12 )
         break;

      if( memcmp( kind, "IHDR", 4 ) == 0 && length >= 13 )
      {
         *width = s_pngNumber( body );
         *height = s_pngNumber( body + 4 );
         depth = body[8];
         type = body[9];
         interlace = body[12];
      }
      else if( memcmp( kind, "PLTE", 4 ) == 0 )
      {
         for( colors = 0; colors < 256 && colors * 3 + 2 < length; ++colors )
         {
            palette[colors][0] = body[colors * 3];
            palette[colors][1] = body[colors * 3 + 1];
            palette[colors][2] = body[colors * 3 + 2];
         }
      }
      else if( memcmp( kind, "tRNS", 4 ) == 0 )
      {
         if( type == 3 )
         {
            for( i = 0; i < length && i < 256; ++i )
               palette[i][3] = body[i];
         }
         else
         {
            for( i = 0; i < 3 && i * 2 + 1 < length; ++i )
               transparent[i] = ( ( unsigned )body[i * 2] << 8 ) | body[i * 2 + 1];
         }
      }
      else if( memcmp( kind, "IDAT", 4 ) == 0 )
      {
         packed = ( unsigned char * )x_xrealloc( packed, packedSize + length + 1 );
         memcpy( packed + packedSize, body, length );
         packedSize += length;
      }
      else if( memcmp( kind, "IEND", 4 ) == 0 )
         done = 1;

      pos += 12 + ( size_t )length;
   }

   valid = packed != NULL && *width > 0 && *height > 0 && *width <= 16384 && *height <= 16384 && interlace == 0;
   valid = valid && type >= 0 && type <= 6 && channels[type] != 0;
   lowDepth = ( type == 0 || type == 3 ) && ( depth == 1 || depth == 2 || depth == 4 || depth == 8 );
   valid = valid && ( lowDepth || ( type != 3 && depth == 16 ) || ( type != 3 && depth == 8 ) );
   if( !valid )
   {
      x_xfree( packed );
      return NULL;
   }

   bits = channels[type] * depth;
   stride = ( ( size_t )*width * ( size_t )bits + 7 ) / 8;
   step = ( size_t )IIF( bits >= 8, bits / 8, 1 );
   rawSize = ( uLongf )( ( stride + 1 ) * *height );
   raw = ( unsigned char * )x_xgrab( ( size_t )rawSize );
   if( uncompress( raw, &rawSize, packed, ( uLong )packedSize ) != Z_OK || rawSize != ( stride + 1 ) * *height )
   {
      x_xfree( packed );
      x_xfree( raw );
      return NULL;
   }

   x_xfree( packed );

   /* The filters of the rows are undone in place */
   for( y = 0; y < *height; ++y )
   {
      unsigned char *row = raw + y * ( stride + 1 ) + 1;
      const unsigned char *previous = IIF( y > 0, row - ( stride + 1 ), NULL );
      size_t n;

      for( n = 0; n < stride; ++n )
      {
         int a = IIF( n >= step, row[n - step], 0 );
         int b = IIF( previous != NULL, previous[n], 0 );
         int c = IIF( previous != NULL && n >= step, previous[n - step], 0 );

         switch( row[-1] )
         {
         case 0:
            break;
         case 1:
            row[n] = ( unsigned char )( row[n] + a );
            break;
         case 2:
            row[n] = ( unsigned char )( row[n] + b );
            break;
         case 3:
            row[n] = ( unsigned char )( row[n] + ( a + b ) / 2 );
            break;
         case 4:
            row[n] = ( unsigned char )( row[n] + s_pngPaeth( a, b, c ) );
            break;
         default:
            x_xfree( raw );
            return NULL;
         }
      }
   }

   pixels = ( unsigned char * )x_xgrab( ( size_t )*width * *height * 4 );
   for( y = 0; y < *height; ++y )
   {
      const unsigned char *row = raw + y * ( stride + 1 ) + 1;

      for( x = 0; x < *width; ++x )
      {
         unsigned char *pixel = pixels + ( ( size_t )y * *width + x ) * 4;
         unsigned first = s_pngSample( row, x * ( unsigned )channels[type], depth );
         unsigned r, g, b;

         switch( type )
         {
         case 0:
            pixel[0] = pixel[1] = pixel[2] = s_pngByte( first, depth );
            pixel[3] = ( unsigned char )IIF( first == transparent[0], 0, 255 );
            break;
         case 2:
            r = first;
            g = s_pngSample( row, x * 3 + 1, depth );
            b = s_pngSample( row, x * 3 + 2, depth );
            pixel[0] = s_pngByte( b, depth );
            pixel[1] = s_pngByte( g, depth );
            pixel[2] = s_pngByte( r, depth );
            pixel[3] = 255;
            if( r == transparent[0] && g == transparent[1] && b == transparent[2] )
               pixel[3] = 0;
            break;
         case 3:
            pixel[0] = palette[first & 0xFF][2];
            pixel[1] = palette[first & 0xFF][1];
            pixel[2] = palette[first & 0xFF][0];
            pixel[3] = palette[first & 0xFF][3];
            break;
         case 4:
            pixel[0] = pixel[1] = pixel[2] = s_pngByte( first, depth );
            pixel[3] = s_pngByte( s_pngSample( row, x * 2 + 1, depth ), depth );
            break;
         default:
            pixel[2] = s_pngByte( first, depth );
            pixel[1] = s_pngByte( s_pngSample( row, x * 4 + 1, depth ), depth );
            pixel[0] = s_pngByte( s_pngSample( row, x * 4 + 2, depth ), depth );
            pixel[3] = s_pngByte( s_pngSample( row, x * 4 + 3, depth ), depth );
            break;
         }
      }
   }

   x_xfree( raw );
   UNUSED( colors );

   return pixels;
}

/* The bytes of a picture: put into the program by xdev under its name or under the name of the file without
   the folder and the extension, otherwise read from the file */
static unsigned char *s_pictureBytes( const char *name, size_t *size, int *loaded )
{
   const unsigned char *found = x_resourceFind( name, size );
   const char *start = name;
   const char *dot;
   char stem[260];
   int stemLength;

   *loaded = 0;
   if( found == NULL )
   {
      for( dot = name; *dot != '\0'; ++dot )
      {
         if( *dot == '\\' || *dot == '/' )
            start = dot + 1;
      }

      dot = strrchr( start, '.' );
      stemLength = ( int )IIF( dot != NULL, dot - start, ( ptrdiff_t )strlen( start ) );
      snprintf( stem, sizeof( stem ), "%.*s", stemLength, start );
      found = x_resourceFind( stem, size );
   }

   if( found != NULL )
      return ( unsigned char * )found;

   *loaded = 1;

   return ( unsigned char * )x_fileLoad( name, ( size_t )64 * 1024 * 1024, size );
}

/* A PNG or BMP picture as a DIB section; NULL with an error raised */
HBITMAP x_guiPicture( X_CALL_CTX *ctx, const X_ITEM *file, const char *operation )
{
   void *handle = NULL;
   size_t length = 0, size = 0;
   const char *name = IIF( file != NULL && X_IS_STRING( file ), x_itemGetStrUTF8( file, &handle, &length ), NULL );
   unsigned char *bytes = NULL, *pixels;
   const BITMAPFILEHEADER *header;
   const BITMAPINFO *info;
   BITMAPINFO format;
   HBITMAP picture = NULL;
   void *bits;
   unsigned width, height;
   int loaded = 0, valid;
   size_t stride, rows;

   if( name != NULL && memchr( name, '\0', length ) == NULL )
      bytes = s_pictureBytes( name, &size, &loaded );

   x_strfree( handle );
   if( bytes == NULL )
   {
      x_guiRaise( ctx, operation );
      return NULL;
   }

   pixels = s_pngDecode( bytes, size, &width, &height );
   if( pixels != NULL )
   {
      memset( &format, 0, sizeof( format ) );
      format.bmiHeader.biSize = sizeof( format.bmiHeader );
      format.bmiHeader.biWidth = ( LONG )width;
      format.bmiHeader.biHeight = -( LONG )height;
      format.bmiHeader.biPlanes = 1;
      format.bmiHeader.biBitCount = 32;
      format.bmiHeader.biCompression = BI_RGB;
      picture = CreateDIBSection( NULL, &format, DIB_RGB_COLORS, &bits, NULL, 0 );
      if( picture != NULL )
         memcpy( bits, pixels, ( size_t )width * height * 4 );

      x_xfree( pixels );
   }
   else if( size > sizeof( BITMAPFILEHEADER ) + sizeof( BITMAPINFOHEADER ) && bytes[0] == 'B' && bytes[1] == 'M' )
   {
      header = ( const BITMAPFILEHEADER * )bytes;
      info = ( const BITMAPINFO * )( bytes + sizeof( BITMAPFILEHEADER ) );
      stride = ( ( ( size_t )info->bmiHeader.biWidth * info->bmiHeader.biBitCount + 31 ) / 32 ) * 4;
      rows = ( size_t )abs( info->bmiHeader.biHeight );
      valid = info->bmiHeader.biCompression == BI_RGB && info->bmiHeader.biWidth > 0 && header->bfOffBits < size;
      if( valid && stride * rows <= size - header->bfOffBits )
         picture = CreateDIBSection( NULL, info, DIB_RGB_COLORS, &bits, NULL, 0 );

      if( picture != NULL )
         memcpy( bits, bytes + header->bfOffBits, stride * rows );
   }

   if( loaded )
      x_xfree( bytes );

   if( picture == NULL )
      x_guiRaise( ctx, operation );

   return picture;
}

/* ---------------------------------------------------------------------------------------------
   Menus
   --------------------------------------------------------------------------------------------- */

/* A menu bar, or with popup a drop-down menu; the kind is kept in the menu data */
HMENU x_guiMenuCreate( X_CALL_CTX *ctx, int popup )
{
   HMENU menu = IIF( popup, CreatePopupMenu(), CreateMenu() );
   MENUINFO info = { sizeof( info ), MIM_MENUDATA, 0, 0, NULL, 0, ( ULONG_PTR )( popup != 0 ) };

   if( menu == NULL )
      x_guiRaiseWin32( ctx, "CreateMenu", GetLastError() );
   else
      SetMenuInfo( menu, &info );

   return menu;
}

/* Whether a menu is a drop-down menu */
static int s_isPopupMenu( HMENU menu )
{
   MENUINFO info = { sizeof( info ), MIM_MENUDATA, 0, 0, NULL, 0, 0 };

   return GetMenuInfo( menu, &info ) && info.dwMenuData != 0;
}

/* Turn the extended style of menus on or off and set the size of their pictures; returns the previous style */
int x_guiMenuStyle( int extended, int imageSize )
{
   int previous = s_menuExtended;

   s_menuExtended = extended;
   if( imageSize > 0 )
      s_menuImageSize = imageSize;

   return previous;
}

/* The menu font for dpi, made again when the DPI changes */
static HFONT s_menuFont( UINT dpi )
{
   NONCLIENTMETRICSW metrics;

   if( s_menuFontHandle != NULL && s_menuFontDpi == dpi )
      return s_menuFontHandle;

   memset( &metrics, 0, sizeof( metrics ) );
   metrics.cbSize = sizeof( metrics );
   if( !SystemParametersInfoForDpi( SPI_GETNONCLIENTMETRICS, sizeof( metrics ), &metrics, 0, dpi ) )
      return ( HFONT )GetStockObject( DEFAULT_GUI_FONT );

   if( s_menuFontHandle != NULL )
      DeleteObject( s_menuFontHandle );

   s_menuFontHandle = CreateFontIndirectW( &metrics.lfMenuFont );
   s_menuFontDpi = dpi;

   return s_menuFontHandle;
}

/* The last item of the menu gets its picture: in the extended style the item is drawn by the program, otherwise
   Windows shows the picture beside the text */
static void s_menuDecorate( X_CALL_CTX *ctx, HMENU menu, const wchar_t *text, const X_ITEM *file, int separator,
                            const char *operation )
{
   HBITMAP picture = NULL;
   X_GUI_MENUITEM *item;
   MENUITEMINFOW info;
   BITMAP size;
   const wchar_t *tab;

   if( file != NULL && !X_IS_NIL( file ) )
   {
      picture = x_guiPicture( ctx, file, operation );
      if( picture == NULL )
         return;
   }

   memset( &info, 0, sizeof( info ) );
   info.cbSize = sizeof( info );
   if( !s_menuExtended )
   {
      if( picture != NULL )
      {
         info.fMask = MIIM_BITMAP;
         info.hbmpItem = picture;
         SetMenuItemInfoW( menu, ( UINT )GetMenuItemCount( menu ) - 1, TRUE, &info );
      }
      return;
   }

   item = ( X_GUI_MENUITEM * )x_xgrab( sizeof( X_GUI_MENUITEM ) );
   memset( item, 0, sizeof( *item ) );
   item->bar = !s_isPopupMenu( menu );
   item->separator = separator;
   if( text != NULL )
   {
      tab = wcschr( text, L'\t' );
      item->text = ( wchar_t * )x_xgrabArray( wcslen( text ) + 1, sizeof( wchar_t ) );
      wcscpy( item->text, text );
      if( tab != NULL )
      {
         item->text[tab - text] = L'\0';
         item->shortcut = item->text + ( tab - text ) + 1;
      }
   }

   if( picture != NULL )
   {
      GetObjectW( picture, sizeof( size ), &size );
      item->image = ImageList_Create( size.bmWidth, size.bmHeight, ILC_COLOR32 | ILC_MASK, 1, 1 );
      if( item->image != NULL )
      {
         if( x_guiHasAlpha( &size ) )
            ImageList_Add( item->image, picture, NULL );
         else
            ImageList_AddMasked( item->image, picture, x_guiCornerColor( picture ) );
      }

      DeleteObject( picture );
   }

   info.fMask = MIIM_FTYPE | MIIM_DATA;
   info.fType = MFT_OWNERDRAW | IIF( separator, MFT_SEPARATOR, 0 );
   info.dwItemData = ( ULONG_PTR )item;
   SetMenuItemInfoW( menu, ( UINT )GetMenuItemCount( menu ) - 1, TRUE, &info );
}

/* Width of the picture strip of drop-down menus */
static int s_menuStrip( UINT dpi )
{
   return x_guiScale( s_menuImageSize + 10, dpi );
}

/* WM_MEASUREITEM: the size of a menu item drawn in the extended style */
static void s_menuMeasure( HWND window, MEASUREITEMSTRUCT *measure )
{
   const X_GUI_MENUITEM *item = ( const X_GUI_MENUITEM * )measure->itemData;
   UINT dpi = GetDpiForWindow( window );
   HDC dc;
   HGDIOBJ old;
   SIZE text = { 0, 0 }, keys = { 0, 0 };
   int shortcut, height, image;

   if( item->separator )
   {
      measure->itemWidth = ( UINT )s_menuStrip( dpi );
      measure->itemHeight = ( UINT )x_guiScale( 5, dpi );
      return;
   }

   dc = GetDC( window );
   old = SelectObject( dc, s_menuFont( dpi ) );
   GetTextExtentPoint32W( dc, item->text, ( int )wcslen( item->text ), &text );
   if( item->shortcut != NULL )
      GetTextExtentPoint32W( dc, item->shortcut, ( int )wcslen( item->shortcut ), &keys );

   SelectObject( dc, old );
   ReleaseDC( window, dc );
   if( item->bar )
   {
      measure->itemWidth = ( UINT )( text.cx + x_guiScale( 8, dpi ) );
      measure->itemHeight = ( UINT )( text.cy + x_guiScale( 4, dpi ) );
      return;
   }

   shortcut = IIF( keys.cx > 0, x_guiScale( 32, dpi ) + keys.cx, 0 );
   height = text.cy + x_guiScale( 8, dpi );
   image = x_guiScale( s_menuImageSize + 6, dpi );
   measure->itemWidth = ( UINT )( s_menuStrip( dpi ) + x_guiScale( 8, dpi ) + text.cx + shortcut + x_guiScale( 16, dpi ) );
   measure->itemHeight = ( UINT )IIF( image > height, image, height );
}

/* A gradient from the first color to the second, across ( lVertical .F. ) or down the rectangle */
static void s_menuGradient( HDC dc, const RECT *area, COLORREF first, COLORREF second, int vertical )
{
   int size = IIF( vertical, area->bottom - area->top, area->right - area->left );
   int steps = IIF( size > 1, size - 1, 1 );
   int i;
   RECT line;

   for( i = 0; i < size; ++i )
   {
      int red = GetRValue( first ) + ( GetRValue( second ) - GetRValue( first ) ) * i / steps;
      int green = GetGValue( first ) + ( GetGValue( second ) - GetGValue( first ) ) * i / steps;
      int blue = GetBValue( first ) + ( GetBValue( second ) - GetBValue( first ) ) * i / steps;

      SetDCBrushColor( dc, RGB( red, green, blue ) );
      if( vertical )
         SetRect( &line, area->left, area->top + i, area->right, area->top + i + 1 );
      else
         SetRect( &line, area->left + i, area->top, area->left + i + 1, area->bottom );

      FillRect( dc, &line, ( HBRUSH )GetStockObject( DC_BRUSH ) );
   }
}

/* WM_DRAWITEM: draw a menu item in the extended style */
static void s_menuDraw( HWND window, const DRAWITEMSTRUCT *draw )
{
   const X_GUI_MENUITEM *item = ( const X_GUI_MENUITEM * )draw->itemData;
   UINT dpi = GetDpiForWindow( window );
   HDC dc = draw->hDC;
   RECT area = draw->rcItem;
   RECT part;
   int strip = s_menuStrip( dpi );
   int grayed = ( draw->itemState & ( ODS_GRAYED | ODS_DISABLED ) ) != 0;
   int selected = !item->separator && !grayed && ( draw->itemState & ( ODS_SELECTED | ODS_HOTLIGHT ) ) != 0;
   UINT prefix = IIF( draw->itemState & ODS_NOACCEL, DT_HIDEPREFIX, 0 );
   HGDIOBJ old;
   int width, height;

   SetBkMode( dc, TRANSPARENT );
   old = SelectObject( dc, s_menuFont( dpi ) );
   if( item->bar )
   {
      if( selected )
      {
         SetDCBrushColor( dc, RGB( 182, 189, 210 ) );
         FillRect( dc, &area, ( HBRUSH )GetStockObject( DC_BRUSH ) );
         SetDCBrushColor( dc, RGB( 10, 36, 106 ) );
         FrameRect( dc, &area, ( HBRUSH )GetStockObject( DC_BRUSH ) );
      }
      else
         s_menuGradient( dc, &area, RGB( 252, 251, 248 ), RGB( 229, 226, 215 ), 1 );

      SetTextColor( dc, GetSysColor( IIF( grayed, COLOR_GRAYTEXT, COLOR_MENUTEXT ) ) );
      DrawTextW( dc, item->text, -1, &area, DT_SINGLELINE | DT_CENTER | DT_VCENTER | prefix );
      SelectObject( dc, old );
      return;
   }

   part = area;
   part.right = part.left + strip;
   s_menuGradient( dc, &part, RGB( 255, 255, 255 ), RGB( 215, 212, 203 ), 0 );
   part = area;
   part.left += strip;
   SetDCBrushColor( dc, RGB( 252, 252, 249 ) );
   FillRect( dc, &part, ( HBRUSH )GetStockObject( DC_BRUSH ) );
   if( item->separator )
   {
      int middle = ( area.top + area.bottom ) / 2;

      SetRect( &part, area.left + strip + x_guiScale( 8, dpi ), middle, area.right, middle + 1 );
      SetDCBrushColor( dc, RGB( 197, 194, 184 ) );
      FillRect( dc, &part, ( HBRUSH )GetStockObject( DC_BRUSH ) );
      SelectObject( dc, old );
      return;
   }

   if( selected )
   {
      part = area;
      InflateRect( &part, -x_guiScale( 1, dpi ), 0 );
      SetDCBrushColor( dc, RGB( 182, 189, 210 ) );
      FillRect( dc, &part, ( HBRUSH )GetStockObject( DC_BRUSH ) );
      SetDCBrushColor( dc, RGB( 10, 36, 106 ) );
      FrameRect( dc, &part, ( HBRUSH )GetStockObject( DC_BRUSH ) );
   }

   if( item->image != NULL && ImageList_GetIconSize( item->image, &width, &height ) )
   {
      int x = area.left + ( strip - width ) / 2;
      int y = area.top + ( area.bottom - area.top - height ) / 2;
      UINT style = ILD_TRANSPARENT | IIF( grayed, ILD_BLEND50, 0 );

      ImageList_DrawEx( item->image, 0, dc, x, y, 0, 0, CLR_NONE, CLR_NONE, style );
   }

   SetTextColor( dc, GetSysColor( IIF( grayed, COLOR_GRAYTEXT, COLOR_MENUTEXT ) ) );
   part = area;
   part.left += strip + x_guiScale( 8, dpi );
   part.right -= x_guiScale( 20, dpi );
   DrawTextW( dc, item->text, -1, &part, DT_SINGLELINE | DT_LEFT | DT_VCENTER | prefix );
   if( item->shortcut != NULL )
      DrawTextW( dc, item->shortcut, -1, &part, DT_SINGLELINE | DT_RIGHT | DT_VCENTER | DT_NOPREFIX );

   SelectObject( dc, old );
}

/* The access key "&" of a menu item drawn by the program */
static LRESULT s_menuChar( HMENU menu, wchar_t key )
{
   int count = GetMenuItemCount( menu );
   int i;
   MENUITEMINFOW info;
   const wchar_t *text;
   const wchar_t *mark;

   for( i = 0; i < count; ++i )
   {
      memset( &info, 0, sizeof( info ) );
      info.cbSize = sizeof( info );
      info.fMask = MIIM_FTYPE | MIIM_DATA;
      if( !GetMenuItemInfoW( menu, ( UINT )i, TRUE, &info ) || !( info.fType & MFT_OWNERDRAW ) )
         continue;

      text = IIF( info.dwItemData != 0, ( ( const X_GUI_MENUITEM * )info.dwItemData )->text, NULL );
      if( text == NULL )
         continue;

      for( mark = wcschr( text, L'&' ); mark != NULL && mark[1] != L'\0'; mark = wcschr( mark + 2, L'&' ) )
      {
         if( mark[1] != L'&' && towupper( mark[1] ) == towupper( key ) )
            return MAKELRESULT( i, MNC_EXECUTE );
      }
   }

   return MAKELRESULT( 0, MNC_IGNORE );
}

/* Set the context menu of a window or a control: a drop-down gui_Menu, NIL removes it */
void x_guiSetContextMenu( X_CALL_CTX *ctx, size_t slot, const char *operation )
{
   const X_ITEM *value = x_paritem( ctx, 1 );
   X_ITEM *data = x_objDataSlot( ctx, slot );
   HMENU menu;

   if( data == NULL )
      return;

   if( value == NULL || X_IS_NIL( value ) )
      x_itemClear( data );
   else
   {
      menu = x_guiObjectMenu( ctx, value, operation );
      if( menu == NULL )
         return;

      if( !s_isPopupMenu( menu ) )
      {
         x_guiRaise( ctx, operation );
         return;
      }

      x_itemCopy( data, value );
   }

   if( value != NULL )
      x_itemReturn( ctx, value );
}

/* A shortcut such as "Ctrl+S" as a virtual key and modifiers, and its text for menus; 0 when it is not valid */
static int s_parseShortcut( const char *text, size_t length, UINT *key, UINT *modifiers, char *display, size_t size )
{
   char token[16];
   char keyName[16] = "";
   size_t start = 0, i, j, n, k;
   const char *ctrl, *shift, *alt;
   int function;

   *key = 0;
   *modifiers = 0;
   for( i = 0; i <= length; ++i )
   {
      if( i < length && text[i] != '+' )
         continue;

      n = i - start;
      if( n == 0 || n >= sizeof( token ) || *key != 0 )
         return 0;

      for( j = 0; j < n; ++j )
         token[j] = ( char )toupper( ( unsigned char )text[start + j] );

      token[n] = '\0';
      start = i + 1;
      function = token[0] == 'F' && n <= 3 && isdigit( ( unsigned char )token[1] );
      function = function && ( n == 2 || isdigit( ( unsigned char )token[2] ) );
      function = function && atoi( token + 1 ) >= 1 && atoi( token + 1 ) <= 24;
      if( strcmp( token, "CTRL" ) == 0 || strcmp( token, "CONTROL" ) == 0 )
         *modifiers |= X_GUI_CTRL;
      else if( strcmp( token, "SHIFT" ) == 0 )
         *modifiers |= X_GUI_SHIFT;
      else if( strcmp( token, "ALT" ) == 0 )
         *modifiers |= X_GUI_ALT;
      else if( n == 1 && isalnum( ( unsigned char )token[0] ) )
      {
         *key = ( UINT )token[0];
         snprintf( keyName, sizeof( keyName ), "%c", token[0] );
      }
      else if( function )
      {
         *key = VK_F1 + ( UINT )atoi( token + 1 ) - 1;
         snprintf( keyName, sizeof( keyName ), "F%d", atoi( token + 1 ) );
      }
      else
      {
         for( k = 0; k < sizeof( s_keyNames ) / sizeof( s_keyNames[0] ) && *key == 0; ++k )
         {
            if( strcmp( token, s_keyNames[k].name ) == 0 )
            {
               *key = s_keyNames[k].key;
               snprintf( keyName, sizeof( keyName ), "%s", s_keyNames[k].display );
            }
         }

         if( *key == 0 )
            return 0;
      }
   }

   if( *key == 0 )
      return 0;

   ctrl = IIF( *modifiers & X_GUI_CTRL, "Ctrl+", "" );
   shift = IIF( *modifiers & X_GUI_SHIFT, "Shift+", "" );
   alt = IIF( *modifiers & X_GUI_ALT, "Alt+", "" );
   snprintf( display, size, "%s%s%s%s", ctrl, shift, alt, keyName );

   return 1;
}

/* oWindow:SetKey( cKey, [bAction] ): the key as in gui_Menu, NIL removes it; returns the previous block */
void x_guiWindowSetKey( X_CALL_CTX *ctx, HWND window, X_ITEM *blocks )
{
   X_GUI_KEYS *keys = ( X_GUI_KEYS * )GetPropW( window, s_keysProperty );
   const X_ITEM *action = x_paritem( ctx, 2 );
   const int remove = action == NULL || X_IS_NIL( action );
   UINT key, modifiers;
   char display[48];
   size_t i;
   int valid;

   valid = blocks != NULL && X_ISCHAR( ctx, 1 ) && ( remove || X_IS_BLOCK( action ) );
   if( valid )
      valid = s_parseShortcut( x_parc( ctx, 1 ), x_parclen( ctx, 1 ), &key, &modifiers, display, sizeof( display ) );

   if( !valid )
   {
      x_guiRaise( ctx, "GUI_WINDOW:SETKEY" );
      return;
   }

   if( !X_IS_ARRAY( blocks ) )
      x_arrayNew( blocks, 0 );

   if( keys == NULL )
   {
      keys = ( X_GUI_KEYS * )x_xgrab( sizeof( X_GUI_KEYS ) );
      memset( keys, 0, sizeof( *keys ) );
      SetPropW( window, s_keysProperty, keys );
   }

   for( i = 0; i < keys->count && ( keys->keys[i] != key || keys->modifiers[i] != modifiers ); ++i )
      ;

   if( i < keys->count )
   {
      x_arrayGet( blocks, i + 1, ctx->result );
      if( remove )
      {
         x_arrayDel( blocks, i + 1 );
         x_arraySize( blocks, keys->count - 1 );
         memmove( keys->keys + i, keys->keys + i + 1, ( keys->count - i - 1 ) * sizeof( UINT ) );
         memmove( keys->modifiers + i, keys->modifiers + i + 1, ( keys->count - i - 1 ) * sizeof( UINT ) );
         --keys->count;
      }
      else
         x_itemCopy( x_arrayGetItemPtr( blocks, i + 1 ), action );
   }
   else if( !remove )
   {
      keys->keys = ( UINT * )x_xrealloc( keys->keys, ( keys->count + 1 ) * sizeof( UINT ) );
      keys->modifiers = ( UINT * )x_xrealloc( keys->modifiers, ( keys->count + 1 ) * sizeof( UINT ) );
      keys->keys[keys->count] = key;
      keys->modifiers[keys->count] = modifiers;
      ++keys->count;
      x_arrayAdd( blocks, action );
   }
}

/* oWindow:<cName>: the first control among all children of the window with this :name, upper and lower case alike */
typedef struct
{
   X_CALL_CTX *ctx;
   const char *name;
   int found;
} X_GUI_FIND;

/* Look for the control with the name among the children of the window */
static BOOL CALLBACK s_findControl( HWND child, LPARAM data )
{
   X_GUI_FIND *find = ( X_GUI_FIND * )data;
   X_ITEM *object = ( X_ITEM * )GetWindowLongPtrW( child, GWLP_USERDATA );
   X_ITEM name __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;

   if( object == NULL || ( ATOM )GetClassLongPtrW( child, GCW_ATOM ) == s_windowClass )
      return TRUE;

   if( !x_objIsKindOf( object, "GUI_CONTROL" ) || !x_objSend( find->ctx, &name, object, "NAME", 0, NULL ) )
      return TRUE;

   if( !X_IS_STRING( &name ) || x_stricmp( x_itemGetCPtr( &name ), find->name ) != 0 )
      return TRUE;

   x_itemCopy( find->ctx->result, object );
   find->found = 1;

   return FALSE;
}

/* oWindow:<cName>: return the control with the name; 0 when there is none */
int x_guiWindowControl( X_CALL_CTX *ctx, HWND window, const char *name )
{
   X_GUI_FIND find = { ctx, name, 0 };

   if( name != NULL && *name != '\0' )
      EnumChildWindows( window, s_findControl, ( LPARAM )&find );

   return find.found;
}

/* The focus goes to the next or the previous control of the window in the order of Tab */
void x_guiWindowFocusNext( HWND window, int previous )
{
   HWND focus = GetFocus();
   HWND next;

   x_callbackEnterDll();
   next = GetNextDlgTabItem( window, IIF( focus != NULL && IsChild( window, focus ), focus, NULL ), previous );
   if( next != NULL )
      SetFocus( next );

   x_callbackLeaveDll();
   x_callbackRaisePending();
}

/* The text of a command with its shortcut after a tab, as UTF-16; NULL with an error raised */
static wchar_t *s_menuText( X_CALL_CTX *ctx, const X_ITEM *text, const X_ITEM *shortcut, UINT *key, UINT *modifiers )
{
   void *handle = NULL;
   size_t length = 0, count, extra, i;
   const char *spec;
   char display[64];
   wchar_t *wide, *result;
   int valid;

   *key = 0;
   *modifiers = 0;
   if( shortcut == NULL || X_IS_NIL( shortcut ) )
      return x_guiText( ctx, text, "GUI_MENU:ADD" );

   spec = IIF( X_IS_STRING( shortcut ), x_itemGetStrUTF8( shortcut, &handle, &length ), NULL );
   valid = spec != NULL && s_parseShortcut( spec, length, key, modifiers, display, sizeof( display ) );
   x_strfree( handle );
   if( !valid )
   {
      x_guiRaise( ctx, "GUI_MENU:ADD" );
      return NULL;
   }

   wide = x_guiText( ctx, text, "GUI_MENU:ADD" );
   if( wide == NULL )
      return NULL;

   count = wcslen( wide );
   extra = strlen( display );
   result = ( wchar_t * )x_xgrabArray( count + extra + 2, sizeof( wchar_t ) );
   memcpy( result, wide, count * sizeof( wchar_t ) );
   result[count] = L'\t';
   for( i = 0; i < extra; ++i )
      result[count + 1 + i] = ( wchar_t )display[i];

   result[count + 1 + extra] = L'\0';
   x_xfree( wide );

   return result;
}

/* Add a command to a menu; returns its identifier, 0 with an error raised */
int x_guiMenuAdd( X_CALL_CTX *ctx, HMENU menu, const X_ITEM *text, const X_ITEM *action, const X_ITEM *shortcut,
                  const X_ITEM *image, const X_ITEM *when )
{
   const X_ITEM nil = X_ITEM_NIL_INIT;
   X_ITEM *onAction, *onWhen;
   wchar_t *wide;
   BOOL done;
   UINT key, modifiers;
   int valid;

   valid = action == NULL || X_IS_NIL( action ) || X_IS_BLOCK( action );
   valid = valid && ( when == NULL || X_IS_NIL( when ) || X_IS_BLOCK( when ) );
   if( !valid )
   {
      x_guiRaise( ctx, "GUI_MENU:ADD" );
      return 0;
   }

   if( s_menuId > X_GUI_MENU_LAST )
   {
      x_guiRaise( ctx, "GUI_MENU:ADD" );
      return 0;
   }

   wide = s_menuText( ctx, text, shortcut, &key, &modifiers );
   if( wide == NULL )
      return 0;

   done = AppendMenuW( menu, MF_STRING, ( UINT_PTR )s_menuId, wide );
   if( !done )
   {
      x_xfree( wide );
      x_guiRaiseWin32( ctx, "AppendMenuW", GetLastError() );
      return 0;
   }

   s_menuDecorate( ctx, menu, wide, image, 0, "GUI_MENU:ADD" );
   x_xfree( wide );
   if( s_actionCount == s_actionCapacity )
   {
      s_actionCapacity = IIF( s_actionCapacity == 0, ( size_t )16, s_actionCapacity * 2 );
      s_actions = ( X_GUI_ACTION * )x_xrealloc( s_actions, s_actionCapacity * sizeof( X_GUI_ACTION ) );
   }

   onAction = x_itemNew( IIF( action != NULL, action, &nil ) );
   onWhen = x_itemNew( IIF( when != NULL, when, &nil ) );
   s_actions[s_actionCount++] = ( X_GUI_ACTION ){ s_menuId, key, modifiers, onAction, onWhen };

   return s_menuId++;
}

/* Add a drop-down menu to a menu */
void x_guiMenuAddMenu( X_CALL_CTX *ctx, HMENU menu, const X_ITEM *text, HMENU submenu, const X_ITEM *image )
{
   wchar_t *wide = x_guiText( ctx, text, "GUI_MENU:ADDMENU" );

   if( wide == NULL )
      return;

   if( AppendMenuW( menu, MF_POPUP, ( UINT_PTR )submenu, wide ) )
      s_menuDecorate( ctx, menu, wide, image, 0, "GUI_MENU:ADDMENU" );
   else
      x_guiRaiseWin32( ctx, "AppendMenuW", GetLastError() );

   x_xfree( wide );
}

/* Add a new drop-down gui_Menu to a menu and return it */
void x_guiMenuAddSubmenu( X_CALL_CTX *ctx, HMENU menu, const X_ITEM *text, const X_ITEM *image )
{
   X_CLASS_HANDLE handle = x_clsFindClass( "GUI_MENU", NULL );
   X_ITEM submenu __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   HMENU popup;

   if( handle == 0 || !x_objCreate( ctx, &submenu, handle ) )
      return;

   if( !x_objSend( ctx, &result, &submenu, "__GUIPOPUP", 0, NULL ) )
      return;

   popup = x_guiObjectMenu( ctx, &submenu, "GUI_MENU:ADDMENU" );
   if( popup == NULL )
      return;

   x_guiMenuAddMenu( ctx, menu, text, popup, image );
   x_itemCopy( ctx->result, &submenu );
}

/* Add a separator line to a menu */
void x_guiMenuSeparator( X_CALL_CTX *ctx, HMENU menu )
{
   if( !AppendMenuW( menu, MF_SEPARATOR, 0, NULL ) )
      x_guiRaiseWin32( ctx, "AppendMenuW", GetLastError() );
   else if( s_isPopupMenu( menu ) )
      s_menuDecorate( ctx, menu, NULL, NULL, 1, "GUI_MENU:ADDSEPARATOR" );
}

/* ---------------------------------------------------------------------------------------------
   Edit fields with a picture
   --------------------------------------------------------------------------------------------- */

/* Posted to an edit field whose OnValid refused the value, so it takes the focus back */
#define X_GUI_REFOCUS ( WM_APP + 0x100 )

/* The picture data of an edit field, or NULL */
static X_GUI_MASK *s_maskOf( HWND edit )
{
   return ( X_GUI_MASK * )GetPropW( edit, s_maskProperty );
}

/* Release the picture data of an edit field */
static void s_maskRelease( HWND edit )
{
   X_GUI_MASK *mask = ( X_GUI_MASK * )RemovePropW( edit, s_maskProperty );

   if( mask == NULL )
      return;

   x_xfree( mask->picture );
   x_xfree( mask->mask );
   x_xfree( mask );
}

/* The template characters that take a character; the others are shown as they are */
static int s_maskSlot( const X_GUI_MASK *mask, size_t pos )
{
   return mask->mask != NULL && pos < mask->length && wcschr( L"9#ANXLY!", towupper( mask->mask[pos] ) ) != NULL;
}

/* The first template position from pos on that takes a character */
static size_t s_maskNext( const X_GUI_MASK *mask, size_t pos )
{
   while( pos < mask->length && !s_maskSlot( mask, pos ) )
      ++pos;

   return pos;
}

/* c as the template character slot takes it, 0 when it does not take it */
static wchar_t s_maskAccept( wchar_t c, wchar_t slot, int upper )
{
   wchar_t big = ( wchar_t )( UINT_PTR )CharUpperW( ( LPWSTR )( UINT_PTR )c );

   switch( towupper( slot ) )
   {
   case L'9':
      return IIF( iswdigit( c ), c, 0 );
   case L'#':
      return IIF( iswdigit( c ) || c == L' ' || c == L'+' || c == L'-', c, 0 );
   case L'A':
      if( !IsCharAlphaW( c ) )
         return 0;

      return IIF( upper, big, c );
   case L'N':
      if( !IsCharAlphaNumericW( c ) )
         return 0;

      return IIF( upper, big, c );
   case L'L':
      if( wcschr( L"TFYN", big ) == NULL )
         return 0;

      return IIF( big == L'T' || big == L'Y', L'T', L'F' );
   case L'Y':
      if( wcschr( L"TFYN", big ) == NULL )
         return 0;

      return IIF( big == L'T' || big == L'Y', L'Y', L'N' );
   case L'!':
      return big;
   }

   return IIF( upper, big, c );
}

/* The text of an edit field as UTF-16 */
static wchar_t *s_editText( HWND edit, size_t *length )
{
   int size = GetWindowTextLengthW( edit );
   wchar_t *text = ( wchar_t * )x_xgrabArray( ( size_t )size + 1, sizeof( wchar_t ) );

   *length = ( size_t )GetWindowTextW( edit, text, size + 1 );

   return text;
}

/* Exactly one character for every template position: empty places are spaces, literals their own */
static wchar_t *s_maskFit( const X_GUI_MASK *mask, const wchar_t *text, size_t length )
{
   wchar_t *buffer = ( wchar_t * )x_xgrabArray( mask->length + 1, sizeof( wchar_t ) );
   size_t i;

   for( i = 0; i < mask->length; ++i )
   {
      if( i < length )
         buffer[i] = text[i];
      else
         buffer[i] = IIF( s_maskSlot( mask, i ), L' ', mask->mask[i] );
   }

   buffer[mask->length] = L'\0';

   return buffer;
}

/* The text of an edit field fitted to its template */
static wchar_t *s_maskBuffer( HWND edit, const X_GUI_MASK *mask )
{
   size_t length;
   wchar_t *text = s_editText( edit, &length );
   wchar_t *buffer = s_maskFit( mask, text, length );

   x_xfree( text );

   return buffer;
}

/* Set the text of an edit field and put the caret at a position */
static void s_maskPut( HWND edit, const wchar_t *text, size_t caret )
{
   SetWindowTextW( edit, text );
   SendMessageW( edit, EM_SETSEL, caret, caret );
}

/* Set UTF-8 text in an edit field, fitted to the template when there is one */
static void s_editSetUtf8( HWND edit, const X_GUI_MASK *mask, const char *text, size_t length )
{
   size_t size;
   wchar_t *wide = ( wchar_t * )x_utf8ToUtf16Alloc( text, length, &size );
   wchar_t *fit;

   if( mask != NULL && mask->mask != NULL )
   {
      fit = s_maskFit( mask, wide, size );
      x_xfree( wide );
      wide = fit;
   }

   SetWindowTextW( edit, wide );
   x_xfree( wide );
}

/* A key typed in a field with a template goes to the next position that takes it */
static void s_maskType( HWND edit, X_GUI_MASK *mask, wchar_t c )
{
   DWORD start = 0, end = 0;
   wchar_t *buffer = s_maskBuffer( edit, mask );
   wchar_t accepted;
   size_t pos, i;

   SendMessageW( edit, EM_GETSEL, ( WPARAM )&start, ( LPARAM )&end );

   /* @K: the first key replaces the whole value; a selection is replaced by the key */
   if( mask->clear && !mask->typed )
   {
      start = 0;
      end = ( DWORD )mask->length;
   }

   for( i = start; i < end && i < mask->length; ++i )
   {
      if( s_maskSlot( mask, i ) )
         buffer[i] = L' ';
   }

   mask->typed = 1;
   pos = s_maskNext( mask, start );
   accepted = 0;
   if( pos < mask->length )
      accepted = s_maskAccept( c, mask->mask[pos], mask->upper );

   if( accepted == 0 )
      MessageBeep( MB_OK );
   else
   {
      buffer[pos] = accepted;
      s_maskPut( edit, buffer, s_maskNext( mask, pos + 1 ) );
   }

   x_xfree( buffer );
}

/* A deleted character pulls the rest of its group of template characters to the left */
static void s_maskDelete( HWND edit, const X_GUI_MASK *mask, size_t pos )
{
   wchar_t *buffer = s_maskBuffer( edit, mask );
   size_t end = pos;

   if( s_maskSlot( mask, pos ) )
   {
      while( end + 1 < mask->length && s_maskSlot( mask, end + 1 ) )
      {
         buffer[end] = buffer[end + 1];
         ++end;
      }

      buffer[end] = L' ';
   }

   s_maskPut( edit, buffer, pos );
   x_xfree( buffer );
}

static void s_maskClearSelection( HWND edit, const X_GUI_MASK *mask );

/* Backspace in a field with a template deletes the character before the caret */
static void s_maskBackSpace( HWND edit, const X_GUI_MASK *mask )
{
   DWORD start = 0, end = 0;
   size_t pos;

   SendMessageW( edit, EM_GETSEL, ( WPARAM )&start, ( LPARAM )&end );
   if( end > start )
   {
      s_maskClearSelection( edit, mask );
      return;
   }

   pos = start;
   while( pos > 0 && !s_maskSlot( mask, pos - 1 ) )
      --pos;

   if( pos == 0 )
      MessageBeep( MB_OK );
   else
      s_maskDelete( edit, mask, pos - 1 );
}

/* Delete the selected characters of a field with a template; the literals stay */
static void s_maskClearSelection( HWND edit, const X_GUI_MASK *mask )
{
   DWORD start = 0, end = 0;
   wchar_t *buffer;
   size_t i;

   SendMessageW( edit, EM_GETSEL, ( WPARAM )&start, ( LPARAM )&end );
   buffer = s_maskBuffer( edit, mask );
   for( i = start; i < end && i < mask->length; ++i )
   {
      if( s_maskSlot( mask, i ) )
         buffer[i] = L' ';
   }

   s_maskPut( edit, buffer, start );
   x_xfree( buffer );
}

/* A number has at most one point, the minus only in front and the digits allowed by the picture;
   the minus takes the place of one digit, as in Transform() */
static int s_numberFits( const X_GUI_MASK *mask, const wchar_t *text )
{
   int points = 0, whole = 0, fraction = 0;
   size_t i;

   for( i = 0; text[i] != L'\0'; ++i )
   {
      if( text[i] == L'-' && i == 0 )
      {
         ++whole;
         continue;
      }

      if( text[i] == L'.' )
         ++points;
      else if( !iswdigit( text[i] ) )
         return 0;
      else if( points == 0 )
         ++whole;
      else
         ++fraction;
   }

   if( points > 1 || ( mask->decimals == 0 && points != 0 ) )
      return 0;

   if( mask->digits != 0 && whole > mask->digits )
      return 0;

   return mask->decimals < 0 || fraction <= mask->decimals;
}

/* A key typed in a numeric field; a key that would make a wrong number is refused */
static void s_numberType( HWND edit, const X_GUI_MASK *mask, wchar_t c )
{
   DWORD start = 0, end = 0;
   size_t length, caret;
   wchar_t *text = s_editText( edit, &length );
   wchar_t *candidate = ( wchar_t * )x_xgrabArray( length + 2, sizeof( wchar_t ) );

   SendMessageW( edit, EM_GETSEL, ( WPARAM )&start, ( LPARAM )&end );
   start = IIF( start > length, ( DWORD )length, start );
   end = IIF( end > length || end < start, start, end );
   if( c == L',' )
      c = L'.';

   /* The minus over the whole selected number starts a new negative number */
   if( c == L'-' && start == 0 && end == length && length > 0 )
   {
      candidate[0] = L'-';
      candidate[1] = L'\0';
      caret = 1;
   }
   else if( c == L'-' )
   {
      if( length > 0 && text[0] == L'-' )
      {
         memcpy( candidate, text + 1, length * sizeof( wchar_t ) );
         caret = IIF( start > 0, start - 1, 0 );
      }
      else
      {
         candidate[0] = L'-';
         memcpy( candidate + 1, text, ( length + 1 ) * sizeof( wchar_t ) );
         caret = start + 1;
      }
   }
   else
   {
      memcpy( candidate, text, start * sizeof( wchar_t ) );
      candidate[start] = c;
      memcpy( candidate + start + 1, text + end, ( length - end + 1 ) * sizeof( wchar_t ) );
      caret = start + 1;
   }

   if( ( c != L'-' && c != L'.' && !iswdigit( c ) ) || !s_numberFits( mask, candidate ) )
      MessageBeep( MB_OK );
   else
      s_maskPut( edit, candidate, caret );

   x_xfree( candidate );
   x_xfree( text );
}

/* While editing the plain number, otherwise the number through the picture */
static void s_numberShow( HWND edit, const X_GUI_MASK *mask, int focused )
{
   X_ITEM value __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM shown __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   char buffer[64];
   const char *text = buffer;

   if( ( focused || mask->picture[0] == '\0' ) && mask->decimals >= 0 )
      snprintf( buffer, sizeof( buffer ), "%.*f", mask->decimals, mask->number );
   else if( focused || mask->picture[0] == '\0' )
      snprintf( buffer, sizeof( buffer ), "%.15g", mask->number );
   else
   {
      x_itemPutND( &value, mask->number );
      x_transform( &shown, &value, mask->picture, strlen( mask->picture ) );
      for( text = x_itemGetCPtr( &shown ); *text == ' '; ++text )
         ;
   }

   s_editSetUtf8( edit, NULL, text, strlen( text ) );
   if( focused )
      SendMessageW( edit, EM_SETSEL, 0, -1 );
}

/* Read the number of a numeric field; a comma counts as the point */
static void s_numberRead( HWND edit, X_GUI_MASK *mask )
{
   size_t length, i;
   wchar_t *text = s_editText( edit, &length );

   for( i = 0; i < length; ++i )
   {
      if( text[i] == L',' )
         text[i] = L'.';
   }

   mask->number = wcstod( text, NULL );
   x_xfree( text );
}

/* Show a date in the format of SET DATE */
static void s_dateShow( HWND edit, const X_GUI_MASK *mask, long julian )
{
   char date[9] = "        ";
   char text[32];
   int year, month, day;

   if( julian != 0 )
   {
      x_dateDecode( julian, &year, &month, &day );
      snprintf( date, sizeof( date ), "%04d%02d%02d", year, month, day );
   }

   x_dateFormat( date, text, x_setDateFormat( NULL ) );
   s_editSetUtf8( edit, mask, text, strlen( text ) );
}

/* 0 for an empty or a wrong date; *filled tells whether the user typed any digit */
static long s_dateRead( HWND edit, int *filled )
{
   size_t length, size, i;
   wchar_t *wide = s_editText( edit, &length );
   char *text = x_utf16ToUtf8Alloc( ( const uint16_t * )wide, length, &size );
   long julian;

   *filled = 0;
   for( i = 0; i < length; ++i )
      *filled |= iswdigit( wide[i] ) != 0;

   julian = x_dateUnformat( text, x_setDateFormat( NULL ) );
   x_xfree( text );
   x_xfree( wide );

   return julian;
}

/* Functions after @, the template, and for a number the digits before and after the point */
static void s_maskParse( X_GUI_MASK *mask, char type )
{
   const char *picture = mask->picture;
   const char *space = strchr( picture, ' ' );
   const char *template = picture;
   const char *p;
   char dateMask[16];
   size_t i;

   x_xfree( mask->mask );
   mask->mask = NULL;
   mask->length = 0;
   mask->type = type;
   mask->upper = 0;
   mask->clear = 0;
   mask->remove = 0;
   mask->digits = 0;
   mask->decimals = -1;
   if( picture[0] == '@' )
   {
      for( p = picture + 1; *p != '\0' && p != space; ++p )
      {
         switch( toupper( ( unsigned char )*p ) )
         {
         case '!':
            mask->upper = 1;
            break;
         case 'K':
            mask->clear = 1;
            break;
         case 'R':
            mask->remove = 1;
            break;
         }
      }

      template = IIF( space != NULL, space + 1, "" );
   }

   if( type == 'D' )
   {
      /* The template follows SET DATE: every y, m and d takes a digit */
      for( p = x_setDateFormat( NULL ), i = 0; *p != '\0' && i < sizeof( dateMask ) - 1; ++p, ++i )
         dateMask[i] = ( char )IIF( strchr( "YMDymd", *p ) != NULL, '9', *p );

      dateMask[i] = '\0';
      template = dateMask;
   }

   if( type == 'N' )
   {
      if( *template != '\0' )
      {
         mask->decimals = 0;
         for( p = template; *p != '\0' && *p != '.'; ++p )
            mask->digits += strchr( "9#$*", *p ) != NULL;

         for( ; *p != '\0'; ++p )
            mask->decimals += strchr( "9#", *p ) != NULL;
      }
   }
   else if( *template != '\0' )
      mask->mask = ( wchar_t * )x_utf8ToUtf16Alloc( template, strlen( template ), &mask->length );
}

/* A number stands on the right; @! without a template is left to the edit field itself */
static void s_maskStyle( HWND edit, const X_GUI_MASK *mask )
{
   LONG_PTR style = GetWindowLongPtrW( edit, GWL_STYLE ) & ~( LONG_PTR )( ES_RIGHT | ES_UPPERCASE );

   if( mask->type == 'N' )
      style |= ES_RIGHT;
   else if( mask->upper && mask->mask == NULL )
      style |= ES_UPPERCASE;

   SetWindowLongPtrW( edit, GWL_STYLE, style );
   SendMessageW( edit, EM_LIMITTEXT, IIF( mask->mask != NULL, mask->length, 0 ), 0 );
   InvalidateRect( edit, NULL, TRUE );
}

/* The picture data of an edit field for a value of the type, made when missing */
static X_GUI_MASK *s_maskEnsure( HWND edit, char type )
{
   X_GUI_MASK *mask = s_maskOf( edit );

   if( mask == NULL )
   {
      mask = ( X_GUI_MASK * )x_xgrab( sizeof( X_GUI_MASK ) );
      memset( mask, 0, sizeof( *mask ) );
      mask->picture = x_strdup( "" );
      SetPropW( edit, s_maskProperty, mask );
   }

   if( mask->type != type )
   {
      s_maskParse( mask, type );
      s_maskStyle( edit, mask );
   }

   return mask;
}

/* The value in the type of the field: text, number or date */
static void s_editCurrent( HWND edit, X_GUI_MASK *mask, X_ITEM *value )
{
   size_t length, size, i, j;
   wchar_t *text;
   int filled;

   if( mask != NULL && mask->type == 'N' )
   {
      if( GetFocus() == edit )
         s_numberRead( edit, mask );

      if( mask->decimals == 0 )
         x_itemPutNInt( value, ( int64_t )( mask->number + IIF( mask->number < 0, -0.5, 0.5 ) ) );
      else
         x_itemPutND( value, mask->number );
      return;
   }

   if( mask != NULL && mask->type == 'D' )
   {
      x_itemCopy( value, &( X_ITEM ){ X_IT_DATE, { .integer = s_dateRead( edit, &filled ) } } );
      return;
   }

   text = s_editText( edit, &length );
   /* @R: the literals of the template are not part of the value */
   if( mask != NULL && mask->mask != NULL && mask->remove )
   {
      for( i = j = 0; i < length; ++i )
      {
         if( s_maskSlot( mask, i ) )
            text[j++] = text[i];
      }

      length = j;
   }

   x_itemPutCLPtr( value, x_utf16ToUtf8Alloc( ( const uint16_t * )text, length, &size ), size );
   x_xfree( text );
}

/* Show a value in an edit field through its picture */
static void s_editShow( HWND edit, X_GUI_MASK *mask, const X_ITEM *value )
{
   X_ITEM shown __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;

   if( X_IS_NUMERIC( value ) )
   {
      mask->number = x_itemGetND( value );
      s_numberShow( edit, mask, GetFocus() == edit );
   }
   else if( X_IS_DATE( value ) )
      s_dateShow( edit, mask, ( long )value->u.integer );
   else if( mask->mask != NULL )
   {
      x_transform( &shown, value, mask->picture, strlen( mask->picture ) );
      s_editSetUtf8( edit, mask, x_itemGetCPtr( &shown ), x_itemGetCLen( &shown ) );
   }
   else
      s_editSetUtf8( edit, mask, x_itemGetCPtr( value ), x_itemGetCLen( value ) );
}

/* A field with a picture gets or loses the focus: a number is edited plain, a template starts at its first place */
static void s_maskFocus( HWND edit, X_GUI_MASK *mask, int focused )
{
   size_t first;

   if( focused )
      mask->typed = 0;

   if( mask->type == 'N' )
   {
      if( !focused )
         s_numberRead( edit, mask );

      s_numberShow( edit, mask, focused );
   }
   else if( focused && mask->mask != NULL )
   {
      first = s_maskNext( mask, 0 );
      SendMessageW( edit, EM_SETSEL, first, first );
   }
}

/* Paste into a field with a picture: the characters go in as typed keys */
static void s_maskPaste( HWND edit, X_GUI_MASK *mask )
{
   HANDLE data;
   const wchar_t *clip;
   wchar_t *text = NULL;
   size_t i;

   if( !OpenClipboard( edit ) )
      return;

   data = GetClipboardData( CF_UNICODETEXT );
   clip = IIF( data != NULL, ( const wchar_t * )GlobalLock( data ), NULL );
   if( clip != NULL )
   {
      text = ( wchar_t * )x_xgrabArray( wcslen( clip ) + 1, sizeof( wchar_t ) );
      wcscpy( text, clip );
      GlobalUnlock( data );
   }

   CloseClipboard();
   for( i = 0; text != NULL && text[i] != L'\0'; ++i )
   {
      if( text[i] >= L' ' )
      {
         if( mask->type == 'N' )
            s_numberType( edit, mask, text[i] );
         else
            s_maskType( edit, mask, text[i] );
      }
   }

   x_xfree( text );
}

/* A GET without PICTURE takes the clipboard as typed keys, so the text writes over the spaces of the
   variable instead of being refused at its length; line breaks and tabs become spaces, the ones at the end
   are dropped */
static void s_plainPaste( HWND edit )
{
   HANDLE data;
   const wchar_t *clip;
   wchar_t *text = NULL;
   size_t i, len;

   if( !OpenClipboard( edit ) )
      return;

   data = GetClipboardData( CF_UNICODETEXT );
   clip = IIF( data != NULL, ( const wchar_t * )GlobalLock( data ), NULL );
   if( clip != NULL )
   {
      text = ( wchar_t * )x_xgrabArray( wcslen( clip ) + 1, sizeof( wchar_t ) );
      wcscpy( text, clip );
      GlobalUnlock( data );
   }

   CloseClipboard();
   if( text == NULL )
      return;

   len = wcslen( text );
   while( len > 0 && wcschr( L"\r\n\t ", text[len - 1] ) != NULL )
      --len;

   for( i = 0; i < len; ++i )
   {
      wchar_t c = text[i];

      if( c == L'\n' && i > 0 && text[i - 1] == L'\r' )
         continue;

      if( c == L'\r' || c == L'\n' || c == L'\t' )
         c = L' ';
      else if( c < L' ' )
         continue;

      SendMessageW( edit, WM_CHAR, ( WPARAM )c, 0 );
   }

   x_xfree( text );
}

/* OnValid, and for a date its correctness, when the focus goes to another control of the same window;
   Cancel and other windows do not check */
static int s_editValid( HWND edit, HWND next, X_GUI_MASK *mask )
{
   X_ITEM *object = ( X_ITEM * )GetWindowLongPtrW( edit, GWLP_USERDATA );
   X_ITEM result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   int valid = 1;
   int filled;

   if( s_validating || next == NULL || GetAncestor( next, GA_ROOT ) != GetAncestor( edit, GA_ROOT ) )
      return 1;

   if( next == ( HWND )GetPropW( GetParent( edit ), s_cancelProperty ) )
      return 1;

   s_validating = 1;
   if( mask != NULL && mask->type == 'D' && s_dateRead( edit, &filled ) == 0 && filled )
      valid = 0;
   else if( object != NULL && x_guiNotify( object, "__GUIVALID", &result, 0, NULL ) )
      valid = !X_IS_LOGICAL( &result ) || result.u.logical;

   s_validating = 0;
   if( !valid )
   {
      MessageBeep( MB_ICONWARNING );
      PostMessageW( edit, X_GUI_REFOCUS, 0, 0 );
   }

   return valid;
}

static void s_statusSetText( HWND status, int index, int style, const wchar_t *text );

/* SCOREBOARD: during ReadModal() the panel oStatusBar:scoreboard of the window shows "Ins" while
   SET INSERT is on; after ReadModal() it is empty */
static void s_readScoreboard( HWND root, int reading )
{
   HWND status = IIF( root != NULL, FindWindowExW( root, NULL, STATUSCLASSNAMEW, NULL ), NULL );
   const X_GUI_STATUS *parts = IIF( status != NULL, s_statusOf( status ), NULL );
   size_t panels;
   int style;

   if( parts == NULL || parts->scoreboard < 1 )
      return;

   panels = IIF( parts->count > 0, parts->count, ( size_t )1 );
   if( ( size_t )parts->scoreboard > panels )
      return;

   style = IIF( parts->count > 0, parts->styles[parts->scoreboard - 1], 0 );
   s_statusSetText( status, parts->scoreboard - 1, style, IIF( reading && x_setInsert, L"Ins", L"" ) );
}

/* A GET of several rows shows one row; while it has the focus it grows over the fields under it to the rows
   its wrapped text needs, at most its number of rows, and shrinks back to one row when the focus leaves */
static void s_getFit( HWND edit, int focused )
{
   const int rows = ( int )( INT_PTR )GetPropW( edit, s_rowsProperty );
   UINT dpi;
   int lines, margin, edge, width, height;
   UINT flags;
   TEXTMETRICW metrics;
   RECT frame;
   HDC dc;
   HGDIOBJ old;

   if( rows <= 1 )
      return;

   dpi = GetDpiForWindow( edit );
   margin = x_guiScale( 2, dpi );
   edge = GetSystemMetricsForDpi( SM_CXEDGE, dpi );
   dc = GetDC( edit );
   old = SelectObject( dc, s_fontOf( edit, dpi ) );
   GetTextMetricsW( dc, &metrics );
   SelectObject( dc, old );
   ReleaseDC( edit, dc );
   lines = IIF( focused, ( int )SendMessageW( edit, EM_GETLINECOUNT, 0, 0 ), 1 );
   if( lines < 1 )
      lines = 1;
   else if( lines > rows )
      lines = rows;

   GetWindowRect( edit, &frame );
   width = frame.right - frame.left;
   height = lines * metrics.tmHeight + 2 * edge + margin;
   flags = SWP_NOMOVE | SWP_NOACTIVATE | IIF( focused, 0, SWP_NOZORDER );
   SetWindowPos( edit, IIF( focused, HWND_TOP, NULL ), 0, 0, width, height, flags );
   if( !focused )
      SendMessageW( edit, EM_LINESCROLL, 0, -SendMessageW( edit, EM_GETFIRSTVISIBLELINE, 0, 0 ) );
}

/* The subclass of edit fields: pictures, the GETs and the check of OnValid */
static LRESULT CALLBACK s_editProc( HWND edit, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR id,
                                    DWORD_PTR data )
{
   X_GUI_MASK *mask = s_maskOf( edit );
   int plain = mask == NULL || ( mask->type != 'N' && mask->mask == NULL );
   LRESULT result;

   UNUSED( data );
   switch( message )
   {
   case WM_CHAR:
      /* A GET without PICTURE is as long as its variable: a character writes over the one under the cursor, and
         when inserting the last character falls off the end */
      if( GetPropW( edit, s_getProperty ) != NULL && wParam >= L' ' && plain )
      {
         DWORD start = 0, end = 0;
         int length = GetWindowTextLengthW( edit );
         int limit = ( int )SendMessageW( edit, EM_GETLIMITTEXT, 0, 0 );

         SendMessageW( edit, EM_GETSEL, ( WPARAM )&start, ( LPARAM )&end );
         if( start == end && !x_setInsert && ( int )start < length )
            SendMessageW( edit, EM_SETSEL, start, start + 1 );
         else if( start == end && x_setInsert && length > 0 && length >= limit )
         {
            SendMessageW( edit, EM_SETSEL, ( WPARAM )( length - 1 ), length );
            SendMessageW( edit, EM_REPLACESEL, FALSE, ( LPARAM )L"" );
            SendMessageW( edit, EM_SETSEL, start, start );
         }
         break;
      }

      if( plain || ( wParam < L' ' && wParam != VK_BACK ) )
         break;

      if( mask->type == 'N' )
      {
         if( wParam == VK_BACK )
            break;

         s_numberType( edit, mask, ( wchar_t )wParam );
      }
      else if( wParam == VK_BACK )
         s_maskBackSpace( edit, mask );
      else
         s_maskType( edit, mask, ( wchar_t )wParam );
      return 0;

   case WM_KEYDOWN:
      /* A GET writes over the character under the cursor unless Insert turned SET INSERT on */
      if( wParam == VK_INSERT && GetPropW( edit, s_getProperty ) != NULL )
      {
         if( GetKeyState( VK_SHIFT ) >= 0 && GetKeyState( VK_CONTROL ) >= 0 )
         {
            x_setInsert = !x_setInsert;
            s_readScoreboard( GetAncestor( edit, GA_ROOT ), 1 );
            return 0;
         }
      }

      if( wParam == VK_DELETE && mask != NULL && mask->mask != NULL )
      {
         DWORD start = 0, end = 0;

         SendMessageW( edit, EM_GETSEL, ( WPARAM )&start, ( LPARAM )&end );
         if( end > start )
            s_maskClearSelection( edit, mask );
         else
            s_maskDelete( edit, mask, start );
         return 0;
      }
      break;

   case WM_PASTE:
      if( mask != NULL && ( mask->type == 'N' || mask->mask != NULL ) )
      {
         s_maskPaste( edit, mask );
         return 0;
      }

      if( GetPropW( edit, s_getProperty ) != NULL )
      {
         s_plainPaste( edit );
         return 0;
      }
      break;

   case WM_CUT:
   case WM_CLEAR:
      if( mask != NULL && mask->mask != NULL )
      {
         if( message == WM_CUT )
            SendMessageW( edit, WM_COPY, 0, 0 );

         s_maskClearSelection( edit, mask );
         return 0;
      }
      break;

   case WM_SETFOCUS:
      result = DefSubclassProc( edit, message, wParam, lParam );
      if( mask != NULL )
         s_maskFocus( edit, mask, 1 );

      s_getFit( edit, 1 );
      return result;

   case WM_KILLFOCUS:
      result = DefSubclassProc( edit, message, wParam, lParam );
      if( mask != NULL )
         s_maskFocus( edit, mask, 0 );

      s_getFit( edit, 0 );
      s_editValid( edit, ( HWND )wParam, mask );
      return result;

   /* A click in another GET takes the cursor to the start of the field */
   case WM_LBUTTONDOWN:
      if( GetPropW( edit, s_getProperty ) != NULL && GetFocus() != edit )
      {
         SetFocus( edit );
         if( GetFocus() == edit )
            SendMessageW( edit, EM_SETSEL, 0, 0 );
         return 0;
      }
      break;

   case X_GUI_REFOCUS:
      SetFocus( edit );
      return 0;

   case WM_NCDESTROY:
      s_maskRelease( edit );
      RemoveWindowSubclass( edit, s_editProc, id );
      break;
   }

   return DefSubclassProc( edit, message, wParam, lParam );
}

/* ---------------------------------------------------------------------------------------------
   Read() of a window, as ReadModal()
   --------------------------------------------------------------------------------------------- */

/* Windows changed by Read() and their states before it */
typedef struct
{
   HWND *windows;
   int *states;
   size_t count;
} X_GUI_WINDOWS;

/* The edit fields made editable, the controls disabled and the labels hidden during Read() */
typedef struct
{
   X_GUI_WINDOWS edits;
   X_GUI_WINDOWS locked;
   X_GUI_WINDOWS covered;
} X_GUI_READ;

/* Add a window and its state to a list */
static void s_windowsAdd( X_GUI_WINDOWS *list, HWND window, int state )
{
   list->windows = ( HWND * )x_xrealloc( list->windows, ( list->count + 1 ) * sizeof( HWND ) );
   list->states = ( int * )x_xrealloc( list->states, ( list->count + 1 ) * sizeof( int ) );
   list->windows[list->count] = window;
   list->states[list->count] = state;
   ++list->count;
}

/* Release a list of windows */
static void s_windowsFree( X_GUI_WINDOWS *list )
{
   x_xfree( list->windows );
   x_xfree( list->states );
}

/* Whether a window is an edit field of one line, not the one of a combo box */
static int s_isLineEdit( HWND window )
{
   if( window == NULL || !s_hasClass( window, L"Edit" ) || s_isMultiline( window ) )
      return 0;

   return !s_hasClass( GetParent( window ), L"ComboBox" );
}

/* The edit fields become editable; tables, toolbars and lists wait until the end of Read() */
static BOOL CALLBACK s_readCollect( HWND child, LPARAM data )
{
   X_GUI_READ *read = ( X_GUI_READ * )data;
   int field = s_hasClass( child, L"Edit" ) && !s_hasClass( GetParent( child ), L"ComboBox" );
   int locked = s_hasClass( child, s_browseClassName ) || s_toolbarOf( child ) != NULL;

   locked = locked || s_hasClass( child, WC_LISTVIEWW ) || s_hasClass( child, WC_TREEVIEWW );
   if( field )
      s_windowsAdd( &read->edits, child, ( GetWindowLongPtrW( child, GWL_STYLE ) & ES_READONLY ) != 0 );
   else if( locked && IsWindowEnabled( child ) )
      s_windowsAdd( &read->locked, child, 1 );

   return TRUE;
}

/* A field of GetList is reached also on a page of a tab control not shown now */
static int s_readReachable( HWND field )
{
   HWND window;

   for( window = field; window != NULL; window = GetParent( window ) )
   {
      if( !( GetWindowLongPtrW( window, GWL_STYLE ) & WS_CHILD ) )
         break;

      if( !s_shown( window ) && !s_hasClass( GetParent( window ), WC_TABCONTROLW ) )
         return 0;
   }

   return 1;
}

/* The one-line edit fields shown now, in the order of Tab; with GetList its fields on all pages */
static size_t s_readFields( HWND window, HWND *fields, size_t size )
{
   HWND first = GetNextDlgTabItem( window, NULL, FALSE );
   HWND item = first;
   size_t count = 0;
   int guard = 0;

   if( s_readGetCount > 0 )
   {
      size_t i;

      for( i = 0; i < s_readGetCount && count < size; ++i )
      {
         HWND get = s_readGets[i];
         int field = s_isLineEdit( get ) || GetPropW( get, s_rowsProperty ) != NULL || s_isChoice( get );

         if( field && s_readReachable( get ) && IsWindowEnabled( get ) )
            fields[count++] = get;
      }
      return count;
   }

   while( item != NULL && count < size && guard++ < 1000 )
   {
      if( s_isLineEdit( item ) && IsWindowVisible( item ) && IsWindowEnabled( item ) )
         fields[count++] = item;

      item = GetNextDlgTabItem( window, item, FALSE );
      if( item == first )
         break;
   }

   return count;
}

/* The field with the focus may end Read() only with a valid value */
static int s_readValid( HWND window )
{
   HWND focus = GetFocus();

   return !s_isLineEdit( focus ) || !IsChild( window, focus ) || s_editValid( focus, window, s_maskOf( focus ) );
}

/* The cursor goes to the start of the field, without selecting its text, as in a GET; the page of
   a tab control holding the field comes up */
static void s_readFocus( HWND field )
{
   const X_GUI_MASK *mask = s_maskOf( field );
   HWND window;

   for( window = field; window != NULL; window = GetParent( window ) )
   {
      if( !( GetWindowLongPtrW( window, GWL_STYLE ) & WS_CHILD ) )
         break;

      if( !s_shown( window ) && s_hasClass( GetParent( window ), WC_TABCONTROLW ) )
      {
         SendMessageW( GetParent( window ), TCM_SETCURSEL, ( WPARAM )( GetDlgCtrlID( window ) - 1 ), 0 );
         s_tabLayout( GetParent( window ), 1 );
      }
   }

   SetFocus( field );

   /* A number stays selected, so the first key typed starts it again, as in a numeric GET */
   if( GetFocus() == field && !( mask != NULL && mask->type == 'N' ) )
      SendMessageW( field, EM_SETSEL, 0, 0 );
}

/* Tab and Shift+Tab go to the next and the previous field, around the end */
static void s_readTab( HWND window, int previous )
{
   HWND fields[256];
   HWND focus = GetFocus();
   size_t count = s_readFields( window, fields, 256 );
   size_t i;

   if( count == 0 )
      return;

   for( i = 0; i < count && fields[i] != focus; ++i )
      ;

   if( i == count )
      s_readFocus( fields[0] );
   else if( previous )
      s_readFocus( fields[IIF( i == 0, count - 1, i - 1 )] );
   else
      s_readFocus( fields[IIF( i + 1 == count, 0, i + 1 )] );
}

/* Enter goes to the next field and on the last field ends Read(); returns 1 when Read() ends */
static int s_readEnter( HWND window )
{
   HWND fields[256];
   HWND focus = GetFocus();
   size_t count = s_readFields( window, fields, 256 );
   size_t i;

   for( i = 0; i < count && fields[i] != focus; ++i )
      ;

   if( i == count )
   {
      if( count > 0 )
         s_readFocus( fields[0] );
      return 0;
   }

   if( i + 1 == count )
      return s_readValid( window );

   s_readFocus( fields[i + 1] );

   return 0;
}

/* EndRead(): end the running Read(), with saving or without */
void x_guiWindowEndRead( int save )
{
   if( s_reading > 0 )
      s_readEnd = save;
}

/* ReadModal() of a window: the fields are edited until Enter on the last one, PgDn, PgUp or Esc;
   returns .T. when the values are saved */
int x_guiWindowRead( HWND window, const HWND *gets, size_t count, int first )
{
   X_GUI_READ read;
   HWND root = GetAncestor( window, GA_ROOT );
   HMENU menu = GetMenu( root );
   HWND previous = GetFocus();
   HWND outer = s_readWindow;
   const HWND *outerGets = s_readGets;
   size_t outerCount = s_readGetCount;
   HWND fields[256];
   size_t start;
   UINT *menuStates = NULL;
   int menuCount = IIF( menu != NULL, GetMenuItemCount( menu ), 0 );
   int result = -1;
   int lastKey = 27;
   int status = 1;
   MSG message;
   size_t i;

   memset( &read, 0, sizeof( read ) );
   ++s_reading;
   s_readEnd = -1;
   s_readWindow = window;
   s_readGets = gets;
   s_readGetCount = count;
   /* The edit fields of GetList come above what lies under them, in their order; check boxes and radio buttons
      keep their place, because the order of the windows makes the groups of radio buttons */
   for( i = 0; i < count; ++i )
   {
      HWND after = HWND_TOP;
      size_t j;

      if( s_isChoice( gets[i] ) )
         continue;

      for( j = i; j-- > 0; )
      {
         if( GetParent( gets[j] ) == GetParent( gets[i] ) && !s_isChoice( gets[j] ) )
         {
            after = gets[j];
            break;
         }
      }

      SetWindowPos( gets[i], after, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE );
   }

   /* The labels showing the values under the fields of GetList wait hidden, as SAY under GET; also on the pages
      of a tab control not shown now */
   for( i = 0; i < count; ++i )
   {
      HWND child;
      RECT field, label, common;

      GetWindowRect( gets[i], &field );
      for( child = GetWindow( GetParent( gets[i] ), GW_CHILD ); child != NULL; child = GetWindow( child, GW_HWNDNEXT ) )
      {
         if( !s_hasClass( child, L"Static" ) || !s_shown( child ) || !GetWindowRect( child, &label ) )
            continue;

         if( IntersectRect( &common, &field, &label ) )
         {
            ShowWindow( child, SW_HIDE );
            s_windowsAdd( &read.covered, child, 1 );
         }
      }
   }

   EnumChildWindows( window, s_readCollect, ( LPARAM )&read );
   /* Without GetList all edit fields of the window become editable; the fields of GetList are new and editable */
   if( count == 0 )
   {
      for( i = 0; i < read.edits.count; ++i )
         SendMessageW( read.edits.windows[i], EM_SETREADONLY, FALSE, 0 );
   }

   for( i = 0; i < read.locked.count; ++i )
      EnableWindow( read.locked.windows[i], FALSE );

   /* The menu cannot open another part of the program in the middle of Read() */
   if( menuCount > 0 )
   {
      menuStates = ( UINT * )x_xgrabArray( ( size_t )menuCount, sizeof( UINT ) );
      for( i = 0; i < ( size_t )menuCount; ++i )
      {
         menuStates[i] = GetMenuState( menu, ( UINT )i, MF_BYPOSITION );
         EnableMenuItem( menu, ( UINT )i, MF_BYPOSITION | MF_GRAYED );
      }

      DrawMenuBar( root );
   }

   /* Read() starts in the field nGet of GetList, its page coming up, or in the first field seen now, on the page
      of a tab control shown now */
   start = s_readFields( window, fields, 256 );
   for( i = 0; i < start && !IsWindowVisible( fields[i] ); ++i )
      ;

   if( first >= 1 && ( size_t )first <= count && IsWindowEnabled( gets[first - 1] ) )
      s_readFocus( gets[first - 1] );
   else if( start > 0 )
      s_readFocus( fields[IIF( i < start, i, 0 )] );

   s_readScoreboard( root, 1 );

   while( result < 0 && IsWindow( window ) && ( status = ( int )GetMessageW( &message, NULL, 0, 0 ) ) > 0 )
   {
      int plain = GetKeyState( VK_CONTROL ) >= 0 && GetKeyState( VK_MENU ) >= 0;
      int rows = GetPropW( message.hwnd, s_rowsProperty ) != NULL;

      if( message.message == WM_KEYDOWN && IsChild( window, message.hwnd ) && plain )
      {
         if( message.wParam == VK_ESCAPE )
         {
            result = 0;
            continue;
         }

         if( message.wParam == VK_NEXT || message.wParam == VK_PRIOR )
         {
            if( s_readValid( window ) )
            {
               result = 1;
               lastKey = IIF( message.wParam == VK_NEXT, 3, 18 );
            }
            continue;
         }

         if( message.wParam == VK_TAB )
         {
            s_readTab( window, GetKeyState( VK_SHIFT ) < 0 );
            continue;
         }

         /* In a GET of several rows Shift+Enter begins a new line and Enter goes to the next field */
         if( message.wParam == VK_RETURN && GetKeyState( VK_SHIFT ) < 0 && rows )
         {
            SendMessageW( message.hwnd, EM_REPLACESEL, TRUE, ( LPARAM )L"\r\n" );
            continue;
         }

         if( message.wParam == VK_RETURN && ( !s_isMultiline( message.hwnd ) || rows ) )
         {
            if( s_readEnter( window ) )
            {
               result = 1;
               lastKey = 13;
            }
            continue;
         }
      }

      s_dispatchMessage( &message );
      if( s_readEnd >= 0 )
      {
         if( s_readEnd == 0 || s_readValid( window ) )
         {
            result = s_readEnd;
            lastKey = IIF( s_readEnd == 1, 3, 27 );
         }

         s_readEnd = -1;
      }
   }

   if( status == 0 )
      PostQuitMessage( ( int )message.wParam );

   if( IsWindow( window ) )
   {
      for( i = 0; i < read.edits.count; ++i )
      {
         if( IsWindow( read.edits.windows[i] ) )
            SendMessageW( read.edits.windows[i], EM_SETREADONLY, ( WPARAM )read.edits.states[i], 0 );
      }

      for( i = 0; i < read.locked.count; ++i )
      {
         if( IsWindow( read.locked.windows[i] ) )
            EnableWindow( read.locked.windows[i], TRUE );
      }

      /* The focus goes back where it was before Read(), without checking the field it leaves */
      s_validating = 1;
      if( previous != NULL && IsWindow( previous ) && IsWindowEnabled( previous ) && IsWindowVisible( previous ) )
         SetFocus( previous );

      s_validating = 0;
   }

   if( menuStates != NULL )
   {
      for( i = 0; i < ( size_t )menuCount && IsMenu( menu ); ++i )
         EnableMenuItem( menu, ( UINT )i, MF_BYPOSITION | ( menuStates[i] & ( MF_GRAYED | MF_DISABLED ) ) );

      if( IsWindow( root ) )
         DrawMenuBar( root );

      x_xfree( menuStates );
   }

   if( IsWindow( root ) )
      s_readScoreboard( root, 0 );

   for( i = 0; i < read.covered.count; ++i )
   {
      if( IsWindow( read.covered.windows[i] ) )
         ShowWindow( read.covered.windows[i], SW_SHOW );
   }

   s_windowsFree( &read.edits );
   s_windowsFree( &read.locked );
   s_windowsFree( &read.covered );
   /* The GETs of @var give their values back to the variables, as READ does */
   if( result == 1 )
   {
      for( i = 0; i < count; ++i )
      {
         X_ITEM *var = ( X_ITEM * )GetPropW( gets[i], s_varProperty );

         if( var != NULL && IsWindow( gets[i] ) )
         {
            X_ITEM value = X_ITEM_NIL_INIT;

            s_editCurrent( gets[i], s_maskOf( gets[i] ), &value );
            x_itemCopy( var, &value );
            x_itemClear( &value );
         }
      }
   }

   /* LastKey() tells how ReadModal() ended: K_ESC, K_PGDN, K_PGUP or K_ENTER */
   x_inkeySetLast( IIF( result == 1, lastKey, 27 ) );
   --s_reading;
   s_readWindow = outer;
   s_readGets = outerGets;
   s_readGetCount = outerCount;
   /* The window the user closed during Read() closes now */
   if( s_reading == 0 && s_readClose != NULL )
   {
      if( IsWindow( s_readClose ) )
         PostMessageW( s_readClose, WM_CLOSE, 0, 0 );

      s_readClose = NULL;
   }

   return result == 1;
}

/* Return the picture of an edit field */
void x_guiEditPicture( X_CALL_CTX *ctx, HWND edit )
{
   const X_GUI_MASK *mask = s_maskOf( edit );

   x_retc( ctx, IIF( mask != NULL, mask->picture, "" ) );
}

/* Set the picture of an edit field; the value is shown again through it */
void x_guiEditSetPicture( X_CALL_CTX *ctx, HWND edit, const X_ITEM *picture )
{
   X_GUI_MASK *mask;
   X_ITEM value __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;

   if( s_isMultiline( edit ) || picture == NULL || !X_IS_STRING( picture ) )
   {
      x_guiRaise( ctx, "GUI_EDIT:PICTURE" );
      return;
   }

   s_editCurrent( edit, s_maskOf( edit ), &value );
   mask = s_maskEnsure( edit, IIF( s_maskOf( edit ) != NULL, s_maskOf( edit )->type, 'C' ) );
   x_xfree( mask->picture );
   mask->picture = x_strdup( x_itemGetCPtr( picture ) );
   s_maskParse( mask, mask->type );
   s_maskStyle( edit, mask );
   x_callbackEnterDll();
   s_editShow( edit, mask, &value );
   x_callbackLeaveDll();
   x_callbackRaisePending();
   x_itemReturn( ctx, picture );
}

/* Return the value of an edit field in its type */
void x_guiEditValue( X_CALL_CTX *ctx, HWND edit )
{
   s_editCurrent( edit, s_maskOf( edit ), ctx->result );
}

/* Set the value of an edit field: a text, a number or a date */
void x_guiEditSetValue( X_CALL_CTX *ctx, HWND edit, const X_ITEM *value )
{
   X_GUI_MASK *mask = s_maskOf( edit );
   char type = 'C';
   int valid;

   valid = value != NULL && ( X_IS_STRING( value ) || X_IS_NUMERIC( value ) || X_IS_DATE( value ) );
   valid = valid && ( X_IS_STRING( value ) || !s_isMultiline( edit ) );
   if( !valid )
   {
      x_guiRaise( ctx, "GUI_EDIT:VALUE" );
      return;
   }

   if( X_IS_STRING( value ) && mask == NULL )
   {
      x_guiSetText( ctx, edit, value );
      x_itemReturn( ctx, value );
      return;
   }

   if( X_IS_NUMERIC( value ) )
      type = 'N';
   else if( X_IS_DATE( value ) )
      type = 'D';

   mask = s_maskEnsure( edit, type );
   x_callbackEnterDll();
   s_editShow( edit, mask, value );
   x_callbackLeaveDll();
   x_callbackRaisePending();
   x_itemReturn( ctx, value );
}

/* ---------------------------------------------------------------------------------------------
   Browses
   --------------------------------------------------------------------------------------------- */

/* The xDev colors of a browse; the program may change them */
#define X_GUI_BROWSE_HEAD 0x264D73
#define X_GUI_BROWSE_HEADTEXT 0xF2F2F2
#define X_GUI_BROWSE_HEADLINE 0x1A3550
#define X_GUI_BROWSE_ROW 0xFFFFFF
#define X_GUI_BROWSE_ROW2 0xE9EDF1
#define X_GUI_BROWSE_TEXT 0x111111
#define X_GUI_BROWSE_LINE 0xD3DBE3
#define X_GUI_BROWSE_CURSOR 0x1A3550
#define X_GUI_BROWSE_CURSORTXT 0xF2F2F2
#define X_GUI_BROWSE_INACTIVE 0xD3DBE3

static const int s_gridColors[X_GUI_GRID_COLORS] = { X_GUI_BROWSE_HEAD, X_GUI_BROWSE_HEADTEXT, X_GUI_BROWSE_HEADLINE,
                                                     X_GUI_BROWSE_ROW, X_GUI_BROWSE_ROW2, X_GUI_BROWSE_TEXT,
                                                     X_GUI_BROWSE_LINE, X_GUI_BROWSE_CURSOR, X_GUI_BROWSE_CURSORTXT,
                                                     X_GUI_BROWSE_INACTIVE, X_GUI_BROWSE_TEXT };

/* Operations of oBrowse:__GuiNavigate() */
enum
{
   X_GUI_GO_TOP = 1,
   X_GUI_GO_BOTTOM,
   X_GUI_SKIP,
   X_GUI_POSITION,
   X_GUI_EMPTY
};

/* Events of oBrowse:__GuiEvent() */
enum
{
   X_GUI_BROWSE_CHANGE = 1,
   X_GUI_BROWSE_CHOOSE
};

/* The grid data of a browse, or NULL */
static X_GUI_GRID *s_gridOf( HWND browse )
{
   return ( X_GUI_GRID * )GetPropW( browse, s_gridProperty );
}

/* Release the texts of the rows on the screen */
static void s_gridCellsFree( X_GUI_GRID *grid )
{
   size_t i;

   for( i = 0; grid->cells != NULL && i < ( size_t )grid->cellRows * grid->columnCount; ++i )
      x_xfree( grid->cells[i] );

   x_xfree( grid->cells );
   grid->cells = NULL;
   grid->cellRows = 0;
   grid->rows = 0;
}

/* Release the grid data of a browse */
static void s_gridRelease( HWND browse )
{
   X_GUI_GRID *grid = ( X_GUI_GRID * )RemovePropW( browse, s_gridProperty );
   size_t i;

   if( grid == NULL )
      return;

   s_gridCellsFree( grid );
   for( i = 0; i < grid->columnCount; ++i )
      x_xfree( grid->columns[i].title );

   x_xfree( grid->columns );
   x_xfree( grid );
}

/* Height of a row and of the heading in pixels of the screen */
static int s_browseRowHeight( HWND browse )
{
   UINT dpi = GetDpiForWindow( browse );
   HDC dc = GetDC( browse );
   HGDIOBJ old = SelectObject( dc, s_fontOf( browse, dpi ) );
   TEXTMETRIC metrics;

   GetTextMetricsW( dc, &metrics );
   SelectObject( dc, old );
   ReleaseDC( browse, dc );

   return metrics.tmHeight + x_guiScale( 6, dpi );
}

/* The number of rows below the heading, at least one */
static int s_browseRowCount( HWND browse )
{
   RECT client;
   int height = s_browseRowHeight( browse );
   int rows;

   GetClientRect( browse, &client );
   rows = ( client.bottom - height ) / height;

   return IIF( rows > 1, rows, 1 );
}

/* A column is as wide as its characters in the font of the table, with a margin on both sides */
static int s_browseColumnWidth( HWND browse, const X_GUI_GRID *grid, size_t column )
{
   UINT dpi = GetDpiForWindow( browse );
   HDC dc = GetDC( browse );
   HGDIOBJ old = SelectObject( dc, s_fontOf( browse, dpi ) );
   TEXTMETRIC metrics;

   GetTextMetricsW( dc, &metrics );
   SelectObject( dc, old );
   ReleaseDC( browse, dc );

   return grid->columns[column].width * metrics.tmAveCharWidth + 2 * x_guiScale( 4, dpi );
}

/* The left side of a column in the whole width of the table */
static int s_browseColumnLeft( HWND browse, const X_GUI_GRID *grid, size_t column )
{
   int left = 0;
   size_t i;

   for( i = 0; i < column; ++i )
      left += s_browseColumnWidth( browse, grid, i );

   return left;
}

/* The width of all columns */
static int s_browseWidth( HWND browse, const X_GUI_GRID *grid )
{
   return s_browseColumnLeft( browse, grid, grid->columnCount );
}

/* The first columns frozen as TBrowse:freeze stand at the left and do not scroll */
static size_t s_browseFrozen( const X_GUI_GRID *grid )
{
   return IIF( ( size_t )grid->freeze > grid->columnCount, grid->columnCount, ( size_t )grid->freeze );
}

/* The width of the frozen columns */
static int s_browseFrozenWidth( HWND browse, const X_GUI_GRID *grid )
{
   return s_browseColumnLeft( browse, grid, s_browseFrozen( grid ) );
}

/* One call of the data blocks through the object; *ok is 0 when the call failed */
static int64_t s_browseCall( HWND browse, int operation, int64_t argument, int *ok )
{
   X_ITEM *object = ( X_ITEM * )GetWindowLongPtrW( browse, GWLP_USERDATA );
   X_ITEM result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   int64_t values[2] = { operation, argument };

   *ok = *ok && object != NULL && x_guiNotify( object, "__GUINAVIGATE", &result, 2, values );

   return IIF( *ok && X_IS_NUMERIC( &result ), x_itemGetNInt( &result ), 0 );
}

/* Tell the object of a browse that the record changed or was chosen */
static void s_browseEvent( HWND browse, int event )
{
   X_ITEM *object = ( X_ITEM * )GetWindowLongPtrW( browse, GWLP_USERDATA );
   X_ITEM result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   int64_t values[1] = { event };

   if( object != NULL )
      x_guiNotify( object, "__GUIEVENT", &result, 1, values );
}

/* The texts of the current record into row number row; the type of each value decides the alignment
   of a column without its own one */
static int s_browseRead( HWND browse, X_GUI_GRID *grid, int row )
{
   X_ITEM *object = ( X_ITEM * )GetWindowLongPtrW( browse, GWLP_USERDATA );
   X_ITEM result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   size_t i, size;
   wchar_t **cell;
   const char *type, *value;

   if( object == NULL || !x_guiNotify( object, "__GUIROW", &result, 0, NULL ) || !X_IS_ARRAY( &result ) )
      return 0;

   if( ( size_t )x_arrayLen( &result ) != grid->columnCount * 2 )
      return 0;

   for( i = 0; i < grid->columnCount; ++i )
   {
      cell = &grid->cells[( size_t )( row - 1 ) * grid->columnCount + i];
      x_xfree( *cell );
      value = x_arrayGetCPtr( &result, i * 2 + 1 );
      *cell = ( wchar_t * )x_utf8ToUtf16Alloc( value, x_arrayGetCLen( &result, i * 2 + 1 ), &size );
      type = x_arrayGetCPtr( &result, i * 2 + 2 );
      if( type != NULL && type[0] == 'L' )
         grid->columns[i].logical = 1;

      if( grid->columns[i].align >= 0 || type == NULL || type[0] == 'U' )
         continue;

      if( type[0] == 'N' )
         grid->columns[i].shown = 1;
      else if( type[0] == 'L' )
         grid->columns[i].shown = 2;
      else
         grid->columns[i].shown = 0;
   }

   return 1;
}

/* Set the scroll bars: the horizontal one by the columns, the vertical one by the position in the data */
static void s_browseScrollBars( HWND browse, X_GUI_GRID *grid )
{
   SCROLLINFO info;
   RECT client;
   int width = s_browseWidth( browse, grid );
   int frozen = s_browseFrozenWidth( browse, grid );

   GetClientRect( browse, &client );
   grid->offset = IIF( grid->offset > width - client.right, width - client.right, grid->offset );
   grid->offset = IIF( grid->offset < 0, 0, grid->offset );

   /* Only the columns behind the frozen ones scroll */
   memset( &info, 0, sizeof( info ) );
   info.cbSize = sizeof( info );
   info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS | SIF_DISABLENOSCROLL;
   info.nMax = IIF( width - frozen > 0, width - frozen - 1, 0 );
   info.nPage = ( UINT )IIF( client.right > frozen, client.right - frozen, 0 );
   info.nPos = grid->offset;
   SetScrollInfo( browse, SB_HORZ, &info, TRUE );

   /* Without a position block only the top, the middle and the bottom of the data are known */
   info.nPage = 0;
   if( grid->position >= 0 )
   {
      info.nMax = 1000;
      info.nPos = grid->position;
   }
   else if( grid->hitTop && grid->hitBottom )
   {
      info.nMax = 0;
      info.nPos = 0;
   }
   else
   {
      info.nMax = 2;
      if( grid->hitTop )
         info.nPos = 0;
      else if( grid->hitBottom )
         info.nPos = 2;
      else
         info.nPos = 1;
   }

   SetScrollInfo( browse, SB_VERT, &info, TRUE );
}

/* Room for the texts of rows rows */
static void s_browseCells( X_GUI_GRID *grid, int rows )
{
   if( grid->cells != NULL && grid->cellRows == rows )
      return;

   s_gridCellsFree( grid );
   if( grid->columnCount == 0 )
      return;

   grid->cells = ( wchar_t ** )x_xgrabArray( ( size_t )rows * grid->columnCount, sizeof( wchar_t * ) );
   grid->cellRows = rows;
   memset( grid->cells, 0, ( size_t )rows * grid->columnCount * sizeof( wchar_t * ) );
}

/* Reads the rows of the screen around the current record: the cursor keeps its row unless there are
   not enough records below it, then the page ends with the last record; the current record is the
   cursor row again at the end */
static void s_browseRefill( HWND browse, X_GUI_GRID *grid )
{
   int rowCount = s_browseRowCount( browse );
   int ok = 1;
   int empty = 1;
   int want, below, above, target, total, filled = 0, extend, i;

   grid->valid = 1;
   s_browseCells( grid, rowCount );
   if( grid->columnCount == 0 || GetWindowLongPtrW( browse, GWLP_USERDATA ) == 0 )
   {
      grid->rows = 0;
      return;
   }

   /* emptyBlock gives .T. when there is no record: the table shows no row instead of the one past the end */
   if( s_browseCall( browse, X_GUI_EMPTY, 0, &empty ) != 0 )
   {
      grid->rows = 0;
      grid->rowPos = 1;
      grid->hitTop = 1;
      grid->hitBottom = 1;
      grid->position = -1;
      s_browseScrollBars( browse, grid );
      InvalidateRect( browse, NULL, FALSE );
      return;
   }

   if( grid->rowPos < 1 )
      grid->rowPos = 1;
   else if( grid->rowPos > rowCount )
      grid->rowPos = rowCount;

   /* One record more than needed in each direction tells whether the data ends there */
   want = rowCount - grid->rowPos;
   below = ( int )s_browseCall( browse, X_GUI_SKIP, want + 1, &ok );
   if( below > 0 )
      s_browseCall( browse, X_GUI_SKIP, -below, &ok );

   grid->hitBottom = below <= want;
   below = IIF( below > want, want, below );
   target = IIF( below >= want, grid->rowPos - 1, rowCount - 1 - below );
   above = -( int )s_browseCall( browse, X_GUI_SKIP, -( target + 1 ), &ok );
   grid->hitTop = above <= target;
   if( above > target )
   {
      s_browseCall( browse, X_GUI_SKIP, 1, &ok );
      above = target;
   }

   total = above + 1 + below;
   grid->rowPos = above + 1;
   /* At the top of the data the page goes on downwards as far as there are records */
   extend = grid->hitTop && !grid->hitBottom && total < rowCount;
   if( extend )
      total = rowCount;

   for( i = 1; ok && i <= total; ++i )
   {
      if( !s_browseRead( browse, grid, i ) )
         break;

      filled = i;
      if( i < total && s_browseCall( browse, X_GUI_SKIP, 1, &ok ) != 1 )
         break;
   }

   if( extend )
   {
      grid->hitBottom = filled < total || s_browseCall( browse, X_GUI_SKIP, 1, &ok ) != 1;
      if( !grid->hitBottom )
         s_browseCall( browse, X_GUI_SKIP, -1, &ok );
   }

   if( filled > grid->rowPos )
      s_browseCall( browse, X_GUI_SKIP, grid->rowPos - filled, &ok );

   grid->rows = filled;
   grid->rowPos = IIF( grid->rowPos > filled && filled > 0, filled, grid->rowPos );
   grid->position = ( int )s_browseCall( browse, X_GUI_POSITION, 0, &ok );
   if( !ok )
      grid->position = -1;

   s_browseScrollBars( browse, grid );
   InvalidateRect( browse, NULL, FALSE );
}

/* Moves the current record by delta and the cursor row with it when lFollow; a move off the data
   moves as far as it can */
static void s_browseMove( HWND browse, X_GUI_GRID *grid, int delta, int follow )
{
   int ok = 1;
   int moved;

   if( delta == 0 || grid->rows == 0 )
      return;

   moved = ( int )s_browseCall( browse, X_GUI_SKIP, delta, &ok );
   if( !ok || moved == 0 )
      return;

   if( follow )
      grid->rowPos += moved;

   s_browseRefill( browse, grid );
   s_browseEvent( browse, X_GUI_BROWSE_CHANGE );
}

/* Go to the first or, with bottom, to the last record */
static void s_browseEdge( HWND browse, X_GUI_GRID *grid, int bottom )
{
   int ok = 1;

   if( grid->rows == 0 )
      return;

   s_browseCall( browse, IIF( bottom, X_GUI_GO_BOTTOM, X_GUI_GO_TOP ), 0, &ok );
   if( !ok )
      return;

   grid->rowPos = IIF( bottom, s_browseRowCount( browse ), 1 );
   s_browseRefill( browse, grid );
   s_browseEvent( browse, X_GUI_BROWSE_CHANGE );
}

/* Scroll the columns behind the frozen ones to offset */
static void s_browseScroll( HWND browse, X_GUI_GRID *grid, int offset )
{
   grid->offset = offset;
   s_browseScrollBars( browse, grid );
   InvalidateRect( browse, NULL, FALSE );
}

/* The cursor goes to another column, as in TBrowse; the table scrolls so that the column is seen */
static void s_browseColumn( HWND browse, X_GUI_GRID *grid, int column )
{
   RECT client;
   int left, width, frozen, area;

   if( grid->columnCount == 0 )
      return;

   if( column < 0 )
      column = 0;
   else if( column >= ( int )grid->columnCount )
      column = ( int )grid->columnCount - 1;

   grid->colPos = column;
   /* A frozen column is always seen; another one is measured in the part of the table that scrolls */
   if( ( size_t )column < s_browseFrozen( grid ) )
   {
      InvalidateRect( browse, NULL, FALSE );
      return;
   }

   frozen = s_browseFrozenWidth( browse, grid );
   left = s_browseColumnLeft( browse, grid, ( size_t )column ) - frozen;
   width = s_browseColumnWidth( browse, grid, ( size_t )column );
   GetClientRect( browse, &client );
   area = client.right - frozen;
   if( left < grid->offset )
      s_browseScroll( browse, grid, left );
   else if( left + width > grid->offset + area )
      s_browseScroll( browse, grid, IIF( width > area, left, left + width - area ) );
   else
      InvalidateRect( browse, NULL, FALSE );
}

/* Space or a click changes the check box of the current record: the block of the column gets the opposite
   value, as the get/set block of TBColumn; column -1 is the first column with check boxes */
static void s_browseToggle( HWND browse, X_GUI_GRID *grid, int column )
{
   X_ITEM *object = ( X_ITEM * )GetWindowLongPtrW( browse, GWLP_USERDATA );
   X_ITEM result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   int64_t values[1];

   if( grid->rows == 0 || object == NULL )
      return;

   if( column < 0 )
   {
      for( column = 0; ( size_t )column < grid->columnCount && !grid->columns[column].logical; ++column )
         ;
   }

   if( ( size_t )column >= grid->columnCount || !grid->columns[column].logical )
      return;

   values[0] = column + 1;
   x_guiNotify( object, "__GUITOGGLE", &result, 1, values );
   s_browseRead( browse, grid, grid->rowPos );
   InvalidateRect( browse, NULL, FALSE );
}

/* Paint the heading, the rows and the cursor cell through a memory bitmap */
static void s_browsePaint( HWND browse, X_GUI_GRID *grid )
{
   PAINTSTRUCT state;
   HDC dc = BeginPaint( browse, &state );
   UINT dpi = GetDpiForWindow( browse );
   int height = s_browseRowHeight( browse );
   int padding = x_guiScale( 4, dpi );
   int focused = GetFocus() == browse;
   static const UINT formats[] = { DT_LEFT, DT_RIGHT, DT_CENTER };
   RECT client, area, text;
   HDC memory;
   HBITMAP bitmap;
   HGDIOBJ oldBitmap, oldFont;
   int row, x, width, cursor, cx, cy, back, fore;
   int cursorBack = grid->colors[IIF( focused, X_GUI_GRID_CURSORBACKGROUND, X_GUI_GRID_INACTIVECURSOR )];
   int *lefts, *widths;
   size_t column, order, frozen;
   const wchar_t *value;
   UINT format;

   GetClientRect( browse, &client );
   memory = CreateCompatibleDC( dc );
   cx = IIF( client.right > 0, client.right, 1 );
   cy = IIF( client.bottom > 0, client.bottom, 1 );
   bitmap = CreateCompatibleBitmap( dc, cx, cy );
   oldBitmap = SelectObject( memory, bitmap );
   oldFont = SelectObject( memory, s_fontOf( browse, dpi ) );
   SetBkMode( memory, TRANSPARENT );

   SetDCBrushColor( memory, s_colorRef( grid->colors[X_GUI_GRID_BACKGROUND] ) );
   FillRect( memory, &client, ( HBRUSH )GetStockObject( DC_BRUSH ) );

   area = ( RECT ){ 0, 0, client.right, height };
   SetDCBrushColor( memory, s_colorRef( grid->colors[X_GUI_GRID_HEADBACKGROUND] ) );
   FillRect( memory, &area, ( HBRUSH )GetStockObject( DC_BRUSH ) );
   SetTextColor( memory, s_colorRef( grid->colors[X_GUI_GRID_HEADFOREGROUND] ) );
   /* The places of the columns; the scrolling columns are drawn first and the frozen ones over them */
   frozen = s_browseFrozen( grid );
   lefts = ( int * )x_xgrabArray( grid->columnCount + 1, sizeof( int ) );
   widths = ( int * )x_xgrabArray( grid->columnCount + 1, sizeof( int ) );
   for( column = 0, x = 0; column < grid->columnCount; ++column, x += widths[column - 1] )
   {
      widths[column] = s_browseColumnWidth( browse, grid, column );
      lefts[column] = x - IIF( column < frozen, 0, grid->offset );
   }

   for( order = 0; order < grid->columnCount; ++order )
   {
      column = IIF( order < grid->columnCount - frozen, order + frozen, order - ( grid->columnCount - frozen ) );
      x = lefts[column];
      width = widths[column];
      format = formats[grid->columns[column].shown] | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS;
      if( column < frozen )
      {
         area = ( RECT ){ x, 0, x + width, height };
         SetDCBrushColor( memory, s_colorRef( grid->colors[X_GUI_GRID_HEADBACKGROUND] ) );
         FillRect( memory, &area, ( HBRUSH )GetStockObject( DC_BRUSH ) );
      }

      text = ( RECT ){ x + padding, 0, x + width - padding, height };
      DrawTextW( memory, grid->columns[column].title, -1, &text, format );
      area = ( RECT ){ x + width - 1, 0, x + width, height };
      SetDCBrushColor( memory, s_colorRef( grid->colors[X_GUI_GRID_HEADLINE] ) );
      FillRect( memory, &area, ( HBRUSH )GetStockObject( DC_BRUSH ) );
   }

   for( row = 1; row <= grid->rows; ++row )
   {
      area = ( RECT ){ 0, height * row, client.right, height * ( row + 1 ) };
      back = grid->colors[IIF( row % 2 == 0, X_GUI_GRID_ALTERNATEBACKGROUND, X_GUI_GRID_BACKGROUND )];
      SetDCBrushColor( memory, s_colorRef( back ) );
      FillRect( memory, &area, ( HBRUSH )GetStockObject( DC_BRUSH ) );
      for( order = 0; order < grid->columnCount; ++order )
      {
         column = IIF( order < grid->columnCount - frozen, order + frozen, order - ( grid->columnCount - frozen ) );
         x = lefts[column];
         width = widths[column];
         format = formats[grid->columns[column].shown] | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS;
         value = grid->cells[( size_t )( row - 1 ) * grid->columnCount + column];

         /* The cursor is one cell, the column colPos of the current record, as in TBrowse */
         cursor = row == grid->rowPos && ( int )column == grid->colPos;
         if( column < frozen && !cursor )
         {
            text = ( RECT ){ x, area.top, x + width, area.bottom };
            SetDCBrushColor( memory, s_colorRef( back ) );
            FillRect( memory, &text, ( HBRUSH )GetStockObject( DC_BRUSH ) );
         }

         if( cursor )
         {
            text = ( RECT ){ x, area.top, x + width, area.bottom };
            SetDCBrushColor( memory, s_colorRef( cursorBack ) );
            FillRect( memory, &text, ( HBRUSH )GetStockObject( DC_BRUSH ) );
         }

         fore = grid->colors[IIF( cursor && focused, X_GUI_GRID_CURSORFOREGROUND, X_GUI_GRID_FOREGROUND )];
         SetTextColor( memory, s_colorRef( fore ) );
         text = ( RECT ){ x + padding, area.top, x + width - padding, area.bottom };
         /* A logical cell shows the tick U+221A of the font of the table when it is true, nothing when false;
            the tick has its own color, on the cursor the color of the cursor text */
         if( grid->columns[column].logical )
         {
            if( !( cursor && focused ) )
               SetTextColor( memory, s_colorRef( grid->colors[X_GUI_GRID_CHECKFOREGROUND] ) );

            text = ( RECT ){ x, area.top, x + width, area.bottom };
            if( value != NULL && value[0] == L'T' )
               DrawTextW( memory, L"\x221A", 1, &text, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX );
         }
         else if( value != NULL )
            DrawTextW( memory, value, -1, &text, format );

         text = ( RECT ){ x + width - 1, area.top, x + width, area.bottom };
         SetDCBrushColor( memory, s_colorRef( grid->colors[X_GUI_GRID_LINE] ) );
         FillRect( memory, &text, ( HBRUSH )GetStockObject( DC_BRUSH ) );
      }

      area.top = area.bottom - 1;
      SetDCBrushColor( memory, s_colorRef( grid->colors[X_GUI_GRID_LINE] ) );
      FillRect( memory, &area, ( HBRUSH )GetStockObject( DC_BRUSH ) );
   }

   x_xfree( lefts );
   x_xfree( widths );
   BitBlt( dc, 0, 0, client.right, client.bottom, memory, 0, 0, SRCCOPY );
   SelectObject( memory, oldFont );
   SelectObject( memory, oldBitmap );
   DeleteObject( bitmap );
   DeleteDC( memory );
   EndPaint( browse, &state );
}

/* The window procedure of browses: painting, keys, the mouse and the scroll bars */
static LRESULT CALLBACK s_browseProc( HWND browse, UINT message, WPARAM wParam, LPARAM lParam )
{
   X_GUI_GRID *grid = s_gridOf( browse );
   int rows;

   switch( message )
   {
   case WM_NCCREATE:
      grid = ( X_GUI_GRID * )x_xgrab( sizeof( X_GUI_GRID ) );
      memset( grid, 0, sizeof( *grid ) );
      grid->rowPos = 1;
      grid->position = -1;
      memcpy( grid->colors, s_gridColors, sizeof( grid->colors ) );
      SetPropW( browse, s_gridProperty, grid );
      break;

   case WM_NCDESTROY:
      s_gridRelease( browse );
      break;

   case WM_ERASEBKGND:
      return 1;

   case WM_PAINT:
      if( grid == NULL )
         break;

      if( !grid->valid )
         s_browseRefill( browse, grid );

      s_browsePaint( browse, grid );
      return 0;

   case WM_SIZE:
      if( grid != NULL )
      {
         grid->valid = 0;
         s_browseScrollBars( browse, grid );
         InvalidateRect( browse, NULL, FALSE );
      }
      return 0;

   case WM_SETFOCUS:
   case WM_KILLFOCUS:
      InvalidateRect( browse, NULL, FALSE );
      return 0;

   case WM_GETDLGCODE:
   {
      const MSG *key = ( const MSG * )lParam;

      if( key != NULL && key->message == WM_KEYDOWN && key->wParam == VK_RETURN )
         return DLGC_WANTMESSAGE;

      return DLGC_WANTARROWS;
   }

   case WM_KEYDOWN:
      if( grid == NULL )
         break;

      rows = s_browseRowCount( browse );
      switch( wParam )
      {
      case VK_UP:
         s_browseMove( browse, grid, -1, 1 );
         return 0;
      case VK_DOWN:
         s_browseMove( browse, grid, 1, 1 );
         return 0;
      case VK_PRIOR:
         s_browseMove( browse, grid, -IIF( rows > 1, rows - 1, 1 ), 0 );
         return 0;
      case VK_NEXT:
         s_browseMove( browse, grid, IIF( rows > 1, rows - 1, 1 ), 0 );
         return 0;
      case VK_HOME:
         if( GetKeyState( VK_CONTROL ) < 0 )
            s_browseEdge( browse, grid, 0 );
         else
            s_browseColumn( browse, grid, 0 );
         return 0;
      case VK_END:
         if( GetKeyState( VK_CONTROL ) < 0 )
            s_browseEdge( browse, grid, 1 );
         else
            s_browseColumn( browse, grid, ( int )grid->columnCount - 1 );
         return 0;
      case VK_LEFT:
         s_browseColumn( browse, grid, grid->colPos - 1 );
         return 0;
      case VK_RIGHT:
         s_browseColumn( browse, grid, grid->colPos + 1 );
         return 0;
      case VK_RETURN:
         if( grid->rows > 0 )
            s_browseEvent( browse, X_GUI_BROWSE_CHOOSE );
         return 0;
      case VK_SPACE:
         s_browseToggle( browse, grid, -1 );
         return 0;
      }
      break;

   case WM_LBUTTONDOWN:
   case WM_LBUTTONDBLCLK:
      SetFocus( browse );
      if( grid != NULL )
      {
         int row = ( short )HIWORD( lParam ) / s_browseRowHeight( browse );

         if( row >= 1 && row <= grid->rows )
         {
            /* A click left of the frozen width hits a frozen column, otherwise a scrolled one */
            int x = ( short )LOWORD( lParam );
            size_t frozen = s_browseFrozen( grid );
            int inside = frozen > 0 && x < s_browseFrozenWidth( browse, grid );
            size_t last = IIF( inside, frozen, grid->columnCount );
            size_t column = IIF( inside, 0, frozen );

            x += IIF( inside, 0, grid->offset );
            for( ; column + 1 < last && x >= s_browseColumnLeft( browse, grid, column + 1 ); ++column )
               ;

            column = IIF( column >= grid->columnCount, grid->columnCount - 1, column );
            s_browseColumn( browse, grid, ( int )column );
            if( row != grid->rowPos )
            {
               int ok = 1;
               int moved = ( int )s_browseCall( browse, X_GUI_SKIP, row - grid->rowPos, &ok );

               if( ok && moved != 0 )
               {
                  grid->rowPos += moved;
                  InvalidateRect( browse, NULL, FALSE );
                  s_browseEvent( browse, X_GUI_BROWSE_CHANGE );
               }
            }

            if( grid->columns[column].logical )
               s_browseToggle( browse, grid, ( int )column );

            if( message == WM_LBUTTONDBLCLK )
               s_browseEvent( browse, X_GUI_BROWSE_CHOOSE );
         }
      }
      return 0;

   case WM_MOUSEWHEEL:
      if( grid != NULL )
         s_browseMove( browse, grid, IIF( GET_WHEEL_DELTA_WPARAM( wParam ) > 0, -3, 3 ), 0 );
      return 0;

   case WM_VSCROLL:
      if( grid == NULL )
         break;

      rows = s_browseRowCount( browse );
      switch( LOWORD( wParam ) )
      {
      case SB_LINEUP:
         s_browseMove( browse, grid, -1, 1 );
         break;
      case SB_LINEDOWN:
         s_browseMove( browse, grid, 1, 1 );
         break;
      case SB_PAGEUP:
         s_browseMove( browse, grid, -IIF( rows > 1, rows - 1, 1 ), 0 );
         break;
      case SB_PAGEDOWN:
         s_browseMove( browse, grid, IIF( rows > 1, rows - 1, 1 ), 0 );
         break;
      case SB_TOP:
         s_browseEdge( browse, grid, 0 );
         break;
      case SB_BOTTOM:
         s_browseEdge( browse, grid, 1 );
         break;
      case SB_THUMBPOSITION:
      {
         SCROLLINFO info = { sizeof( info ), SIF_RANGE | SIF_TRACKPOS, 0, 0, 0, 0, 0 };

         GetScrollInfo( browse, SB_VERT, &info );
         if( info.nTrackPos <= info.nMin )
            s_browseEdge( browse, grid, 0 );
         else if( info.nTrackPos >= info.nMax )
            s_browseEdge( browse, grid, 1 );
         else
            s_browseScrollBars( browse, grid );
         break;
      }
      }
      return 0;

   case WM_HSCROLL:
      if( grid == NULL )
         break;

      switch( LOWORD( wParam ) )
      {
      case SB_LINELEFT:
         s_browseScroll( browse, grid, grid->offset - x_guiScale( 20, GetDpiForWindow( browse ) ) );
         break;
      case SB_LINERIGHT:
         s_browseScroll( browse, grid, grid->offset + x_guiScale( 20, GetDpiForWindow( browse ) ) );
         break;
      case SB_PAGELEFT:
      case SB_PAGERIGHT:
      {
         RECT client;
         int page;

         GetClientRect( browse, &client );
         page = IIF( LOWORD( wParam ) == SB_PAGELEFT, -client.right, client.right );
         s_browseScroll( browse, grid, grid->offset + page );
         break;
      }
      case SB_THUMBTRACK:
      case SB_THUMBPOSITION:
      {
         SCROLLINFO info = { sizeof( info ), SIF_TRACKPOS, 0, 0, 0, 0, 0 };

         GetScrollInfo( browse, SB_HORZ, &info );
         s_browseScroll( browse, grid, info.nTrackPos );
         break;
      }
      }
      return 0;
   }

   return DefWindowProcW( browse, message, wParam, lParam );
}

/* Register the window class of browses once */
static BOOL s_registerBrowseClass( HINSTANCE instance )
{
   WNDCLASSEXW browseClass;

   if( s_browseClass != 0 )
      return TRUE;

   memset( &browseClass, 0, sizeof( browseClass ) );
   browseClass.cbSize = sizeof( browseClass );
   browseClass.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
   browseClass.lpfnWndProc = s_browseProc;
   browseClass.hInstance = instance;
   browseClass.hCursor = LoadCursorW( NULL, IDC_ARROW );
   browseClass.lpszClassName = s_browseClassName;

   s_browseClass = RegisterClassExW( &browseClass );

   return s_browseClass != 0;
}

/* The number of characters of a UTF-8 text */
static int s_utf8Length( const char *text, size_t length )
{
   int count = 0;
   size_t i;

   for( i = 0; i < length; ++i )
      count += ( ( unsigned char )text[i] & 0xC0 ) != 0x80;

   return count;
}

/* The width of a column in characters, as TBColumn: the longer of the title and the value, whose length
   is the length of the field, of the picture or of the type */
int x_guiBrowseTextWidth( const X_ITEM *title, const X_ITEM *value, const char *picture )
{
   const char *space = IIF( picture != NULL && picture[0] == '@', strchr( picture, ' ' ), NULL );
   const char *mask = "";
   int width = 10;
   int titleWidth = 0;

   if( space != NULL )
      mask = space + 1;
   else if( picture != NULL && picture[0] != '@' )
      mask = picture;

   if( mask[0] != '\0' )
      width = s_utf8Length( mask, strlen( mask ) );
   else if( value != NULL && X_IS_STRING( value ) )
      width = s_utf8Length( x_itemGetCPtr( value ), x_itemGetCLen( value ) );
   else if( value != NULL && X_IS_LOGICAL( value ) )
      width = 2;

   if( title != NULL && X_IS_STRING( title ) )
      titleWidth = s_utf8Length( x_itemGetCPtr( title ), x_itemGetCLen( title ) );

   if( titleWidth > width )
      width = titleWidth;

   return IIF( width > 0, width, 1 );
}

/* Add a column; returns its number, 0 with an error raised */
int x_guiBrowseAddColumn( X_CALL_CTX *ctx, HWND browse, const X_ITEM *title, int width, int align )
{
   X_GUI_GRID *grid = s_gridOf( browse );
   wchar_t *text;

   if( grid == NULL || width <= 0 || align < -1 || align > 2 )
   {
      x_guiRaise( ctx, "GUI_BROWSE:ADDCOLUMN" );
      return 0;
   }

   text = x_guiText( ctx, title, "GUI_BROWSE:ADDCOLUMN" );
   if( text == NULL )
      return 0;

   s_gridCellsFree( grid );
   grid->columns = ( X_GUI_COLUMN * )x_xreallocArray( grid->columns, grid->columnCount + 1, sizeof( X_GUI_COLUMN ) );
   grid->columns[grid->columnCount++] = ( X_GUI_COLUMN ){ text, width, align, IIF( align < 0, 0, align ), 0 };
   grid->valid = 0;
   s_browseScrollBars( browse, grid );
   InvalidateRect( browse, NULL, FALSE );

   return ( int )grid->columnCount;
}

/* Return a color of a browse */
void x_guiBrowseColor( X_CALL_CTX *ctx, X_GUI_GRID_COLOR part )
{
   HWND browse = x_guiSelfHandle( ctx, "GUI_BROWSE:COLOR" );
   const X_GUI_GRID *grid = IIF( browse != NULL, s_gridOf( browse ), NULL );

   if( grid != NULL )
      x_retni( ctx, grid->colors[part] );
}

/* Set a color of a browse */
void x_guiBrowseSetColor( X_CALL_CTX *ctx, X_GUI_GRID_COLOR part )
{
   HWND browse = x_guiSelfHandle( ctx, "GUI_BROWSE:COLOR" );
   X_GUI_GRID *grid = IIF( browse != NULL, s_gridOf( browse ), NULL );

   if( grid == NULL )
      return;

   if( !X_ISNUM( ctx, 1 ) || x_parnint( ctx, 1 ) < 0 || x_parnint( ctx, 1 ) > 0xFFFFFF )
   {
      x_guiRaise( ctx, "GUI_BROWSE:COLOR" );
      return;
   }

   grid->colors[part] = ( int )x_parnint( ctx, 1 );
   InvalidateRect( browse, NULL, FALSE );
   x_itemReturn( ctx, x_paritem( ctx, 1 ) );
}

/* The column nColumn of logical values gets the opposite value of the current record through its block */
void x_guiBrowseToggle( X_CALL_CTX *ctx, const X_ITEM *columns, int number )
{
   X_ITEM value __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM block __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   const X_ITEM *column = NULL;
   X_ARG args[1];

   if( columns != NULL && X_IS_ARRAY( columns ) && number >= 1 && number <= ( int )x_arrayLen( columns ) )
      column = x_arrayGetItemPtr( columns, ( size_t )number );

   if( column == NULL || !X_IS_ARRAY( column ) || !x_arrayGet( column, 1, &block ) || !X_IS_BLOCK( &block ) )
      return;

   x_evalBlock( ctx, &value, &block, 0, NULL );
   if( !X_IS_LOGICAL( &value ) )
      return;

   x_itemPutL( &value, !value.u.logical );
   args[0] = ( X_ARG ){ &value, 0 };
   x_evalBlock( ctx, &result, &block, 1, args );
}

/* Return the number of frozen columns */
void x_guiBrowseFreeze( X_CALL_CTX *ctx )
{
   HWND browse = x_guiSelfHandle( ctx, "GUI_BROWSE:FREEZE" );
   const X_GUI_GRID *grid = IIF( browse != NULL, s_gridOf( browse ), NULL );

   if( grid != NULL )
      x_retni( ctx, grid->freeze );
}

/* Set the number of frozen columns, as TBrowse:freeze */
void x_guiBrowseSetFreeze( X_CALL_CTX *ctx )
{
   HWND browse = x_guiSelfHandle( ctx, "GUI_BROWSE:FREEZE" );
   X_GUI_GRID *grid = IIF( browse != NULL, s_gridOf( browse ), NULL );

   if( grid == NULL )
      return;

   if( !X_ISNUM( ctx, 1 ) || x_parni( ctx, 1 ) < 0 )
   {
      x_guiRaise( ctx, "GUI_BROWSE:FREEZE" );
      return;
   }

   grid->freeze = x_parni( ctx, 1 );
   grid->offset = 0;
   s_browseScrollBars( browse, grid );
   InvalidateRect( browse, NULL, FALSE );
   x_itemReturn( ctx, x_paritem( ctx, 1 ) );
}

/* Return the column of the cursor, from 1 */
void x_guiBrowseColPos( X_CALL_CTX *ctx )
{
   HWND browse = x_guiSelfHandle( ctx, "GUI_BROWSE:COLPOS" );
   const X_GUI_GRID *grid = IIF( browse != NULL, s_gridOf( browse ), NULL );

   if( grid != NULL )
      x_retni( ctx, grid->colPos + 1 );
}

/* Move the cursor to a column, as TBrowse:colPos */
void x_guiBrowseSetColPos( X_CALL_CTX *ctx )
{
   HWND browse = x_guiSelfHandle( ctx, "GUI_BROWSE:COLPOS" );
   X_GUI_GRID *grid = IIF( browse != NULL, s_gridOf( browse ), NULL );

   if( grid == NULL )
      return;

   if( !X_ISNUM( ctx, 1 ) || x_parni( ctx, 1 ) < 1 || ( size_t )x_parni( ctx, 1 ) > grid->columnCount )
   {
      x_guiRaise( ctx, "GUI_BROWSE:COLPOS" );
      return;
   }

   s_browseColumn( browse, grid, x_parni( ctx, 1 ) - 1 );
   x_itemReturn( ctx, x_paritem( ctx, 1 ) );
}

/* Read the rows again around the current record */
void x_guiBrowseRefresh( HWND browse )
{
   X_GUI_GRID *grid = s_gridOf( browse );

   if( grid == NULL )
      return;

   x_callbackEnterDll();
   s_browseRefill( browse, grid );
   x_callbackLeaveDll();
   x_callbackRaisePending();
}

/* The blocks of the browse for oBrowse:__GuiNavigate( nOperation, nArgument ) */
void x_guiBrowseNavigate( X_CALL_CTX *ctx, const X_ITEM *goTop, const X_ITEM *goBottom, const X_ITEM *skip,
                          const X_ITEM *position, const X_ITEM *empty )
{
   X_ITEM block __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM count __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   const X_ITEM *chosen;
   X_ARG args[1];
   double part;

   switch( x_parni( ctx, 1 ) )
   {
   case X_GUI_GO_TOP:
      chosen = goTop;
      break;
   case X_GUI_GO_BOTTOM:
      chosen = goBottom;
      break;
   case X_GUI_SKIP:
      chosen = skip;
      break;
   case X_GUI_POSITION:
      chosen = position;
      break;
   case X_GUI_EMPTY:
      chosen = empty;
      break;
   default:
      chosen = NULL;
   }

   x_retni( ctx, IIF( x_parni( ctx, 1 ) == X_GUI_POSITION, -1, 0 ) );
   if( chosen == NULL || !X_IS_BLOCK( chosen ) )
      return;

   x_itemCopy( &block, chosen );
   x_itemPutNInt( &count, x_parnint( ctx, 2 ) );
   args[0] = ( X_ARG ){ &count, 0 };
   x_evalBlock( ctx, &result, &block, IIF( x_parni( ctx, 1 ) == X_GUI_SKIP, 1, 0 ), args );
   if( x_parni( ctx, 1 ) == X_GUI_SKIP && X_IS_NUMERIC( &result ) )
      x_retnint( ctx, x_itemGetNInt( &result ) );
   else if( x_parni( ctx, 1 ) == X_GUI_POSITION && X_IS_NUMERIC( &result ) )
   {
      part = x_itemGetND( &result );
      if( part < 0 )
         part = 0;
      else if( part > 1 )
         part = 1;

      x_retni( ctx, ( int )( part * 1000 + 0.5 ) );
   }
   else if( x_parni( ctx, 1 ) == X_GUI_EMPTY && X_IS_LOGICAL( &result ) && result.u.logical )
      x_retni( ctx, 1 );
}

/* oBrowse:__GuiRow(): for every column { bBlock, cPicture } the text of its value and its type */
void x_guiBrowseRow( X_CALL_CTX *ctx, const X_ITEM *columns )
{
   X_ITEM value __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM shown __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_ITEM block __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   const X_ITEM *column;
   const char *picture, *text;
   size_t count = IIF( columns != NULL && X_IS_ARRAY( columns ), ( size_t )x_arrayLen( columns ), 0 );
   size_t i, length;
   char type[2] = "U";

   if( !x_arrayNew( ctx->result, ( int64_t )count * 2 ) )
      return;

   for( i = 1; i <= count; ++i )
   {
      column = x_arrayGetItemPtr( columns, i );
      x_itemClear( &value );
      x_itemClear( &shown );
      type[0] = 'U';
      text = "";
      length = 0;
      if( column != NULL && X_IS_ARRAY( column ) && x_arrayGet( column, 1, &block ) && X_IS_BLOCK( &block ) )
      {
         x_evalBlock( ctx, &value, &block, 0, NULL );
         if( X_IS_STRING( &value ) || X_IS_NUMERIC( &value ) || X_IS_DATE( &value ) || X_IS_LOGICAL( &value ) )
         {
            if( X_IS_STRING( &value ) )
               type[0] = 'C';
            else if( X_IS_NUMERIC( &value ) )
               type[0] = 'N';
            else if( X_IS_DATE( &value ) )
               type[0] = 'D';
            else
               type[0] = 'L';

            picture = x_arrayGetCPtr( column, 2 );
            picture = IIF( picture != NULL, picture, "" );
            x_transform( &shown, &value, picture, strlen( picture ) );
            text = x_itemGetCPtr( &shown );
            length = x_itemGetCLen( &shown );
            /* A number stands on the right without the spaces of its width, a text without the spaces
               of its field */
            while( type[0] == 'N' && length > 0 && *text == ' ' )
            {
               ++text;
               --length;
            }

            while( length > 0 && text[length - 1] == ' ' )
               --length;
         }
      }

      x_itemPutCL( x_arrayGetItemPtr( ctx->result, i * 2 - 1 ), text, length );
      x_itemPutC( x_arrayGetItemPtr( ctx->result, i * 2 ), type );
   }
}

/* ---------------------------------------------------------------------------------------------
   Controls
   --------------------------------------------------------------------------------------------- */

/* Register the common controls of Windows once */
static void s_initCommonControls( void )
{
   INITCOMMONCONTROLSEX controls;

   if( s_commonControls )
      return;

   controls.dwSize = sizeof( controls );
   controls.dwICC = ICC_BAR_CLASSES | ICC_PROGRESS_CLASS | ICC_DATE_CLASSES | ICC_TAB_CLASSES;
   controls.dwICC |= ICC_LISTVIEW_CLASSES | ICC_TREEVIEW_CLASSES | ICC_COOL_CLASSES;
   InitCommonControlsEx( &controls );
   s_commonControls = 1;
}

/* Create the control of a control object through its __GuiCreate() message */
static void s_controlSend( X_CALL_CTX *ctx, X_GUI_KIND kind, int textParam, int x, int y, int width, int height )
{
   X_ITEM values[7];
   X_ARG args[7];
   int i;

   for( i = 0; i < 7; ++i )
      values[i] = ( X_ITEM )X_ITEM_NIL_INIT;

   if( x_paritem( ctx, 1 ) != NULL )
      x_itemCopy( &values[0], x_paritem( ctx, 1 ) );

   x_itemPutNI( &values[1], ( int )kind );
   if( textParam != 0 && x_paritem( ctx, textParam ) != NULL && !X_ISNIL( ctx, textParam ) )
      x_itemCopy( &values[2], x_paritem( ctx, textParam ) );
   else
      x_itemPutC( &values[2], "" );

   x_itemPutNI( &values[3], x );
   x_itemPutNI( &values[4], y );
   x_itemPutNI( &values[5], width );
   x_itemPutNI( &values[6], height );
   for( i = 0; i < 7; ++i )
      args[i] = ( X_ARG ){ &values[i], 0 };

   x_objSend( ctx, ctx->result, ctx->self, "__GUICREATE", 7, args );
   for( i = 0; i < 7; ++i )
      x_itemClear( &values[i] );
}

/* New() of a control: parent, x, y, width and height, the text as the sixth parameter when it has one */
void x_guiControlNew( X_CALL_CTX *ctx, X_GUI_KIND kind, int width, int height, int hasText )
{
   int textParam = IIF( hasText, 6, 0 );
   int x = x_parnidef( ctx, 2, 0 );
   int y = x_parnidef( ctx, 3, 0 );

   s_controlSend( ctx, kind, textParam, x, y, x_parnidef( ctx, 4, width ), x_parnidef( ctx, 5, height ) );
}

/* A GET: with @var as the sixth parameter the field shows the variable and ReadModal() gives the value back to it,
   as a GET does; the variable is a LOCAL of the function that runs ReadModal() */
void x_guiGetNew( X_CALL_CTX *ctx )
{
   const int byRef = X_ISBYREF( ctx, 6 );
   const X_ITEM *var = x_paritem( ctx, 6 );
   const int rows = IIF( X_ISNUM( ctx, 8 ) && x_parni( ctx, 8 ) > 1, x_parni( ctx, 8 ), 1 );
   int length = 0;
   TEXTMETRICW metrics;
   HWND edit;
   HDC dc;
   HGDIOBJ old;
   UINT dpi;
   int chars, margin, edge, width, height;

   /* nMax characters, otherwise the length of the variable */
   if( X_ISNUM( ctx, 9 ) && x_parni( ctx, 9 ) > 0 )
      length = x_parni( ctx, 9 );
   else if( var != NULL && X_IS_STRING( var ) )
      length = ( int )x_itemGetCLen( var );

   /* With nRows the field has one row and grows to nRows while it has the focus, see s_getFit() */
   x_guiControlNew( ctx, IIF( rows > 1, X_GUI_GETMEMO, X_GUI_EDIT ), 200, 23, !byRef );
   edit = s_objectHandle( ctx, ctx->self );
   if( edit == NULL )
      return;

   dpi = GetDpiForWindow( edit );
   margin = x_guiScale( 2, dpi );
   edge = GetSystemMetricsForDpi( SM_CXEDGE, dpi );
   SetPropW( edit, s_getProperty, ( HANDLE )1 );
   if( rows > 1 )
      SetPropW( edit, s_rowsProperty, ( HANDLE )( INT_PTR )rows );

   s_xfontAttach( edit );
   SendMessageW( edit, WM_SETFONT, ( WPARAM )s_fontOf( edit, dpi ), FALSE );
   SendMessageW( edit, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM( margin, margin ) );
   if( byRef )
   {
      SetPropW( edit, s_varProperty, ( HANDLE )var );
      if( var != NULL && !X_IS_NIL( var ) )
         x_guiEditSetValue( ctx, edit, var );
   }

   /* PICTURE; without it no more characters than the variable has, as a GET */
   if( X_ISCHAR( ctx, 7 ) )
      x_guiEditSetPicture( ctx, edit, x_paritem( ctx, 7 ) );
   else if( length > 0 )
      SendMessageW( edit, EM_LIMITTEXT, ( WPARAM )length, 0 );

   /* nLen characters are seen, without it the whole variable; a longer value scrolls in the field */
   if( X_ISNUM( ctx, 4 ) )
      chars = x_parni( ctx, 4 );
   else
      chars = IIF( length > 0, length, GetWindowTextLengthW( edit ) );

   dc = GetDC( edit );
   old = SelectObject( dc, s_fontOf( edit, dpi ) );
   GetTextMetricsW( dc, &metrics );
   SelectObject( dc, old );
   ReleaseDC( edit, dc );
   width = IIF( chars > 0, chars, 10 ) * metrics.tmAveCharWidth + 2 * ( margin + edge ) + 1;
   height = IIF( X_ISNUM( ctx, 5 ), x_guiScale( x_parni( ctx, 5 ), dpi ), metrics.tmHeight + 2 * edge + margin );
   SetWindowPos( edit, NULL, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE );
   x_guiReturnSelf( ctx );
}

/* A SAY: nLen characters of the fixed font 9x18 wide as a GET of nLen characters, so a SAY and a GET from the
   same column end in the same place; without nLen as long as the text */
void x_guiSayNew( X_CALL_CTX *ctx )
{
   X_GUI_OWNFONT own;
   TEXTMETRICW metrics;
   HWND label;
   HFONT font;
   HDC dc;
   HGDIOBJ old;
   UINT dpi;
   int chars, margin, edge, width, height;

   x_guiControlNew( ctx, X_GUI_LABEL, 200, 20, 1 );
   label = s_objectHandle( ctx, ctx->self );
   if( label == NULL )
      return;

   dpi = GetDpiForWindow( label );
   margin = x_guiScale( 2, dpi );
   edge = GetSystemMetricsForDpi( SM_CXEDGE, dpi );
   memset( &own, 0, sizeof( own ) );
   lstrcpynW( own.name, s_xfontName, LF_FACESIZE );
   font = s_ownFontCreate( &own, dpi );
   dc = GetDC( label );
   old = SelectObject( dc, font );
   GetTextMetricsW( dc, &metrics );
   SelectObject( dc, old );
   ReleaseDC( label, dc );
   DeleteObject( font );
   chars = IIF( X_ISNUM( ctx, 4 ), x_parni( ctx, 4 ), GetWindowTextLengthW( label ) );
   width = IIF( chars > 0, chars, 10 ) * metrics.tmAveCharWidth + 2 * ( margin + edge ) + 1;
   height = IIF( X_ISNUM( ctx, 5 ), x_guiScale( x_parni( ctx, 5 ), dpi ), metrics.tmHeight + 2 * edge + margin );
   SetWindowPos( label, NULL, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE );
   x_guiReturnSelf( ctx );
}

/* gui_StatusBar():New(): a status bar at the bottom of the window */
void x_guiStatusBarNew( X_CALL_CTX *ctx )
{
   s_controlSend( ctx, X_GUI_STATUSBAR, 2, 0, 0, 0, 0 );
}

/* The text of a panel with its border; NULL keeps the text the panel has */
static void s_statusSetText( HWND status, int index, int style, const wchar_t *text )
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
   X_GUI_STATUS *parts = s_statusOf( status );
   wchar_t *label = NULL;

   if( width < 0 || ( parts != NULL && parts->count >= 255 ) )
   {
      x_guiRaise( ctx, "GUI_STATUSBAR:ADD" );
      return 0;
   }

   if( text != NULL && !X_IS_NIL( text ) )
   {
      label = x_guiText( ctx, text, "GUI_STATUSBAR:ADD" );
      if( label == NULL )
         return 0;
   }

   parts = s_statusParts( status );
   parts->widths = ( int * )x_xrealloc( parts->widths, ( parts->count + 1 ) * sizeof( int ) );
   parts->styles = ( int * )x_xrealloc( parts->styles, ( parts->count + 1 ) * sizeof( int ) );
   parts->widths[parts->count] = width;
   parts->styles[parts->count] = IIF( raised, SBT_POPOUT, 0 );
   ++parts->count;
   x_callbackEnterDll();
   s_statusLayout( status );
   s_statusSetText( status, ( int )parts->count - 1, parts->styles[parts->count - 1], label );
   x_callbackLeaveDll();
   x_xfree( label );
   x_callbackRaisePending();

   return ( int )parts->count;
}

/* Set the text of a panel, counted from 1 */
void x_guiStatusBarSetText( X_CALL_CTX *ctx, HWND status, int panel, const X_ITEM *text )
{
   const X_GUI_STATUS *parts = s_statusOf( status );
   int own = parts != NULL && parts->count > 0;
   size_t panels = IIF( own, parts->count, 1 );
   wchar_t *label;
   int style;

   if( panel < 1 || ( size_t )panel > panels )
   {
      x_guiRaise( ctx, "GUI_STATUSBAR:SETTEXT" );
      return;
   }

   label = x_guiText( ctx, text, "GUI_STATUSBAR:SETTEXT" );
   if( label == NULL )
      return;

   style = IIF( own, parts->styles[panel - 1], 0 );
   x_callbackEnterDll();
   s_statusSetText( status, panel - 1, style, label );
   x_callbackLeaveDll();
   x_xfree( label );
   x_callbackRaisePending();
}

/* __GuiCreate(): create the window of a control object in its parent */
void x_guiControlCreate( X_CALL_CTX *ctx )
{
   X_ITEM *handleSlot = x_objDataSlot( ctx, 1 );
   X_ITEM *parentSlot = x_objDataSlot( ctx, 2 );
   X_ITEM *parentItem = ( X_ITEM * )x_paritem( ctx, 1 );
   const int kind = x_parni( ctx, 2 );
   const wchar_t *className = L"STATIC";
   DWORD style = WS_CHILD | WS_VISIBLE;
   DWORD exStyle = 0;
   wchar_t *text;
   HWND parent, control;
   const DWORD listStyle = LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES;
   HINSTANCE instance = GetModuleHandleW( NULL );
   UINT dpi;
   DWORD error;
   HMENU id;
   int valid, x, y, cx, cy;

   valid = handleSlot != NULL && parentSlot != NULL && parentItem != NULL && x_objIsKindOf( parentItem, "GUI_WINDOW" );
   parent = IIF( valid, s_objectHandle( ctx, parentItem ), NULL );
   valid = parent != NULL && IsWindow( parent ) && x_parni( ctx, 6 ) >= 0 && x_parni( ctx, 7 ) >= 0;
   if( !valid )
   {
      x_guiRaise( ctx, "GUI_CONTROL:NEW" );
      return;
   }

   switch( kind )
   {
   case X_GUI_LABEL:
      style |= SS_LEFT;
      break;
   /* Edit fields do not paint over a GET of several rows that grows over them */
   case X_GUI_EDIT:
      className = L"EDIT";
      style |= WS_TABSTOP | WS_CLIPSIBLINGS | ES_AUTOHSCROLL;
      exStyle = WS_EX_CLIENTEDGE;
      break;
   case X_GUI_MEMO:
      className = L"EDIT";
      style |= WS_TABSTOP | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN;
      exStyle = WS_EX_CLIENTEDGE;
      break;
   /* A GET of several rows: the text wraps, without a scroll bar; Enter goes to the next field */
   case X_GUI_GETMEMO:
      className = L"EDIT";
      style |= WS_TABSTOP | WS_CLIPSIBLINGS | ES_MULTILINE | ES_AUTOVSCROLL;
      exStyle = WS_EX_CLIENTEDGE;
      break;
   case X_GUI_BUTTON:
      className = L"BUTTON";
      style |= WS_TABSTOP | BS_PUSHBUTTON;
      break;
   case X_GUI_CHECKBOX:
      className = L"BUTTON";
      style |= WS_TABSTOP | BS_AUTOCHECKBOX;
      break;
   case X_GUI_RADIO:
      className = L"BUTTON";
      style |= WS_TABSTOP | BS_AUTORADIOBUTTON;
      break;
   /* A frame does not paint over the controls lying in it */
   case X_GUI_GROUPBOX:
      className = L"BUTTON";
      style |= BS_GROUPBOX | WS_CLIPSIBLINGS;
      break;
   case X_GUI_LISTBOX:
      className = L"LISTBOX";
      style |= WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT;
      exStyle = WS_EX_CLIENTEDGE;
      break;
   case X_GUI_COMBOBOX:
      className = L"COMBOBOX";
      style |= WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST;
      break;
   case X_GUI_PROGRESSBAR:
      s_initCommonControls();
      className = PROGRESS_CLASSW;
      break;
   case X_GUI_STATUSBAR:
      s_initCommonControls();
      className = STATUSCLASSNAMEW;
      style |= SBARS_SIZEGRIP | CCS_BOTTOM;
      break;
   case X_GUI_DATEPICKER:
      s_initCommonControls();
      className = DATETIMEPICK_CLASSW;
      style |= WS_TABSTOP | DTS_SHORTDATEFORMAT;
      break;
   case X_GUI_SLIDER:
      s_initCommonControls();
      className = TRACKBAR_CLASSW;
      style |= WS_TABSTOP | TBS_HORZ;
      break;
   case X_GUI_TABCONTROL:
      s_initCommonControls();
      className = WC_TABCONTROLW;
      style |= WS_TABSTOP | WS_CLIPSIBLINGS;
      exStyle = WS_EX_CONTROLPARENT;
      break;
   case X_GUI_LISTVIEW:
      s_initCommonControls();
      className = WC_LISTVIEWW;
      style |= WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS;
      exStyle = WS_EX_CLIENTEDGE;
      break;
   case X_GUI_TREEVIEW:
      s_initCommonControls();
      className = WC_TREEVIEWW;
      style |= WS_TABSTOP | TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT | TVS_SHOWSELALWAYS;
      exStyle = WS_EX_CLIENTEDGE;
      break;
   case X_GUI_TOOLBAR:
      s_initCommonControls();
      className = TOOLBARCLASSNAMEW;
      style |= TBSTYLE_FLAT | TBSTYLE_LIST | TBSTYLE_TOOLTIPS | WS_CLIPSIBLINGS;
      style |= CCS_NOPARENTALIGN | CCS_NORESIZE | CCS_NODIVIDER;
      break;
   case X_GUI_BROWSE:
      if( !s_registerBrowseClass( instance ) )
      {
         x_guiRaiseWin32( ctx, "RegisterClassExW", GetLastError() );
         return;
      }

      className = s_browseClassName;
      style |= WS_TABSTOP | WS_VSCROLL | WS_HSCROLL;
      exStyle = WS_EX_CLIENTEDGE;
      break;
   default:
      x_guiRaise( ctx, "GUI_CONTROL:NEW" );
      return;
   }

   text = x_guiText( ctx, x_paritem( ctx, 3 ), "GUI_CONTROL:NEW" );
   if( text == NULL )
      return;

   if( kind == X_GUI_MEMO )
      text = s_crlf( text );

   if( kind != X_GUI_RADIO || !s_isRadio( GetWindow( GetWindow( parent, GW_CHILD ), GW_HWNDLAST ) ) )
      style |= WS_GROUP;

   dpi = GetDpiForWindow( parent );
   x_callbackEnterDll();
   x = x_guiScale( x_parni( ctx, 4 ), dpi );
   y = x_guiScale( x_parni( ctx, 5 ), dpi );
   cx = x_guiScale( x_parni( ctx, 6 ), dpi );
   cy = x_guiScale( x_parni( ctx, 7 ), dpi );
   id = ( HMENU )( INT_PTR )s_controlId;
   control = CreateWindowExW( exStyle, className, text, style, x, y, cx, cy, parent, id, instance, NULL );
   error = GetLastError();
   s_controlId = IIF( s_controlId >= X_GUI_CONTROL_LAST, X_GUI_CONTROL_FIRST, s_controlId + 1 );
   if( control != NULL )
   {
      SetWindowLongPtrW( control, GWLP_USERDATA, ( LONG_PTR )x_itemNew( ctx->self ) );
      SendMessageW( control, WM_SETFONT, ( WPARAM )x_guiFont( dpi ), FALSE );
      if( kind == X_GUI_LISTVIEW )
         SendMessageW( control, LVM_SETEXTENDEDLISTVIEWSTYLE, listStyle, listStyle );
      else if( kind == X_GUI_TOOLBAR )
         s_toolbarInit( control );
      else if( kind == X_GUI_EDIT || kind == X_GUI_GETMEMO )
         SetWindowSubclass( control, s_editProc, 1, 0 );
      else if( kind == X_GUI_STATUSBAR )
         SetWindowSubclass( control, s_statusProc, 1, 0 );
      else if( kind == X_GUI_BROWSE )
         s_xfontAttach( control );
      else if( kind == X_GUI_GROUPBOX || kind == X_GUI_CHECKBOX || kind == X_GUI_RADIO )
      {
         if( kind == X_GUI_GROUPBOX )
            SetWindowSubclass( control, s_groupProc, 1, 0 );

         s_paintTheme( control );
      }
   }

   x_callbackLeaveDll();
   x_xfree( text );
   x_callbackRaisePending();

   if( control == NULL )
   {
      x_guiRaiseWin32( ctx, "CreateWindowExW", error );
      return;
   }

   x_itemPutPtr( handleSlot, control );
   x_itemCopy( parentSlot, parentItem );
   x_guiReturnSelf( ctx );
}

/* Set the text of a window or a control; an edit field of several lines gets CR LF line ends */
void x_guiSetText( X_CALL_CTX *ctx, HWND window, const X_ITEM *value )
{
   wchar_t *text = x_guiText( ctx, value, "GUI_CONTROL:TEXT" );
   BOOL done;
   DWORD error;

   if( text == NULL )
      return;

   if( s_isMultiline( window ) )
      text = s_crlf( text );

   x_callbackEnterDll();
   done = SetWindowTextW( window, text );
   error = GetLastError();
   x_callbackLeaveDll();
   x_xfree( text );
   x_callbackRaisePending();
   if( !done )
      x_guiRaiseWin32( ctx, "SetWindowTextW", error );
}

/* A data of a gui_Font object */
static int s_fontValue( X_CALL_CTX *ctx, const X_ITEM *font, const char *name, X_ITEM *value )
{
   x_itemClear( value );

   return x_objSend( ctx, value, ( X_ITEM * )font, name, 0, NULL );
}

/* A gui_Font gives the control its own font, NIL returns it to the system font */
void x_guiSetFont( X_CALL_CTX *ctx, HWND window, const X_ITEM *value )
{
   X_ITEM item __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   X_GUI_OWNFONT *own = NULL;
   X_GUI_GRID *grid = s_gridOf( window );
   wchar_t *name;

   if( value != NULL && !X_IS_NIL( value ) && !x_objIsKindOf( value, "GUI_FONT" ) )
   {
      x_guiRaise( ctx, "GUI_CONTROL:FONT" );
      return;
   }

   if( value != NULL && !X_IS_NIL( value ) )
   {
      own = ( X_GUI_OWNFONT * )x_xgrab( sizeof( X_GUI_OWNFONT ) );
      memset( own, 0, sizeof( *own ) );
      if( s_fontValue( ctx, value, "SIZE", &item ) && X_IS_NUMERIC( &item ) && x_itemGetNI( &item ) > 0 )
         own->size = x_itemGetNI( &item );

      if( s_fontValue( ctx, value, "BOLD", &item ) && X_IS_LOGICAL( &item ) )
         own->bold = item.u.logical;

      if( s_fontValue( ctx, value, "ITALIC", &item ) && X_IS_LOGICAL( &item ) )
         own->italic = item.u.logical;

      if( s_fontValue( ctx, value, "NAME", &item ) && X_IS_STRING( &item ) && x_itemGetCLen( &item ) > 0 )
      {
         name = x_guiText( ctx, &item, "GUI_CONTROL:FONT" );
         if( name != NULL )
         {
            lstrcpynW( own->name, name, LF_FACESIZE );
            x_xfree( name );
         }
      }
   }

   x_callbackEnterDll();
   s_fontRelease( window );
   if( own != NULL )
      SetPropW( window, s_fontProperty, own );

   SendMessageW( window, WM_SETFONT, ( WPARAM )s_fontOf( window, GetDpiForWindow( window ) ), TRUE );
   if( grid != NULL )
      grid->valid = 0;

   InvalidateRect( window, NULL, TRUE );
   x_callbackLeaveDll();
   x_callbackRaisePending();
}

/* An edit field of a combo box holds the focus for it, so the focus may be in a child of the control */
int x_guiFocused( HWND window )
{
   HWND focus = GetFocus();

   return focus != NULL && ( focus == window || IsChild( window, focus ) );
}

/* Show or hide a control */
void x_guiSetVisible( HWND window, int visible )
{
   x_guiShow( window, visible );
}

/* Enable or disable a control */
void x_guiSetEnabled( HWND window, int enabled )
{
   x_callbackEnterDll();
   EnableWindow( window, enabled );
   x_callbackLeaveDll();
   x_callbackRaisePending();
}

/* Give a control the focus */
void x_guiSetFocus( HWND window )
{
   x_callbackEnterDll();
   SetFocus( window );
   x_callbackLeaveDll();
   x_callbackRaisePending();
}

/* The anchored sides of a control and its distances from the sides of its parent */
static void s_anchorMeasure( HWND window, X_GUI_ANCHOR *anchor, int flags )
{
   RECT frame, client;
   int right, bottom;

   s_childFrame( window, &frame );
   GetClientRect( GetParent( window ), &client );
   right = client.right - frame.right;
   bottom = client.bottom - frame.bottom;
   *anchor = ( X_GUI_ANCHOR ){ flags, frame.left, frame.top, right, bottom };
}

/* Return the anchored sides as "LTRB" letters */
void x_guiGetAnchor( X_CALL_CTX *ctx, HWND window )
{
   const X_GUI_ANCHOR *anchor = ( const X_GUI_ANCHOR * )GetPropW( window, s_anchorProperty );
   char text[5];
   int length = 0;

   if( anchor == NULL )
   {
      x_retc( ctx, "LT" );
      return;
   }

   if( anchor->flags & X_GUI_ANCHOR_LEFT )
      text[length++] = 'L';

   if( anchor->flags & X_GUI_ANCHOR_TOP )
      text[length++] = 'T';

   if( anchor->flags & X_GUI_ANCHOR_RIGHT )
      text[length++] = 'R';

   if( anchor->flags & X_GUI_ANCHOR_BOTTOM )
      text[length++] = 'B';

   text[length] = '\0';
   x_retc( ctx, text );
}

/* Anchor a control to sides of its parent given as "LTRB" letters */
void x_guiSetAnchor( X_CALL_CTX *ctx, HWND window, const X_ITEM *value )
{
   const char *spec;
   size_t length = 0;
   void *handle = NULL;
   int flags = 0;
   X_GUI_ANCHOR *anchor;
   size_t i;

   spec = IIF( value != NULL && X_IS_STRING( value ), x_itemGetStrUTF8( value, &handle, &length ), NULL );
   for( i = 0; spec != NULL && i < length; ++i )
   {
      switch( spec[i] )
      {
      case 'L': case 'l': flags |= X_GUI_ANCHOR_LEFT; break;
      case 'T': case 't': flags |= X_GUI_ANCHOR_TOP; break;
      case 'R': case 'r': flags |= X_GUI_ANCHOR_RIGHT; break;
      case 'B': case 'b': flags |= X_GUI_ANCHOR_BOTTOM; break;
      default: spec = NULL; break;
      }
   }

   x_strfree( handle );
   if( spec == NULL )
   {
      x_guiRaise( ctx, "GUI_CONTROL:ANCHOR" );
      return;
   }

   anchor = ( X_GUI_ANCHOR * )GetPropW( window, s_anchorProperty );
   if( anchor == NULL )
   {
      anchor = ( X_GUI_ANCHOR * )x_xgrab( sizeof( *anchor ) );
      if( !SetPropW( window, s_anchorProperty, anchor ) )
      {
         x_xfree( anchor );
         x_guiRaiseWin32( ctx, "SetPropW", GetLastError() );
         return;
      }
   }

   s_anchorMeasure( window, anchor, flags );
}

/* The position or the size of a control at 96 DPI */
int x_guiBounds( HWND window, X_GUI_BOUND part )
{
   RECT frame;
   int value;

   s_childFrame( window, &frame );
   switch( part )
   {
   case X_GUI_X: value = frame.left; break;
   case X_GUI_Y: value = frame.top; break;
   case X_GUI_WIDTH: value = frame.right - frame.left; break;
   default: value = frame.bottom - frame.top; break;
   }

   return MulDiv( value, USER_DEFAULT_SCREEN_DPI, ( int )GetDpiForWindow( window ) );
}

/* Set the position or the size of a control, given at 96 DPI; its anchor follows */
void x_guiSetBounds( X_CALL_CTX *ctx, HWND window, X_GUI_BOUND part, int value )
{
   const int scaled = x_guiScale( value, GetDpiForWindow( window ) );
   X_GUI_ANCHOR *anchor = ( X_GUI_ANCHOR * )GetPropW( window, s_anchorProperty );
   RECT frame;
   int x, y, width, height;

   if( value < 0 && ( part == X_GUI_WIDTH || part == X_GUI_HEIGHT ) )
   {
      x_guiRaise( ctx, "GUI_CONTROL:SIZE" );
      return;
   }

   s_childFrame( window, &frame );
   x = IIF( part == X_GUI_X, scaled, frame.left );
   y = IIF( part == X_GUI_Y, scaled, frame.top );
   width = IIF( part == X_GUI_WIDTH, scaled, frame.right - frame.left );
   height = IIF( part == X_GUI_HEIGHT, scaled, frame.bottom - frame.top );
   x_callbackEnterDll();
   SetWindowPos( window, NULL, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE );
   if( s_hasClass( window, WC_TABCONTROLW ) )
      s_tabLayout( window, 0 );

   x_callbackLeaveDll();
   if( anchor != NULL )
      s_anchorMeasure( window, anchor, anchor->flags );

   x_callbackRaisePending();
}

/* Send a message to a control between x_callbackEnterDll() and x_callbackLeaveDll() */
LRESULT x_guiSend( HWND window, UINT message, WPARAM wParam, LPARAM lParam )
{
   LRESULT result;

   x_callbackEnterDll();
   result = SendMessageW( window, message, wParam, lParam );
   x_callbackLeaveDll();
   x_callbackRaisePending();

   return result;
}

/* Set the tool tip of a control; the window keeps one tool tip control for all */
void x_guiSetToolTip( X_CALL_CTX *ctx, HWND control, const X_ITEM *value )
{
   HWND root = GetAncestor( control, GA_ROOT );
   HWND tip = ( HWND )GetPropW( root, s_toolTipProperty );
   wchar_t *text = x_guiText( ctx, value, "GUI_CONTROL:TOOLTIP" );
   TTTOOLINFOW info;

   if( text == NULL )
      return;

   x_callbackEnterDll();
   if( tip == NULL )
   {
      DWORD exStyle = WS_EX_TOPMOST;
      DWORD style = WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX;
      HINSTANCE instance = GetModuleHandleW( NULL );
      int pos = CW_USEDEFAULT;

      s_initCommonControls();
      tip = CreateWindowExW( exStyle, TOOLTIPS_CLASSW, NULL, style, pos, pos, pos, pos, root, NULL, instance, NULL );
      if( tip != NULL )
         SetPropW( root, s_toolTipProperty, tip );
   }

   if( tip != NULL )
   {
      memset( &info, 0, sizeof( info ) );
      info.cbSize = sizeof( info );
      info.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
      info.hwnd = root;
      info.uId = ( UINT_PTR )control;
      info.lpszText = text;
      SendMessageW( tip, TTM_DELTOOLW, 0, ( LPARAM )&info );
      if( text[0] != L'\0' )
         SendMessageW( tip, TTM_ADDTOOLW, 0, ( LPARAM )&info );
   }

   x_callbackLeaveDll();
   x_xfree( text );
   x_callbackRaisePending();
   if( tip == NULL )
      x_guiRaiseWin32( ctx, "CreateWindowExW", GetLastError() );
}

/* Whether an edit field is read-only */
int x_guiReadOnly( HWND window )
{
   return ( GetWindowLongPtrW( window, GWL_STYLE ) & ES_READONLY ) != 0;
}

/* Make an edit field read-only or editable */
void x_guiSetReadOnly( HWND window, int readOnly )
{
   x_guiSend( window, EM_SETREADONLY, ( WPARAM )IIF( readOnly, TRUE, FALSE ), 0 );
}

/* The most characters an edit field takes */
int x_guiMaxLength( HWND window )
{
   return ( int )SendMessageW( window, EM_GETLIMITTEXT, 0, 0 );
}

/* Set the most characters an edit field takes */
void x_guiSetMaxLength( HWND window, int length )
{
   x_guiSend( window, EM_SETLIMITTEXT, ( WPARAM )length, 0 );
}

/* Whether a check box or a radio button is checked */
int x_guiGetCheck( HWND window )
{
   return x_guiSend( window, BM_GETCHECK, 0, 0 ) == BST_CHECKED;
}

/* Check or clear a check box or a radio button; a checked radio button clears the others of its group */
void x_guiSetCheck( HWND window, int checked )
{
   HWND parent = GetParent( window );
   HWND sibling;

   if( checked && s_isRadio( window ) )
   {
      sibling = GetNextDlgGroupItem( parent, window, FALSE );
      while( sibling != NULL && sibling != window )
      {
         if( s_isRadio( sibling ) )
            x_guiSend( sibling, BM_SETCHECK, BST_UNCHECKED, 0 );

         sibling = GetNextDlgGroupItem( parent, sibling, FALSE );
      }
   }

   x_guiSend( window, BM_SETCHECK, IIF( checked, BST_CHECKED, BST_UNCHECKED ), 0 );
}

/* Whether a button is the default or, with cancel, the cancel button of its window */
int x_guiButtonRole( HWND button, int cancel )
{
   return ( HWND )GetPropW( GetParent( button ), IIF( cancel, s_cancelProperty, s_defaultProperty ) ) == button;
}

/* Make a button the default or the cancel button of its window, or take the role away */
void x_guiSetButtonRole( HWND button, int cancel, int enabled )
{
   const wchar_t *property = IIF( cancel, s_cancelProperty, s_defaultProperty );
   HWND parent = GetParent( button );
   HWND previous = ( HWND )GetPropW( parent, property );

   if( !cancel && previous != NULL && IsWindow( previous ) && ( previous != button || !enabled ) )
      x_guiSend( previous, BM_SETSTYLE, BS_PUSHBUTTON, TRUE );

   if( enabled )
      SetPropW( parent, property, button );
   else if( previous == button )
      RemovePropW( parent, property );

   if( !cancel && enabled )
      x_guiSend( button, BM_SETSTYLE, BS_DEFPUSHBUTTON, TRUE );
}

/* ---------------------------------------------------------------------------------------------
   Lists
   --------------------------------------------------------------------------------------------- */

/* The messages of a combo box or of a list box */
static const X_GUI_LIST_MESSAGES *s_listMessages( HWND window )
{
   return IIF( s_hasClass( window, L"ComboBox" ), &s_comboBoxMessages, &s_listBoxMessages );
}

/* Add an item to a list; returns its number, 0 with an error raised */
int x_guiListAdd( X_CALL_CTX *ctx, HWND window, const X_ITEM *text )
{
   wchar_t *wide = x_guiText( ctx, text, "GUI_LISTBOX:ADD" );
   LRESULT index;

   if( wide == NULL )
      return 0;

   index = x_guiSend( window, s_listMessages( window )->add, 0, ( LPARAM )wide );
   x_xfree( wide );
   if( index < 0 )
   {
      x_guiRaiseWin32( ctx, "GUI_LISTBOX:ADD", ERROR_NOT_ENOUGH_MEMORY );
      return 0;
   }

   return ( int )index + 1;
}

/* Remove all items of a list */
void x_guiListClear( HWND window )
{
   x_guiSend( window, s_listMessages( window )->reset, 0, 0 );
}

/* The number of items of a list */
int x_guiListCount( HWND window )
{
   return ( int )SendMessageW( window, s_listMessages( window )->count, 0, 0 );
}

/* The selected item of a list, 0 for none */
int x_guiListSelected( HWND window )
{
   return ( int )SendMessageW( window, s_listMessages( window )->getSelection, 0, 0 ) + 1;
}

/* Select an item of a list, 0 for none */
void x_guiListSetSelected( X_CALL_CTX *ctx, HWND window, int index )
{
   if( index < 0 || index > x_guiListCount( window ) )
      x_guiRaise( ctx, "GUI_LISTBOX:SELECTED" );
   else
      x_guiSend( window, s_listMessages( window )->setSelection, ( WPARAM )( index - 1 ), 0 );
}

/* Return the text of an item of a list */
void x_guiListItem( X_CALL_CTX *ctx, HWND window, int index )
{
   const X_GUI_LIST_MESSAGES *messages = s_listMessages( window );
   LRESULT length;
   wchar_t *wide;
   size_t size;
   char *text;

   if( index < 1 || index > x_guiListCount( window ) )
   {
      x_guiRaise( ctx, "GUI_LISTBOX:ITEM" );
      return;
   }

   length = SendMessageW( window, messages->textLength, ( WPARAM )( index - 1 ), 0 );
   wide = ( wchar_t * )x_xgrabArray( ( size_t )IIF( length < 0, 0, length ) + 1, sizeof( wchar_t ) );
   length = SendMessageW( window, messages->text, ( WPARAM )( index - 1 ), ( LPARAM )wide );
   text = x_utf16ToUtf8Alloc( ( const uint16_t * )wide, ( size_t )IIF( length < 0, 0, length ), &size );
   x_xfree( wide );
   x_retclen_buffer( ctx, text, size );
}

/* ---------------------------------------------------------------------------------------------
   Progress bars
   --------------------------------------------------------------------------------------------- */

/* The position of a progress bar */
int x_guiProgressValue( HWND window )
{
   return ( int )SendMessageW( window, PBM_GETPOS, 0, 0 );
}

/* Set the position of a progress bar */
void x_guiProgressSetValue( HWND window, int value )
{
   x_guiSend( window, PBM_SETPOS, ( WPARAM )value, 0 );
}

/* The maximum of a progress bar */
int x_guiProgressMaximum( HWND window )
{
   return ( int )SendMessageW( window, PBM_GETRANGE, FALSE, 0 );
}

/* Set the maximum of a progress bar */
void x_guiProgressSetMaximum( HWND window, int maximum )
{
   x_guiSend( window, PBM_SETRANGE32, 0, ( LPARAM )maximum );
}

/* ---------------------------------------------------------------------------------------------
   Date pickers and sliders
   --------------------------------------------------------------------------------------------- */

/* The date of a date picker as a Julian day, 0 for none */
long x_guiDateValue( HWND window )
{
   SYSTEMTIME time;

   if( SendMessageW( window, DTM_GETSYSTEMTIME, 0, ( LPARAM )&time ) != GDT_VALID )
      return 0;

   return x_dateEncode( time.wYear, time.wMonth, time.wDay );
}

/* Set the date of a date picker from a Julian day */
int x_guiDateSetValue( HWND window, long julian )
{
   SYSTEMTIME time;
   int year, month, day;

   if( julian <= 0 )
      return 0;

   x_dateDecode( julian, &year, &month, &day );
   memset( &time, 0, sizeof( time ) );
   time.wYear = ( WORD )year;
   time.wMonth = ( WORD )month;
   time.wDay = ( WORD )day;

   return x_guiSend( window, DTM_SETSYSTEMTIME, GDT_VALID, ( LPARAM )&time ) != 0;
}

/* The position, the minimum or the maximum of a slider */
int x_guiSliderGet( HWND window, X_GUI_RANGE part )
{
   static const UINT messages[] = { TBM_GETPOS, TBM_GETRANGEMIN, TBM_GETRANGEMAX };

   return ( int )SendMessageW( window, messages[part], 0, 0 );
}

/* Set the position, the minimum or the maximum of a slider */
void x_guiSliderSet( HWND window, X_GUI_RANGE part, int value )
{
   static const UINT messages[] = { TBM_SETPOS, TBM_SETRANGEMIN, TBM_SETRANGEMAX };

   x_guiSend( window, messages[part], TRUE, ( LPARAM )value );
}

/* ---------------------------------------------------------------------------------------------
   Tab controls
   --------------------------------------------------------------------------------------------- */

/* Add a page to a tab control and create its window object */
void x_guiTabAddPage( X_CALL_CTX *ctx, HWND tab, const X_ITEM *title, X_ITEM *page )
{
   const int count = ( int )SendMessageW( tab, TCM_GETITEMCOUNT, 0, 0 );
   wchar_t *text = x_guiText( ctx, title, "GUI_TABCONTROL:ADDPAGE" );
   TCITEMW item;
   LRESULT index;

   if( text == NULL )
      return;

   memset( &item, 0, sizeof( item ) );
   item.mask = TCIF_TEXT;
   item.pszText = text;
   index = x_guiSend( tab, TCM_INSERTITEMW, ( WPARAM )count, ( LPARAM )&item );
   x_xfree( text );
   if( index < 0 )
   {
      x_guiRaiseWin32( ctx, "TCM_INSERTITEMW", ERROR_NOT_ENOUGH_MEMORY );
      return;
   }

   s_windowSend( ctx, page, ctx->self, title, 10, 10, X_GUI_WINDOW_PAGE, count + 1 );
   x_guiTabRefresh( tab );
}

/* Place the pages again and show the selected one */
void x_guiTabRefresh( HWND tab )
{
   x_callbackEnterDll();
   s_tabLayout( tab, 1 );
   x_callbackLeaveDll();
   x_callbackRaisePending();
}

/* Return the object of the page nIndex */
void x_guiTabPage( X_CALL_CTX *ctx, HWND tab, int index )
{
   X_GUI_TABS tabs;
   const X_ITEM *object;

   memset( &tabs, 0, sizeof( tabs ) );
   tabs.tab = tab;
   tabs.selected = index;
   tabs.find = 1;
   EnumChildWindows( tab, s_tabPage, ( LPARAM )&tabs );
   object = IIF( tabs.found != NULL, ( const X_ITEM * )GetWindowLongPtrW( tabs.found, GWLP_USERDATA ), NULL );
   if( object == NULL )
      x_guiRaise( ctx, "GUI_TABCONTROL:PAGE" );
   else
      x_itemReturn( ctx, object );
}

/* The number of pages */
int x_guiTabCount( HWND tab )
{
   return ( int )SendMessageW( tab, TCM_GETITEMCOUNT, 0, 0 );
}

/* The selected page, from 1 */
int x_guiTabSelected( HWND tab )
{
   return ( int )SendMessageW( tab, TCM_GETCURSEL, 0, 0 ) + 1;
}

/* Select a page */
void x_guiTabSelect( X_CALL_CTX *ctx, HWND tab, int index )
{
   if( index < 1 || index > x_guiTabCount( tab ) )
   {
      x_guiRaise( ctx, "GUI_TABCONTROL:SELECTED" );
      return;
   }

   x_guiSend( tab, TCM_SETCURSEL, ( WPARAM )( index - 1 ), 0 );
   x_guiTabRefresh( tab );
}

/* ---------------------------------------------------------------------------------------------
   List views
   --------------------------------------------------------------------------------------------- */

/* The number of columns of a list view */
int x_guiListViewColumnCount( HWND window )
{
   HWND header = ( HWND )SendMessageW( window, LVM_GETHEADER, 0, 0 );

   return IIF( header != NULL, ( int )SendMessageW( header, HDM_GETITEMCOUNT, 0, 0 ), 0 );
}

/* The number of rows of a list view */
int x_guiListViewCount( HWND window )
{
   return ( int )SendMessageW( window, LVM_GETITEMCOUNT, 0, 0 );
}

/* Add a column; returns its number, 0 with an error raised */
int x_guiListViewAddColumn( X_CALL_CTX *ctx, HWND window, const X_ITEM *title, int width, int align )
{
   static const int formats[] = { LVCFMT_LEFT, LVCFMT_RIGHT, LVCFMT_CENTER };
   LVCOLUMNW column;
   wchar_t *text;
   LRESULT index;

   if( width <= 0 || align < 0 || align > 2 )
   {
      x_guiRaise( ctx, "GUI_LISTVIEW:ADDCOLUMN" );
      return 0;
   }

   text = x_guiText( ctx, title, "GUI_LISTVIEW:ADDCOLUMN" );
   if( text == NULL )
      return 0;

   memset( &column, 0, sizeof( column ) );
   column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
   column.fmt = formats[align];
   column.cx = x_guiScale( width, GetDpiForWindow( window ) );
   column.pszText = text;
   index = x_guiSend( window, LVM_INSERTCOLUMNW, ( WPARAM )x_guiListViewColumnCount( window ), ( LPARAM )&column );
   x_xfree( text );
   if( index < 0 )
   {
      x_guiRaiseWin32( ctx, "LVM_INSERTCOLUMNW", ERROR_NOT_ENOUGH_MEMORY );
      return 0;
   }

   return ( int )index + 1;
}

/* Set the text of a cell, row and column counted from 0 */
static int s_listViewSetText( X_CALL_CTX *ctx, HWND window, int row, int column, const X_ITEM *value,
                              const char *operation )
{
   wchar_t *text = x_guiText( ctx, value, operation );
   LVITEMW item;
   LRESULT done;

   if( text == NULL )
      return 0;

   memset( &item, 0, sizeof( item ) );
   item.iSubItem = column;
   item.pszText = text;
   done = x_guiSend( window, LVM_SETITEMTEXTW, ( WPARAM )row, ( LPARAM )&item );
   x_xfree( text );

   return done != 0;
}

/* Append a row with the texts of its first columns; returns its number, 0 with an error raised */
int x_guiListViewAddRow( X_CALL_CTX *ctx, HWND window, const X_ITEM *values )
{
   const int columns = x_guiListViewColumnCount( window );
   int64_t length = 0, i;
   wchar_t *text;
   LVITEMW item;
   LRESULT index;

   if( values != NULL && X_IS_ARRAY( values ) )
      length = x_arrayLen( values );

   for( i = 1; i <= length; ++i )
   {
      if( !X_IS_STRING( x_arrayGetItemPtr( values, ( size_t )i ) ) )
         length = 0;
   }

   if( length == 0 || length > columns )
   {
      x_guiRaise( ctx, "GUI_LISTVIEW:ADDROW" );
      return 0;
   }

   text = x_guiText( ctx, x_arrayGetItemPtr( values, 1 ), "GUI_LISTVIEW:ADDROW" );
   if( text == NULL )
      return 0;

   memset( &item, 0, sizeof( item ) );
   item.mask = LVIF_TEXT;
   item.iItem = x_guiListViewCount( window );
   item.pszText = text;
   index = x_guiSend( window, LVM_INSERTITEMW, 0, ( LPARAM )&item );
   x_xfree( text );
   if( index < 0 )
   {
      x_guiRaiseWin32( ctx, "LVM_INSERTITEMW", ERROR_NOT_ENOUGH_MEMORY );
      return 0;
   }

   for( i = 2; i <= length; ++i )
   {
      const X_ITEM *value = x_arrayGetItemPtr( values, ( size_t )i );

      if( !s_listViewSetText( ctx, window, ( int )index, ( int )i - 1, value, "GUI_LISTVIEW:ADDROW" ) )
         return 0;
   }

   return ( int )index + 1;
}

/* Whether a cell, counted from 1, exists; an error is raised when it does not */
static int s_listViewCellValid( X_CALL_CTX *ctx, HWND window, int row, int column, const char *operation )
{
   if( row < 1 || row > x_guiListViewCount( window ) || column < 1 || column > x_guiListViewColumnCount( window ) )
   {
      x_guiRaise( ctx, operation );
      return 0;
   }

   return 1;
}

/* Return the text of a cell */
void x_guiListViewCell( X_CALL_CTX *ctx, HWND window, int row, int column )
{
   size_t capacity = 256, size;
   wchar_t *buffer;
   LVITEMW item;
   LRESULT length;
   char *text;

   if( !s_listViewCellValid( ctx, window, row, column, "GUI_LISTVIEW:CELL" ) )
      return;

   for( ;; )
   {
      buffer = ( wchar_t * )x_xgrabArray( capacity, sizeof( wchar_t ) );
      memset( &item, 0, sizeof( item ) );
      item.iSubItem = column - 1;
      item.pszText = buffer;
      item.cchTextMax = ( int )capacity;
      length = SendMessageW( window, LVM_GETITEMTEXTW, ( WPARAM )( row - 1 ), ( LPARAM )&item );
      if( ( size_t )length < capacity - 1 )
         break;

      x_xfree( buffer );
      capacity *= 2;
   }

   text = x_utf16ToUtf8Alloc( ( const uint16_t * )buffer, ( size_t )length, &size );
   x_xfree( buffer );
   x_retclen_buffer( ctx, text, size );
}

/* Set the text of a cell */
void x_guiListViewSetCell( X_CALL_CTX *ctx, HWND window, int row, int column, const X_ITEM *text )
{
   if( s_listViewCellValid( ctx, window, row, column, "GUI_LISTVIEW:SETCELL" ) )
      s_listViewSetText( ctx, window, row - 1, column - 1, text, "GUI_LISTVIEW:SETCELL" );
}

/* Delete a row */
void x_guiListViewDelete( X_CALL_CTX *ctx, HWND window, int row )
{
   if( row < 1 || row > x_guiListViewCount( window ) )
      x_guiRaise( ctx, "GUI_LISTVIEW:DELETE" );
   else
      x_guiSend( window, LVM_DELETEITEM, ( WPARAM )( row - 1 ), 0 );
}

/* Delete all rows */
void x_guiListViewClear( HWND window )
{
   x_guiSend( window, LVM_DELETEALLITEMS, 0, 0 );
}

/* The selected row, 0 for none */
int x_guiListViewSelected( HWND window )
{
   return ( int )SendMessageW( window, LVM_GETNEXTITEM, ( WPARAM )-1, LVNI_SELECTED ) + 1;
}

/* Select a row, 0 for none; OnSelect does not run */
void x_guiListViewSetSelected( X_CALL_CTX *ctx, HWND window, int row )
{
   LVITEMW item;

   if( row < 0 || row > x_guiListViewCount( window ) )
   {
      x_guiRaise( ctx, "GUI_LISTVIEW:SELECTED" );
      return;
   }

   memset( &item, 0, sizeof( item ) );
   item.stateMask = LVIS_SELECTED | LVIS_FOCUSED;
   s_quiet = 1;
   x_guiSend( window, LVM_SETITEMSTATE, ( WPARAM )-1, ( LPARAM )&item );
   if( row > 0 )
   {
      item.state = LVIS_SELECTED | LVIS_FOCUSED;
      x_guiSend( window, LVM_SETITEMSTATE, ( WPARAM )( row - 1 ), ( LPARAM )&item );
      x_guiSend( window, LVM_ENSUREVISIBLE, ( WPARAM )( row - 1 ), FALSE );
   }

   s_quiet = 0;
}

/* ---------------------------------------------------------------------------------------------
   Tree views
   --------------------------------------------------------------------------------------------- */

/* The tree item of a value; an error is raised for another value */
static HTREEITEM s_treeItem( X_CALL_CTX *ctx, const X_ITEM *item, const char *operation )
{
   HTREEITEM handle = IIF( item != NULL && X_IS_POINTER( item ), ( HTREEITEM )x_itemGetPtr( item ), NULL );

   if( handle == NULL )
      x_guiRaise( ctx, operation );

   return handle;
}

/* Add an item under the parent item or at the root; returns the item */
void x_guiTreeAdd( X_CALL_CTX *ctx, HWND window, const X_ITEM *text, const X_ITEM *parent )
{
   HTREEITEM parentItem = TVI_ROOT;
   HTREEITEM added;
   TVINSERTSTRUCTW insert;
   wchar_t *wide;

   if( parent != NULL && !X_IS_NIL( parent ) )
   {
      parentItem = s_treeItem( ctx, parent, "GUI_TREEVIEW:ADD" );
      if( parentItem == NULL )
         return;
   }

   wide = x_guiText( ctx, text, "GUI_TREEVIEW:ADD" );
   if( wide == NULL )
      return;

   memset( &insert, 0, sizeof( insert ) );
   insert.hParent = parentItem;
   insert.hInsertAfter = TVI_LAST;
   insert.item.mask = TVIF_TEXT;
   insert.item.pszText = wide;
   added = ( HTREEITEM )x_guiSend( window, TVM_INSERTITEMW, 0, ( LPARAM )&insert );
   x_xfree( wide );
   if( added == NULL )
      x_guiRaiseWin32( ctx, "TVM_INSERTITEMW", ERROR_NOT_ENOUGH_MEMORY );
   else
      x_retptr( ctx, added );
}

/* Return the text of an item */
void x_guiTreeText( X_CALL_CTX *ctx, HWND window, const X_ITEM *item )
{
   enum { TEXT_CAPACITY = 1024 };
   HTREEITEM handle = s_treeItem( ctx, item, "GUI_TREEVIEW:ITEMTEXT" );
   wchar_t buffer[TEXT_CAPACITY];
   TVITEMW data;
   size_t size;
   char *text;

   if( handle == NULL )
      return;

   memset( &data, 0, sizeof( data ) );
   data.mask = TVIF_TEXT | TVIF_HANDLE;
   data.hItem = handle;
   data.pszText = buffer;
   data.cchTextMax = TEXT_CAPACITY;
   if( !SendMessageW( window, TVM_GETITEMW, 0, ( LPARAM )&data ) )
   {
      x_guiRaise( ctx, "GUI_TREEVIEW:ITEMTEXT" );
      return;
   }

   text = x_utf16ToUtf8Alloc( ( const uint16_t * )buffer, wcslen( buffer ), &size );
   x_retclen_buffer( ctx, text, size );
}

/* Set the text of an item */
void x_guiTreeSetText( X_CALL_CTX *ctx, HWND window, const X_ITEM *item, const X_ITEM *text )
{
   HTREEITEM handle = s_treeItem( ctx, item, "GUI_TREEVIEW:SETITEMTEXT" );
   wchar_t *wide;
   TVITEMW data;
   LRESULT done;

   if( handle == NULL )
      return;

   wide = x_guiText( ctx, text, "GUI_TREEVIEW:SETITEMTEXT" );
   if( wide == NULL )
      return;

   memset( &data, 0, sizeof( data ) );
   data.mask = TVIF_TEXT | TVIF_HANDLE;
   data.hItem = handle;
   data.pszText = wide;
   done = x_guiSend( window, TVM_SETITEMW, 0, ( LPARAM )&data );
   x_xfree( wide );
   if( !done )
      x_guiRaise( ctx, "GUI_TREEVIEW:SETITEMTEXT" );
}

/* Expand or collapse an item */
void x_guiTreeExpand( X_CALL_CTX *ctx, HWND window, const X_ITEM *item, int expand )
{
   HTREEITEM handle = s_treeItem( ctx, item, IIF( expand, "GUI_TREEVIEW:EXPAND", "GUI_TREEVIEW:COLLAPSE" ) );

   if( handle != NULL )
      x_guiSend( window, TVM_EXPAND, IIF( expand, TVE_EXPAND, TVE_COLLAPSE ), ( LPARAM )handle );
}

/* Return the selected item, NIL for none */
void x_guiTreeSelected( X_CALL_CTX *ctx, HWND window )
{
   HTREEITEM handle = ( HTREEITEM )SendMessageW( window, TVM_GETNEXTITEM, TVGN_CARET, 0 );

   if( handle != NULL )
      x_retptr( ctx, handle );
}

/* Select an item, NIL for none */
void x_guiTreeSelect( X_CALL_CTX *ctx, HWND window, const X_ITEM *item )
{
   HTREEITEM handle = NULL;

   if( item != NULL && !X_IS_NIL( item ) )
   {
      handle = s_treeItem( ctx, item, "GUI_TREEVIEW:SELECTED" );
      if( handle == NULL )
         return;
   }

   if( !x_guiSend( window, TVM_SELECTITEM, TVGN_CARET, ( LPARAM )handle ) && handle != NULL )
      x_guiRaise( ctx, "GUI_TREEVIEW:SELECTED" );
}

/* Delete all items */
void x_guiTreeClear( HWND window )
{
   x_guiSend( window, TVM_DELETEITEM, 0, ( LPARAM )TVI_ROOT );
}

/* The number of items */
int x_guiTreeCount( HWND window )
{
   return ( int )SendMessageW( window, TVM_GETCOUNT, 0, 0 );
}

/* ---------------------------------------------------------------------------------------------
   Toolbars
   --------------------------------------------------------------------------------------------- */

/* A bar at a side has its buttons one under another with the text under the picture; on a bar at the top or
   at the bottom the text stands in the middle of its wide button */
static void s_toolbarDock( HWND toolbar, X_GUI_BAR *bar )
{
   UINT dpi = GetDpiForWindow( toolbar );

   if( bar->side == X_GUI_SIDE_TOP || bar->side == X_GUI_SIDE_BOTTOM )
      SendMessageW( toolbar, TB_SETDRAWTEXTFLAGS, DT_CENTER | DT_RIGHT, DT_CENTER );
   else
   {
      LRESULT style = SendMessageW( toolbar, TB_GETSTYLE, 0, 0 );

      style = ( style & ~TBSTYLE_LIST ) | CCS_VERT | TBSTYLE_WRAPABLE;
      SendMessageW( toolbar, TB_SETSTYLE, 0, ( LPARAM )style );
      SendMessageW( toolbar, TB_SETPADDING, 0, MAKELPARAM( x_guiScale( 12, dpi ), x_guiScale( 12, dpi ) ) );
      SendMessageW( toolbar, TB_SETBUTTONWIDTH, 0, MAKELPARAM( x_guiScale( 80, dpi ), x_guiScale( 120, dpi ) ) );
   }
}

/* gui_Toolbar():New( oParent, [lBottom | cSide] ): a toolbar at a side of the window, at the top by default */
void x_guiToolbarNew( X_CALL_CTX *ctx )
{
   X_GUI_SIDE side = X_GUI_SIDE_TOP;
   HWND toolbar, parent;
   X_GUI_BAR *bar;

   if( X_ISLOG( ctx, 2 ) )
      side = IIF( x_parl( ctx, 2 ), X_GUI_SIDE_BOTTOM, X_GUI_SIDE_TOP );
   else if( X_ISCHAR( ctx, 2 ) )
   {
      switch( toupper( ( unsigned char )x_parc( ctx, 2 )[0] ) )
      {
      case 'T':
         side = X_GUI_SIDE_TOP;
         break;
      case 'B':
         side = X_GUI_SIDE_BOTTOM;
         break;
      case 'L':
         side = X_GUI_SIDE_LEFT;
         break;
      case 'R':
         side = X_GUI_SIDE_RIGHT;
         break;
      default:
         x_guiRaise( ctx, "GUI_TOOLBAR:NEW" );
         return;
      }
   }
   else if( !X_ISNIL( ctx, 2 ) )
   {
      x_guiRaise( ctx, "GUI_TOOLBAR:NEW" );
      return;
   }

   s_controlSend( ctx, X_GUI_TOOLBAR, 0, 0, 0, 0, 0 );
   toolbar = s_objectHandle( ctx, ctx->self );
   bar = IIF( toolbar != NULL, s_toolbarOf( toolbar ), NULL );
   if( bar != NULL )
   {
      bar->side = side;
      x_callbackEnterDll();
      s_toolbarDock( toolbar, bar );
      s_toolbarPlace( toolbar );
      /* The side bars, the sidebar and the panels make room for the new bar */
      parent = GetParent( toolbar );
      EnumChildWindows( parent, s_layoutSideToolbar, ( LPARAM )parent );
      EnumChildWindows( parent, s_layoutSidebar, ( LPARAM )parent );
      EnumChildWindows( parent, s_layoutPanel, ( LPARAM )parent );
      x_callbackLeaveDll();
      x_callbackRaisePending();
   }
}

/* A 32-bit picture with any alpha value keeps its own transparency */
int x_guiHasAlpha( const BITMAP *info )
{
   const BYTE *bits = ( const BYTE * )info->bmBits;
   size_t size, i;

   if( info->bmBitsPixel != 32 || bits == NULL )
      return 0;

   size = ( size_t )info->bmWidthBytes * ( size_t )info->bmHeight;
   for( i = 3; i < size; i += 4 )
   {
      if( bits[i] != 0 )
         return 1;
   }

   return 0;
}

/* Without alpha the color of the top left pixel is transparent */
COLORREF x_guiCornerColor( HBITMAP picture )
{
   HDC dc = CreateCompatibleDC( NULL );
   HGDIOBJ old = SelectObject( dc, picture );
   COLORREF color = GetPixel( dc, 0, 0 );

   SelectObject( dc, old );
   DeleteDC( dc );

   return color;
}

/* The first picture sets the size of all pictures of the toolbar */
static int s_toolbarImage( X_CALL_CTX *ctx, HWND toolbar, X_GUI_BAR *bar, const X_ITEM *file )
{
   HBITMAP picture = x_guiPicture( ctx, file, "GUI_TOOLBAR:ADD" );
   BITMAP info;
   int image = -1;

   if( picture == NULL )
      return -1;

   GetObjectW( picture, sizeof( info ), &info );
   if( bar->images == NULL )
   {
      bar->images = ImageList_Create( info.bmWidth, info.bmHeight, ILC_COLOR32 | ILC_MASK, 8, 8 );
      if( bar->images != NULL )
         SendMessageW( toolbar, TB_SETIMAGELIST, 0, ( LPARAM )bar->images );
   }

   if( bar->images != NULL && x_guiHasAlpha( &info ) )
      image = ImageList_Add( bar->images, picture, NULL );
   else if( bar->images != NULL )
      image = ImageList_AddMasked( bar->images, picture, x_guiCornerColor( picture ) );

   DeleteObject( picture );
   if( image < 0 )
      x_guiRaise( ctx, "GUI_TOOLBAR:ADD" );

   return image;
}

/* The buttons and their tips, shortcuts and pictures are removed; the bar stays */
void x_guiToolbarClear( HWND toolbar )
{
   X_GUI_BAR *bar = s_toolbarOf( toolbar );
   size_t i;

   if( bar == NULL )
      return;

   x_callbackEnterDll();
   while( SendMessageW( toolbar, TB_BUTTONCOUNT, 0, 0 ) > 0 )
      SendMessageW( toolbar, TB_DELETEBUTTON, 0, 0 );

   if( bar->images != NULL )
      ImageList_RemoveAll( bar->images );

   x_callbackLeaveDll();
   for( i = 0; i < bar->count; ++i )
      x_xfree( bar->tips[i] );

   bar->count = 0;
   s_toolbarPlace( toolbar );
   x_callbackRaisePending();
}

/* A grey button gets no click from the toolbar and s_toolbarKeyChild() skips its shortcut;
   0 when there is no such button */
int x_guiToolbarEnable( HWND toolbar, int button, int enable )
{
   const X_GUI_BAR *bar = s_toolbarOf( toolbar );

   if( bar == NULL || button < 1 || ( size_t )button > bar->count )
      return 0;

   x_callbackEnterDll();
   SendMessageW( toolbar, TB_ENABLEBUTTON, ( WPARAM )button, MAKELPARAM( IIF( enable, TRUE, FALSE ), 0 ) );
   x_callbackLeaveDll();
   x_callbackRaisePending();

   return 1;
}

/* Add a button; returns its number, 0 with an error raised */
int x_guiToolbarAdd( X_CALL_CTX *ctx, HWND toolbar, const X_ITEM *text, const X_ITEM *bitmap, const X_ITEM *toolTip,
                     const X_ITEM *shortcut, int dropDown )
{
   X_GUI_BAR *bar = s_toolbarOf( toolbar );
   wchar_t *label = NULL;
   wchar_t *tip = NULL;
   int image = I_IMAGENONE;
   UINT key = 0, modifiers = 0;
   char display[48] = "";
   const char *spec;
   TBBUTTON button;
   BOOL done;
   DWORD error;
   int valid, vertical;

   valid = bar != NULL;
   if( valid && shortcut != NULL && !X_IS_NIL( shortcut ) )
   {
      spec = IIF( X_IS_STRING( shortcut ), x_itemGetCPtr( shortcut ), NULL );
      valid = spec != NULL;
      if( valid )
         valid = s_parseShortcut( spec, x_itemGetCLen( shortcut ), &key, &modifiers, display, sizeof( display ) );
   }

   if( !valid )
   {
      x_guiRaise( ctx, "GUI_TOOLBAR:ADD" );
      return 0;
   }

   label = x_guiText( ctx, text, "GUI_TOOLBAR:ADD" );
   if( label == NULL )
      return 0;

   if( toolTip != NULL && !X_IS_NIL( toolTip ) )
   {
      tip = x_guiText( ctx, toolTip, "GUI_TOOLBAR:ADD" );
      valid = tip != NULL;
   }

   if( valid && bitmap != NULL && !X_IS_NIL( bitmap ) )
   {
      image = s_toolbarImage( ctx, toolbar, bar, bitmap );
      valid = image >= 0;
   }

   if( !valid )
   {
      x_xfree( label );
      x_xfree( tip );
      return 0;
   }

   memset( &button, 0, sizeof( button ) );
   button.iBitmap = image;
   button.idCommand = ( int )bar->count + 1;
   vertical = ( SendMessageW( toolbar, TB_GETSTYLE, 0, 0 ) & CCS_VERT ) != 0;
   button.fsState = ( BYTE )( TBSTATE_ENABLED | IIF( vertical, TBSTATE_WRAP, 0 ) );
   button.fsStyle = ( BYTE )IIF( dropDown, BTNS_WHOLEDROPDOWN, BTNS_BUTTON );
   button.iString = ( INT_PTR )label;
   x_callbackEnterDll();
   done = ( BOOL )SendMessageW( toolbar, TB_ADDBUTTONSW, 1, ( LPARAM )&button );
   error = GetLastError();
   x_callbackLeaveDll();
   x_xfree( label );
   x_callbackRaisePending();
   if( !done )
   {
      x_xfree( tip );
      x_guiRaiseWin32( ctx, "TB_ADDBUTTONSW", error );
      return 0;
   }

   bar->tips = ( wchar_t ** )x_xrealloc( bar->tips, ( bar->count + 1 ) * sizeof( wchar_t * ) );
   bar->keys = ( UINT * )x_xrealloc( bar->keys, ( bar->count + 1 ) * sizeof( UINT ) );
   bar->modifiers = ( UINT * )x_xrealloc( bar->modifiers, ( bar->count + 1 ) * sizeof( UINT ) );
   bar->tips[bar->count] = tip;
   bar->keys[bar->count] = key;
   bar->modifiers[bar->count] = modifiers;
   ++bar->count;
   s_toolbarPlace( toolbar );

   return ( int )bar->count;
}

/* The menu of a button with an arrow opens under the button, or above it on a bar at the bottom of the window;
   its commands go to the window as the commands of the menu bar */
void x_guiToolbarPopup( HWND toolbar, int button, HMENU menu )
{
   const X_GUI_BAR *bar = s_toolbarOf( toolbar );
   int bottom = bar != NULL && bar->side == X_GUI_SIDE_BOTTOM;
   UINT flags = TPM_LEFTALIGN | TPM_RIGHTBUTTON;
   RECT rect;
   int y;

   if( menu == NULL || !SendMessageW( toolbar, TB_GETRECT, ( WPARAM )button, ( LPARAM )&rect ) )
      return;

   MapWindowPoints( toolbar, NULL, ( POINT * )&rect, 2 );
   flags |= IIF( bottom, TPM_BOTTOMALIGN, TPM_TOPALIGN );
   y = IIF( bottom, rect.top, rect.bottom );
   x_callbackEnterDll();
   SendMessageW( toolbar, TB_PRESSBUTTON, ( WPARAM )button, TRUE );
   TrackPopupMenuEx( menu, flags, rect.left, y, GetAncestor( toolbar, GA_ROOT ), NULL );
   SendMessageW( toolbar, TB_PRESSBUTTON, ( WPARAM )button, FALSE );
   x_callbackLeaveDll();
   x_callbackRaisePending();
}

/* Add a gap between buttons */
void x_guiToolbarSeparator( X_CALL_CTX *ctx, HWND toolbar )
{
   TBBUTTON button;

   memset( &button, 0, sizeof( button ) );
   button.iBitmap = x_guiScale( 8, GetDpiForWindow( toolbar ) );
   button.fsStyle = BTNS_SEP;
   if( SendMessageW( toolbar, TB_GETSTYLE, 0, 0 ) & CCS_VERT )
      button.fsState = TBSTATE_WRAP;

   if( !x_guiSend( toolbar, TB_ADDBUTTONSW, 1, ( LPARAM )&button ) )
      x_guiRaiseWin32( ctx, "TB_ADDBUTTONSW", GetLastError() );
   else
      s_toolbarPlace( toolbar );
}

/* Return the panel of the scoreboard, 0 for none */
void x_guiStatusBarScoreboard( X_CALL_CTX *ctx )
{
   HWND status = x_guiSelfHandle( ctx, "GUI_STATUSBAR:SCOREBOARD" );
   const X_GUI_STATUS *parts = IIF( status != NULL, s_statusOf( status ), NULL );

   if( status != NULL )
      x_retni( ctx, IIF( parts != NULL, parts->scoreboard, 0 ) );
}

/* Set the panel that shows "Ins" during ReadModal(), 0 for none */
void x_guiStatusBarSetScoreboard( X_CALL_CTX *ctx )
{
   HWND status = x_guiSelfHandle( ctx, "GUI_STATUSBAR:SCOREBOARD" );

   if( status == NULL )
      return;

   if( !X_ISNUM( ctx, 1 ) || x_parni( ctx, 1 ) < 0 )
   {
      x_guiRaise( ctx, "GUI_STATUSBAR:SCOREBOARD" );
      return;
   }

   s_statusParts( status )->scoreboard = x_parni( ctx, 1 );
   x_itemReturn( ctx, x_paritem( ctx, 1 ) );
}

/* Return a color of a status bar, NIL for the system color */
void x_guiStatusBarColor( X_CALL_CTX *ctx, X_GUI_COLOR part )
{
   HWND status = x_guiSelfHandle( ctx, "GUI_STATUSBAR:COLOR" );
   const X_GUI_STATUS *parts = IIF( status != NULL, s_statusOf( status ), NULL );

   if( parts != NULL && parts->colors[part] >= 0 )
      x_retni( ctx, parts->colors[part] );
}

/* Set a color of a status bar, NIL for the system color */
void x_guiStatusBarSetColor( X_CALL_CTX *ctx, X_GUI_COLOR part )
{
   HWND status = x_guiSelfHandle( ctx, "GUI_STATUSBAR:COLOR" );

   if( status == NULL )
      return;

   if( !X_ISNIL( ctx, 1 ) && ( !X_ISNUM( ctx, 1 ) || x_parnint( ctx, 1 ) < 0 || x_parnint( ctx, 1 ) > 0xFFFFFF ) )
   {
      x_guiRaise( ctx, "GUI_STATUSBAR:COLOR" );
      return;
   }

   s_statusParts( status )->colors[part] = IIF( X_ISNIL( ctx, 1 ), -1, ( int )x_parnint( ctx, 1 ) );
   InvalidateRect( status, NULL, TRUE );
   x_itemReturn( ctx, x_paritem( ctx, 1 ) );
}

/* Return a color of a toolbar, NIL for the system color */
void x_guiToolbarColor( X_CALL_CTX *ctx, X_GUI_COLOR part )
{
   HWND toolbar = x_guiSelfHandle( ctx, "GUI_TOOLBAR:COLOR" );
   const X_GUI_BAR *bar = IIF( toolbar != NULL, s_toolbarOf( toolbar ), NULL );

   if( bar != NULL && bar->colors[part] >= 0 )
      x_retni( ctx, bar->colors[part] );
}

/* Set a color of a toolbar, NIL for the system color */
void x_guiToolbarSetColor( X_CALL_CTX *ctx, X_GUI_COLOR part )
{
   HWND toolbar = x_guiSelfHandle( ctx, "GUI_TOOLBAR:COLOR" );
   X_GUI_BAR *bar = IIF( toolbar != NULL, s_toolbarOf( toolbar ), NULL );

   if( bar == NULL )
      return;

   if( !X_ISNIL( ctx, 1 ) && ( !X_ISNUM( ctx, 1 ) || x_parnint( ctx, 1 ) < 0 || x_parnint( ctx, 1 ) > 0xFFFFFF ) )
   {
      x_guiRaise( ctx, "GUI_TOOLBAR:COLOR" );
      return;
   }

   bar->colors[part] = IIF( X_ISNIL( ctx, 1 ), -1, ( int )x_parnint( ctx, 1 ) );
   InvalidateRect( toolbar, NULL, TRUE );
   x_itemReturn( ctx, x_paritem( ctx, 1 ) );
}

/* ---------------------------------------------------------------------------------------------
   Sidebars
   --------------------------------------------------------------------------------------------- */

/* A color between two colors: amount of 255 parts of the second one */
COLORREF x_guiMix( COLORREF first, COLORREF second, int amount )
{
   int red = GetRValue( first ) + ( GetRValue( second ) - GetRValue( first ) ) * amount / 255;
   int green = GetGValue( first ) + ( GetGValue( second ) - GetGValue( first ) ) * amount / 255;
   int blue = GetBValue( first ) + ( GetBValue( second ) - GetBValue( first ) ) * amount / 255;

   return RGB( red, green, blue );
}

/* The panel and its title have the colors of the sidebar window, otherwise those of the system */
void x_guiSidebarColors( HWND sidebar, COLORREF *back, COLORREF *fore )
{
   const X_GUI_PAINT *paint = s_paintOf( sidebar );

   *back = GetSysColor( COLOR_BTNFACE );
   *fore = GetSysColor( COLOR_BTNTEXT );
   if( paint != NULL && paint->brush != NULL )
      *back = s_colorRef( paint->colors[X_GUI_COLOR_BACKGROUND] );

   if( paint != NULL && paint->colors[X_GUI_COLOR_FOREGROUND] >= 0 )
      *fore = s_colorRef( paint->colors[X_GUI_COLOR_FOREGROUND] );
}

/* The strip of pictures has the color barBackground, otherwise X_GUI_SIDEBAR_BAR */
static COLORREF s_sidebarStripColor( const X_GUI_SIDEBAR *side )
{
   int bar = side->colors[X_GUI_SIDEBAR_COLOR_BAR];

   return s_colorRef( IIF( bar >= 0, bar, X_GUI_SIDEBAR_BAR ) );
}

/* Whether a window is one of the bars of the sidebar */
static int s_sidebarBar( const X_GUI_SIDEBAR *side, HWND window )
{
   size_t i;

   if( window == side->activity || window == side->buttons )
      return 1;

   for( i = 0; i < side->count; ++i )
   {
      if( window == side->pages[i].actions )
         return 1;
   }

   return 0;
}

/* A bar of picture buttons without text, placed by the sidebar; the bar of pages and the bar of buttons have their
   pictures one under another, the bar of actions of a page stays hidden until the page is shown */
static HWND s_sidebarToolbar( HWND sidebar, int vertical )
{
   DWORD style = WS_CHILD | WS_CLIPSIBLINGS | TBSTYLE_FLAT | TBSTYLE_TOOLTIPS;
   HINSTANCE instance = GetModuleHandleW( NULL );
   HWND toolbar;

   style |= CCS_NOPARENTALIGN | CCS_NORESIZE | CCS_NODIVIDER;
   style |= IIF( vertical, ( DWORD )( WS_VISIBLE | CCS_VERT | TBSTYLE_WRAPABLE ), 0 );
   s_initCommonControls();
   toolbar = CreateWindowExW( 0, TOOLBARCLASSNAMEW, NULL, style, 0, 0, 0, 0, sidebar, NULL, instance, NULL );
   if( toolbar == NULL )
      return NULL;

   s_toolbarInit( toolbar );
   s_toolbarOf( toolbar )->side = X_GUI_SIDE_NONE;
   SendMessageW( toolbar, TB_SETMAXTEXTROWS, 0, 0 );
   SendMessageW( toolbar, TB_SETPADDING, 0, 0 );

   return toolbar;
}

/* The buttons of a bar of the sidebar are squares of size pixels at 96 DPI, larger for larger pictures; returns
   their size in pixels */
static int s_sidebarButtons( HWND toolbar, int size )
{
   const X_GUI_BAR *bar = s_toolbarOf( toolbar );
   UINT dpi = GetDpiForWindow( toolbar );
   int length = x_guiScale( size, dpi );
   int width = 0, height = 0, picture;

   if( bar != NULL && bar->images != NULL )
      ImageList_GetIconSize( bar->images, &width, &height );

   picture = IIF( width > height, width, height ) + x_guiScale( 6, dpi );
   length = IIF( picture > length, picture, length );
   SendMessageW( toolbar, TB_SETBUTTONWIDTH, 0, MAKELPARAM( length, length ) );
   SendMessageW( toolbar, TB_SETBUTTONSIZE, 0, MAKELPARAM( length, length ) );

   return length;
}

/* Place the parts of the sidebar: the bar of pages at the top of the strip, the bar of buttons at its bottom, the
   bar of actions at the right of the header and the panel of the shown page under the header; hidden panels get
   the same place, so the anchors of their controls count from it */
static void s_sidebarLayout( HWND sidebar )
{
   const X_GUI_SIDEBAR *side = s_sidebarOf( sidebar );
   UINT dpi = GetDpiForWindow( sidebar );
   UINT flags = SWP_NOZORDER | SWP_NOACTIVATE;
   int strip = x_guiScale( X_GUI_SIDEBAR_STRIP, dpi );
   int header = x_guiScale( X_GUI_SIDEBAR_HEADER, dpi );
   int width = x_guiScale( side->width, dpi );
   int size, count, top, x, y, height;
   RECT client;
   size_t i;

   GetClientRect( sidebar, &client );
   size = s_sidebarButtons( side->buttons, X_GUI_SIDEBAR_STRIP );
   count = ( int )SendMessageW( side->buttons, TB_BUTTONCOUNT, 0, 0 );
   top = IIF( client.bottom > size * count, client.bottom - size * count, 0 );
   SetWindowPos( side->buttons, NULL, 0, top, strip - 1, size * count, flags );
   s_sidebarButtons( side->activity, X_GUI_SIDEBAR_STRIP );
   SetWindowPos( side->activity, NULL, 0, 0, strip - 1, top, flags );

   height = IIF( client.bottom > header, client.bottom - header, 0 );
   for( i = 0; i < side->count; ++i )
   {
      const X_GUI_SIDEPAGE *page = &side->pages[i];
      UINT show = IIF( ( int )i + 1 == side->page, SWP_SHOWWINDOW, SWP_HIDEWINDOW );

      size = s_sidebarButtons( page->actions, X_GUI_SIDEBAR_ACTION );
      count = ( int )SendMessageW( page->actions, TB_BUTTONCOUNT, 0, 0 );
      x = strip + width - x_guiScale( X_GUI_SIDEBAR_GAP, dpi ) - size * count;
      y = ( header - size ) / 2;
      SetWindowPos( page->actions, NULL, x, y, size * count, size, flags | show );
      SetWindowPos( page->panel, NULL, strip, header, width, height, flags | show );
   }

   InvalidateRect( sidebar, NULL, TRUE );
   InvalidateRect( side->activity, NULL, TRUE );
}

/* The sidebar takes its new width and the panels filling the window follow it */
static void s_sidebarResize( HWND sidebar )
{
   HWND parent = GetParent( sidebar );

   s_sidebarPlace( sidebar );
   s_sidebarLayout( sidebar );
   EnumChildWindows( parent, s_layoutPanel, ( LPARAM )parent );
}

/* Show the panel of a page, page 0 hides the panel; the focus leaves a hidden panel for the window */
static void s_sidebarShow( HWND sidebar, X_GUI_SIDEBAR *side, int page )
{
   HWND focus = GetFocus();

   side->page = page;
   s_sidebarResize( sidebar );
   if( focus != NULL && IsChild( sidebar, focus ) && !IsWindowVisible( focus ) )
      SetFocus( GetParent( sidebar ) );
}

/* The sidebar paints its strip with the line of one pixel at its right edge, the title of the shown page and the
   line at its edge */
static void s_sidebarPaint( HWND sidebar, const X_GUI_SIDEBAR *side )
{
   PAINTSTRUCT state;
   HDC dc = BeginPaint( sidebar, &state );
   UINT dpi = GetDpiForWindow( sidebar );
   HGDIOBJ old = SelectObject( dc, x_guiFont( dpi ) );
   HBRUSH brush = ( HBRUSH )GetStockObject( DC_BRUSH );
   int strip = x_guiScale( X_GUI_SIDEBAR_STRIP, dpi );
   int left = strip + x_guiScale( X_GUI_SIDEBAR_TITLE, dpi );
   int header = x_guiScale( X_GUI_SIDEBAR_HEADER, dpi );
   const X_GUI_SIDEPAGE *page;
   COLORREF back, fore;
   RECT client, part, actions;

   x_guiSidebarColors( sidebar, &back, &fore );
   GetClientRect( sidebar, &client );
   part = ( RECT ){ 0, 0, strip, client.bottom };
   SetDCBrushColor( dc, s_sidebarStripColor( side ) );
   FillRect( dc, &part, brush );
   part = ( RECT ){ strip - 1, 0, strip, client.bottom };
   SetDCBrushColor( dc, s_colorRef( X_GUI_SIDEBAR_BORDER ) );
   FillRect( dc, &part, brush );
   if( side->page > 0 )
   {
      page = &side->pages[side->page - 1];
      GetWindowRect( page->actions, &actions );
      MapWindowPoints( NULL, sidebar, ( POINT * )&actions, 2 );
      part = ( RECT ){ left, 0, actions.left, header };
      SetBkMode( dc, TRANSPARENT );
      SetTextColor( dc, fore );
      DrawTextW( dc, page->title, -1, &part, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS );

      part = ( RECT ){ client.right - 1, 0, client.right, client.bottom };
      SetDCBrushColor( dc, x_guiMix( back, fore, 48 ) );
      FillRect( dc, &part, brush );
   }

   SelectObject( dc, old );
   EndPaint( sidebar, &state );
}

/* The buttons of the bars of the sidebar are pictures on the color of their bar; the shown page has a mark at the
   left edge of its picture, the other pages and the buttons are paler until the mouse comes over them; an action
   under the mouse lies on a field a shade towards the text */
static LRESULT s_sidebarDraw( HWND sidebar, const X_GUI_SIDEBAR *side, NMTBCUSTOMDRAW *draw )
{
   HWND toolbar = draw->nmcd.hdr.hwndFrom;
   HDC dc = draw->nmcd.hdc;
   const X_GUI_BAR *bar = s_toolbarOf( toolbar );
   UINT dpi = GetDpiForWindow( toolbar );
   HBRUSH brush = ( HBRUSH )GetStockObject( DC_BRUSH );
   int strip = toolbar == side->activity || toolbar == side->buttons;
   int hot = ( draw->nmcd.uItemState & ( CDIS_HOT | CDIS_SELECTED ) ) != 0;
   int shown = toolbar == side->activity && ( int )draw->nmcd.dwItemSpec == side->page;
   int accent = side->colors[X_GUI_SIDEBAR_COLOR_ACCENT];
   int width = 0, height = 0;
   IMAGELISTDRAWPARAMS image;
   TBBUTTONINFOW info;
   COLORREF back, fore;
   RECT face;

   x_guiSidebarColors( sidebar, &back, &fore );
   if( strip )
      back = s_sidebarStripColor( side );

   if( draw->nmcd.dwDrawStage == CDDS_PREPAINT )
   {
      SetDCBrushColor( dc, back );
      FillRect( dc, &draw->nmcd.rc, brush );
      return CDRF_NOTIFYITEMDRAW;
   }

   if( draw->nmcd.dwDrawStage != CDDS_ITEMPREPAINT )
      return CDRF_DODEFAULT;

   face = draw->nmcd.rc;
   SetDCBrushColor( dc, back );
   FillRect( dc, &face, brush );
   if( hot && !strip )
   {
      InflateRect( &face, -x_guiScale( 1, dpi ), -x_guiScale( 1, dpi ) );
      SetDCBrushColor( dc, x_guiMix( back, fore, 40 ) );
      FillRect( dc, &face, brush );
   }

   if( shown )
   {
      face = draw->nmcd.rc;
      face.right = face.left + x_guiScale( X_GUI_SIDEBAR_MARK, dpi );
      SetDCBrushColor( dc, IIF( accent >= 0, s_colorRef( accent ), GetSysColor( COLOR_HIGHLIGHT ) ) );
      FillRect( dc, &face, brush );
   }

   memset( &info, 0, sizeof( info ) );
   info.cbSize = sizeof( info );
   info.dwMask = TBIF_IMAGE;
   info.iImage = I_IMAGENONE;
   SendMessageW( toolbar, TB_GETBUTTONINFOW, draw->nmcd.dwItemSpec, ( LPARAM )&info );
   if( bar == NULL || bar->images == NULL || info.iImage < 0 )
      return CDRF_SKIPDEFAULT;

   ImageList_GetIconSize( bar->images, &width, &height );
   memset( &image, 0, sizeof( image ) );
   image.cbSize = sizeof( image );
   image.himl = bar->images;
   image.i = info.iImage;
   image.hdcDst = dc;
   image.x = ( draw->nmcd.rc.left + draw->nmcd.rc.right - width ) / 2;
   image.y = ( draw->nmcd.rc.top + draw->nmcd.rc.bottom - height ) / 2;
   image.rgbBk = CLR_NONE;
   image.rgbFg = CLR_NONE;
   image.fStyle = ILD_TRANSPARENT;
   image.fState = ILS_ALPHA;
   image.Frame = ( DWORD )IIF( strip && !shown && !hot, X_GUI_SIDEBAR_PALE, 255 );
   ImageList_DrawIndirect( &image );

   return CDRF_SKIPDEFAULT;
}

/* A click on a bar of the sidebar: a page shows its panel and the shown page hides it, both run OnChange; a button
   and an action run their blocks; 0 when the bar is not one of the sidebar */
static int s_sidebarCommand( HWND sidebar, X_GUI_SIDEBAR *side, HWND toolbar, int button )
{
   X_ITEM *object = ( X_ITEM * )GetWindowLongPtrW( sidebar, GWLP_USERDATA );
   X_ITEM result __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   int64_t values[2] = { 0, button };
   size_t i;

   if( object == NULL || !s_sidebarBar( side, toolbar ) )
      return 0;

   if( toolbar == side->activity )
   {
      s_sidebarShow( sidebar, side, IIF( button == side->page, 0, button ) );
      if( side->page > 0 )
         SetFocus( side->pages[side->page - 1].panel );

      x_guiNotify( object, "__GUICHANGE", &result, 0, NULL );
      return 1;
   }

   for( i = 0; i < side->count; ++i )
   {
      if( toolbar == side->pages[i].actions )
         values[0] = ( int64_t )i + 1;
   }

   x_guiNotify( object, "__GUICOMMAND", &result, 2, values );

   return 1;
}

/* Whether the point x of the sidebar lies on the edge of the shown panel, where the mouse changes its width */
static int s_sidebarOnEdge( HWND sidebar, const X_GUI_SIDEBAR *side, int x )
{
   UINT dpi = GetDpiForWindow( sidebar );

   return side->page > 0 && x >= x_guiScale( X_GUI_SIDEBAR_STRIP, dpi ) + x_guiScale( side->width, dpi );
}

/* The edge dragged to x: the panel takes the new width, not less than the smallest one and leaving room for the
   rest of the window */
static void s_sidebarDrag( HWND sidebar, X_GUI_SIDEBAR *side, int x )
{
   UINT dpi = GetDpiForWindow( sidebar );
   int least = x_guiScale( X_GUI_SIDEBAR_MINWIDTH, dpi );
   int width = x - x_guiScale( X_GUI_SIDEBAR_STRIP, dpi ) - x_guiScale( X_GUI_SIDEBAR_EDGE, dpi ) / 2;
   int most;
   RECT client;

   GetClientRect( GetParent( sidebar ), &client );
   most = client.right - x_guiScale( X_GUI_SIDEBAR_STRIP + X_GUI_SIDEBAR_EDGE + X_GUI_SIDEBAR_REST, dpi );
   width = IIF( width > most, most, width );
   width = IIF( width < least, least, width );
   width = MulDiv( width, USER_DEFAULT_SCREEN_DPI, ( int )dpi );
   if( width == side->width )
      return;

   side->width = width;
   s_sidebarResize( sidebar );
}

/* Release the pages of a sidebar; its bars and panels are released as windows */
static void s_sidebarRelease( HWND sidebar )
{
   X_GUI_SIDEBAR *side = ( X_GUI_SIDEBAR * )RemovePropW( sidebar, s_sidebarProperty );
   size_t i;

   if( side == NULL )
      return;

   for( i = 0; i < side->count; ++i )
      x_xfree( side->pages[i].title );

   x_xfree( side->pages );
   x_xfree( side );
}

/* The sidebar paints itself, places its parts, runs the clicks on its bars and lets the mouse drag its edge */
static LRESULT CALLBACK s_sidebarProc( HWND sidebar, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR id,
                                       DWORD_PTR data )
{
   X_GUI_SIDEBAR *side = s_sidebarOf( sidebar );
   const NMHDR *header = ( const NMHDR * )lParam;
   POINT point;

   UNUSED( data );
   if( side == NULL )
      return DefSubclassProc( sidebar, message, wParam, lParam );

   switch( message )
   {
   case WM_PAINT:
      s_sidebarPaint( sidebar, side );
      return 0;

   case WM_SIZE:
      s_sidebarLayout( sidebar );
      break;

   case WM_NOTIFY:
      if( header->code == NM_CUSTOMDRAW && s_sidebarBar( side, header->hwndFrom ) )
         return s_sidebarDraw( sidebar, side, ( NMTBCUSTOMDRAW * )lParam );

      break;

   case WM_COMMAND:
      if( lParam != 0 && s_sidebarCommand( sidebar, side, ( HWND )lParam, LOWORD( wParam ) ) )
         return 0;

      break;

   case WM_SETCURSOR:
      GetCursorPos( &point );
      ScreenToClient( sidebar, &point );
      if( ( HWND )wParam == sidebar && LOWORD( lParam ) == HTCLIENT && s_sidebarOnEdge( sidebar, side, point.x ) )
      {
         SetCursor( LoadCursorW( NULL, IDC_SIZEWE ) );
         return TRUE;
      }
      break;

   case WM_LBUTTONDOWN:
      if( s_sidebarOnEdge( sidebar, side, ( short )LOWORD( lParam ) ) )
      {
         side->drag = 1;
         SetCapture( sidebar );
         return 0;
      }
      break;

   case WM_MOUSEMOVE:
      if( side->drag )
      {
         s_sidebarDrag( sidebar, side, ( short )LOWORD( lParam ) );
         return 0;
      }
      break;

   case WM_LBUTTONUP:
      if( side->drag )
      {
         ReleaseCapture();
         return 0;
      }
      break;

   case WM_CAPTURECHANGED:
      side->drag = 0;
      break;

   case WM_NCDESTROY:
      s_sidebarRelease( sidebar );
      RemoveWindowSubclass( sidebar, s_sidebarProc, id );
      break;
   }

   return DefSubclassProc( sidebar, message, wParam, lParam );
}

/* gui_Sidebar():New( oWindow ): a sidebar without pages at the left edge of the window; the panels filling the
   window make room for it */
void x_guiSidebarNew( X_CALL_CTX *ctx )
{
   const X_ITEM *owner = x_paritem( ctx, 1 );
   HWND parent = x_guiObjectWindow( ctx, owner, "GUI_SIDEBAR:NEW" );
   HWND sidebar, activity, buttons;
   X_GUI_SIDEBAR *side;
   DWORD error;

   if( parent == NULL )
      return;

   s_windowSend( ctx, ctx->self, owner, NULL, 1, 1, X_GUI_WINDOW_PANEL, 0 );
   sidebar = s_objectHandle( ctx, ctx->self );
   if( sidebar == NULL || !IsWindow( sidebar ) )
      return;

   x_callbackEnterDll();
   activity = s_sidebarToolbar( sidebar, 1 );
   buttons = IIF( activity != NULL, s_sidebarToolbar( sidebar, 1 ), NULL );
   error = GetLastError();
   if( buttons == NULL )
   {
      s_destroyWindow( sidebar );
      x_callbackLeaveDll();
      x_callbackRaisePending();
      x_guiRaiseWin32( ctx, "CreateWindowExW", error );
      return;
   }

   side = ( X_GUI_SIDEBAR * )x_xgrab( sizeof( X_GUI_SIDEBAR ) );
   memset( side, 0, sizeof( *side ) );
   side->activity = activity;
   side->buttons = buttons;
   side->width = X_GUI_SIDEBAR_WIDTH;
   side->colors[X_GUI_SIDEBAR_COLOR_BAR] = -1;
   side->colors[X_GUI_SIDEBAR_COLOR_ACCENT] = -1;
   SetPropW( sidebar, s_sidebarProperty, side );
   SetWindowSubclass( sidebar, s_sidebarProc, 1, 0 );
   s_sidebarResize( sidebar );
   x_callbackLeaveDll();
   x_callbackRaisePending();
}

/* Add a page: its panel, the bar of its actions and its picture on the bar of pages with the title in the tip
   without cToolTip; arguments cTitle, cBitmap, cToolTip and cShortcut as in oSidebar:Add(); with a menu the panel
   shows its items; returns the number of the page, 0 with an error raised */
int x_guiSidebarAdd( X_CALL_CTX *ctx, HWND sidebar, X_ITEM *page, HMENU menu )
{
   X_GUI_SIDEBAR *side = s_sidebarOf( sidebar );
   const X_ITEM *toolTip = IIF( X_ISNIL( ctx, 3 ), x_paritem( ctx, 1 ), x_paritem( ctx, 3 ) );
   X_ITEM empty __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   wchar_t *title = x_guiText( ctx, x_paritem( ctx, 1 ), "GUI_SIDEBAR:ADD" );
   HWND actions, panel;
   DWORD error;

   if( title == NULL )
      return 0;

   x_callbackEnterDll();
   actions = s_sidebarToolbar( sidebar, 0 );
   error = GetLastError();
   x_callbackLeaveDll();
   if( actions == NULL )
   {
      x_xfree( title );
      x_guiRaiseWin32( ctx, "CreateWindowExW", error );
      return 0;
   }

   s_windowSend( ctx, page, ctx->self, NULL, 10, 10, X_GUI_WINDOW_PAGE, ( int )side->count + 1 );
   panel = s_objectHandle( ctx, page );
   x_itemPutC( &empty, "" );
   if( panel == NULL || !IsWindow( panel ) )
   {
      x_xfree( title );
      x_guiControlDestroy( actions );
      return 0;
   }

   if( !x_guiToolbarAdd( ctx, side->activity, &empty, x_paritem( ctx, 2 ), toolTip, x_paritem( ctx, 4 ), 0 ) )
   {
      x_xfree( title );
      x_guiControlDestroy( actions );
      x_guiDestroy( panel );
      return 0;
   }

   side->pages = ( X_GUI_SIDEPAGE * )x_xrealloc( side->pages, ( side->count + 1 ) * sizeof( X_GUI_SIDEPAGE ) );
   side->pages[side->count] = ( X_GUI_SIDEPAGE ){ panel, actions, title };
   ++side->count;
   if( menu != NULL )
      x_guiSideMenu( panel, menu );

   x_callbackEnterDll();
   s_sidebarLayout( sidebar );
   x_callbackLeaveDll();
   x_callbackRaisePending();

   return ( int )side->count;
}

/* Return the panel of page nPage */
void x_guiSidebarPanel( X_CALL_CTX *ctx, HWND sidebar, int page )
{
   const X_GUI_SIDEBAR *side = s_sidebarOf( sidebar );
   const X_ITEM *object = NULL;

   if( page >= 1 && ( size_t )page <= side->count )
      object = ( const X_ITEM * )GetWindowLongPtrW( side->pages[page - 1].panel, GWLP_USERDATA );

   if( object == NULL )
      x_guiRaise( ctx, "GUI_SIDEBAR:PANEL" );
   else
      x_itemReturn( ctx, object );
}

/* Add a button with a picture: to the bottom of the strip for page 0, otherwise to the header of the page; returns
   its number on its bar, 0 with an error raised */
int x_guiSidebarAddButton( X_CALL_CTX *ctx, HWND sidebar, int page, const X_ITEM *bitmap, const X_ITEM *toolTip )
{
   const X_GUI_SIDEBAR *side = s_sidebarOf( sidebar );
   X_ITEM empty __attribute__( ( cleanup( x_itemClear ) ) ) = X_ITEM_NIL_INIT;
   HWND toolbar;
   int button;

   if( page < 0 || ( size_t )page > side->count )
   {
      x_guiRaise( ctx, "GUI_SIDEBAR:ADDACTION" );
      return 0;
   }

   toolbar = IIF( page == 0, side->buttons, side->pages[page - 1].actions );
   x_itemPutC( &empty, "" );
   button = x_guiToolbarAdd( ctx, toolbar, &empty, bitmap, toolTip, NULL, 0 );
   if( button != 0 )
   {
      x_callbackEnterDll();
      s_sidebarLayout( sidebar );
      x_callbackLeaveDll();
      x_callbackRaisePending();
   }

   return button;
}

/* The number of the shown page, 0 when the panel is hidden */
int x_guiSidebarPage( HWND sidebar )
{
   return s_sidebarOf( sidebar )->page;
}

/* Show page nPage, 0 hides the panel */
void x_guiSidebarSelect( X_CALL_CTX *ctx, HWND sidebar, int page )
{
   X_GUI_SIDEBAR *side = s_sidebarOf( sidebar );

   if( page < 0 || ( size_t )page > side->count )
   {
      x_guiRaise( ctx, "GUI_SIDEBAR:PAGE" );
      return;
   }

   x_callbackEnterDll();
   s_sidebarShow( sidebar, side, page );
   x_callbackLeaveDll();
   x_callbackRaisePending();
}

/* The width of the panel at 96 DPI */
int x_guiSidebarWidth( HWND sidebar )
{
   return s_sidebarOf( sidebar )->width;
}

/* Set the width of the panel at 96 DPI, not less than the smallest one */
void x_guiSidebarSetWidth( HWND sidebar, int width )
{
   X_GUI_SIDEBAR *side = s_sidebarOf( sidebar );

   side->width = IIF( width < X_GUI_SIDEBAR_MINWIDTH, X_GUI_SIDEBAR_MINWIDTH, width );
   x_callbackEnterDll();
   s_sidebarResize( sidebar );
   x_callbackLeaveDll();
   x_callbackRaisePending();
}

/* Return a color of a sidebar, NIL for the system color */
void x_guiSidebarColor( X_CALL_CTX *ctx, X_GUI_SIDEBAR_COLOR part )
{
   HWND sidebar = x_guiSelfHandle( ctx, "GUI_SIDEBAR:COLOR" );
   const X_GUI_SIDEBAR *side = IIF( sidebar != NULL, s_sidebarOf( sidebar ), NULL );

   if( side != NULL && side->colors[part] >= 0 )
      x_retni( ctx, side->colors[part] );
}

/* Set a color of a sidebar, NIL for the system color */
void x_guiSidebarSetColor( X_CALL_CTX *ctx, X_GUI_SIDEBAR_COLOR part )
{
   HWND sidebar = x_guiSelfHandle( ctx, "GUI_SIDEBAR:COLOR" );
   X_GUI_SIDEBAR *side = IIF( sidebar != NULL, s_sidebarOf( sidebar ), NULL );

   if( side == NULL )
      return;

   if( !X_ISNIL( ctx, 1 ) && ( !X_ISNUM( ctx, 1 ) || x_parnint( ctx, 1 ) < 0 || x_parnint( ctx, 1 ) > 0xFFFFFF ) )
   {
      x_guiRaise( ctx, "GUI_SIDEBAR:COLOR" );
      return;
   }

   side->colors[part] = IIF( X_ISNIL( ctx, 1 ), -1, ( int )x_parnint( ctx, 1 ) );
   RedrawWindow( sidebar, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN );
   x_itemReturn( ctx, x_paritem( ctx, 1 ) );
}

/* ---------------------------------------------------------------------------------------------
   Dialogs
   --------------------------------------------------------------------------------------------- */

/* A message box over the active window; the Yes/No kind returns .T. for Yes */
void x_guiMessage( X_CALL_CTX *ctx, X_GUI_MESSAGE kind )
{
   static const UINT styles[] = { 0, MB_OK | MB_ICONINFORMATION, MB_OK | MB_ICONWARNING, MB_OK | MB_ICONERROR,
                                  MB_YESNO | MB_ICONQUESTION };
   HWND owner = GetActiveWindow();
   wchar_t ownerTitle[256];
   wchar_t *text, *title = NULL;
   int answer;
   DWORD error;
   UINT style;

   text = x_guiText( ctx, x_paritem( ctx, 1 ), "GUIMESSAGE" );
   if( text == NULL )
      return;

   if( X_ISCHAR( ctx, 2 ) )
   {
      title = x_guiText( ctx, x_paritem( ctx, 2 ), "GUIMESSAGE" );
      if( title == NULL )
      {
         x_xfree( text );
         return;
      }
   }

   ownerTitle[0] = L'\0';
   if( title == NULL && owner != NULL )
      GetWindowTextW( owner, ownerTitle, sizeof( ownerTitle ) / sizeof( ownerTitle[0] ) );

   style = styles[kind] | IIF( owner == NULL, MB_TASKMODAL, 0 );
   x_callbackEnterDll();
   answer = MessageBoxW( owner, text, IIF( title != NULL, title, ownerTitle ), style );
   error = GetLastError();
   x_callbackLeaveDll();
   x_xfree( text );
   if( title != NULL )
      x_xfree( title );

   x_callbackRaisePending();
   if( answer == 0 )
      x_guiRaiseWin32( ctx, "MessageBoxW", error );
   else if( kind == X_GUI_MESSAGE_YESNO )
      x_retl( ctx, answer == IDYES );
}

/* Append a text to the double zero ended list of filters */
static int s_appendFilter( X_CALL_CTX *ctx, const X_ITEM *item, wchar_t **list, size_t *length )
{
   wchar_t *wide = x_guiText( ctx, item, "GUIFILEDIALOG" );
   size_t count;

   if( wide == NULL )
      return 0;

   count = wcslen( wide );
   if( count == 0 )
   {
      x_xfree( wide );
      x_guiRaise( ctx, "GUIFILEDIALOG" );
      return 0;
   }

   *list = ( wchar_t * )x_xrealloc( *list, ( *length + count + 2 ) * sizeof( wchar_t ) );
   memcpy( *list + *length, wide, count * sizeof( wchar_t ) );
   *length += count;
   ( *list )[( *length )++] = L'\0';
   ( *list )[*length] = L'\0';
   x_xfree( wide );

   return 1;
}

/* The filters { { cName, cPattern }, ... } as the list of OPENFILENAMEW; *valid is 0 with an error raised */
static wchar_t *s_filters( X_CALL_CTX *ctx, const X_ITEM *filters, int *valid )
{
   wchar_t *list = NULL;
   size_t length = 0;
   int64_t i;

   *valid = 1;
   if( filters == NULL || X_IS_NIL( filters ) )
      return NULL;

   *valid = X_IS_ARRAY( filters ) && x_arrayLen( filters ) > 0;
   if( !*valid )
      x_guiRaise( ctx, "GUIFILEDIALOG" );

   for( i = 1; *valid && i <= x_arrayLen( filters ); ++i )
   {
      const X_ITEM *filter = x_arrayGetItemPtr( filters, ( size_t )i );

      *valid = X_IS_ARRAY( filter ) && x_arrayLen( filter ) == 2;
      if( !*valid )
         x_guiRaise( ctx, "GUIFILEDIALOG" );
      else
      {
         *valid = s_appendFilter( ctx, x_arrayGetItemPtr( filter, 1 ), &list, &length );
         if( *valid )
            *valid = s_appendFilter( ctx, x_arrayGetItemPtr( filter, 2 ), &list, &length );
      }
   }

   if( !*valid && list != NULL )
   {
      x_xfree( list );
      list = NULL;
   }

   return list;
}

/* The dialog to open or, with save, to save a file; returns the file or "" when cancelled */
void x_guiFileDialog( X_CALL_CTX *ctx, int save )
{
   enum { FILE_CAPACITY = 32768 };
   OPENFILENAMEW dialog;
   wchar_t *title = NULL, *initial = NULL, *filters, *file, extension[16];
   const wchar_t *pattern;
   int valid, done;
   DWORD error;
   size_t i, size;
   char *text;

   filters = s_filters( ctx, x_paritem( ctx, 2 ), &valid );
   if( !valid )
      return;

   if( X_ISCHAR( ctx, 1 ) )
   {
      title = x_guiText( ctx, x_paritem( ctx, 1 ), "GUIFILEDIALOG" );
      valid = title != NULL;
   }

   if( valid && X_ISCHAR( ctx, 3 ) )
   {
      initial = x_guiText( ctx, x_paritem( ctx, 3 ), "GUIFILEDIALOG" );
      valid = initial != NULL;
   }

   if( valid && initial != NULL && wcslen( initial ) >= FILE_CAPACITY )
   {
      x_guiRaise( ctx, "GUIFILEDIALOG" );
      valid = 0;
   }

   if( !valid )
   {
      if( filters != NULL )
         x_xfree( filters );

      if( title != NULL )
         x_xfree( title );

      if( initial != NULL )
         x_xfree( initial );
      return;
   }

   file = ( wchar_t * )x_xgrabzArray( FILE_CAPACITY, sizeof( wchar_t ) );
   if( initial != NULL )
      wcscpy( file, initial );

   extension[0] = L'\0';
   pattern = IIF( filters != NULL, filters + wcslen( filters ) + 1, NULL );
   if( save && pattern != NULL && pattern[0] == L'*' && pattern[1] == L'.' && pattern[2] != L'*' )
   {
      for( i = 0; i + 1 < sizeof( extension ) / sizeof( extension[0] ); ++i )
      {
         if( pattern[i + 2] == L'\0' || pattern[i + 2] == L';' )
            break;

         extension[i] = pattern[i + 2];
      }

      extension[i] = L'\0';
   }

   memset( &dialog, 0, sizeof( dialog ) );
   dialog.lStructSize = sizeof( dialog );
   dialog.hwndOwner = GetActiveWindow();
   dialog.lpstrFilter = filters;
   dialog.nFilterIndex = 1;
   dialog.lpstrFile = file;
   dialog.nMaxFile = FILE_CAPACITY;
   dialog.lpstrTitle = title;
   dialog.lpstrDefExt = IIF( extension[0] != L'\0', extension, NULL );
   dialog.Flags = OFN_EXPLORER | OFN_HIDEREADONLY | OFN_PATHMUSTEXIST;
   dialog.Flags |= IIF( save, OFN_OVERWRITEPROMPT, OFN_FILEMUSTEXIST );

   x_callbackEnterDll();
   done = IIF( save, GetSaveFileNameW( &dialog ), GetOpenFileNameW( &dialog ) );
   error = CommDlgExtendedError();
   x_callbackLeaveDll();
   if( filters != NULL )
      x_xfree( filters );

   if( title != NULL )
      x_xfree( title );

   if( initial != NULL )
      x_xfree( initial );

   if( done )
   {
      text = x_utf16ToUtf8Alloc( ( const uint16_t * )file, wcslen( file ), &size );
      x_xfree( file );
      x_callbackRaisePending();
      x_retclen_buffer( ctx, text, size );
      return;
   }

   x_xfree( file );
   x_callbackRaisePending();
   if( error != 0 )
      x_guiRaiseWin32( ctx, IIF( save, "GetSaveFileNameW", "GetOpenFileNameW" ), error );
   else
      x_retc( ctx, "" );
}

/* The file opens in the program the system has for its type, e.g. a PDF in the PDF viewer */
void x_guiOpenFile( X_CALL_CTX *ctx )
{
   wchar_t *file;
   HINSTANCE done;

   if( !X_ISCHAR( ctx, 1 ) )
   {
      x_guiRaise( ctx, "GUI_OPENFILE" );
      return;
   }

   file = x_guiText( ctx, x_paritem( ctx, 1 ), "GUI_OPENFILE" );
   if( file == NULL )
      return;

   x_callbackEnterDll();
   done = ShellExecuteW( GetActiveWindow(), L"open", file, NULL, NULL, SW_SHOWNORMAL );
   x_callbackLeaveDll();
   x_xfree( file );
   x_callbackRaisePending();
   x_retl( ctx, ( INT_PTR )done > 32 );
}

/* ---------------------------------------------------------------------------------------------
   Module registration and shutdown
   --------------------------------------------------------------------------------------------- */

/* Collect a window of the library (EnumThreadWindows) */
static BOOL CALLBACK s_collectWindow( HWND window, LPARAM list )
{
   HWND **cursor = ( HWND ** )list;

   if( ( ATOM )GetClassLongPtrW( window, GCW_ATOM ) == s_windowClass )
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
   if( s_windowClass != 0 )
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

   while( s_actionCount > 0 )
   {
      --s_actionCount;
      x_itemRelease( s_actions[s_actionCount].action );
      x_itemRelease( s_actions[s_actionCount].when );
   }

   if( s_actions != NULL )
   {
      x_xfree( s_actions );
      s_actions = NULL;
      s_actionCapacity = 0;
   }

   while( s_fontCount > 0 )
      DeleteObject( s_fonts[--s_fontCount].font );
}

/* Register the module of the library before Main() runs */
__attribute__( ( constructor( 65534 ) ) ) static void s_moduleRegister( void )
{
   x_moduleRegister( s_moduleNone, s_moduleNone, s_moduleExit, s_moduleNone );
}
