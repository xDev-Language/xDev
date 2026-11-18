/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library: xDev Runtime
 * File: terminal.h
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef TERMINAL_H
#define TERMINAL_H

#include "internal.h"

/* ---------------------------------------------------------------------------------------------
   Keys and mouse
   --------------------------------------------------------------------------------------------- */

#define X_CLRSTR_LEN 64
#define X_DEFAULT_INKEY_BUFSIZE 50

/* An extended key code: the type in the high byte, then 8 flag bits and 16 value bits */
#define X_INKEY_EXT_BIT 0x40000000
#define X_INKEY_EXT_MASK 0xC0000000
#define X_INKEY_EXT_TYPEMASK 0xFF000000
#define X_INKEY_EXT_VALBITS 16
#define X_INKEY_EXT_VALMASK ( ( 1 << X_INKEY_EXT_VALBITS ) - 1 )
#define X_INKEY_EXT_FLAGMASK ( 0xFF << X_INKEY_EXT_VALBITS )
#define X_INKEY_EXT_KEY 0x01000000
#define X_INKEY_EXT_CHAR 0x02000000
#define X_INKEY_EXT_UNICODE 0x03000000
#define X_INKEY_EXT_MOUSEKEY 0x04000000
#define X_INKEY_EXT_MOUSEPOS 0x05000000
#define X_INKEY_EXT_EVENT 0x06000000
#define X_INKEY_EXT_UNICODEFULL 0x07000000
#define X_INKEY_EXT_POSBITS 12
#define X_INKEY_EXT_POSMASK ( ( 1 << X_INKEY_EXT_POSBITS ) - 1 )

#define X_INKEY_TYPE( key ) ( ( ( key ) ^ X_INKEY_EXT_BIT ) & X_INKEY_EXT_TYPEMASK )

/* New key codes */
#define X_INKEY_EXT_FLAGS( flags ) ( ( ( flags ) << X_INKEY_EXT_VALBITS ) & X_INKEY_EXT_FLAGMASK )
#define X_INKEY_NEW_VALF( value, flags ) ( ( ( value ) & X_INKEY_EXT_VALMASK ) | X_INKEY_EXT_FLAGS( flags ) )
#define X_INKEY_NEW_KEY( key, flags ) ( X_INKEY_NEW_VALF( key, flags ) | X_INKEY_EXT_BIT | X_INKEY_EXT_KEY )
#define X_INKEY_NEW_MKEY( key, flags ) ( X_INKEY_NEW_VALF( key, flags ) | X_INKEY_EXT_BIT | X_INKEY_EXT_MOUSEKEY )

/* A Unicode key: up to U+FFFF with 8 flag bits, above it 21 value bits with 3 flag bits */
#define X_INKEY_NEW_UNICODE16( value, flags ) \
   ( X_INKEY_NEW_VALF( value, flags ) | X_INKEY_EXT_BIT | X_INKEY_EXT_UNICODE )
#define X_INKEY_NEW_UNICODE21( value, flags ) \
   ( ( ( value ) & 0x1FFFFF ) | ( ( ( flags ) & 7 ) << 21 ) | X_INKEY_EXT_BIT | X_INKEY_EXT_UNICODEFULL )
#define X_INKEY_NEW_UNICODEF( value, flags ) \
   IIF( ( value ) > 0xFFFF, X_INKEY_NEW_UNICODE21( value, flags ), X_INKEY_NEW_UNICODE16( value, flags ) )
#define X_INKEY_NEW_UNICODE( value ) X_INKEY_NEW_UNICODEF( value, 0 )

/* A mouse position: y in the high bits, x in the low bits */
#define X_INKEY_EXT_POS( x, y ) \
   ( ( ( ( y ) & X_INKEY_EXT_POSMASK ) << X_INKEY_EXT_POSBITS ) | ( ( x ) & X_INKEY_EXT_POSMASK ) )
#define X_INKEY_NEW_MPOS( x, y ) ( X_INKEY_EXT_POS( x, y ) | X_INKEY_EXT_BIT | X_INKEY_EXT_MOUSEPOS )

/* The parts of a key code */
#define X_INKEY_ISEXT( key ) ( ( ( key ) & X_INKEY_EXT_MASK ) == X_INKEY_EXT_BIT )
#define X_INKEY_ISKEY( key ) ( X_INKEY_TYPE( key ) == X_INKEY_EXT_KEY )
#define X_INKEY_ISCHAR( key ) ( X_INKEY_TYPE( key ) == X_INKEY_EXT_CHAR )
#define X_INKEY_ISUNICODE( key ) \
   ( X_INKEY_TYPE( key ) == X_INKEY_EXT_UNICODE || X_INKEY_TYPE( key ) == X_INKEY_EXT_UNICODEFULL )
#define X_INKEY_ISMOUSEKEY( key ) ( X_INKEY_TYPE( key ) == X_INKEY_EXT_MOUSEKEY )
#define X_INKEY_ISMOUSEPOS( key ) ( X_INKEY_TYPE( key ) == X_INKEY_EXT_MOUSEPOS )
#define X_INKEY_ISEVENT( key ) ( X_INKEY_TYPE( key ) == X_INKEY_EXT_EVENT )
#define X_INKEY_VALUE( key ) ( ( key ) & IIF( X_INKEY_TYPE( key ) == X_INKEY_EXT_UNICODEFULL, 0x1FFFFF, 0xFFFF ) )
#define X_INKEY_FLAGS16( key ) ( ( ( key ) & X_INKEY_EXT_FLAGMASK ) >> X_INKEY_EXT_VALBITS )
#define X_INKEY_FLAGS21( key ) ( ( ( key ) >> 21 ) & 7 )
#define X_INKEY_FLAGS( key ) \
   IIF( X_INKEY_TYPE( key ) == X_INKEY_EXT_UNICODEFULL, X_INKEY_FLAGS21( key ), X_INKEY_FLAGS16( key ) )

#define X_MBUTTON_LEFT 0
#define X_MBUTTON_RIGHT 1
#define X_MBUTTON_MIDDLE 2

/* A cell color that follows the logical palette */
#define X_GT_RGB_DEFAULT UINT32_MAX

#ifdef __cplusplus
extern "C"
{
#endif

/* ---------------------------------------------------------------------------------------------
   Screen drivers (GT)
   --------------------------------------------------------------------------------------------- */

typedef struct X_GT_BASE X_GT_BASE;

/* One screen cell */
typedef struct
{
   uint16_t character;
   uint8_t color;
   uint8_t attr;
   uint32_t foreground; /* 0xRRGGBB or X_GT_RGB_DEFAULT for the logical palette */
   uint32_t background; /* 0xRRGGBB or X_GT_RGB_DEFAULT for the logical palette */
} X_GT_CELL;

/* The values of one GtInfo() call */
typedef struct
{
   X_ITEM *pNewVal;
   X_ITEM *pNewVal2;
   X_ITEM *pResult;
} X_GT_INFO;

/* The functions of a driver; a driver takes over the ones it implements */
typedef struct
{
   void ( *Init )( X_GT_BASE *, int, int, int );
   void ( *Exit )( X_GT_BASE * );
   int ( *Resize )( X_GT_BASE *, int, int );
   int ( *SetMode )( X_GT_BASE *, int, int );
   void ( *GetSize )( X_GT_BASE *, int *, int * );
   void ( *SemiCold )( X_GT_BASE * );
   void ( *ExposeArea )( X_GT_BASE *, int, int, int, int );
   void ( *Redraw )( X_GT_BASE *, int, int, int );
   void ( *RedrawDiff )( X_GT_BASE * );
   void ( *Refresh )( X_GT_BASE * );
   void ( *Flush )( X_GT_BASE * );
   void ( *SetPos )( X_GT_BASE *, int, int );
   void ( *SetCursorStyle )( X_GT_BASE *, int );
   void ( *GetScrCursor )( X_GT_BASE *, int *, int *, int * );
   int ( *GetScrChar )( X_GT_BASE *, int, int, int *, uint8_t *, uint16_t * );
   void ( *DispBegin )( X_GT_BASE * );
   void ( *DispEnd )( X_GT_BASE * );
   int ( *DispCount )( X_GT_BASE * );
   int ( *PutChar )( X_GT_BASE *, int, int, int, uint8_t, uint16_t );
   void ( *WriteCon )( X_GT_BASE *, const char *, size_t );
   void ( *Scroll )( X_GT_BASE *, int, int, int, int, int, uint16_t, int, int );
   const char *( *Version )( X_GT_BASE *, int );
   int ( *Suspend )( X_GT_BASE * );
   int ( *Resume )( X_GT_BASE * );
   void ( *Bell )( X_GT_BASE * );
   int ( *Info )( X_GT_BASE *, int, X_GT_INFO * );
   int ( *Alert )( X_GT_BASE *, X_ITEM *, X_ITEM *, int, int, double );
   int ( *SetFlag )( X_GT_BASE *, int, int );
   int ( *ReadKey )( X_GT_BASE *, int );
   int ( *InkeyGet )( X_GT_BASE *, int, double, int );
   void ( *InkeyPut )( X_GT_BASE *, int );
   void ( *InkeyIns )( X_GT_BASE *, int );
   int ( *InkeyLast )( X_GT_BASE *, int );
   int ( *InkeyNext )( X_GT_BASE *, int );
   void ( *InkeyPoll )( X_GT_BASE * );
   void ( *InkeyReset )( X_GT_BASE * );
   void ( *InkeySetText )( X_GT_BASE *, const char *, size_t, int );
   int ( *InkeySetLast )( X_GT_BASE *, int );
   int ( *MouseIsPresent )( X_GT_BASE * );
   void ( *MouseGetPos )( X_GT_BASE *, int *, int * );
   void ( *MouseSetPos )( X_GT_BASE *, int, int );
   int ( *MouseGetDoubleClickSpeed )( X_GT_BASE * );
   int ( *MouseCountButton )( X_GT_BASE * );
   int ( *MouseButtonState )( X_GT_BASE *, int );
   int ( *GfxPrimitive )( X_GT_BASE *, int, int, int, int, int, int );
   void ( *GfxText )( X_GT_BASE *, int, int, const char *, int, int, int );
   int ( *TrueColor )( X_GT_BASE * );
} X_GT_FUNCS;

/* The state of a screen: cells, cursor, colors, keyboard and mouse */
struct X_GT_BASE
{
   X_GT_FUNCS *pFuncTable;
   int hStdin;
   int hStdout;
   int hStderr;
   int row;
   int col;
   int rows;
   int cols;
   int cursorStyle;
   int outputError;
   X_GT_CELL *screenBuffer;
   X_GT_CELL *prevBuffer;
   uint8_t *dirtyLines;
   unsigned int dispCount;
   int cursorDirty;
   int screenActive;
   int blink;
   int colors[16];
   unsigned int colorCount;
   unsigned int colorIndex;
   uint32_t foreground;
   uint32_t background;
   unsigned int drivers;
   unsigned int references;
   int inkeyBuffer[X_DEFAULT_INKEY_BUFSIZE];
   size_t inkeyHead;
   size_t inkeyCount;
   int inkeyLast;
   uint32_t *inkeyText;
   size_t inkeyTextLength;
   size_t inkeyTextPos;
   int mousePresent;
   int mouseButtonCount;
   int mouseRow;
   int mouseCol;
   unsigned int mouseButtons;
   int mouseDoubleClickSpeed;
   int compatBuffer;
   int redrawMax;
   void *driverData[32];
   int stdoutConsole;
   int stderrConsole;
   X_ITEM *inkeyFilterBlock;
   X_ITEM *inkeyReadBlock;
   X_ITEM *resizeBlock;
   X_ITEM *cargo;
   char *savedScreen;
   unsigned short savedRow;
   unsigned short savedCol;
   unsigned short savedMaxRow;
   unsigned short savedMaxCol;
   int inkeyFilterActive;
   int inkeyReadActive;
   int resizeActive;
};

/* The registration of a driver */
typedef struct
{
   const char *id;
   int ( *init )( X_GT_FUNCS * );
   X_GT_FUNCS *pSuperTable;
   int *pGtId;
} X_GT_INIT;

int x_gtRegister( const X_GT_INIT *gtInit );
X_GT_BASE *x_gtLoad( const char *gtName, X_GT_BASE *gt, X_GT_FUNCS *superTable );
int x_gtInit( int inputHandle, int outputHandle, int errorHandle );
int x_gtExit( void );
int x_gtActive( void );
int x_gtHasTrueColor( void );
int x_gtIsConsole( int stream );
void *x_gtCreate( const char *gtName, int inputHandle, int outputHandle, int errorHandle );
void *x_gtSwap( void *handle );
void *x_gtAlloc( void *handle );
void x_gtAttach( void *handle );
void x_gtRelease( void *handle );
int x_gtReload( const char *gtName, int inputHandle, int outputHandle, int errorHandle );
int x_gtReadKey( int eventMask );
int x_gtSuspend( void );
int x_gtResume( void );
const char *x_gtVersion( int type );
int x_gtInfo( int type, X_ITEM *newValue, X_ITEM *newValue2, X_ITEM *result );
int x_gtAlert( X_ITEM *message, X_ITEM *options, int colorNormal, int colorHigh, double delay );
int x_gtGfxPrimitive( int type, int top, int left, int bottom, int right, int color );
void x_gtGfxText( int top, int left, const char *text, int color, int size, int width );

/* Mouse */
int x_mouseIsPresent( void );
int x_mouseRow( void );
int x_mouseCol( void );
int x_mouseCountButton( void );
int x_mouseButtonState( int button );
void x_mouseGetPos( int *row, int *col );
void x_mouseSetPos( int row, int col );
int x_mouseGetDoubleClickSpeed( void );
int x_mouseSetDoubleClickSpeed( int speed );

/* Calls of the driver functions of a screen */
#define X_GTSELF_RESIZE( gt, rows, cols ) ( gt )->pFuncTable->Resize( gt, rows, cols )
#define X_GTSELF_SEMICOLD( gt ) ( gt )->pFuncTable->SemiCold( gt )
#define X_GTSELF_EXPOSEAREA( gt, top, left, bottom, right ) \
   ( gt )->pFuncTable->ExposeArea( gt, top, left, bottom, right )
#define X_GTSELF_GETSCRCHAR( gt, row, col, color, attr, character ) \
   ( gt )->pFuncTable->GetScrChar( gt, row, col, color, attr, character )
#define X_GTSELF_GETSCRCURSOR( gt, row, col, style ) ( gt )->pFuncTable->GetScrCursor( gt, row, col, style )
#define X_GTSELF_DISPBEGIN( gt ) ( gt )->pFuncTable->DispBegin( gt )
#define X_GTSELF_DISPEND( gt ) ( gt )->pFuncTable->DispEnd( gt )
#define X_GTSELF_DISPCOUNT( gt ) ( gt )->pFuncTable->DispCount( gt )
#define X_GTSELF_SETFLAG( gt, type, value ) ( gt )->pFuncTable->SetFlag( gt, type, value )
#define X_GTSELF_MOUSEGETDOUBLECLICKSPEED( gt ) ( gt )->pFuncTable->MouseGetDoubleClickSpeed( gt )

/* Error 1901 and return when there is no screen */
#define X_GTCHECK( ctx, name ) \
   do \
   { \
      if( !x_gtActive() ) \
      { \
         x_errorRaiseNamed( ( ctx )->result, 1901, name ); \
         return; \
      } \
   } while( 0 )

/* ---------------------------------------------------------------------------------------------
   Keyboard
   --------------------------------------------------------------------------------------------- */

int x_inkey( int wait, double seconds, int eventMask );
int x_inkeyKeyStd( int key );
void x_inkeyPut( int key );
void x_inkeyIns( int key );
int x_inkeyLast( int eventMask );
int x_inkeyNext( int eventMask );
void x_inkeyPoll( void );
void x_inkeyReset( void );
void x_inkeySetText( const char *text, size_t length, int eol );
int x_inkeySetLast( int key );

/* ---------------------------------------------------------------------------------------------
   Screen output
   --------------------------------------------------------------------------------------------- */

int x_gtBox( unsigned short top, unsigned short left, unsigned short bottom, unsigned short right,
             unsigned char *boxString );
int x_gtBoxD( unsigned short top, unsigned short left, unsigned short bottom, unsigned short right );
int x_gtBoxS( unsigned short top, unsigned short left, unsigned short bottom, unsigned short right );
int x_gtBoxExt( unsigned short top, unsigned short left, unsigned short bottom, unsigned short right,
                unsigned char *boxString, uint32_t foreground, uint32_t background );
int x_gtBoxDExt( unsigned short top, unsigned short left, unsigned short bottom, unsigned short right,
                 uint32_t foreground, uint32_t background );
int x_gtBoxSExt( unsigned short top, unsigned short left, unsigned short bottom, unsigned short right,
                 uint32_t foreground, uint32_t background );
int x_gtColorSelect( unsigned short colorIndex );
int x_gtDispBegin( void );
unsigned short x_gtDispCount( void );
int x_gtDispEnd( void );
int x_gtGetBlink( int *blink );
int x_gtGetColorStr( unsigned char *colorString );
int x_gtGetCursor( unsigned short *cursorShape );
int x_gtGetPos( unsigned short *row, unsigned short *col );
int x_gtIsColor( void );
unsigned short x_gtMaxCol( void );
unsigned short x_gtMaxRow( void );
int x_gtPostExt( void );
int x_gtPreExt( void );
int x_gtRectSize( unsigned short top, unsigned short left, unsigned short bottom, unsigned short right,
                  size_t *bufferSize );
int x_gtRepChar( unsigned short row, unsigned short column, unsigned short character, unsigned short count );
int x_gtGetCell( int row, int col, int *color, uint8_t *attr, uint16_t *character );
void x_gtPutCell( int row, int col, int color, uint8_t attr, uint16_t character );
void x_gtFlush( void );
int x_gtRest( unsigned short top, unsigned short left, unsigned short bottom, unsigned short right, void *buffer );
int x_gtSave( unsigned short top, unsigned short left, unsigned short bottom, unsigned short right, void *buffer );
int x_gtSaveScreen( void );
int x_gtRestScreen( void );
int x_gtScrDim( unsigned short *height, unsigned short *width );
int x_gtScrollEx( unsigned short top, unsigned short left, unsigned short bottom, unsigned short right, int rows,
                  int cols, const X_ITEM *color, const X_ITEM *fill );
int x_gtScroll( unsigned short top, unsigned short left, unsigned short bottom, unsigned short right, int rows,
                int cols );
int x_gtSetBlink( int blink );
int x_gtSetColorStr( unsigned char *colorString );
int x_gtSetColorRGB( uint32_t foreground, uint32_t background );
int x_gtGetColorRGB( uint32_t *foreground, uint32_t *background );
int x_gtColorToN( const char *colorString );
int x_gtColorName( int color, char *buffer, size_t size );
int x_gtSetCursor( unsigned short cursorShape );
int x_gtSetMode( unsigned short rows, unsigned short cols );
void x_gtResizeNotify( X_GT_BASE *gt );
int x_gtSetPos( unsigned short row, unsigned short column );
int x_gtSetSnowFlag( int noSnow );
int x_gtWrite( unsigned char *text, unsigned short length );
int x_gtWriteAt( unsigned short row, unsigned short column, unsigned char *text, unsigned short length );
int x_gtWriteExt( unsigned char *text, unsigned short length, uint32_t foreground, uint32_t background );
int x_gtWriteAtExt( unsigned short row, unsigned short column, unsigned char *text, unsigned short length,
                    uint32_t foreground, uint32_t background, int setPos );
int x_gtWriteCon( const char *text, size_t length );
X_GT_BASE *x_gtCurrent( void );

/* ---------------------------------------------------------------------------------------------
   internal-ct.c: text windows
   --------------------------------------------------------------------------------------------- */

X_GT_BASE *x_ctScreen( X_GT_BASE *current );
void x_ctScreenResize( X_GT_BASE *gt );
void x_ctExit( void );
int x_ctOpen( int top, int left, int bottom, int right, int clear );
int x_ctClose( void );
int x_ctCloseAll( void );
int x_ctCurrent( void );
int x_ctSelect( int handle );
int x_ctMaxWindow( void );
int x_ctMove( int top, int left );
int x_ctCenter( int center );
void x_ctCoords( int center, int *top, int *left, int *bottom, int *right );
void x_ctFormatCoords( int relative, int *top, int *left, int *bottom, int *right );
int x_ctFormat( int top, int left, int bottom, int right );
int x_ctBox( const char *frame );
int x_ctBoxType( int type );
int x_ctBoard( int set, int top, int left, int bottom, int right );
int x_ctMode( int top, int left, int bottom, int right );
int x_ctSetShadow( int color );
int x_ctShadow( void );
int x_ctSetMove( int mode );
int x_ctStep( int rows, int cols );

#ifdef __cplusplus
}
#endif

#endif /* TERMINAL_H */
