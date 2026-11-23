/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev WebGUI
 * File......: internal-webgui.h
 *
 */

#ifndef INTERNAL_WEBGUI_H
#define INTERNAL_WEBGUI_H

#ifndef UNICODE
   #define UNICODE
#endif
#ifndef _UNICODE
   #define _UNICODE
#endif

#include <windows.h>

#include "platform.h"
#include "internal.h"
#include "classes.h"

#if !X_OS_WINDOWS
   #error "xDev WebGUI requires Windows"
#endif

/* ---------------------------------------------------------------------------------------------
   Types
   --------------------------------------------------------------------------------------------- */

/* The data of a wgControl, in the order of its class */
typedef enum
{
   X_WEBGUI_PARENT = 1,
   X_WEBGUI_ID,
   X_WEBGUI_KIND,
   X_WEBGUI_TEXT,
   X_WEBGUI_VALUE,
   X_WEBGUI_ENABLED,
   X_WEBGUI_VISIBLE,
   X_WEBGUI_ONCLICK,
   X_WEBGUI_ONCHANGE,
   X_WEBGUI_COLUMNS,
   X_WEBGUI_ROWS,
   X_WEBGUI_ACTIONS,
   X_WEBGUI_X,
   X_WEBGUI_Y,
   X_WEBGUI_WIDTH,
   X_WEBGUI_HEIGHT,
   X_WEBGUI_GROW,
   X_WEBGUI_CONTROL_DATA = X_WEBGUI_GROW
} X_WEBGUI_SLOT;

/* ---------------------------------------------------------------------------------------------
   Windows
   --------------------------------------------------------------------------------------------- */

void x_webguiRaise( X_CALL_CTX *ctx, const char *operation );
HWND x_webguiSelfHandle( X_CALL_CTX *ctx, const char *operation );
void x_webguiWindowNew( X_CALL_CTX *ctx );
int x_webguiWindowOpen( X_CALL_CTX *ctx, HWND window, const X_ITEM *file );
void x_webguiWindowSend( X_CALL_CTX *ctx, HWND window, const X_ITEM *json );
void x_webguiWindowRun( X_CALL_CTX *ctx, HWND window );
void x_webguiWindowClose( HWND window );

/* ---------------------------------------------------------------------------------------------
   Controls
   --------------------------------------------------------------------------------------------- */

int x_webguiParentValid( X_CALL_CTX *ctx );
void x_webguiControlSend( X_CALL_CTX *ctx, const char *kind, const X_ITEM *text, const X_ITEM *value,
                          const X_ITEM *items );
void x_webguiControlCreate( X_CALL_CTX *ctx );
void x_webguiControlSet( X_CALL_CTX *ctx, const char *property, int slot, const char *operation );
void x_webguiControlCommand( X_CALL_CTX *ctx, int destroy );
void x_webguiControlEvent( X_CALL_CTX *ctx );
void x_webguiForward( X_CALL_CTX *ctx, const char *message, int count );

/* ---------------------------------------------------------------------------------------------
   Browses
   --------------------------------------------------------------------------------------------- */

void x_webguiBrowseColumn( X_CALL_CTX *ctx );
void x_webguiBrowseRows( X_CALL_CTX *ctx );
void x_webguiBrowseRefresh( X_CALL_CTX *ctx );

/* ---------------------------------------------------------------------------------------------
   Menus
   --------------------------------------------------------------------------------------------- */

void x_webguiMenuItem( X_CALL_CTX *ctx );

/* ---------------------------------------------------------------------------------------------
   Sidebars
   --------------------------------------------------------------------------------------------- */

int x_webguiSidebarAdd( X_CALL_CTX *ctx, X_ITEM *panel );
void x_webguiSidebarPanel( X_CALL_CTX *ctx, int page );
void x_webguiSidebarButton( X_CALL_CTX *ctx, int first );
void x_webguiSidebarAction( X_CALL_CTX *ctx );

/* ---------------------------------------------------------------------------------------------
   Splits
   --------------------------------------------------------------------------------------------- */

void x_webguiSplitParts( X_CALL_CTX *ctx, X_ITEM *first, X_ITEM *second );
void x_webguiSplitPanel( X_CALL_CTX *ctx, int part );

#endif
