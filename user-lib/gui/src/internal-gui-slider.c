/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev GUI
 * File......: internal-gui-slider.c
 *
 */

#include "internal-gui.h"

/* ---------------------------------------------------------------------------------------------
   Sliders
   --------------------------------------------------------------------------------------------- */

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
