/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library: xDev GTWVT
 * File: gtwvt.h
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef X_GTWVT_H
#define X_GTWVT_H

#include "terminal.h"
#include "gtinfo.h"
#include "gfxdef.h"
#include <xdev/inkey.ch>
#include <xdev/setcurs.ch>

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <mmsystem.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <wincodec.h>

#include <stdint.h>
#include <stddef.h>

#define X_GT_NAME WVT

#define WVT_MAX( left, right ) ( ( left ) > ( right ) ? ( left ) : ( right ) )
#define WVT_MIN( left, right ) ( ( left ) < ( right ) ? ( left ) : ( right ) )

#define WVT_MAX_WINDOWS 256
#define WVT_DEFAULT_ROWS 25
#define WVT_DEFAULT_COLS 80
#define WVT_DEFAULT_FONT_HEIGHT 20
#define WVT_DEFAULT_FONT_WIDTH 10
#define WVT_DEFAULT_FONT_ATTR X_GTI_FONTA_DRAWBOX /* box characters fill the cells, as in GTSDL3 */
#define WVT_DEFAULT_FONT_NAME L"Courier New"
#define WVT_KEY_QUEUE_SIZE 128
#define WVT_CARET_TIMER 1 /* the cursor blinks on its own timer, Windows stops caret blinking */

#define WVT_EXTKEY_FLAG ( 1 << 24 )
#define SYS_EV_MARK 1000

typedef struct _X_GTWVT_MENU
{
   int                   key;
   int                   event;
   wchar_t              *name;
   struct _X_GTWVT_MENU *next;
} X_GTWVT_MENU;

/* X_GTI_MSGBOX dialog: the dialog procedure builds the controls from this description */
typedef struct
{
   const wchar_t *title;
   const wchar_t *text;
   wchar_t      **buttons;
   int            count;
   HFONT          font;
} X_GTWVT_MSGBOX;

#define WVT_MSGBOX_FIRST_ID 100

typedef struct
{
   X_GT_BASE *gt;
   int        number; /* window number */

   HINSTANCE instance; /* parent window instance */
   int       showCommand;

   int rows;
   int cols;

   uint8_t colors[16][3]; /* palette, the same layout as GTSDL3 */
   RECT    caretRect;     /* cursor drawn on the window, empty when hidden */
   int     caretOn;       /* blink phase: the cursor is drawn */
   int     caretFocus;    /* the window has the keyboard focus */
   int     caretBlink;    /* X_GTI_CURSORBLINKRATE: ms shown, then hidden; 0 does not blink */

   POINT mousePos;

   int keys[WVT_KEY_QUEUE_SIZE]; /* characters and events */
   int keyIn;                    /* offset of the next free slot */
   int keyOut;                   /* offset of the next character to read */
   int keyLast;                  /* last inkey code position in buffer */
   int keyFlags;                 /* keyboard modifiers */

   int     dpi;      /* dots per inch of the monitor; program pixels are 1/96 inch */
   POINT   cellSize; /* size of one character cell in pixels of the window */
   int     fontHeight;
   int     fontWidth;
   int     fontWeight; /* bold level */
   int     fontQuality;
   int     fontAttribute; /* font attribute: X_GTI_FONTA_* */
   wchar_t fontFace[LF_FACESIZE];
   HFONT   font;

   HWND window;
   int  openRequested; /* the window should be opened */
   int  quitRequested; /* a quit was requested inside the window procedure */

   HICON icon;      /* title bar and task list icon */
   HICON ownedIcon; /* icon loaded by this driver */

   wchar_t *title;

   int centred; /* centre the window on the work area */
   int ignoreSysChar;

   int resizable;
   int maximized; /* the window has been maximized */
   int fullScreen;
   int altEnter; /* Alt+Enter switches full screen mode */
   int marginTop;
   int marginLeft;

   int newX;
   int newY;

   int marking; /* console like mark and copy operation */
   int markStarted;

   int      selectCopy; /* X_GTI_SELECTCOPY: "Mark" and Shift with the left button select */
   int      skipUp;     /* the button release of a cancelled selection is skipped */
   wchar_t *selectCopyText;

   X_GTWVT_MENU *menu;

   RECT selectionNew;
   RECT selectionOld;

   int closeMode;
   int resizeMode; /* resizing by FONT or by rows */

   int resizing;
   int sizing;

   RECT lastClient; /* client area at the last WM_SIZE */

   int   mouseOn; /* X_GTI_MOUSESTATUS: mouse events are reported */
   int   mouseX;  /* the last mouse position in canvas pixels */
   int   mouseY;
   int   sizeStep; /* X_GTI_RESIZESTEP: the window size follows whole cells */
   int   msgAlert; /* X_GTI_MSGBOX: Alert() uses the message box */
   float opacity;  /* X_GTI_WINDOWOPACITY */

   HDC       canvasDc; /* canvas shared by text cells and x_gfx*() */
   HBITMAP   canvasBitmap;
   HGDIOBJ   canvasOld;
   uint32_t *pixels; /* 0x00RRGGBB pixels, top-down */
   int       canvasWidth;
   int       canvasHeight;
   int       resized; /* the canvas was recreated and needs the text again */

   int clipTop; /* inclusive x_gfx*() clip rectangle */
   int clipLeft;
   int clipBottom;
   int clipRight;
   int clipOn;
   int gfxDepth;
} X_GTWVT;

#endif /* X_GTWVT_H */
