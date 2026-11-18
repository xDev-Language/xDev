/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library: xDev GTXWC
 * File: gtxwc.h
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef GTXWC_H
#define GTXWC_H

#include "terminal.h"
#include "gtinfo.h"
#include "gfxdef.h"
#include <xdev/inkey.ch>
#include <xdev/setcurs.ch>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>

#include <stddef.h>
#include <stdint.h>

#define X_GT_NAME XWC

#define X_GTXWC_DEFAULT_ROWS 25
#define X_GTXWC_DEFAULT_COLS 80
#define X_GTXWC_MIN_ROWS 1
#define X_GTXWC_MIN_COLS 1
#define X_GTXWC_MAX_ROWS 256
#define X_GTXWC_MAX_COLS 256
#define X_GTXWC_KEY_QUEUE_SIZE 128
#define X_GTXWC_PREEDIT_MAX 65535
#define X_GTXWC_FONT_WIDTH 9
#define X_GTXWC_FONT_HEIGHT 18
#define X_GTXWC_FONT_NAME "-*-fixed-medium-r-normal--18-*-*-*-c-90-iso10646-1"

#define GTXWC_MAX( left, right ) ( ( left ) > ( right ) ? ( left ) : ( right ) )
#define GTXWC_MIN( left, right ) ( ( left ) < ( right ) ? ( left ) : ( right ) )

typedef struct
{
   int top;
   int left;
   int bottom;
   int right;
} X_GTXWC_RECT;

typedef struct
{
   uint8_t  loaded[256];
   uint16_t bits[256][X_GTXWC_FONT_HEIGHT];
} X_GTXWC_FONT_PAGE;

typedef struct
{
   uint32_t    code;
   XIMFeedback feedback;
} X_GTXWC_PREEDIT_CELL;

typedef struct
{
   X_GT_BASE                 *pGT;
   Display                   *display;
   int                        screen;
   Visual                    *visual;
   Colormap                   colormap;
   int                        ownColormap;
   unsigned long             *pixelMap;
   Window                     window;
   GC                         gc;
   Atom                       wmDelete;
   Atom                       wmProtocols;
   Atom                       netWmState;
   Atom                       netWmFullscreen;
   Atom                       netWmMaximizedVert;
   Atom                       netWmMaximizedHorz;
   Atom                       netWmAbove;
   Atom                       netWmOpacity;
   Atom                       netWmName;
   Atom                       utf8String;
   Atom                       motifWmHints;
   Atom                       netWmIcon;
   Atom                       wmState;
   Atom                       netFrameExtents;
   XIM                        inputMethod;
   XIC                        inputContext;
   XIM                        pendingInputMethod;
   XIM                        closingInputMethod;
   long                       eventMask;
   int                        inputRegistered;
   int                        inputRetry;
   int                        inputFallback;
   int                        inputOpening;
   int                        inputClosing;
   X_GTXWC_PREEDIT_CELL      *preedit;
   int                        preeditLength;
   int                        preeditCaret;
   XIMCaretStyle              preeditCaretStyle;
   int                        preeditActive;
   int                        preeditVisible;
   struct _X_GTXWC_CLIPBOARD *clipboard;
   struct _X_GTXWC_DIALOG    *dialog;
   int                        msgAlert;
   XImage                    *image;
   char                      *imageData;
   Pixmap                     framePixmap;
   int                        frameWidth;
   int                        frameHeight;
   int                        windowWidth; /* window size from the last resize, the strip outside the grid */
   int                        windowHeight;
   XFontStruct               *font;
   char                      *fontName;
   char                      *fontSel;
   int                        fontWeight;
   int                        fontAttribute;
   Pixmap                     fontPixmap;
   GC                         fontGC;
   X_GTXWC_FONT_PAGE         *fontPages[256];

   uint32_t *canvas; /* 0x00RRGGBB, shared by cells and x_gfx.* */
   int       canvasWidth;
   int       canvasHeight;
   int       rows;
   int       cols;
   int       cellWidth;
   int       cellHeight;
   uint8_t   colors[16][3];

   char         *title;
   Pixmap        iconPixmap;
   Pixmap        iconMask;
   int           windowX;
   int           windowY;
   int           pendingWidth;
   int           pendingHeight;
   unsigned long resizeSerial;
   int           desktopWidth;
   int           desktopHeight;
   int           desktopDepth;

   int         *keys;
   int          keyCapacity;
   int          keyIn;
   int          keyOut;
   int          keyFlags;
   unsigned int altMask;
   unsigned int capsLockMask;
   unsigned int numLockMask;
   unsigned int scrollLockMask;

   int          mouseX;
   int          mouseY;
   int          mouseRow;
   int          mouseCol;
   unsigned int mouseButtons;
   Time         clickTime[3];
   int          clickX[3];
   int          clickY[3];
   int          doubleClickDistance;
   int          clickPending[3];
   unsigned int consumedButtons;

   int selectionAnchor;
   int selectionEnd;
   int selectCopy;
   int selecting;
   int selectionActive;

   int    cursorRow;
   int    cursorCol;
   int    cursorStyle;
   int    cursorBlinkRate;
   double cursorTime;
   int    cursorVisible;

   int clipTop;
   int clipLeft;
   int clipBottom;
   int clipRight;
   int gfxDepth;
   int gfxMode;
   int closeMode;
   int resizeMode;

   int clipOn;
   int resizable;
   int resizeStep;
   int sizeHintsPending;
   int resizeRequested;
   int altEnter;
   int mouseOn;
   int mapped;
   int dirty;
   int frameEnding;
   int deferResizePaint;
   int resizeFramePending;
   int resizeNotifyPending;
   int focused;
} X_GTXWC;

#endif /* GTXWC_H */
