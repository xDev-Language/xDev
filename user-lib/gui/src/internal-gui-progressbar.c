/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev GUI
 * File......: internal-gui-progressbar.c
 *
 */

#include "internal-gui.h"

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
