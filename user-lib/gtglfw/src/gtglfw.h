/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev GTGLFW
 * File......: gtglfw.h
 *
 */

#ifndef X_GTGLFW_H
#define X_GTGLFW_H

#include "terminal.h"
#include "gtinfo.h"
#include "gfxdef.h"
#include <xdev/inkey.ch>
#include <xdev/setcurs.ch>
#include "platform.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <stddef.h>
#include <stdint.h>

#define X_GT_NAME GLFW

#define X_GTGLFW_DEFAULT_ROWS 25
#define X_GTGLFW_DEFAULT_COLS 80
#define X_GTGLFW_MIN_ROWS 1
#define X_GTGLFW_MIN_COLS 1
#define X_GTGLFW_MAX_ROWS 256
#define X_GTGLFW_MAX_COLS 256
#define X_GTGLFW_KEY_QUEUE_SIZE 1024
#define X_GTGLFW_CELL_WIDTH 9
#define X_GTGLFW_CELL_HEIGHT 18

typedef struct _X_GTGLFW
{
   X_GT_BASE        *gt;
   struct _X_GTGLFW *next;
   GLFWwindow       *window;
   char             *title;

   uint32_t    *canvas;
   uint8_t     *frame;
   unsigned int texture;
   int          canvasWidth;
   int          canvasHeight;
   int          frameWidth;
   int          frameHeight;
   int          rows;
   int          cols;
   int          windowWidth;
   int          windowHeight;
   int          windowX;
   int          windowY;
   int          framebufferWidth;
   int          framebufferHeight;
   float        contentScaleX;
   float        contentScaleY;
   uint8_t      colors[16][3];

   int    cursorRow;
   int    cursorCol;
   int    cursorStyle;
   int    cursorBlinkRate;
   double cursorTime;
   int    cursorVisible;

   int          mouseX;
   int          mouseY;
   int          mouseRow;
   int          mouseCol;
   unsigned int mouseButtons;
   double       clickTime[3];
   double       wheelX;
   double       wheelY;

   int     keyQueue[X_GTGLFW_KEY_QUEUE_SIZE];
   int     keyScancode[X_GTGLFW_KEY_QUEUE_SIZE];
   uint8_t keyRepeat[X_GTGLFW_KEY_QUEUE_SIZE];
   int     keyIn;
   int     keyOut;
   int     keyFlags;
   int     keyMods;
   int     textScancode;
   int     textFlags;
   int     textRepeat;

   int selectionAnchor;
   int selectionEnd;
   int selectCopy;
   int selecting;
   int selectionActive;
   int selectionSkipUp;

   int clipTop;
   int clipLeft;
   int clipBottom;
   int clipRight;
   int gfxDepth;
   int gfxMode;

   int closeMode;
   int resizeMode;
   int savedX;
   int savedY;
   int savedWidth;
   int savedHeight;
   int clipOn;
   int resizable;
   int resizeStep;
   int windowResizePending;
   int resizePending;
   int resizeActive;
   int closePending;
   int cursorInside;
   int altEnter;
   int msgAlert;
   int mouseOn;
   int focused;
   int minimized;
   int maximized;
   int fullscreen;
   int canvasDirty;
   int presentPending;
   int presentBothBuffers;
#if X_OS_WINDOWS
   void *iconHandle;
   int   iconOwned;
#endif
} X_GTGLFW;

#endif /* X_GTGLFW_H */
