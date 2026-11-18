/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library: xDev Runtime
 * File: xdev-api.c
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

/* The functions of the API are weak, so a library can replace them */
#define X_RTL_DEFAULT_API

#include "classes.h"
#include "gtinfo.h"
#include "internal.h"
#include "platform.h"
#include "terminal.h"
#include "version.h"

#include "../../include/xdev/fileio.ch"
#include "../../include/xdev/inkey.ch"

#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ---------------------------------------------------------------------------------------------
   xDev API
   --------------------------------------------------------------------------------------------- */

// Add a new element to the end of an array
// AAdd( aTarget, expValue ) --> Value
X_FUNC( AADD )
{
   X_ARGCHECK( ctx, X_ISARRAY( ctx, 1 ), "AADD" );
   X_ARGCHECK( ctx, x_pcount( ctx ) >= 2, "AADD" );

   if( !x_arrayAdd( ( X_ITEM * )x_paritem( ctx, 1 ), x_paritem( ctx, 2 ) ) )
   {
      x_errorRaiseNamed( ctx->result, 1102, "AADD" );
      return;
   }

   x_itemReturn( ctx, x_paritem( ctx, 2 ) );
}

// Return the absolute value of a numeric expression
// Abs( nExp ) --> nPositive
X_FUNC( ABS )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "ABS" );

   x_mathAbs( ctx );
}

// Read a line of keyboard input, target of the ACCEPT command
// Accept( [xPrompt] ) --> cString
X_FUNC( ACCEPT )
{
   X_GTCHECK( ctx, "ACCEPT" );

   x_screenAccept( ctx );
}

// Execute a pop-up menu
// AChoice( nTop, nLeft, nBottom, nRight, acMenuItems, [alSelectableItems | lSelectableItems], [bUserBlock], [nInitialItem], [nWindowRow] ) --> nPosition
X_FUNC( ACHOICE )
{
   X_GTCHECK( ctx, "ACHOICE" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ) && X_ISNUM( ctx, 2 ) && X_ISNUM( ctx, 3 ) && X_ISNUM( ctx, 4 ), "ACHOICE" );
   X_ARGCHECK( ctx, X_ISARRAY( ctx, 5 ), "ACHOICE" );
   X_ARGCHECK_OPT( ctx, 6, X_ISNIL( ctx, 6 ) || X_ISLOG( ctx, 6 ) || X_ISARRAY( ctx, 6 ), "ACHOICE" );
   X_ARGCHECK_OPT( ctx, 7, X_ISNIL( ctx, 7 ) || X_ISBLOCK( ctx, 7 ), "ACHOICE" );
   X_ARGCHECK_OPT( ctx, 8, X_ISNIL( ctx, 8 ) || X_ISNUM( ctx, 8 ), "ACHOICE" );
   X_ARGCHECK_OPT( ctx, 9, X_ISNIL( ctx, 9 ) || X_ISNUM( ctx, 9 ), "ACHOICE" );

   x_screenAChoice( ctx );
}

// Duplicate a nested or multidimensional array
// AClone( aSource ) --> aDuplicate
X_FUNC( ACLONE )
{
   X_ARGCHECK( ctx, X_ISARRAY( ctx, 1 ), "ACLONE" );

   x_arrayClone( ctx->result, x_paritem( ctx, 1 ) );
}

// Copy elements from one array to another
// ACopy( aSource, aTarget, [nStart], [nCount], [nTargetPos] ) --> aTarget
X_FUNC( ACOPY )
{
   X_ARGCHECK( ctx, X_ISARRAY( ctx, 1 ), "ACOPY" );
   X_ARGCHECK( ctx, X_ISARRAY( ctx, 2 ), "ACOPY" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISNUM( ctx, 3 ), "ACOPY" );
   X_ARGCHECK_OPT( ctx, 4, X_ISNIL( ctx, 4 ) || X_ISNUM( ctx, 4 ), "ACOPY" );
   X_ARGCHECK_OPT( ctx, 5, X_ISNIL( ctx, 5 ) || X_ISNUM( ctx, 5 ), "ACOPY" );

   x_arrayACopy( ctx );
}

// Calculate the arc cosine in radians
// ACos( nValue ) --> nRadians
X_FUNC( ACOS )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "ACOS" );

   x_mathRetnd( ctx, acos( x_parnd( ctx, 1 ) ), "ACOS" );
}

// Add a numeric value to a byte in a string, optionally carrying through earlier bytes
// AddASCII( cString, nValue, [nPosition], [lCarry] ) --> cString
X_FUNC( ADDASCII )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "ADDASCII" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 2 ), "ADDASCII" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISNUM( ctx, 3 ), "ADDASCII" );
   X_ARGCHECK_OPT( ctx, 4, X_ISNIL( ctx, 4 ) || X_ISLOG( ctx, 4 ), "ADDASCII" );

   x_textAddASCII( ctx );
}

// Shift a date by a number of calendar months
// AddMonth( [dDate,] nMonths ) --> dDate
X_FUNC( ADDMONTH )
{
   /* AddMonth( nMonths ) or AddMonth( dDate, nMonths ) */
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ) || X_ISDATE( ctx, 1 ), "ADDMONTH" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ) || X_ISNUM( ctx, 2 ), "ADDMONTH" );

   x_dateAddMonth( ctx );
}

// Delete an array element
// ADel( aTarget, nPosition, [lAutoSize] ) --> aTarget
X_FUNC( ADEL )
{
   X_ARGCHECK( ctx, X_ISARRAY( ctx, 1 ), "ADEL" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 2 ), "ADEL" );
   X_ARGCHECK_OPT( ctx, 3, X_ISLOG( ctx, 3 ), "ADEL" );

   x_arrayADel( ctx );
}

// Fill a series of arrays with directory information
// ADir( [cFilespec], [aFilenames], [aSizes], [aDates], [aTimes], [aAttributes] ) --> nFiles
X_FUNC( ADIR )
{
   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISCHAR( ctx, 1 ), "ADIR" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISARRAY( ctx, 2 ), "ADIR" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISARRAY( ctx, 3 ), "ADIR" );
   X_ARGCHECK_OPT( ctx, 4, X_ISNIL( ctx, 4 ) || X_ISARRAY( ctx, 4 ), "ADIR" );
   X_ARGCHECK_OPT( ctx, 5, X_ISNIL( ctx, 5 ) || X_ISARRAY( ctx, 5 ), "ADIR" );
   X_ARGCHECK_OPT( ctx, 6, X_ISNIL( ctx, 6 ) || X_ISARRAY( ctx, 6 ), "ADIR" );

   x_fsADirCall( ctx );
}

// Execute a code block for each element in an array
// AEval( aArray, bBlock, [nStart], [nCount] ) --> aArray
X_FUNC( AEVAL )
{
   X_ARGCHECK( ctx, X_ISARRAY( ctx, 1 ), "AEVAL" );
   X_ARGCHECK( ctx, X_ISBLOCK( ctx, 2 ), "AEVAL" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISNUM( ctx, 3 ), "AEVAL" );
   X_ARGCHECK_OPT( ctx, 4, X_ISNIL( ctx, 4 ) || X_ISNUM( ctx, 4 ), "AEVAL" );

   x_arrayAEval( ctx );
}

// Fill an array with a specified value
// AFill( aTarget, expValue, [nStart], [nCount] ) --> aTarget
X_FUNC( AFILL )
{
   X_ARGCHECK( ctx, X_ISARRAY( ctx, 1 ), "AFILL" );
   X_ARGCHECK( ctx, x_pcount( ctx ) >= 2, "AFILL" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNUM( ctx, 3 ), "AFILL" );
   X_ARGCHECK_OPT( ctx, 4, X_ISNUM( ctx, 4 ), "AFILL" );

   x_arrayAFill( ctx );
}

// Return the portion of a string after the nth occurrence of a substring
// AfterAtNum( cSearch, cString, [nOccurrence], [nIgnore] ) --> cRest
X_FUNC( AFTERATNUM )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "AFTERATNUM" );
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 2 ), "AFTERATNUM" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISNUM( ctx, 3 ), "AFTERATNUM" );
   X_ARGCHECK_OPT( ctx, 4, X_ISNIL( ctx, 4 ) || X_ISNUM( ctx, 4 ), "AFTERATNUM" );

   x_textAfterAtNum( ctx );
}

// Insert a NIL element into an array
// AIns( aTarget, nPosition, [xValue], [lAutoSize] ) --> aTarget
X_FUNC( AINS )
{
   X_ARGCHECK( ctx, X_ISARRAY( ctx, 1 ), "AINS" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 2 ), "AINS" );
   X_ARGCHECK_OPT( ctx, 4, X_ISLOG( ctx, 4 ), "AINS" );

   x_arrayAIns( ctx );
}

// Return the automatically managed array pre-allocation step
// ALenAlloc( aArray ) --> nStep
X_FUNC( ALENALLOC )
{
   X_ARGCHECK( ctx, X_ISARRAY( ctx, 1 ), "ALENALLOC" );

   x_retnint( ctx, ( x_arrayLen( x_paritem( ctx, 1 ) ) >> 1 ) + 1 );
}

// Display a simple modal dialog box
// Alert( xMessage, [aOptions], [cColorNorm], [nDelay] ) --> nChoice
// Alert( xMessage, aOptions, nNormal, nHigh, [nDelay] ) --> nChoice
X_FUNC( ALERT )
{
   X_GTCHECK( ctx, "ALERT" );
   X_ARGCHECK( ctx, x_pcount( ctx ) >= 1, "ALERT" );
   X_ARGCHECK_OPT( ctx, 2, X_ISARRAY( ctx, 2 ) || X_ISNIL( ctx, 2 ), "ALERT" );
   X_ARGCHECK_OPT( ctx, 3, X_ISCHAR( ctx, 3 ) || X_ISNUM( ctx, 3 ) || X_ISNIL( ctx, 3 ), "ALERT" );
   X_ARGCHECK_OPT( ctx, 4, X_ISNUM( ctx, 4 ) || X_ISNIL( ctx, 4 ), "ALERT" );
   X_ARGCHECK_OPT( ctx, 5, X_ISNUM( ctx, 5 ) || X_ISNIL( ctx, 5 ), "ALERT" );

   x_screenAlert( ctx );
}

// Remove leading and trailing spaces from a character string
// AllTrim( cString ) --> cTrimString
X_FUNC( ALLTRIM )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "ALLTRIM" );

   const char *text = x_parc( ctx, 1 );
   size_t length = x_parclen( ctx, 1 );

   while( length > 0 && *text == ' ' )
   {
      ++text;
      --length;
   }

   length = x_strRTrimLen( text, length, 0 );

   x_retclen( ctx, text, length );
}

// Invoke the xDev debugger
// AltD( [nAction] ) --> NIL
X_FUNC( ALTD )
{
   /* xDev has no debugger yet: the call is accepted and does nothing */
   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISNUM( ctx, 1 ), "ALTD" );
   x_ret( ctx );
}

// Convert a 24-hour time string to 12-hour form with an am/pm suffix
// AmPm( cTime ) --> cTime
X_FUNC( AMPM )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "AMPM" );

   x_timeAmPm( ctx );
}

// Escape HTML markup characters in a UTF-8 string while preserving existing entities
// AnsiToHtml( cText ) --> cHtml
X_FUNC( ANSITOHTML )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "ANSITOHTML" );

   x_textAnsiToHtml( ctx );
}

// Return the values of all parameters passed to a function, method or procedure
// AParams( ) --> aValues
X_FUNC( APARAMS )
{
   x_arrayParams( ctx->result, ctx->argc, ctx->args );
}

// Return the number of command line arguments
// ArgC( ) --> nArgCount
X_FUNC( ARGC )
{
   x_commandArgCount( ctx );
}

// Return the value of a command line argument
// ArgV( nPos ) --> cArgValue
X_FUNC( ARGV )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "ARGV" );

   x_commandArgValue( ctx, x_parnint( ctx, 1 ) );
}

// Create an uninitialized array of specified length
// Array( nElements [, nElements...] ) --> aArray
X_FUNC( ARRAY )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "ARRAY" );

   int32_t i;

   for( i = 2; i <= x_pcount( ctx ); ++i )
      X_ARGCHECK_OPT( ctx, i, X_ISNUM( ctx, i ), "ARRAY" );

   x_arrayArray( ctx );
}

// Create a set/get code block for an array element
// ArrayBlock( aArray, nIndex ) --> bCodeblock
X_FUNC( ARRAYBLOCK )
{
   X_ARGCHECK( ctx, X_ISARRAY( ctx, 1 ) && X_ISNUM( ctx, 2 ), "ARRAYBLOCK" );

   x_arrayBlock( ctx, x_paritem( ctx, 1 ), x_paritem( ctx, 2 ) );
}

// Return a unique identifier of an array or an object
// ArrayId( aArray | oObject ) --> pID
X_FUNC( ARRAYID )
{
   X_ARGCHECK( ctx, X_ISARRAY( ctx, 1 ), "ARRAYID" );

   x_retptr( ctx, x_arrayId( x_paritem( ctx, 1 ) ) );
}

// Convert a character to its ASCII value
// Asc( cExp ) --> nCode
X_FUNC( ASC )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "ASC" );

   size_t length = x_parclen( ctx, 1 );

   x_retni( ctx, IIF( length > 0, ( unsigned char )x_parc( ctx, 1 )[0], 0 ) );
}

// Scan an array for a value or until a block returns true ( .T. )
// AScan( aTarget, expSearch, [nStart], [nCount], [lExact] ) --> nStoppedAt
X_FUNC( ASCAN )
{
   X_ARGCHECK( ctx, X_ISARRAY( ctx, 1 ), "ASCAN" );
   X_ARGCHECK( ctx, x_pcount( ctx ) >= 2, "ASCAN" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISNUM( ctx, 3 ), "ASCAN" );
   X_ARGCHECK_OPT( ctx, 4, X_ISNIL( ctx, 4 ) || X_ISNUM( ctx, 4 ), "ASCAN" );
   X_ARGCHECK_OPT( ctx, 5, X_ISNIL( ctx, 5 ) || X_ISLOG( ctx, 5 ), "ASCAN" );

   int64_t start, count;

   start = x_parnintdef( ctx, 3, 1 );
   count = x_parnintdef( ctx, 4, x_arrayLen( x_paritem( ctx, 1 ) ) );
   x_retnint( ctx, x_arrayScan( ctx, x_paritem( ctx, 1 ), x_paritem( ctx, 2 ), start, count, x_parl( ctx, 5 ) ) );
}

// Return the sum of all byte values in a string
// ASCIISum( cString ) --> nSum
X_FUNC( ASCIISUM )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "ASCIISUM" );

   x_textASCIISum( ctx );
}

// Return the byte value at a position in a string
// AscPos( cString, [nPosition] ) --> nCode
X_FUNC( ASCPOS )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "ASCPOS" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISNUM( ctx, 2 ), "ASCPOS" );

   size_t length = x_parclen( ctx, 1 );
   int64_t position = x_parnintdef( ctx, 2, ( int64_t )length );

   if( position >= 1 && ( uint64_t )position <= length )
      x_retni( ctx, ( unsigned char )x_parc( ctx, 1 )[position - 1] );
   else
      x_retni( ctx, 0 );
}

// Calculate the arc sine in radians
// ASin( nValue ) --> nRadians
X_FUNC( ASIN )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "ASIN" );

   x_mathRetnd( ctx, asin( x_parnd( ctx, 1 ) ), "ASIN" );
}

// Grow or shrink an array
// ASize( aTarget, nLength ) --> aTarget
X_FUNC( ASIZE )
{
   X_ARGCHECK( ctx, X_ISARRAY( ctx, 1 ), "ASIZE" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 2 ), "ASIZE" );

   int64_t length = x_parnint( ctx, 2 );

   x_arraySize( ( X_ITEM * )x_paritem( ctx, 1 ), ( size_t )IIF( length < 0, 0, length ) );
   x_itemReturn( ctx, x_paritem( ctx, 1 ) );
}

// Accept an array pre-allocation hint; xDev manages array capacity automatically
// ASizeAlloc( aArray, nStep ) --> aArray
X_FUNC( ASIZEALLOC )
{
   X_ARGCHECK( ctx, X_ISARRAY( ctx, 1 ), "ASIZEALLOC" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 2 ) && x_parnint( ctx, 2 ) >= 0, "ASIZEALLOC" );

   x_itemReturn( ctx, x_paritem( ctx, 1 ) );
}

// Sort an array
// ASort( aTarget, [nStart], [nCount], [bOrder] ) --> aTarget
X_FUNC( ASORT )
{
   X_ARGCHECK( ctx, X_ISARRAY( ctx, 1 ), "ASORT" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISNUM( ctx, 2 ), "ASORT" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISNUM( ctx, 3 ), "ASORT" );
   X_ARGCHECK_OPT( ctx, 4, X_ISNIL( ctx, 4 ) || X_ISBLOCK( ctx, 4 ), "ASORT" );

   x_arrayASort( ctx );
}

// Return the position of a substring within a character string
// At( cSearch, cTarget, [nStart], [nEnd] ) --> nPosition
X_FUNC( AT )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "AT" );
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 2 ), "AT" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNUM( ctx, 3 ), "AT" );
   X_ARGCHECK_OPT( ctx, 4, X_ISNUM( ctx, 4 ), "AT" );

   x_textAt( ctx );
}

// Justify a character sequence within a string
// AtAdjust( cSearch, cString, nEndPos, [nCount], [nSkipChars], [xInsChar] ) --> cJustified
X_FUNC( ATADJUST )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) && X_ISCHAR( ctx, 2 ) && X_ISNUM( ctx, 3 ), "ATADJUST" );
   X_ARGCHECK_OPT( ctx, 4, X_ISNIL( ctx, 4 ) || X_ISNUM( ctx, 4 ), "ATADJUST" );
   X_ARGCHECK_OPT( ctx, 5, X_ISNIL( ctx, 5 ) || X_ISNUM( ctx, 5 ), "ATADJUST" );
   X_ARGCHECK_OPT( ctx, 6, X_ISNIL( ctx, 6 ) || X_ISCHAR( ctx, 6 ) || X_ISNUM( ctx, 6 ), "ATADJUST" );
   X_ARGCHECK_OPT( ctx, 6, !X_ISNUM( ctx, 6 ) || x_parnint( ctx, 6 ) >= 0, "ATADJUST" );
   X_ARGCHECK_OPT( ctx, 6, !X_ISNUM( ctx, 6 ) || x_parnint( ctx, 6 ) <= 255, "ATADJUST" );

   x_atAdjust( ctx );
}

// Return the highest numbered element of an array
// ATail( aArray ) --> Element
X_FUNC( ATAIL )
{
   X_ARGCHECK( ctx, X_ISARRAY( ctx, 1 ), "ATAIL" );

   const X_ITEM *array = x_paritem( ctx, 1 );

   x_itemReturn( ctx, x_arrayGetItemPtr( array, ( size_t )x_arrayLen( array ) ) );
}

// Calculate the arc tangent in radians
// ATan( nValue ) --> nRadians
X_FUNC( ATAN )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "ATAN" );

   x_mathRetnd( ctx, atan( x_parnd( ctx, 1 ) ), "ATAN" );
}

// Calculate the arc tangent from Cartesian coordinates
// Atn2( nY, nX ) --> nRadians
X_FUNC( ATN2 )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ) && X_ISNUM( ctx, 2 ), "ATN2" );

   x_mathRetnd( ctx, atan2( x_parnd( ctx, 1 ), x_parnd( ctx, 2 ) ), "ATN2" );
}

// Search multiple occurrences of a substring within a string
// AtNum( cSearch, cString, [nCount], [nSkipChars] ) --> nPos
X_FUNC( ATNUM )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) && X_ISCHAR( ctx, 2 ), "ATNUM" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISNUM( ctx, 3 ), "ATNUM" );
   X_ARGCHECK_OPT( ctx, 4, X_ISNIL( ctx, 4 ) || X_ISNUM( ctx, 4 ), "ATNUM" );

   const char *text = x_parc( ctx, 2 );
   size_t textLength = x_parclen( ctx, 2 );
   int64_t occurrence = x_parnintdef( ctx, 3, 0 );
   int64_t ignored = x_parnintdef( ctx, 4, 0 );
   size_t found = x_atNumFind( x_parc( ctx, 1 ), x_parclen( ctx, 1 ), text, textLength, occurrence, ignored );

   x_retnint( ctx, IIF( found == SIZE_MAX, 0, ( int64_t )found + 1 ) );
}

// Split a string into tokens based on a delimiter
// ATokens( cString, [cDelimiter], [lSkipQuotes], [lDoubleQuotesOnly] ) --> aTokens
X_FUNC( ATOKENS )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "ATOKENS" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISCHAR( ctx, 2 ) || X_ISLOG( ctx, 2 ), "ATOKENS" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISLOG( ctx, 3 ), "ATOKENS" );
   X_ARGCHECK_OPT( ctx, 4, X_ISNIL( ctx, 4 ) || X_ISLOG( ctx, 4 ), "ATOKENS" );

   x_textATokens( ctx );
}

// Search and replace a substring within a string
// AtRepl( cSearch, cString, cReplace, [nCount], [lOneOnly] ) --> cResult
X_FUNC( ATREPL )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) && X_ISCHAR( ctx, 2 ) && X_ISCHAR( ctx, 3 ), "ATREPL" );
   X_ARGCHECK_OPT( ctx, 4, X_ISNIL( ctx, 4 ) || X_ISNUM( ctx, 4 ), "ATREPL" );
   X_ARGCHECK_OPT( ctx, 5, X_ISNIL( ctx, 5 ) || X_ISLOG( ctx, 5 ), "ATREPL" );

   x_atReplace( ctx );
}

// Locate the position of a substring within a character string
// AtSkipStrings( cSearch, cString ) --> nPos
X_FUNC( ATSKIPSTRINGS )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) && X_ISCHAR( ctx, 2 ), "ATSKIPSTRINGS" );

   size_t found = x_atSkipStrings( x_parc( ctx, 1 ), x_parclen( ctx, 1 ), x_parc( ctx, 2 ), x_parclen( ctx, 2 ) );

   x_retnint( ctx, ( int64_t )found );
}

// Return the position of the n-th token in a string
// AtToken( cString, [cDelimiter], [nCount] ) --> nPos
X_FUNC( ATTOKEN )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "ATTOKEN" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISCHAR( ctx, 2 ) || X_ISNUM( ctx, 2 ), "ATTOKEN" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISNUM( ctx, 3 ), "ATTOKEN" );

   x_tokenAtToken( ctx );
}

// Query or change the activity of a single background task
// BackGroundActive( nTaskHandle, [lNewActive] ) --> lOldActive
X_FUNC( BACKGROUNDACTIVE )
{
   x_errorRaiseNamed( ctx->result, 1900, "BACKGROUNDACTIVE" );
}

// Add a new background task
// BackGroundAdd( bAction, [nMillisecs], [lActive] ) --> nTaskHandle
X_FUNC( BACKGROUNDADD )
{
   x_errorRaiseNamed( ctx->result, 1900, "BACKGROUNDADD" );
}

// Remove a background task from the internal task list
// BackGroundDel( nTaskHandle ) --> bAction
X_FUNC( BACKGROUNDDEL )
{
   x_errorRaiseNamed( ctx->result, 1900, "BACKGROUNDDEL" );
}

// Reset the internal counter of background tasks
// BackGroundReset( ) --> NIL
X_FUNC( BACKGROUNDRESET )
{
   x_errorRaiseNamed( ctx->result, 1900, "BACKGROUNDRESET" );
}

// Execute one or all background tasks
// BackGroundRun( [nTaskHandle] ) --> NIL
X_FUNC( BACKGROUNDRUN )
{
   x_errorRaiseNamed( ctx->result, 1900, "BACKGROUNDRUN" );
}

// Query or change the interval in milliseconds after which a background task is executed
// BackGroundTime( nTaskHandle, [nNewInterval] ) --> nOldInterval
X_FUNC( BACKGROUNDTIME )
{
   x_errorRaiseNamed( ctx->result, 1900, "BACKGROUNDTIME" );
}

// Decode a base 64 encoded string
// Base64Decode( cBase64 ) --> cString
X_FUNC( BASE64DECODE )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "BASE64DECODE" );

   size_t length = 0;
   char *result = x_base64Decode( x_parc( ctx, 1 ), x_parclen( ctx, 1 ), &length );

   x_retclen_buffer( ctx, result, length );
}

// Decode a base 64 encoded file
// Base64DecodeFile( cBase64File, cTargetFile ) --> NIL
X_FUNC( BASE64DECODEFILE )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) && X_ISCHAR( ctx, 2 ), "BASE64DECODEFILE" );

   x_fsBase64DecodeFileCall( ctx );
}

// Encode a string in base 64
// Base64Encode( cString, [nBytes] ) --> cBase64
X_FUNC( BASE64ENCODE )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "BASE64ENCODE" );

   size_t length = x_parclen( ctx, 1 );

   if( X_ISNUM( ctx, 2 ) && x_parnl( ctx, 2 ) >= 0 && ( size_t )x_parnl( ctx, 2 ) < length )
      length = ( size_t )x_parnl( ctx, 2 );

   char *result = x_base64Encode( x_parc( ctx, 1 ), length, &length );

   x_retclen_buffer( ctx, result, length );
}

// Encode a file in base 64
// Base64EncodeFile( cFilename, cBase64File ) --> NIL
X_FUNC( BASE64ENCODEFILE )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) && X_ISCHAR( ctx, 2 ), "BASE64ENCODEFILE" );

   x_fsBase64EncodeFileCall( ctx );
}

// Extract the part of a string before the last occurrence of a search string
// BeforAtNum( cSearch, cString, [nCount], [nSkipChars] ) --> cResult
X_FUNC( BEFORATNUM )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) && X_ISCHAR( ctx, 2 ), "BEFORATNUM" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISNUM( ctx, 3 ), "BEFORATNUM" );
   X_ARGCHECK_OPT( ctx, 4, X_ISNIL( ctx, 4 ) || X_ISNUM( ctx, 4 ), "BEFORATNUM" );

   const char *text = x_parc( ctx, 2 );

   int64_t occurrence = x_parnintdef( ctx, 3, 0 );
   int64_t ignored = x_parnintdef( ctx, 4, 0 );
   size_t found = x_atNumFind( x_parc( ctx, 1 ), x_parclen( ctx, 1 ), text, x_parclen( ctx, 2 ), occurrence, ignored );

   if( found == SIZE_MAX )
      x_retc_null( ctx );
   else
      x_retclen( ctx, text, found );
}

// Convert a 4 byte IEEE single precision float to a numeric value
// Bin2F( cFloat ) --> nNumber
X_FUNC( BIN2F )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "BIN2F" );

   x_retnd( ctx, x_binFloat32( x_parc( ctx, 1 ), x_parclen( ctx, 1 ) ) );
}

// Convert a 16-bit signed integer to a numeric value
// Bin2I( cSignedInt ) --> nNumber
X_FUNC( BIN2I )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "BIN2I" );

   const char *text = x_parc( ctx, 1 );
   size_t length = x_parclen( ctx, 1 );
   unsigned char b0 = IIF( length > 0, ( unsigned char )text[0], 0 );
   unsigned char b1 = IIF( length > 1, ( unsigned char )text[1], 0 );

   x_retni( ctx, ( int16_t )( ( uint16_t )b0 | ( ( uint16_t )b1 << 8 ) ) );
}

// Convert a 32-bit signed integer to a numeric value
// Bin2L( cSignedInt ) --> nNumber
X_FUNC( BIN2L )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "BIN2L" );

   size_t i;
   uint32_t value = 0;

   const char *text = x_parc( ctx, 1 );
   size_t length = x_parclen( ctx, 1 );

   for( i = 0; i < 4; ++i )
      value |= ( uint32_t )( IIF( i < length, ( unsigned char )text[i], 0 ) ) << ( i * 8 );

   x_retnint( ctx, ( int32_t )value );
}

// Convert an unsigned long binary integer (4 bytes) into a numeric value
// Bin2U( cInteger ) --> nNumber
X_FUNC( BIN2U )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "BIN2U" );

   x_retnint( ctx, x_binUInt32( x_parc( ctx, 1 ), x_parclen( ctx, 1 ) ) );
}

// Convert a 16-bit unsigned integer to a numeric value
// Bin2W( cUnsignedInt ) --> nNumber
X_FUNC( BIN2W )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "BIN2W" );

   const char *text = x_parc( ctx, 1 );
   size_t length = x_parclen( ctx, 1 );
   unsigned char b0 = IIF( length > 0, ( unsigned char )text[0], 0 );
   unsigned char b1 = IIF( length > 1, ( unsigned char )text[1], 0 );

   x_retni( ctx, ( uint16_t )b0 | ( ( uint16_t )b1 << 8 ) );
}

// Perform a bitwise AND of integer values
// BitAnd( nInteger1, nInteger2 ) --> nResult
X_FUNC( BITAND )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ) && x_parnd( ctx, 1 ) == ( double )x_parnint( ctx, 1 ), "BITAND" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 2 ) && x_parnd( ctx, 2 ) == ( double )x_parnint( ctx, 2 ), "BITAND" );

   x_retnint( ctx, x_parnint( ctx, 1 ) & x_parnint( ctx, 2 ) );
}

// Check if a bit is set in an integer value; nPosition 0 is the lowest bit
// BitIsSet( nInteger, nPosition ) --> lBitIsSet
X_FUNC( BITISSET )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ) && X_ISNUM( ctx, 2 ), "BITISSET" );
   X_ARGCHECK( ctx, x_parnint( ctx, 2 ) >= 0 && x_parnint( ctx, 2 ) < 64, "BITISSET" );

   x_retl( ctx, ( int )( ( ( uint64_t )x_parnint( ctx, 1 ) >> x_parnint( ctx, 2 ) ) & 1 ) );
}

// Perform a bitwise NOT of an integer value
// BitNot( nInteger ) --> nResult
X_FUNC( BITNOT )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ) && x_parnd( ctx, 1 ) == ( double )x_parnint( ctx, 1 ), "BITNOT" );

   x_retnint( ctx, ~x_parnint( ctx, 1 ) );
}

// Perform a bitwise OR of integer values
// BitOr( nInteger1, nInteger2 ) --> nResult
X_FUNC( BITOR )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ) && x_parnd( ctx, 1 ) == ( double )x_parnint( ctx, 1 ), "BITOR" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 2 ) && x_parnd( ctx, 2 ) == ( double )x_parnint( ctx, 2 ), "BITOR" );

   x_retnint( ctx, x_parnint( ctx, 1 ) | x_parnint( ctx, 2 ) );
}

// Set a bit of an integer value to 0; nPosition 0 is the lowest bit
// BitReset( nInteger, nPosition ) --> nResult
X_FUNC( BITRESET )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ) && X_ISNUM( ctx, 2 ), "BITRESET" );
   X_ARGCHECK( ctx, x_parnint( ctx, 2 ) >= 0 && x_parnint( ctx, 2 ) < 64, "BITRESET" );

   x_retnint( ctx, ( int64_t )( ( uint64_t )x_parnint( ctx, 1 ) & ~( ( uint64_t )1 << x_parnint( ctx, 2 ) ) ) );
}

// Rotate the bits of an integer; a positive nRotate rotates to the left, a negative one to the right
// BitRotate( nInteger, nRotate, [nBits] ) --> nResult
X_FUNC( BITROTATE )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ) && X_ISNUM( ctx, 2 ), "BITROTATE" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISNUM( ctx, 3 ), "BITROTATE" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || x_parnint( ctx, 3 ) >= 1, "BITROTATE" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || x_parnint( ctx, 3 ) <= 64, "BITROTATE" );

   int bits;

   bits = ( int )x_parnintdef( ctx, 3, 64 );
   x_retnint( ctx, ( int64_t )x_mathRotateLeft( ( uint64_t )x_parnint( ctx, 1 ), bits, x_parnint( ctx, 2 ) ) );
}

// Set a bit in a numeric integer value to 1; nPosition 0 is the lowest bit
// BitSet( nInteger, nPosition ) --> nResult
X_FUNC( BITSET )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ) && X_ISNUM( ctx, 2 ), "BITSET" );
   X_ARGCHECK( ctx, x_parnint( ctx, 2 ) >= 0 && x_parnint( ctx, 2 ) < 64, "BITSET" );

   x_retnint( ctx, ( int64_t )( ( uint64_t )x_parnint( ctx, 1 ) | ( ( uint64_t )1 << x_parnint( ctx, 2 ) ) ) );
}

// Shift bits; a positive nShift shifts to the left, a negative one to the right filling with 0
// BitShift( nInteger, nShift ) --> nResult
X_FUNC( BITSHIFT )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ) && X_ISNUM( ctx, 2 ), "BITSHIFT" );

   x_mathBitShift( ctx );
}

// Translate bits of an integer to a character string; the last character of cBitPattern stands for bit 0
// BitToC( nInteger, cBitPattern, [lSpaces] ) --> cResult
X_FUNC( BITTOC )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ) && X_ISCHAR( ctx, 2 ) && x_parclen( ctx, 2 ) <= 64, "BITTOC" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISLOG( ctx, 3 ), "BITTOC" );

   x_mathBitToC( ctx );
}

// Perform a bitwise XOR operation with numeric integer values
// BitXOr( nInteger1, nInteger2 ) --> nResult
X_FUNC( BITXOR )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ) && x_parnd( ctx, 1 ) == ( double )x_parnint( ctx, 1 ), "BITXOR" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 2 ) && x_parnd( ctx, 2 ) == ( double )x_parnint( ctx, 2 ), "BITXOR" );

   x_retnint( ctx, x_parnint( ctx, 1 ) ^ x_parnint( ctx, 2 ) );
}

// Return an empty value for the data types A, C, D, L, M and N
// Blank( xValue, [lSpaces] ) --> xEmptyValue
X_FUNC( BLANK )
{
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISLOG( ctx, 2 ), "BLANK" );

   x_textBlank( ctx );
}

// Convert a list of parameters into a text string
// BldLogMsg( xParam, ... ) --> cParamValues
X_FUNC( BLDLOGMSG )
{
   x_errorRaiseNamed( ctx->result, 1900, "BLDLOGMSG" );
}

// Return the date of the first day of a month
// BoM( [dDate] ) --> dFirstDayOfMonth
X_FUNC( BOM )
{
   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISDATE( ctx, 1 ), "BOM" );

   long julian = x_dateParam( ctx, 1 );
   int year, month, day;

   if( julian == 0 )
   {
      x_retdl( ctx, 0 );
      return;
   }

   x_dateDecode( julian, &year, &month, &day );
   x_retdl( ctx, x_dateEncode( year, month, 1 ) );
}

// Return the date of the first day of a quarter
// BoQ( [dDate] ) --> dFirstDayOfQuarter
X_FUNC( BOQ )
{
   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISDATE( ctx, 1 ), "BOQ" );

   long julian = x_dateParam( ctx, 1 );
   int year, month, day;

   if( julian == 0 )
   {
      x_retdl( ctx, 0 );
      return;
   }

   x_dateDecode( julian, &year, &month, &day );
   x_retdl( ctx, x_dateEncode( year, ( month - 1 ) / 3 * 3 + 1, 1 ) );
}

// Return the date of the first day of a year
// BoY( [dDate] ) --> dFirstDayOfYear
X_FUNC( BOY )
{
   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISDATE( ctx, 1 ), "BOY" );

   long julian = x_dateParam( ctx, 1 );
   int year, month, day;

   if( julian == 0 )
   {
      x_retdl( ctx, 0 );
      return;
   }

   x_dateDecode( julian, &year, &month, &day );
   x_retdl( ctx, x_dateEncode( year, 1, 1 ) );
}

// Branch out of a BEGIN SEQUENCE...END construct
// Break( exp ) --> NIL
X_FUNC( BREAK )
{
   if( x_pcount( ctx ) > 1 )
      x_errorRaiseNamed( ctx->result, 3003, "BREAK" );
   else
      x_breakRaise( x_paritem( ctx, 1 ) );
}

// Retrieve the formatted build date of the xDev compiler
// BuildDate( ) --> cDateTime
X_FUNC( BUILDDATE )
{
   x_errorRaiseNamed( ctx->result, 1900, "BUILDDATE" );
}

// Retrieve build information of the xDev compiler
// BuildInfo( nWhichInfo ) --> cBuildInfo
X_FUNC( BUILDINFO )
{
   x_errorRaiseNamed( ctx->result, 1900, "BUILDINFO" );
}

// Execute a function located in a dynamically loaded external library
// CallDll( pFunction [, xParams...] ) --> xResult
X_FUNC( CALLDLL )
{
   x_errorRaiseNamed( ctx->result, 1900, "CALLDLL" );
}

// Convert a date value to a character day of the week
// CDoW( dExp ) --> cDayName
X_FUNC( CDOW )
{
   X_ARGCHECK( ctx, X_ISDATE( ctx, 1 ), "CDOW" );

   long julian = x_pardl( ctx, 1 );

   x_retc( ctx, IIF( julian != 0, x_dateCDOW( x_dateJulianDOW( julian ) ), "" ) );
}

// Round a number toward positive infinity
// Ceiling( nValue ) --> nInteger
X_FUNC( CEILING )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "CEILING" );

   x_mathRetnd( ctx, ceil( x_parnd( ctx, 1 ) ), "CEILING" );
}

// Convert degrees Fahrenheit to Celsius
// Celsius( nFahrenheit ) --> nCelsius
X_FUNC( CELSIUS )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "CELSIUS" );

   x_mathRetnd( ctx, ( x_parnd( ctx, 1 ) - 32.0 ) * 5.0 / 9.0, "CELSIUS" );
}

// Return a string for centered display
// Center( cString, [nLength], [cPadChar], [lBothSides] ) --> cCenteredString
X_FUNC( CENTER )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "CENTER" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISNUM( ctx, 2 ), "CENTER" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISCHAR( ctx, 3 ) || X_ISLOG( ctx, 3 ), "CENTER" );
   X_ARGCHECK_OPT( ctx, 4, X_ISNIL( ctx, 4 ) || X_ISLOG( ctx, 4 ), "CENTER" );

   x_textCenter( ctx );
}

// Create a string from the sum of ASCII codes of two strings
// CharAdd( cString1, cString2 ) --> cResult
X_FUNC( CHARADD )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) && X_ISCHAR( ctx, 2 ), "CHARADD" );

   x_charCombine( ctx, '+' );
}

// Combine the characters of two strings with a binary AND
// CharAND( cString1, cString2 ) --> cResult
X_FUNC( CHARAND )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) && X_ISCHAR( ctx, 2 ), "CHARAND" );

   x_charCombine( ctx, '&' );
}

// Extract the characters at even positions from a string
// CharEven( cString ) --> cResult
X_FUNC( CHAREVEN )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "CHAREVEN" );

   /* The positions count UTF-8 characters */
   size_t length, count = 0, i;
   uint32_t *codes = x_ucodes( x_parc( ctx, 1 ), x_parclen( ctx, 1 ), &length );

   for( i = 1; i < length; i += 2 )
      codes[count++] = codes[i];

   x_ucodesReturn( ctx, codes, count );
}

// Create a histogram of the characters in a string
// CharHist( [cString] ) --> aHistogram
X_FUNC( CHARHIST )
{
   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISCHAR( ctx, 1 ), "CHARHIST" );

   x_textCharHist( ctx );
}

// Remove duplicate characters from a string
// CharList( cString ) --> cResult
X_FUNC( CHARLIST )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "CHARLIST" );

   x_textCharList( ctx );
}

// Reverse the order of the characters in a string
// CharMirr( cString, [lNoSpaces] ) --> cResult
X_FUNC( CHARMIRR )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "CHARMIRR" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISLOG( ctx, 2 ), "CHARMIRR" );

   x_textCharMirr( ctx );
}

// Merge the characters of two strings
// CharMix( cString1, cString2 ) --> cResult
X_FUNC( CHARMIX )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) && X_ISCHAR( ctx, 2 ), "CHARMIX" );

   x_textCharMix( ctx );
}

// Return a string of all characters not included in a string
// CharNoList( [cString] ) --> cNotInString
X_FUNC( CHARNOLIST )
{
   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISCHAR( ctx, 1 ), "CHARNOLIST" );

   x_textCharNoList( ctx );
}

// Invert the bits of the characters in a string with a binary NOT
// CharNOT( cString ) --> cResult
X_FUNC( CHARNOT )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "CHARNOT" );

   const char *text = x_parc( ctx, 1 );
   size_t length = x_parclen( ctx, 1 );
   char *result = ( char * )x_xgrab( length + 1 );
   size_t i;

   for( i = 0; i < length; ++i )
      result[i] = ( char )~text[i];

   result[length] = '\0';
   x_storclen( ctx, result, length, 1 );
   x_retclen_buffer( ctx, result, length );
}

// Extract the characters at odd positions from a string
// CharOdd( cString ) --> cResult
X_FUNC( CHARODD )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "CHARODD" );

   /* The positions count UTF-8 characters */
   size_t length, count = 0, i;
   uint32_t *codes = x_ucodes( x_parc( ctx, 1 ), x_parclen( ctx, 1 ), &length );

   for( i = 0; i < length; i += 2 )
      codes[count++] = codes[i];

   x_ucodesReturn( ctx, codes, count );
}

// Remove duplicate adjacent characters from a string
// CharOne( [cDelete], cString ) --> cResult
X_FUNC( CHARONE )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "CHARONE" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISCHAR( ctx, 2 ), "CHARONE" );

   x_textCharOne( ctx );
}

// Remove all characters but the specified ones from a string
// CharOnly( cRemaining, cString ) --> cResult
X_FUNC( CHARONLY )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) && X_ISCHAR( ctx, 2 ), "CHARONLY" );

   x_charFilter( ctx, x_parc( ctx, 1 ), x_parclen( ctx, 1 ), x_parc( ctx, 2 ), x_parclen( ctx, 2 ), 1 );
}

// Combine the characters of two strings with a binary OR
// CharOR( cString1, cString2 ) --> cResult
X_FUNC( CHAROR )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) && X_ISCHAR( ctx, 2 ), "CHAROR" );

   x_charCombine( ctx, '|' );
}

// Compress a string
// CharPack( cString ) --> cCompressed
X_FUNC( CHARPACK )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "CHARPACK" );

   x_textCharPack( ctx );
}

// Test if two substrings in two strings have the same position
// CharRela( cSearch1, cString1, cSearch2, cString2 ) --> nPosition
X_FUNC( CHARRELA )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) && X_ISCHAR( ctx, 2 ) && X_ISCHAR( ctx, 3 ) && X_ISCHAR( ctx, 4 ), "CHARRELA" );

   x_textCharRela( ctx );
}

// Replace characters if two substrings in two strings have the same position
// CharRelRep( cSearch1, cString1, cSearch2, cString2, cReplace ) --> cResult
X_FUNC( CHARRELREP )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) && X_ISCHAR( ctx, 2 ) && X_ISCHAR( ctx, 3 ), "CHARRELREP" );
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 4 ) && X_ISCHAR( ctx, 5 ), "CHARRELREP" );

   x_textCharRelRep( ctx );
}

// Delete specified characters from a string
// CharRem( cDelete, cString ) --> cResult
X_FUNC( CHARREM )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) && X_ISCHAR( ctx, 2 ), "CHARREM" );

   x_charFilter( ctx, x_parc( ctx, 1 ), x_parclen( ctx, 1 ), x_parc( ctx, 2 ), x_parclen( ctx, 2 ), 0 );
}

// Replace a list of characters with a corresponding list
// CharRepl( cSearch, cString, cReplace, [lOnePass] ) --> cResult
X_FUNC( CHARREPL )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) && X_ISCHAR( ctx, 2 ) && X_ISCHAR( ctx, 3 ), "CHARREPL" );
   X_ARGCHECK_OPT( ctx, 4, X_ISNIL( ctx, 4 ) || X_ISLOG( ctx, 4 ), "CHARREPL" );

   x_textCharRepl( ctx );
}

// Rotate the bits of the characters in a string to the left
// CharRLL( cString, nShift ) --> cResult
X_FUNC( CHARRLL )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) && X_ISNUM( ctx, 2 ), "CHARRLL" );

   x_charBits( ctx, 1, 1 );
}

// Rotate the bits of the characters in a string to the right
// CharRLR( cString, nShift ) --> cResult
X_FUNC( CHARRLR )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) && X_ISNUM( ctx, 2 ), "CHARRLR" );

   x_charBits( ctx, 1, 0 );
}

// Shift the bits of the characters in a string to the left
// CharSHL( cString, nShift ) --> cResult
X_FUNC( CHARSHL )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) && X_ISNUM( ctx, 2 ), "CHARSHL" );

   x_charBits( ctx, 0, 1 );
}

// Shift the bits of the characters in a string to the right
// CharSHR( cString, nShift ) --> cResult
X_FUNC( CHARSHR )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) && X_ISNUM( ctx, 2 ), "CHARSHR" );

   x_charBits( ctx, 0, 0 );
}

// Remove duplicate characters from a string and sort the result
// CharSList( cString ) --> cSortedList
X_FUNC( CHARSLIST )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "CHARSLIST" );

   x_textCharSList( ctx );
}

// Sort characters or character sequences within a string
// CharSort( cString, [nSeqLen], [nCompareLen], [nSkipChars], [nSkipSeq], [nSortLen], [lDescending] ) --> cResult
X_FUNC( CHARSORT )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "CHARSORT" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISNUM( ctx, 2 ), "CHARSORT" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISNUM( ctx, 3 ), "CHARSORT" );
   X_ARGCHECK_OPT( ctx, 4, X_ISNIL( ctx, 4 ) || X_ISNUM( ctx, 4 ), "CHARSORT" );
   X_ARGCHECK_OPT( ctx, 5, X_ISNIL( ctx, 5 ) || X_ISNUM( ctx, 5 ), "CHARSORT" );
   X_ARGCHECK_OPT( ctx, 6, X_ISNIL( ctx, 6 ) || X_ISNUM( ctx, 6 ), "CHARSORT" );
   X_ARGCHECK_OPT( ctx, 7, X_ISNIL( ctx, 7 ) || X_ISLOG( ctx, 7 ), "CHARSORT" );

   x_textCharSort( ctx );
}

// Format a character string for block paragraphs
// CharSpread( cString, nLineLen, [xInsChar] ) --> cResult
X_FUNC( CHARSPREAD )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) && X_ISNUM( ctx, 2 ), "CHARSPREAD" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISCHAR( ctx, 3 ) || X_ISNUM( ctx, 3 ), "CHARSPREAD" );

   x_textCharSpread( ctx );
}

// Create a string by subtracting the ASCII codes of two strings
// CharSub( cString1, cString2 ) --> cResult
X_FUNC( CHARSUB )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) && X_ISCHAR( ctx, 2 ), "CHARSUB" );

   x_charCombine( ctx, '-' );
}

// Exchange adjacent characters in a string
// CharSwap( cString ) --> cResult
X_FUNC( CHARSWAP )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "CHARSWAP" );

   /* The pairs are pairs of UTF-8 characters */
   size_t length, i;
   uint32_t *codes = x_ucodes( x_parc( ctx, 1 ), x_parclen( ctx, 1 ), &length );
   uint32_t swap;

   for( i = 0; i + 1 < length; i += 2 )
   {
      swap = codes[i];
      codes[i] = codes[i + 1];
      codes[i + 1] = swap;
   }

   x_ucodesReturn( ctx, codes, length );
}

// Uncompress a string compressed with CharPack()
// CharUnpack( cCompressed ) --> cUncompressed
X_FUNC( CHARUNPACK )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "CHARUNPACK" );

   x_textCharUnpack( ctx );
}

// Replace characters in a screen area
// CharWin( [nTop], [nLeft], [nBottom], [nRight], [xNewChar], [xOldChar] ) --> cNull
X_FUNC( CHARWIN )
{
   X_GTCHECK( ctx, "CHARWIN" );

   x_screenCharWin( ctx );
}

// Combine the characters of two strings with a binary XOR
// CharXOR( cString1, cString2 ) --> cResult
X_FUNC( CHARXOR )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) && X_ISCHAR( ctx, 2 ), "CHARXOR" );

   x_charCombine( ctx, '^' );
}

// Calculate the checksum of a string
// CheckSum( cString ) --> nCheckSum
X_FUNC( CHECKSUM )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "CHECKSUM" );

   x_retnint( ctx, x_textCheckSum( x_parc( ctx, 1 ), x_parclen( ctx, 1 ), 0 ) );
}

// Convert a byte value to a one-byte string
// Chr( nCode ) --> cChar
X_FUNC( CHR )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "CHR" );

   /* A byte for control codes and binary data: above 127 it is no UTF-8 character on its own;
      UTF8Chr() gives the character of a code point */
   char value = ( char )( x_parni( ctx, 1 ) & 0xFF );

   x_retclen( ctx, &value, 1 );
}

// Clear bits of a number; nBitPos 1 is the lowest bit, -1 for an invalid argument
// ClearBit( nLong | cHexLong, nBitPos1, [nBitPosN, ...] ) --> nNewValue
X_FUNC( CLEARBIT )
{
   x_numBit( ctx, 0 );
}

// Clear a row on the screen from a specified position
// ClearEol( [nRow], [nCol], [xColor], [xChar] ) --> cNull
X_FUNC( CLEAREOL )
{
   X_GTCHECK( ctx, "CLEAREOL" );

   x_screenClearEol( ctx );
}

// Clear a screen area gradually with an imploding effect
// ClearSlow( nDelay, [nTop], [nLeft], [nBottom], [nRight], [xChar] ) --> cNull
X_FUNC( CLEARSLOW )
{
   x_errorRaiseNamed( ctx->result, 1900, "CLEARSLOW" );
}

// Clear all or part of the screen
// ClearWin( [nTop], [nLeft], [nBottom], [nRight], [xColor], [xChar] ) --> cNull
X_FUNC( CLEARWIN )
{
   X_GTCHECK( ctx, "CLEARWIN" );

   x_screenClearWin( ctx );
}

// Clear the characters and colors in a row on the screen
// ClEol( [nRow], [nCol] ) --> cNull
X_FUNC( CLEOL )
{
   X_GTCHECK( ctx, "CLEOL" );

   unsigned short row, col;
   int top, left;

   x_gtGetPos( &row, &col );
   top = ( int )x_parnintdef( ctx, 1, row );
   left = ( int )x_parnintdef( ctx, 2, col );
   x_screenFill( top, left, top, x_gtMaxCol(), x_clearColor, x_clearChar );
   x_retc( ctx, "" );
}

// Close a child process
// CloseProcess( nProcessHandle, [lWaitForTermination] ) --> lSuccess
X_FUNC( CLOSEPROCESS )
{
   x_errorRaiseNamed( ctx->result, 1900, "CLOSEPROCESS" );
}

// Replace a color attribute in a screen region
// ClrArea( nTop, nLeft, nBottom, nRight, nColorAttr ) --> NIL
X_FUNC( CLRAREA )
{
   x_errorRaiseNamed( ctx->result, 1900, "CLRAREA" );
}

// Clear the screen and move the cursor to the top left corner
// Cls( ) --> NIL
X_FUNC( CLS )
{
   X_GTCHECK( ctx, "CLS" );

   x_screenCls( ctx );
}

// Clear the characters and colors in a screen area
// ClWin( [nTop], [nLeft], [nBottom], [nRight] ) --> cNull
X_FUNC( CLWIN )
{
   X_GTCHECK( ctx, "CLWIN" );

   unsigned short rect[4];

   x_screenRange( ctx, rect, 1, 1 );
   x_screenFill( rect[0], rect[1], rect[2], rect[3], x_clearColor, x_clearChar );
   x_retc( ctx, "" );
}

// Return the first command line argument (EXE file name)
// CmdArgArgV( ) --> cExeFile
X_FUNC( CMDARGARGV )
{
   x_errorRaiseNamed( ctx->result, 1900, "CMDARGARGV" );
}

// Convert a date to a character month name
// CMonth( dDate ) --> cMonth
X_FUNC( CMONTH )
{
   X_ARGCHECK( ctx, X_ISDATE( ctx, 1 ), "CMONTH" );

   int year, month, day;

   long julian = x_pardl( ctx, 1 );

   if( julian == 0 )
   {
      x_retc( ctx, "" );
      return;
   }

   x_dateDecode( julian, &year, &month, &day );
   x_retc( ctx, x_dateCMonth( month ) );
}

// Return the screen cursor column position
// Col( ) --> nCol
X_FUNC( COL )
{
   X_GTCHECK( ctx, "COL" );

   unsigned short row = 0, col = 0;

   x_gtGetPos( &row, &col );
   x_retni( ctx, col );
}

// Extract a color value from a color string
// ColorIndex( cColorString, nColorIndex ) --> cColorValue
X_FUNC( COLORINDEX )
{
   x_errorRaiseNamed( ctx->result, 1900, "COLORINDEX" );
}

// Replace color attributes on the screen
// ColorRepl( [xNewColor], [xOldColor] ) --> cNull
X_FUNC( COLORREPL )
{
   x_errorRaiseNamed( ctx->result, 1900, "COLORREPL" );
}

// Activate attribute in current color settings
// ColorSelect( nColorIndex ) --> NIL
X_FUNC( COLORSELECT )
{
   X_GTCHECK( ctx, "COLORSELECT" );

   if( X_ISNUM( ctx, 1 ) )
      x_gtColorSelect( ( unsigned short )x_parni( ctx, 1 ) );

   x_ret( ctx );
}

// Convert a color string to a numeric color attribute
// ColorToN( cColor ) --> nColorAttribute
X_FUNC( COLORTON )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "COLORTON" );

   int color = x_gtColorToN( x_parc( ctx, 1 ) );

   x_retni( ctx, IIF( color < 0, 0, color ) );
}

// Replace a color attribute in a screen area
// ColorWin( [nTop], [nLeft], [nBottom], [nRight], [xNewColor], [xOldColor] ) --> cNull
X_FUNC( COLORWIN )
{
   X_GTCHECK( ctx, "COLORWIN" );

   x_screenColorWin( ctx );
}

// Retrieve the version of the C compiler shipped with xDev
// Compiler( ) --> cCompilerVersion
X_FUNC( COMPILER )
{
   x_errorRaiseNamed( ctx->result, 1900, "COMPILER" );
}

// Create the complement of a value of type C, D, L, M or N
// Complement( xValue ) --> xComplement
X_FUNC( COMPLEMENT )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) || X_ISNUM( ctx, 1 ) || X_ISLOG( ctx, 1 ) || X_ISDATE( ctx, 1 ), "COMPLEMENT" );

   x_textComplement( ctx );
}

// Compress a character string (ZIP)
// Compress( cString ) --> cCompressed
X_FUNC( COMPRESS )
{
   x_errorRaiseNamed( ctx->result, 1900, "COMPRESS" );
}

// Calculate the buffer size required for compression
// CompressBufLen( nUncompressedLen )--> nDefaultBufLen
X_FUNC( COMPRESSBUFLEN )
{
   x_errorRaiseNamed( ctx->result, 1900, "COMPRESSBUFLEN" );
}

// Return the error code of the last (un)compression
// CompressError( ) --> nLastError
X_FUNC( COMPRESSERROR )
{
   x_errorRaiseNamed( ctx->result, 1900, "COMPRESSERROR" );
}

// Return an error description from a numeric error code
// CompressErrorDesc( nErrorCode ) --> cErrorDescription
X_FUNC( COMPRESSERRORDESC )
{
   x_errorRaiseNamed( ctx->result, 1900, "COMPRESSERRORDESC" );
}

// Lock the console for the current thread
// ConsoleLock( ) --> NIL
X_FUNC( CONSOLELOCK )
{
   x_errorRaiseNamed( ctx->result, 1900, "CONSOLELOCK" );
}

// Release the console lock
// ConsoleUnlock( ) --> NIL
X_FUNC( CONSOLEUNLOCK )
{
   x_errorRaiseNamed( ctx->result, 1900, "CONSOLEUNLOCK" );
}

// Calculate the cosine of an angle in radians
// Cos( nRadians ) --> nCosine
X_FUNC( COS )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "COS" );

   x_mathRetnd( ctx, cos( x_parnd( ctx, 1 ) ), "COS" );
}

// Calculate the hyperbolic cosine
// CosH( nValue ) --> nHyperbolicCosine
X_FUNC( COSH )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "COSH" );

   x_mathRetnd( ctx, cosh( x_parnd( ctx, 1 ) ), "COSH" );
}

// Calculate the cotangent
// Cot( nAngle ) --> nCotangent
X_FUNC( COT )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "COT" );

   x_mathRetnd( ctx, 1.0 / tan( x_parnd( ctx, 1 ) ), "COT" );
}

// Count a specified character on the left side of a string
// CountLeft( cString, [xChar] ) --> nCount
X_FUNC( COUNTLEFT )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "COUNTLEFT" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISCHAR( ctx, 2 ) || X_ISNUM( ctx, 2 ), "COUNTLEFT" );

   X_UCHAR c;

   /* The count is a count of characters; c may be any UTF-8 character */
   x_ucharArg( ctx, 2, &c );
   x_retnint( ctx, ( int64_t )x_ucharRun( x_parc( ctx, 1 ), x_parclen( ctx, 1 ), &c, 0 ) );
}

// Count a specified character on the right side of a string
// CountRight( cString, [xChar] ) --> nCount
X_FUNC( COUNTRIGHT )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "COUNTRIGHT" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISCHAR( ctx, 2 ) || X_ISNUM( ctx, 2 ), "COUNTRIGHT" );

   X_UCHAR c;

   /* The count is a count of characters; c may be any UTF-8 character */
   x_ucharArg( ctx, 2, &c );
   x_retnint( ctx, ( int64_t )x_ucharRun( x_parc( ctx, 1 ), x_parclen( ctx, 1 ), &c, 1 ) );
}

// Calculate the checksum for a stream of data using the CRC 32 algorithm
// CRC32( cString ) --> nCRC32
X_FUNC( CRC32 )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "CRC32" );

   x_retnint( ctx, x_textCheckSum( x_parc( ctx, 1 ), x_parclen( ctx, 1 ), 1 ) );
}

// Convert a numeric value to an eight byte character string in network byte order
// CreateLen8( nValue ) --> cBinary
X_FUNC( CREATELEN8 )
{
   x_errorRaiseNamed( ctx->result, 1900, "CREATELEN8" );
}

// (Windows only) Create a new OLE Automation object
// CreateObject( cProgID ) --> oOleAuto
X_FUNC( CREATEOBJECT )
{
   x_errorRaiseNamed( ctx->result, 1900, "CREATEOBJECT" );
}

// Encrypt or decrypt a string
// Crypt( cString, cPassWord ) --> cResult
X_FUNC( CRYPT )
{
   x_errorRaiseNamed( ctx->result, 1900, "CRYPT" );
}

// Query or change the multi-pass mode for the At...() functions
// CSetAtMuPa( [lNewMode] ) --> lOldMode
X_FUNC( CSETATMUPA )
{
   x_errorRaiseNamed( ctx->result, 1900, "CSETATMUPA" );
}

// Query or change the SET CENTURY setting
// CSetCent( [lNewMode] ) --> lOldMode
X_FUNC( CSETCENT )
{
   x_errorRaiseNamed( ctx->result, 1900, "CSETCENT" );
}

// Query or change the SET CURSOR setting
// CSetCurs( [lNewMode] ) --> lOldMode
X_FUNC( CSETCURS )
{
   x_errorRaiseNamed( ctx->result, 1900, "CSETCURS" );
}

// Retrieve the code block associated with a key
// CSetKey( nKey ) --> bCodeBlock
X_FUNC( CSETKEY )
{
   x_errorRaiseNamed( ctx->result, 1900, "CSETKEY" );
}

// Query or change the safety switch used in file operations
// CSetSafety( [lNewMode] ) --> lOldMode
X_FUNC( CSETSAFETY )
{
   x_errorRaiseNamed( ctx->result, 1900, "CSETSAFETY" );
}

// Convert a value to a character string
// CStr( xValue ) --> cString
X_FUNC( CSTR )
{
   X_ARGCHECK( ctx, x_pcount( ctx ) >= 1, "CSTR" );

   size_t length;
   char *text = x_valToCStr( x_paritem( ctx, 1 ), &length );

   x_retclen_buffer( ctx, text, length );
}

// Convert a character string to a value of a specific data type
// CStrToVal( cString, cValtype ) --> xValue
X_FUNC( CSTRTOVAL )
{
   x_errorRaiseNamed( ctx->result, 1900, "CSTRTOVAL" );
}

// Convert a character string to an integer based on a bit pattern
// CToBit( cString, cBitPattern ) --> nInteger
X_FUNC( CTOBIT )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) && X_ISCHAR( ctx, 2 ) && x_parclen( ctx, 2 ) <= 64, "CTOBIT" );

   x_mathCToBit( ctx );
}

// Convert a date string to a date value
// CToD( cDate, [cFormat] ) --> dDate
X_FUNC( CTOD )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "CTOD" );
   X_ARGCHECK_OPT( ctx, 2, X_ISCHAR( ctx, 2 ), "CTOD" );

   const char *format = IIF( X_ISCHAR( ctx, 2 ), x_parc( ctx, 2 ), x_setDateFormat( NULL ) );

   x_retdl( ctx, x_dateUnformat( x_parc( ctx, 1 ), format ) );
}

// Return the number of a week day from its name
// CToDoW( cDayName ) --> nDayOfWeek
X_FUNC( CTODOW )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "CTODOW" );

   x_retni( ctx, x_dateNameNumber( x_parc( ctx, 1 ), x_parclen( ctx, 1 ), x_dateCDOW, 7 ) );
}

// Convert an 8 byte string to a floating point number
// CToF( cFloat ) --> nNumber
X_FUNC( CTOF )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "CTOF" );

   x_retnd( ctx, x_binFloat64( x_parc( ctx, 1 ), x_parclen( ctx, 1 ) ) );
}

// Return the number of a month from its name
// CToMonth( cMonthName ) --> nMonth
X_FUNC( CTOMONTH )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "CTOMONTH" );

   x_retni( ctx, x_dateNameNumber( x_parc( ctx, 1 ), x_parclen( ctx, 1 ), x_dateCMonth, 12 ) );
}

// Convert a string of digits to an integer of the specified base
// CToN( cDigits, [nBase], [lNegative] ) --> nInteger
X_FUNC( CTON )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "CTON" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISNUM( ctx, 2 ), "CTON" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISLOG( ctx, 3 ), "CTON" );

   x_mathCToN( ctx );
}

// Convert a character string into a DateTime value
// CToT( cDateTimeString ) --> dDateTime
X_FUNC( CTOT )
{
   x_errorRaiseNamed( ctx->result, 1900, "CTOT" );
}

// Return the current DOS directory
// CurDir( [cDrivespec] ) --> cDirectory
X_FUNC( CURDIR )
{
   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISCHAR( ctx, 1 ), "CURDIR" );

   x_fsCurDirCall( ctx );
}

// Return the current directory of a drive including directory separators
// CurDirX( [cDrive] ) --> cDirPath
X_FUNC( CURDIRX )
{
   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISCHAR( ctx, 1 ), "CURDIRX" );

   x_fsCurDirXCall( ctx );
}

// (Windows only) Determine or change the current disk drive; Linux has no drives and gives ""
// CurDrive( [cNewDrive] ) --> cOldDrive
X_FUNC( CURDRIVE )
{
   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISCHAR( ctx, 1 ), "CURDRIVE" );

   x_fsCurDriveCall( ctx );
}

// Return the current directory with the drive and a trailing separator
// Cwd( ) --> cDirectory
X_FUNC( CWD )
{
   x_fsCwdCall( ctx );
}

// Return the system date as a date value
// Date( [nYear, nMonth, nDay] ) --> dDate
X_FUNC( DATE )
{
   if( x_pcount( ctx ) == 0 )
   {
      int year, month, day;

      x_dateToday( &year, &month, &day );
      x_retd( ctx, year, month, day );
   }
   else
   {
      X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "DATE" );
      X_ARGCHECK( ctx, X_ISNUM( ctx, 2 ), "DATE" );
      X_ARGCHECK( ctx, X_ISNUM( ctx, 3 ), "DATE" );
      x_retd( ctx, x_parni( ctx, 1 ), x_parni( ctx, 2 ), x_parni( ctx, 3 ) );
   }
}

// Return the current date and time from the operating system
// DateTime( ) --> dDateTime
X_FUNC( DATETIME )
{
   x_errorRaiseNamed( ctx->result, 1900, "DATETIME" );
}

// Return the day of the month as a numeric value
// Day( dDate ) --> nDay
X_FUNC( DAY )
{
   X_ARGCHECK( ctx, X_ISDATE( ctx, 1 ), "DAY" );

   int year, month, day;

   long julian = x_pardl( ctx, 1 );

   if( julian == 0 )
   {
      x_retni( ctx, 0 );
      return;
   }

   x_dateDecode( julian, &year, &month, &day );
   x_retni( ctx, day );
}

// Calculate the number of days from elapsed seconds
// Days( nSeconds ) --> nDays
X_FUNC( DAYS )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "DAYS" );

   x_retnint( ctx, ( int64_t )( x_parnd( ctx, 1 ) / 86400.0 ) );
}

// Return the number of days in a month
// DaysInMonth( nMonth, [lLeapYear] ) --> nDaysInMonth
X_FUNC( DAYSINMONTH )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "DAYSINMONTH" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISLOG( ctx, 2 ), "DAYSINMONTH" );

   int64_t month = x_parnint( ctx, 1 );

   int leap = X_ISLOG( ctx, 2 ) && x_parl( ctx, 2 );

   x_retni( ctx, IIF( month >= 1 && month <= 12, x_dateMonthDays( ( int )month, leap ), 0 ) );
}

// Return the number of days from the first of January to the beginning of a month
// DaysToMonth( nMonth, [lLeapYear] ) --> nDays
X_FUNC( DAYSTOMONTH )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "DAYSTOMONTH" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISLOG( ctx, 2 ), "DAYSTOMONTH" );

   x_dateDaysToMonth( ctx );
}

// Provide a functional equivalent for the DO CASE statement
// Decode( xInitVal, xCase1, xRet1, [xCaseN, xRetN], [xDefault] ) --> xReturn
X_FUNC( DECODE )
{
   x_errorRaiseNamed( ctx->result, 1900, "DECODE" );
}

// Provide a functional equivalent for the DO CASE statement
// DecodeOrEmpty( xInitVal, xCase1, xRet1, [xCaseN, xRetN], [xDefault] ) --> xReturn
X_FUNC( DECODEOREMPTY )
{
   x_errorRaiseNamed( ctx->result, 1900, "DECODEOREMPTY" );
}

// Decrypt an encrypted character string
// Decrypt( cEncryptedString, cKey ) --> cString
X_FUNC( DECRYPT )
{
   x_errorRaiseNamed( ctx->result, 1900, "DECRYPT" );
}

// Assign a default value to a variable
// Default( @varName, xDefaultValue ) --> NIL
X_FUNC( DEFAULT )
{
   x_errorRaiseNamed( ctx->result, 1900, "DEFAULT" );
}

// Return a value, or the default when the value is NIL
// DefaultValue( xValue, xDefault ) --> xResult
X_FUNC( DEFAULTVALUE )
{
   x_itemReturn( ctx, x_paritem( ctx, IIF( X_ISNIL( ctx, 1 ), 2, 1 ) ) );
}

// Return the SET DEFAULT directory
// DefPath( ) --> cDefaultPath
X_FUNC( DEFPATH )
{
   x_errorRaiseNamed( ctx->result, 1900, "DEFPATH" );
}

// Delete a file with error handling
// DeleteFile( cFileName ) --> nErrorCode
X_FUNC( DELETEFILE )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "DELETEFILE" );

   x_fsDeleteFileCall( ctx );
}

// Create a descending index key value
// Descend( exp ) --> ValueInverted
X_FUNC( DESCEND )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) || X_ISNUM( ctx, 1 ), "DESCEND" );

   x_textDescend( ctx );
}

// Initiate deserialization of a group of variables of simple or complex data types
// DeserialBegin( cBinaryData ) --> cFirstSerialdata
X_FUNC( DESERIALBEGIN )
{
   x_errorRaiseNamed( ctx->result, 1900, "DESERIALBEGIN" );
}

// Convert a binary string back to its original data type
// DeSerialize( cBinary ) --> xValue
X_FUNC( DESERIALIZE )
{
   x_errorRaiseNamed( ctx->result, 1900, "DESERIALIZE" );
}

// Deserialize values of simple data types
// DeserializeSimple( cBinaryData ) --> xValue
X_FUNC( DESERIALIZESIMPLE )
{
   x_errorRaiseNamed( ctx->result, 1900, "DESERIALIZESIMPLE" );
}

// Deserialize the next variable of simple or complex data types
// DeserialNext( @cSerial, nBytes ) --> xValue
X_FUNC( DESERIALNEXT )
{
   x_errorRaiseNamed( ctx->result, 1900, "DESERIALNEXT" );
}

// Write a value to the current device
// DevOut( exp, [cColorString] ) --> NIL
X_FUNC( DEVOUT )
{
   X_GTCHECK( ctx, "DEVOUT" );

   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) || X_ISNUM( ctx, 1 ) || X_ISDATE( ctx, 1 ) || X_ISLOG( ctx, 1 ), "DEVOUT" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISCHAR( ctx, 2 ), "DEVOUT" );

   x_dispOut( ctx, 1, "", 0, 2 );
}

// Write a value to the current device with 24-bit foreground and background colors
// DevOutExt( exp, nForeground, nBackground ) --> NIL
X_FUNC( DEVOUTEXT )
{
   if( !x_trueColorCheck( ctx, "DEVOUTEXT" ) )
      return;

   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) || X_ISNUM( ctx, 1 ) || X_ISDATE( ctx, 1 ) || X_ISLOG( ctx, 1 ), "DEVOUTEXT" );
   x_dispOutExt( ctx, "", 0, 2, 3, "DEVOUTEXT" );
}

// Write a value to the current device using a picture clause
// DevOutPict( exp, cPicture, [cColorString] ) --> NIL
X_FUNC( DEVOUTPICT )
{
   X_GTCHECK( ctx, "DEVOUTPICT" );

   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) || X_ISNUM( ctx, 1 ) || X_ISDATE( ctx, 1 ) || X_ISLOG( ctx, 1 ), "DEVOUTPICT" );
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 2 ), "DEVOUTPICT" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISCHAR( ctx, 3 ), "DEVOUTPICT" );

   x_dispOut( ctx, 1, x_parc( ctx, 2 ), x_parclen( ctx, 2 ), 3 );
}

// Write a value using a picture clause and 24-bit foreground and background colors
// DevOutPictExt( exp, cPicture, nForeground, nBackground ) --> NIL
X_FUNC( DEVOUTPICTEXT )
{
   int simple;

   if( !x_trueColorCheck( ctx, "DEVOUTPICTEXT" ) )
      return;

   simple = X_ISCHAR( ctx, 1 ) || X_ISNUM( ctx, 1 ) || X_ISDATE( ctx, 1 ) || X_ISLOG( ctx, 1 );
   X_ARGCHECK( ctx, simple, "DEVOUTPICTEXT" );
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 2 ), "DEVOUTPICTEXT" );

   x_dispOutExt( ctx, x_parc( ctx, 2 ), x_parclen( ctx, 2 ), 3, 4, "DEVOUTPICTEXT" );
}

// Move the cursor or printhead to a new position
// DevPos( nRow, nCol ) --> NIL
X_FUNC( DEVPOS )
{
   X_GTCHECK( ctx, "DEVPOS" );

   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "DEVPOS" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 2 ), "DEVPOS" );

   x_gtSetPos( ( unsigned short )x_parni( ctx, 1 ), ( unsigned short )x_parni( ctx, 2 ) );
   x_ret( ctx );
}

// Create missing directories in a path.
// DirBuild( cDir ) --> lOK
X_FUNC( DIRBUILD )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "DIRBUILD" );

   x_retl( ctx, x_fsDirBuild( x_parc( ctx, 1 ) ) );
}

// Change the current DOS directory
// DirChange( cDir ) --> nSuccess
X_FUNC( DIRCHANGE )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "DIRCHANGE" );

   x_retni( ctx, IIF( x_fsDirChange( x_parc( ctx, 1 ) ), 0, x_fsError() ) );
}

// Create an array of directory and file information
// Directory( cDirSpec, [cAttributes] ) --> aDirectory
X_FUNC( DIRECTORY )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "DIRECTORY" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISCHAR( ctx, 2 ), "DIRECTORY" );

   x_fileDirectory( ctx->result, x_parc( ctx, 1 ), IIF( X_ISCHAR( ctx, 2 ), x_parc( ctx, 2 ), "" ) );
}

// Load file information recursively into a two-dimensional array
// DirectoryRecurse( [cDirSpec], [cAttributes] ) --> aDirectory
X_FUNC( DIRECTORYRECURSE )
{
   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISCHAR( ctx, 1 ), "DIRECTORYRECURSE" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISCHAR( ctx, 2 ), "DIRECTORYRECURSE" );

   const char *spec = IIF( X_ISCHAR( ctx, 1 ), x_parc( ctx, 1 ), "*" );
   const char *attr = IIF( X_ISCHAR( ctx, 2 ), x_parc( ctx, 2 ), "" );

   x_fileDirectoryRecurse( ctx->result, spec, attr );
}

// Check whether a directory exists.
// DirExists( cDir ) --> lExists
X_FUNC( DIREXISTS )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "DIREXISTS" );

   x_fsDirExistsCall( ctx );
}

// Create a directory
// DIRMAKE( cNewDir ) --> nSuccess
X_FUNC( DIRMAKE )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "DIRMAKE" );

   x_retni( ctx, IIF( x_fsDirMake( x_parc( ctx, 1 ) ), 0, x_fsError() ) );
}

// Return the current directory
// DirName( [cDrive] ) --> cDirectory
X_FUNC( DIRNAME )
{
   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISCHAR( ctx, 1 ), "DIRNAME" );

   x_fsDirNameCall( ctx );
}

// Remove a directory
// DirRemove( cDirName ) --> nSuccess
X_FUNC( DIRREMOVE )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "DIRREMOVE" );

   x_retni( ctx, IIF( x_fsDirRemove( x_parc( ctx, 1 ) ), 0, x_fsError() ) );
}

// Remove a directory with all its files and subdirectories; a link to a directory is removed, not
// followed. An empty name is an error, so the current directory is never emptied by mistake.
// DirRemoveAll( cDirName ) --> lSuccess
X_FUNC( DIRREMOVEALL )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) && x_parclen( ctx, 1 ) > 0, "DIRREMOVEALL" );

   x_retl( ctx, x_fsDirRemoveAll( x_parc( ctx, 1 ) ) );
}

// Change every / and \ of a path to the directory separator of the operating system
// DirSepToOS( cPath ) --> cPath
X_FUNC( DIRSEPTOOS )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "DIRSEPTOOS" );

   x_fsDirSepToOS( ctx );
}

// Return available, free, used or total disk space in bytes.
// DirSpace( cDir, [nInfoType := DISK_AVAIL] ) --> nBytes | -1
X_FUNC( DIRSPACE )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "DIRSPACE" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISNUM( ctx, 2 ), "DIRSPACE" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || x_parni( ctx, 2 ) >= DISK_AVAIL, "DIRSPACE" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || x_parni( ctx, 2 ) <= DISK_TOTAL, "DIRSPACE" );

   x_retnint( ctx, x_fsDirSpace( x_parc( ctx, 1 ), x_parnidef( ctx, 2, DISK_AVAIL ) ) );
}

// Remove empty directories upwards without crossing the path root.
// DirUnbuild( cDir ) --> lOK
X_FUNC( DIRUNBUILD )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "DIRUNBUILD" );

   x_retl( ctx, x_fsDirUnbuild( x_parc( ctx, 1 ) ) );
}

// Change the current DOS disk drive
// DiskChange( cDrive ) --> lSuccess
X_FUNC( DISKCHANGE )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "DISKCHANGE" );

   x_fsDiskChangeCall( ctx );
}

// Return the free storage space of a disk drive in bytes
// DiskFree( [cDrive] ) --> nFreeDiskSpace
X_FUNC( DISKFREE )
{
   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISCHAR( ctx, 1 ), "DISKFREE" );

   x_fsDiskFreeCall( ctx );
}

// Return the current DOS drive
// DiskName( ) --> cDrive
X_FUNC( DISKNAME )
{
   x_fsDiskNameCall( ctx );
}

// Return the space available on a specified disk
// DiskSpace( [nDrive] ) --> nBytes
X_FUNC( DISKSPACE )
{
   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISNUM( ctx, 1 ), "DISKSPACE" );

   x_fsDiskSpaceCall( ctx );
}

// Return the total storage space of a disk drive in bytes
// DiskTotal( [cDrive] ) --> nTotalBytes
X_FUNC( DISKTOTAL )
{
   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISCHAR( ctx, 1 ), "DISKTOTAL" );

   x_fsDiskTotalCall( ctx );
}

// Return the used storage space of a disk drive in bytes
// DiskUsed( [cDrive] ) --> nUsedDiskSpace
X_FUNC( DISKUSED )
{
   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISCHAR( ctx, 1 ), "DISKUSED" );

   x_fsDiskUsedCall( ctx );
}

// Begin buffering screen output
// DispBegin( ) --> NIL
X_FUNC( DISPBEGIN )
{
   X_GTCHECK( ctx, "DISPBEGIN" );

   x_gtDispBegin();
}

// Display a box on the screen
// DispBox( nTop, nLeft, nBottom, nRight, [cnBoxString], [cColorString] ) --> NIL
X_FUNC( DISPBOX )
{
   X_GTCHECK( ctx, "DISPBOX" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ) && x_parnd( ctx, 1 ) >= 0 && x_parnd( ctx, 1 ) <= USHRT_MAX, "DISPBOX" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 2 ) && x_parnd( ctx, 2 ) >= 0 && x_parnd( ctx, 2 ) <= USHRT_MAX, "DISPBOX" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 3 ) && x_parnd( ctx, 3 ) >= 0 && x_parnd( ctx, 3 ) <= USHRT_MAX, "DISPBOX" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 4 ) && x_parnd( ctx, 4 ) >= 0 && x_parnd( ctx, 4 ) <= USHRT_MAX, "DISPBOX" );
   X_ARGCHECK_OPT( ctx, 5, X_ISNIL( ctx, 5 ) || X_ISNUM( ctx, 5 ) || X_ISCHAR( ctx, 5 ), "DISPBOX" );
   X_ARGCHECK_OPT( ctx, 6, X_ISNIL( ctx, 6 ) || X_ISCHAR( ctx, 6 ), "DISPBOX" );

   x_screenDispBox( ctx );
}

// Display a box with 24-bit foreground and background colors
// DispBoxExt( nTop, nLeft, nBottom, nRight, [cnBoxString], nForeground, nBackground ) --> NIL
X_FUNC( DISPBOXEXT )
{
   uint32_t foreground, background;

   if( !x_trueColorCheck( ctx, "DISPBOXEXT" ) )
      return;

   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ) && x_parnd( ctx, 1 ) >= 0 && x_parnd( ctx, 1 ) <= USHRT_MAX, "DISPBOXEXT" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 2 ) && x_parnd( ctx, 2 ) >= 0 && x_parnd( ctx, 2 ) <= USHRT_MAX, "DISPBOXEXT" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 3 ) && x_parnd( ctx, 3 ) >= 0 && x_parnd( ctx, 3 ) <= USHRT_MAX, "DISPBOXEXT" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 4 ) && x_parnd( ctx, 4 ) >= 0 && x_parnd( ctx, 4 ) <= USHRT_MAX, "DISPBOXEXT" );
   X_ARGCHECK_OPT( ctx, 5, X_ISNIL( ctx, 5 ) || X_ISNUM( ctx, 5 ) || X_ISCHAR( ctx, 5 ), "DISPBOXEXT" );
   if( !x_rgb( ctx, 6, "DISPBOXEXT", &foreground ) || !x_rgb( ctx, 7, "DISPBOXEXT", &background ) )
      return;

   x_screenDispBoxExt( ctx, foreground, background );
}

// Return the number of pending DISPEND( ) requests
// DispCount( ) --> nDispCount
X_FUNC( DISPCOUNT )
{
   X_GTCHECK( ctx, "DISPCOUNT" );

   x_retni( ctx, x_gtDispCount() );
}

// Display buffered screen updates
// DispEnd( ) --> NIL
X_FUNC( DISPEND )
{
   X_GTCHECK( ctx, "DISPEND" );

   x_gtDispEnd();
}

// Write a value to the display
// DispOut( exp, [cColorString] ) --> NIL
X_FUNC( DISPOUT )
{
   X_GTCHECK( ctx, "DISPOUT" );

   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) || X_ISNUM( ctx, 1 ) || X_ISDATE( ctx, 1 ) || X_ISLOG( ctx, 1 ), "DISPOUT" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISCHAR( ctx, 2 ), "DISPOUT" );

   x_dispOut( ctx, 1, "", 0, 2 );
}

// Display a value on the screen at a certain position
// DispOutAt( nRow, nCol, expression, [cColor], [lSetPos] ) --> NIL
X_FUNC( DISPOUTAT )
{
   X_GTCHECK( ctx, "DISPOUTAT" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ) && x_parnd( ctx, 1 ) >= 0 && x_parnd( ctx, 1 ) <= USHRT_MAX, "DISPOUTAT" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 2 ) && x_parnd( ctx, 2 ) >= 0 && x_parnd( ctx, 2 ) <= USHRT_MAX, "DISPOUTAT" );
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 3 ) || X_ISNUM( ctx, 3 ) || X_ISDATE( ctx, 3 ) || X_ISLOG( ctx, 3 ), "DISPOUTAT" );
   X_ARGCHECK_OPT( ctx, 4, X_ISNIL( ctx, 4 ) || X_ISCHAR( ctx, 4 ), "DISPOUTAT" );
   X_ARGCHECK_OPT( ctx, 5, X_ISNIL( ctx, 5 ) || X_ISLOG( ctx, 5 ), "DISPOUTAT" );

   x_screenDispOutAt( ctx );
}

// Display a value at a position with 24-bit foreground and background colors
// DispOutAtExt( nRow, nCol, exp, nForeground, nBackground, [lSetPos] ) --> NIL
X_FUNC( DISPOUTATEXT )
{
   uint32_t foreground, background;
   int simple;

   if( !x_trueColorCheck( ctx, "DISPOUTATEXT" ) )
      return;

   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ) && x_parnd( ctx, 1 ) >= 0 && x_parnd( ctx, 1 ) <= USHRT_MAX, "DISPOUTATEXT" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 2 ) && x_parnd( ctx, 2 ) >= 0 && x_parnd( ctx, 2 ) <= USHRT_MAX, "DISPOUTATEXT" );
   simple = X_ISCHAR( ctx, 3 ) || X_ISNUM( ctx, 3 ) || X_ISDATE( ctx, 3 ) || X_ISLOG( ctx, 3 );
   X_ARGCHECK( ctx, simple, "DISPOUTATEXT" );
   X_ARGCHECK_OPT( ctx, 6, X_ISNIL( ctx, 6 ) || X_ISLOG( ctx, 6 ), "DISPOUTATEXT" );

   if( !x_rgb( ctx, 4, "DISPOUTATEXT", &foreground ) || !x_rgb( ctx, 5, "DISPOUTATEXT", &background ) )
      return;

   x_screenDispOutAtExt( ctx, foreground, background );
}

// Toggle the update of the screen cursor by DispOutAt()
// DispOutAtSetPos( [lNewSetting] ) --> lOldSetting
X_FUNC( DISPOUTATSETPOS )
{
   X_GTCHECK( ctx, "DISPOUTATSETPOS" );
   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISLOG( ctx, 1 ), "DISPOUTATSETPOS" );
   x_retl( ctx, x_dispOutAtSetPos );
   if( X_ISLOG( ctx, 1 ) )
      x_dispOutAtSetPos = x_parl( ctx, 1 );
}

// Write a value with 24-bit foreground and background colors
// DispOutExt( exp, nForeground, nBackground ) --> NIL
X_FUNC( DISPOUTEXT )
{
   if( !x_trueColorCheck( ctx, "DISPOUTEXT" ) )
      return;

   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) || X_ISNUM( ctx, 1 ) || X_ISDATE( ctx, 1 ) || X_ISLOG( ctx, 1 ), "DISPOUTEXT" );
   x_dispOutExt( ctx, "", 0, 2, 3, "DISPOUTEXT" );
}

// Execute a function located in a dynamically loaded external library
// DllCall( cDllFile | nDllHandle, [nCallingConvention], cFuncName | nOrdinal, [xParams, ...] ) --> nResult
X_FUNC( DLLCALL )
{
   x_errorRaiseNamed( ctx->result, 1900, "DLLCALL" );
}

// Execute a DLL function through a call template
// DllExecuteCall( pCallTemplate, [xParams, ...] ) --> nResult
X_FUNC( DLLEXECUTECALL )
{
   x_errorRaiseNamed( ctx->result, 1900, "DLLEXECUTECALL" );
}

// Load a DLL file into memory
// DllLoad( cDLLFileName ) --> nDllHandle
X_FUNC( DLLLOAD )
{
   x_errorRaiseNamed( ctx->result, 1900, "DLLLOAD" );
}

// Create a call template for an external DLL function
// DllPrepareCall( cDllFile | nDllHandle, [nCallingConvention], cFuncName | nOrdinal ) --> pCallTemplate
X_FUNC( DLLPREPARECALL )
{
   x_errorRaiseNamed( ctx->result, 1900, "DLLPREPARECALL" );
}

// Release a dynamically loaded external DLL from memory
// DllUnload( nDllHandle ) --> lSuccess
X_FUNC( DLLUNLOAD )
{
   x_errorRaiseNamed( ctx->result, 1900, "DLLUNLOAD" );
}

// Format a date as "dd. Month yyyy"
// DMY( [dDate], [lPeriod] ) --> cDate
X_FUNC( DMY )
{
   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISDATE( ctx, 1 ) || X_ISLOG( ctx, 1 ), "DMY" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISLOG( ctx, 2 ), "DMY" );

   x_dateDMY( ctx );
}

// Return the last DOS error number
// DosError( [nNewOsCode] ) --> nOsCode
X_FUNC( DOSERROR )
{
   static int s_dosError;

   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISNUM( ctx, 1 ), "DOSERROR" );

   x_retni( ctx, s_dosError );
   if( X_ISNUM( ctx, 1 ) )
      s_dosError = x_parni( ctx, 1 );
}

// Convert a date value to a numeric day of the week
// DoW( dDate ) --> nDay
X_FUNC( DOW )
{
   X_ARGCHECK( ctx, X_ISDATE( ctx, 1 ), "DOW" );

   long julian = x_pardl( ctx, 1 );

   x_retni( ctx, IIF( julian != 0, x_dateJulianDOW( julian ), 0 ) );
}

// Return the day number of a date in its year
// DoY( [dDate] ) --> nDayOfYear
X_FUNC( DOY )
{
   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISDATE( ctx, 1 ), "DOY" );

   long julian = x_dateParam( ctx, 1 );
   int year, month, day;

   if( julian == 0 )
   {
      x_retni( ctx, 0 );
      return;
   }

   x_dateDecode( julian, &year, &month, &day );
   x_retnint( ctx, julian - x_dateEncode( year, 1, 1 ) + 1 );
}

// Convert a date value to a character string
// DToC( dDate ) --> cDate
X_FUNC( DTOC )
{
   X_ARGCHECK( ctx, X_ISDATE( ctx, 1 ), "DTOC" );

   char buffer[13];

   x_dateFormat( x_pards( ctx, 1 ), buffer, x_setDateFormat( NULL ) );
   x_retc( ctx, buffer );
}

// Convert degrees to radians
// DToR( nDegrees ) --> nRadians
X_FUNC( DTOR )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "DTOR" );

   x_mathRetnd( ctx, x_mathDegToRad( x_parnd( ctx, 1 ) ), "DTOR" );
}

// Convert a date value to a string formatted as yyyymmdd
// DToS( dDate ) --> cDate
X_FUNC( DTOS )
{
   X_ARGCHECK( ctx, X_ISDATE( ctx, 1 ), "DTOS" );

   int year, month, day;
   char buffer[9];

   long julian = x_pardl( ctx, 1 );

   if( julian == 0 )
   {
      x_retclen( ctx, "        ", 8 );
      return;
   }

   x_dateDecode( julian, &year, &month, &day );
   snprintf( buffer, sizeof( buffer ), "%04d%02d%02d", year, month, day );
   x_retclen( ctx, buffer, 8 );
}

// Convert a value to a character string holding human readable text
// DumpVar( xValue ) --> cText
X_FUNC( DUMPVAR )
{
   x_errorRaiseNamed( ctx->result, 1900, "DUMPVAR" );
}

// Calculate the time elapsed between a start and an end time
// ElapTime( cStartTime, cEndTime ) --> cElapsedTime
X_FUNC( ELAPTIME )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) && X_ISCHAR( ctx, 2 ), "ELAPTIME" );

   double start = x_timeToSeconds( x_parc( ctx, 1 ), x_parclen( ctx, 1 ) );
   double end = x_timeToSeconds( x_parc( ctx, 2 ), x_parclen( ctx, 2 ) );

   if( start < 0 || end < 0 )
   {
      x_retc( ctx, "" );
      return;
   }

   x_timeReturn( ctx, IIF( end < start, end + 86400 - start, end - start ), 0 );
}

// Determine if the result of an expression is empty
// Empty( exp ) --> lEmpty
X_FUNC( EMPTY )
{
   x_itemEmpty( ctx );
}

// Select the enhanced color of SetColor()
// Enhanced( ) --> cNull
X_FUNC( ENHANCED )
{
   X_GTCHECK( ctx, "ENHANCED" );

   x_gtColorSelect( 1 );
   x_retc( ctx, "" );
}

// Return the current ordinal position of a FOR EACH iteration
// EnumIndex( ) --> nIteration
X_FUNC( ENUMINDEX )
{
   x_errorRaiseNamed( ctx->result, 1900, "ENUMINDEX" );
}

// Return the platform end-of-line sequence, or the one used in cText
// (CRLF before LF before CR; the platform one when cText has none)
// Eol( [cText] ) --> cEol
X_FUNC( EOL )
{
   X_ARGCHECK_OPT( ctx, 1, X_ISCHAR( ctx, 1 ), "EOL" );

   x_retc( ctx, IIF( x_pcount( ctx ) >= 1, x_eolDetect( x_parc( ctx, 1 ), x_parclen( ctx, 1 ) ), x_eol() ) );
}

// Return the date of the last day of a month
// EoM( [dDate] ) --> dLastDayOfMonth
X_FUNC( EOM )
{
   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISDATE( ctx, 1 ), "EOM" );

   x_dateEnd( ctx, 0 );
}

// Return the date of the last day of a quarter
// EoQ( [dDate] ) --> dEndOfQuarter
X_FUNC( EOQ )
{
   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISDATE( ctx, 1 ), "EOQ" );

   x_dateEnd( ctx, 1 );
}

// Return the date of the last day of a year
// EoY( [dDate] ) --> dLastDayOfYear
X_FUNC( EOY )
{
   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISDATE( ctx, 1 ), "EOY" );

   long julian = x_dateParam( ctx, 1 );
   int year, month, day;

   if( julian == 0 )
   {
      x_retdl( ctx, 0 );
      return;
   }

   x_dateDecode( julian, &year, &month, &day );
   x_retdl( ctx, x_dateEncode( year, 12, 31 ) );
}

// Post a code block to execute when a runtime error occurs
// ErrorBlock( [bErrorHandler] ) --> bCurrentErrorHandler
X_FUNC( ERRORBLOCK )
{
   X_ARGCHECK_OPT( ctx, 1, X_ISBLOCK( ctx, 1 ), "ERRORBLOCK" );

   x_errorErrorBlock( ctx );
}

// Set the xDev return code
// ErrorLevel( [nNewReturnCode] ) --> nCurrentReturnCode
X_FUNC( ERRORLEVEL )
{
   int previous = x_errorLevelGet();

   X_ARGCHECK_OPT( ctx, 1, X_ISNUM( ctx, 1 ), "ERRORLEVEL" );

   if( x_pcount( ctx ) >= 1 )
      x_errorLevelSet( x_parni( ctx, 1 ) );

   x_retni( ctx, previous );
}

// Install the default error code block
// ErrorSys( bDefaultErrorBlock ) --> NIL
X_FUNC( ERRORSYS )
{
   x_errorRaiseNamed( ctx->result, 1900, "ERRORSYS" );
}

// Evaluate a code block
// Eval( bBlock, [BlockArg list] ) --> LastBlockValue
X_FUNC( EVAL )
{
   X_ARGCHECK( ctx, X_ISBLOCK( ctx, 1 ), "EVAL" );

   const X_ARG *args = IIF( x_pcount( ctx ) > 1, ctx->args + 1, NULL );

   x_evalBlock( ctx, ctx->result, x_paritem( ctx, 1 ), x_pcount( ctx ) - 1, args );
}

// Execute a function, procedure or method from its pointer
// Exec( pFunction_or_Method_Pointer, [NIL_or_SelfObject], [...] ) --> xResult
X_FUNC( EXEC )
{
   x_errorRaiseNamed( ctx->result, 1900, "EXEC" );
}

// Execute a function, procedure or method indirectly
// ExecFromArray( aExecutableArray ) --> xResult
X_FUNC( EXECFROMARRAY )
{
   x_errorRaiseNamed( ctx->result, 1900, "EXECFROMARRAY" );
}

// Return the executable file name of the application
// ExeName( ) --> cExeFileName
X_FUNC( EXENAME )
{
   x_fsExeNameCall( ctx );
}

// Calculate e**x
// Exp( nExponent ) --> nAntilogarithm
X_FUNC( EXP )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "EXP" );

   x_retnd( ctx, exp( x_parnd( ctx, 1 ) ) );
}

// Insert characters between all characters of a string
// Expand( cString, [nCount], [xChar] ) --> cResult
X_FUNC( EXPAND )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "EXPAND" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISNUM( ctx, 2 ) || X_ISCHAR( ctx, 2 ), "EXPAND" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISCHAR( ctx, 3 ) || X_ISNUM( ctx, 3 ), "EXPAND" );

   x_textExpand( ctx );
}

// Calculate the exponent of a floating point number
// Exponent( nFloat ) --> nExponent
X_FUNC( EXPONENT )
{
   int exponent = 0;

   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "EXPONENT" );

   /* A frexp() mantissa is from 0.5 to 1, an xBase one from 1 to 2 */
   if( x_parnd( ctx, 1 ) != 0.0 )
   {
      frexp( x_parnd( ctx, 1 ), &exponent );
      --exponent;
   }

   x_retni( ctx, exponent );
}

// Convert a numeric value to a 4 byte IEEE single precision float
// F2Bin( nNumber ) --> cFloat
X_FUNC( F2BIN )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "F2BIN" );

   x_binRetFloat32( ctx, x_parnd( ctx, 1 ) );
}

// Calculate the factorial of a number
// Fact( nInteger ) --> nFactorial
X_FUNC( FACT )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "FACT" );

   x_mathFact( ctx );
}

// Convert degrees Celsius to Fahrenheit
// Fahrenheit( nCelsius ) --> nFahrenheit
X_FUNC( FAHRENHEIT )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "FAHRENHEIT" );

   x_mathRetnd( ctx, x_parnd( ctx, 1 ) * 9.0 / 5.0 + 32.0, "FAHRENHEIT" );
}

// Count the characters in a text file, ignoring white space characters
// FCharCount( cFileName ) --> nCharCount
X_FUNC( FCHARCOUNT )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "FCHARCOUNT" );

   x_fsCountCall( ctx, 1 );
}

// Close an open binary file and write DOS buffers to disk
// FClose( nHandle ) --> lError
X_FUNC( FCLOSE )
{
   x_retl( ctx, x_fsClose( x_parnint( ctx, 1 ) ) );
}

// Force a disk write
// FCommit( nFileHandle ) --> NIL
X_FUNC( FCOMMIT )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "FCOMMIT" );

   x_fsCommitCall( ctx );
}

// Create and/or truncate a binary file to zero-length
// FCreate( cFile, [nAttribute] ) --> nHandle
X_FUNC( FCREATE )
{
   x_retnint( ctx, x_fsCreate( x_parc( ctx, 1 ), x_parnidef( ctx, 2, FC_NORMAL ) ) );
}

// Delete a file from disk
// FErase( cFile ) --> nSuccess
X_FUNC( FERASE )
{
   x_retni( ctx, IIF( x_fsDelete( x_parc( ctx, 1 ) ), 0, F_ERROR ) );
}

// Test for errors after a binary file operation
// FError( ) --> nErrorCode
X_FUNC( FERROR )
{
   x_retni( ctx, x_fsError() );
}

// Determine if files exist in the default directory or path
// File( cFilespec ) --> lExists
X_FUNC_ALIAS( FILE, XDEV_FILE )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "FILE" );

   x_retl( ctx, x_fileFind( x_parc( ctx, 1 ) ) );
}

// Append the contents of one file to another
// FileAppend( cSourceFile, cTargetFile ) --> nBytesAdded
X_FUNC( FILEAPPEND )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) && X_ISCHAR( ctx, 2 ), "FILEAPPEND" );

   x_fileAppendCall( ctx );
}

// Return the attributes of a file
// FileAttr( cFileName ) --> nAttributes
X_FUNC( FILEATTR )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "FILEATTR" );

   x_fileAttrCall( ctx );
}

// Read file attributes.
// FileAttrGet( cFileName, @nAttr ) --> lOK
X_FUNC( FILEATTRGET )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "FILEATTRGET" );
   X_ARGCHECK( ctx, X_ISBYREF( ctx, 2 ), "FILEATTRGET" );

   x_fileAttrGetCall( ctx );
}

// Set file attributes.
// FileAttrSet( cFileName, nAttr ) --> lOK
X_FUNC( FILEATTRSET )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "FILEATTRSET" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 2 ), "FILEATTRSET" );

   x_retl( ctx, x_fsAttrSet( x_parc( ctx, 1 ), ( uint32_t )x_parnint( ctx, 2 ) ) );
}

// Close the file handle shared by its references.
// FileClose( pHandle ) --> lOK
X_FUNC( FILECLOSE )
{
   X_ARGCHECK( ctx, x_itemGetPtrGC( x_paritem( ctx, 1 ), &x_fileGC ) != NULL, "FILECLOSE" );

   x_retl( ctx, x_fileClose( x_paritem( ctx, 1 ) ) );
}

// Synchronize file data with storage.
// FileCommit( pHandle ) --> lOK
X_FUNC( FILECOMMIT )
{
   X_ARGCHECK( ctx, x_itemGetPtrGC( x_paritem( ctx, 1 ), &x_fileGC ) != NULL, "FILECOMMIT" );

   x_retl( ctx, x_fileCommit( x_paritem( ctx, 1 ) ) );
}

// Query or configure the local file handle.
// FileConfig( pHandle, nSet, [xValue] ) --> xResult
X_FUNC( FILECONFIG )
{
   X_ARGCHECK( ctx, x_itemGetPtrGC( x_paritem( ctx, 1 ), &x_fileGC ) != NULL, "FILECONFIG" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 2 ), "FILECONFIG" );

   x_fileConfigCall( ctx );
}

// Copy a file, replacing the destination.
// FileCopy( cSource, cTarget ) --> nResult
X_FUNC( FILECOPY )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "FILECOPY" );
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 2 ), "FILECOPY" );

   x_retni( ctx, IIF( x_fileCopy( x_parc( ctx, 1 ), x_parc( ctx, 2 ) ), 0, -1 ) );
}

// Copy a file with optional progress reporting and timestamp preservation.
// FileCopyEx( cSource, cTarget, [nBufSize := 65536], [lKeepTime := .T.], [bProgress] ) --> nResult
X_FUNC( FILECOPYEX )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "FILECOPYEX" );
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 2 ), "FILECOPYEX" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || ( X_ISNUM( ctx, 3 ) && x_parnint( ctx, 3 ) > 0 ), "FILECOPYEX" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || ( uint64_t )x_parnint( ctx, 3 ) <= SIZE_MAX, "FILECOPYEX" );
   X_ARGCHECK_OPT( ctx, 4, X_ISNIL( ctx, 4 ) || X_ISLOG( ctx, 4 ), "FILECOPYEX" );
   X_ARGCHECK_OPT( ctx, 5, X_ISNIL( ctx, 5 ) || X_ISBLOCK( ctx, 5 ), "FILECOPYEX" );

   x_fileCopyExCall( ctx );
}

// Return the date of a file
// FileDate( cFileName ) --> dDate
X_FUNC( FILEDATE )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "FILEDATE" );

   x_fileDateCall( ctx );
}

// Delete files specified by a file mask and file attributes
// FileDelete( cFileMask, [nAttributes] ) --> lDeleted
X_FUNC( FILEDELETE )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "FILEDELETE" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISNUM( ctx, 2 ), "FILEDELETE" );

   /* .T. when at least one file was deleted; nAttributes adds the hidden (2) and system (4) files */
   x_retl( ctx, x_fsDeleteMask( x_parc( ctx, 1 ), ( int )x_parnintdef( ctx, 2, 0 ) ) > 0 );
}

// Check whether the file position has reached or passed the end.
// FileEof( pHandle ) --> lEOF
X_FUNC( FILEEOF )
{
   X_ARGCHECK( ctx, x_itemGetPtrGC( x_paritem( ctx, 1 ), &x_fileGC ) != NULL, "FILEEOF" );

   x_retl( ctx, x_fileEof( x_paritem( ctx, 1 ) ) );
}

// Delete a file.
// FileErase( cFileName ) --> nResult
X_FUNC( FILEERASE )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "FILEERASE" );

   x_retni( ctx, IIF( x_fsDelete( x_parc( ctx, 1 ) ), 0, -1 ) );
}

// Return the last file-operation error without clearing it.
// FileError( ) --> nError
X_FUNC( FILEERROR )
{
   x_retni( ctx, x_fsError() );
}

// Check whether a regular file exists at the supplied path.
// FileExists( cFileName, [@cResolved] ) --> lExists
X_FUNC( FILEEXISTS )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "FILEEXISTS" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISBYREF( ctx, 2 ), "FILEEXISTS" );

   x_fileExistsCall( ctx );
}

// Flush the RTL buffer; the local driver has no buffered data.
// FileFlush( pHandle, [lDirtyOnly := .F.] ) --> lOK
X_FUNC( FILEFLUSH )
{
   X_ARGCHECK( ctx, x_itemGetPtrGC( x_paritem( ctx, 1 ), &x_fileGC ) != NULL, "FILEFLUSH" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISLOG( ctx, 2 ), "FILEFLUSH" );

   x_retl( ctx, x_fileHandle( x_paritem( ctx, 1 ) ) != F_ERROR );
}

// Return the borrowed operating-system file handle.
// FileHandle( pHandle ) --> nOsHandle | -1
X_FUNC( FILEHANDLE )
{
   X_ARGCHECK( ctx, x_itemGetPtrGC( x_paritem( ctx, 1 ), &x_fileGC ) != NULL, "FILEHANDLE" );

   x_retnint( ctx, x_fileHandle( x_paritem( ctx, 1 ) ) );
}

// Test EOF on a borrowed numeric file handle
// FileHandleEof( nHandle ) --> lEof
X_FUNC( FILEHANDLEEOF )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "FILEHANDLEEOF" );

   x_fileHandleEofCall( ctx );
}

// Check whether the path uses the local file driver.
// FileIsLocal( cFileName ) --> lLocal
X_FUNC( FILEISLOCAL )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "FILEISLOCAL" );

   x_fileIsLocalCall( ctx );
}

// Create a hard link to an existing file.
// FileLink( cExisting, cNewName ) --> nResult
X_FUNC( FILELINK )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "FILELINK" );
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 2 ), "FILELINK" );

   x_retni( ctx, IIF( x_fsLink( x_parc( ctx, 1 ), x_parc( ctx, 2 ) ), 0, -1 ) );
}

// Read the target stored in a symbolic link.
// FileLinkRead( cFileName ) --> cTarget | ""
X_FUNC( FILELINKREAD )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "FILELINKREAD" );

   x_fileLinkReadCall( ctx );
}

// Create a symbolic link to a file or directory.
// FileLinkSym( cTarget, cNewName, [lDir := .F.] ) --> nResult
X_FUNC( FILELINKSYM )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "FILELINKSYM" );
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 2 ), "FILELINKSYM" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISLOG( ctx, 3 ), "FILELINKSYM" );

   x_retni( ctx, IIF( x_fsLinkSym( x_parc( ctx, 1 ), x_parc( ctx, 2 ), x_parldef( ctx, 3, 0 ) ), 0, -1 ) );
}

// Load file bytes into a string, optionally limiting the size.
// FileLoad( cFileName, [nMaxSize] ) --> cData | NIL
X_FUNC( FILELOAD )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "FILELOAD" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || ( X_ISNUM( ctx, 2 ) && x_parnint( ctx, 2 ) >= 0 ), "FILELOAD" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || ( uint64_t )x_parnint( ctx, 2 ) < SIZE_MAX, "FILELOAD" );

   x_fileLoadCall( ctx );
}

// Lock a byte range for exclusive or shared access.
// FileLock( pHandle, nStart, nLen, [nType := FLX_EXCLUSIVE + FLX_NO_WAIT] ) --> lOK
X_FUNC( FILELOCK )
{
   X_ARGCHECK( ctx, x_itemGetPtrGC( x_paritem( ctx, 1 ), &x_fileGC ) != NULL, "FILELOCK" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 2 ), "FILELOCK" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 3 ), "FILELOCK" );
   X_ARGCHECK_OPT( ctx, 4, X_ISNIL( ctx, 4 ) || X_ISNUM( ctx, 4 ), "FILELOCK" );

   int type = x_parnidef( ctx, 4, FLX_EXCLUSIVE | FLX_NO_WAIT );

   x_retl( ctx, x_fileLock( x_paritem( ctx, 1 ), x_parnint( ctx, 2 ), x_parnint( ctx, 3 ), type, 0 ) );
}

// Check a byte range for a conflicting lock.
// FileLockTest( pHandle, nStart, nLen, [nType := FLX_EXCLUSIVE] ) --> nPID | 0 | -1
X_FUNC( FILELOCKTEST )
{
   X_ARGCHECK( ctx, x_itemGetPtrGC( x_paritem( ctx, 1 ), &x_fileGC ) != NULL, "FILELOCKTEST" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 2 ), "FILELOCKTEST" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 3 ), "FILELOCKTEST" );
   X_ARGCHECK_OPT( ctx, 4, X_ISNIL( ctx, 4 ) || X_ISNUM( ctx, 4 ), "FILELOCKTEST" );

   int type = x_parnidef( ctx, 4, FLX_EXCLUSIVE );

   x_retnint( ctx, x_fileLockTest( x_paritem( ctx, 1 ), x_parnint( ctx, 2 ), x_parnint( ctx, 3 ), type ) );
}

// Move a file, replacing the destination.
// FileMove( cSource, cTarget ) --> nResult
X_FUNC( FILEMOVE )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "FILEMOVE" );
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 2 ), "FILEMOVE" );

   x_retni( ctx, IIF( x_fileMove( x_parc( ctx, 1 ), x_parc( ctx, 2 ) ), 0, -1 ) );
}

// Check whether a filesystem entry exists.
// FileNameExists( cName ) --> lExists
X_FUNC( FILENAMEEXISTS )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "FILENAMEEXISTS" );

   x_fileNameExistsCall( ctx );
}

// Open an existing file and return a managed handle.
// FileOpen( cFileName, [nMode := FO_READ + FO_COMPAT] ) --> pHandle | NIL
X_FUNC( FILEOPEN )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "FILEOPEN" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISNUM( ctx, 2 ), "FILEOPEN" );

   x_fileOpenCall( ctx );
}

// Parse a CSV file into an array of records
// FileParse( cFileName, [cDelimiter] ) --> aRecords
X_FUNC( FILEPARSE )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "FILEPARSE" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || ( X_ISCHAR( ctx, 2 ) && x_parclen( ctx, 2 ) == 1 ), "FILEPARSE" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || x_parc( ctx, 2 )[0] != '"', "FILEPARSE" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || x_parc( ctx, 2 )[0] != '\r', "FILEPARSE" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || x_parc( ctx, 2 )[0] != '\n', "FILEPARSE" );

   x_fileParseCall( ctx );
}

// Read bytes into a string passed by reference.
// FileRead( pHandle, @cBuffer, [nToRead], [nTimeOut := -1] ) --> nRead | -1
X_FUNC( FILEREAD )
{
   X_ARGCHECK( ctx, x_itemGetPtrGC( x_paritem( ctx, 1 ), &x_fileGC ) != NULL, "FILEREAD" );
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 2 ), "FILEREAD" );
   X_ARGCHECK( ctx, X_ISBYREF( ctx, 2 ), "FILEREAD" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISNUM( ctx, 3 ), "FILEREAD" );
   X_ARGCHECK_OPT( ctx, 4, X_ISNIL( ctx, 4 ) || X_ISNUM( ctx, 4 ), "FILEREAD" );

   x_fileReadCall( ctx );
}

// Read bytes at an offset and preserve the file position.
// FileReadAt( pHandle, @cBuffer, nToRead, nOffset ) --> nRead | -1
X_FUNC( FILEREADAT )
{
   X_ARGCHECK( ctx, x_itemGetPtrGC( x_paritem( ctx, 1 ), &x_fileGC ) != NULL, "FILEREADAT" );
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 2 ), "FILEREADAT" );
   X_ARGCHECK( ctx, X_ISBYREF( ctx, 2 ), "FILEREADAT" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 3 ), "FILEREADAT" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 4 ), "FILEREADAT" );

   x_fileReadAtCall( ctx );
}

// Return a new TStreamFileReader object
// FileReader( cFileName, [nMode] ) --> oTStreamFileReader
X_FUNC( FILEREADER )
{
   x_errorRaiseNamed( ctx->result, 1900, "FILEREADER" );
}

// Read an INI file into nested hashes
// FileReadIni( cFileName, [lCaseSens], [cDelimiters], [lAutoMain] ) --> hData | NIL
X_FUNC( FILEREADINI )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "FILEREADINI" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISLOG( ctx, 2 ), "FILEREADINI" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || ( X_ISCHAR( ctx, 3 ) && x_parclen( ctx, 3 ) > 0 ), "FILEREADINI" );
   X_ARGCHECK_OPT( ctx, 4, X_ISNIL( ctx, 4 ) || X_ISLOG( ctx, 4 ), "FILEREADINI" );

   x_fileReadIniCall( ctx );
}

// Read up to the requested number of bytes into a new string.
// FileReadLen( pHandle, nToRead, [nTimeOut := -1] ) --> cBuffer
X_FUNC( FILEREADLEN )
{
   X_ARGCHECK( ctx, x_itemGetPtrGC( x_paritem( ctx, 1 ), &x_fileGC ) != NULL, "FILEREADLEN" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 2 ), "FILEREADLEN" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISNUM( ctx, 3 ), "FILEREADLEN" );

   x_fileReadLenCall( ctx );
}

// Rename a file without replacing an existing destination.
// FileRename( cSource, cTarget ) --> nResult
X_FUNC( FILERENAME )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "FILERENAME" );
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 2 ), "FILERENAME" );

   x_retni( ctx, IIF( x_fileRename( x_parc( ctx, 1 ), x_parc( ctx, 2 ) ), 0, -1 ) );
}

// Write a string to a file, replacing its contents.
// FileSave( cFileName, cData ) --> lOK
X_FUNC( FILESAVE )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "FILESAVE" );
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 2 ), "FILESAVE" );

   x_retl( ctx, x_fileSave( x_parc( ctx, 1 ), x_parc( ctx, 2 ), x_parclen( ctx, 2 ) ) );
}

// Read the contents of a screen from a file
// FileScreen( cFileName, [nOffSet] ) --> nBytesRead
X_FUNC( FILESCREEN )
{
   x_errorRaiseNamed( ctx->result, 1900, "FILESCREEN" );
}

// Set the file position and return the resulting offset.
// FileSeek( pHandle, nOffset, [nWhence := FS_SET] ) --> nPosition | -1
X_FUNC( FILESEEK )
{
   X_ARGCHECK( ctx, x_itemGetPtrGC( x_paritem( ctx, 1 ), &x_fileGC ) != NULL, "FILESEEK" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 2 ), "FILESEEK" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISNUM( ctx, 3 ), "FILESEEK" );

   x_retnint( ctx, x_fileSeek( x_paritem( ctx, 1 ), x_parnint( ctx, 2 ), x_parnidef( ctx, 3, FS_SET ) ) );
}

// Set INI line and inline comment markers
// FileSetIniComment( cLineComment, cInlineComment ) --> NIL
X_FUNC( FILESETINICOMMENT )
{
   int i;

   for( i = 1; i <= 2; ++i )
   {
      X_ARGCHECK( ctx, X_ISCHAR( ctx, i ) && x_parclen( ctx, i ) <= 1, "FILESETINICOMMENT" );
      X_ARGCHECK( ctx, x_parclen( ctx, i ) == 0 || x_parc( ctx, i )[0] != '\0', "FILESETINICOMMENT" );
      X_ARGCHECK( ctx, x_parclen( ctx, i ) == 0 || x_parc( ctx, i )[0] != '\r', "FILESETINICOMMENT" );
      X_ARGCHECK( ctx, x_parclen( ctx, i ) == 0 || x_parc( ctx, i )[0] != '\n', "FILESETINICOMMENT" );
      X_ARGCHECK( ctx, x_parclen( ctx, i ) == 0 || x_parc( ctx, i )[0] != '"', "FILESETINICOMMENT" );
   }

   x_fileIniCommentsCall( ctx );
}

// Return the file size without changing its position.
// FileSize( pHandle | cFileName, [lUseDirEntry := .T.] ) --> nBytes | -1
X_FUNC( FILESIZE )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) || x_itemGetPtrGC( x_paritem( ctx, 1 ), &x_fileGC ) != NULL, "FILESIZE" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISLOG( ctx, 2 ), "FILESIZE" );

   x_retnint( ctx, x_fileSize( x_paritem( ctx, 1 ), x_parldef( ctx, 2, 1 ) ) );
}

// Retrieve information about a single file
// FileStats( cFileName, [@cFileAttr], [@nFileSize], [@dCreateDate], [@nCreateTime], [@dChangeDate], [@nChangeTime] ) --> lSuccess
X_FUNC( FILESTATS )
{
   int i;

   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "FILESTATS" );
   for( i = 2; i <= 7; ++i )
      X_ARGCHECK_OPT( ctx, i, X_ISNIL( ctx, i ) || X_ISBYREF( ctx, i ), "FILESTATS" );

   x_fsStatsCall( ctx );
}

// Read a string from a file beginning at a specified offset
// FileStr( cFileName, [nBytes], [nStart], [lCtrl_Z] ) --> cString
X_FUNC( FILESTR )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "FILESTR" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISNUM( ctx, 2 ) || X_ISLOG( ctx, 2 ), "FILESTR" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISNUM( ctx, 3 ), "FILESTR" );
   X_ARGCHECK_OPT( ctx, 4, X_ISNIL( ctx, 4 ) || X_ISLOG( ctx, 4 ), "FILESTR" );

   x_fileStrCall( ctx );
}

// Create and open a temporary file, returning its name by reference.
// FileTemp( @cFileName, [cDir], [cPrefix], [cExt], [nAttr := FC_NORMAL] ) --> pHandle | NIL
X_FUNC( FILETEMP )
{
   X_ARGCHECK( ctx, X_ISBYREF( ctx, 1 ), "FILETEMP" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISCHAR( ctx, 2 ), "FILETEMP" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISCHAR( ctx, 3 ), "FILETEMP" );
   X_ARGCHECK_OPT( ctx, 4, X_ISNIL( ctx, 4 ) || X_ISCHAR( ctx, 4 ), "FILETEMP" );
   X_ARGCHECK_OPT( ctx, 5, X_ISNIL( ctx, 5 ) || X_ISNUM( ctx, 5 ), "FILETEMP" );

   x_fileTempCall( ctx );
}

// Operate on the selected text file area
// FileTextEof( ) --> lEof
X_FUNC( FILETEXTEOF )
{
   x_fileTextCall( ctx, 2 );
}

// Operate on the selected text file area
// FileTextGoBottom( ) --> NIL
X_FUNC( FILETEXTGOBOTTOM )
{
   x_fileTextCall( ctx, 4 );
}

// Operate on the selected text file area
// FileTextGoto( nLine ) --> NIL
X_FUNC( FILETEXTGOTO )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "FILETEXTGOTO" );
   X_ARGCHECK( ctx, !X_IS_DOUBLE( x_paritem( ctx, 1 ) ) || x_parnd( ctx, 1 ) >= -0x1p63, "FILETEXTGOTO" );
   X_ARGCHECK( ctx, !X_IS_DOUBLE( x_paritem( ctx, 1 ) ) || x_parnd( ctx, 1 ) < 0x1p63, "FILETEXTGOTO" );

   x_fileTextCall( ctx, 5 );
}

// Operate on the selected text file area
// FileTextGoTop( ) --> NIL
X_FUNC( FILETEXTGOTOP )
{
   x_fileTextCall( ctx, 3 );
}

// Operate on the selected text file area
// FileTextInfo( ) --> aInfo
X_FUNC( FILETEXTINFO )
{
   x_fileTextCall( ctx, 9 );
}

// Operate on the selected text file area
// FileTextLastRec( ) --> nLineCount
X_FUNC( FILETEXTLASTREC )
{
   x_fileTextCall( ctx, 8 );
}

// Operate on the selected text file area
// FileTextReadAndSkip( ) --> cRecord
X_FUNC( FILETEXTREADANDSKIP )
{
   x_fileTextCall( ctx, 11 );
}

// Operate on the selected text file area
// FileTextReadLn( ) --> cLine
X_FUNC( FILETEXTREADLN )
{
   x_fileTextCall( ctx, 10 );
}

// Operate on the selected text file area
// FileTextRecNo( ) --> nLineNumber
X_FUNC( FILETEXTRECNO )
{
   x_fileTextCall( ctx, 7 );
}

// Operate on the selected text file area
// FileTextSelect( [nNewArea] ) --> nOldArea
X_FUNC( FILETEXTSELECT )
{
   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || ( X_ISNUM( ctx, 1 ) && x_parnd( ctx, 1 ) >= 0 ), "FILETEXTSELECT" );
   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || x_parnd( ctx, 1 ) <= 10, "FILETEXTSELECT" );

   x_fileTextCall( ctx, 1 );
}

// Operate on the selected text file area
// FileTextSkip( [nLines] ) --> NIL
X_FUNC( FILETEXTSKIP )
{
   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISNUM( ctx, 1 ), "FILETEXTSKIP" );

   int whole = X_ISNIL( ctx, 1 ) || !X_IS_DOUBLE( x_paritem( ctx, 1 ) );

   X_ARGCHECK_OPT( ctx, 1, whole || x_parnd( ctx, 1 ) >= -0x1p63, "FILETEXTSKIP" );
   X_ARGCHECK_OPT( ctx, 1, whole || x_parnd( ctx, 1 ) < 0x1p63, "FILETEXTSKIP" );

   x_fileTextCall( ctx, 6 );
}

// Operate on the selected text file area
// FileTextUse( [cFileName], [nMode] ) --> nBorrowedHandle
X_FUNC( FILETEXTUSE )
{
   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISCHAR( ctx, 1 ), "FILETEXTUSE" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISNUM( ctx, 2 ), "FILETEXTUSE" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || ( x_parni( ctx, 2 ) & 3 ) != FO_WRITE, "FILETEXTUSE" );

   x_fileTextCall( ctx, 0 );
}

// Return the change time of a file
// FileTime( cFileName ) --> cFileTime
X_FUNC( FILETIME )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "FILETIME" );

   x_fileTimeCall( ctx );
}

// Read the modification time in milliseconds since the Unix epoch.
// FileTimeGet( cFileName, @nTimeMs ) --> lOK
X_FUNC( FILETIMEGET )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "FILETIMEGET" );
   X_ARGCHECK( ctx, X_ISBYREF( ctx, 2 ), "FILETIMEGET" );

   x_fileTimeGetCall( ctx );
}

// Set the modification time in milliseconds since the Unix epoch.
// FileTimeSet( cFileName, nTimeMs ) --> lOK
X_FUNC( FILETIMESET )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "FILETIMESET" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 2 ), "FILETIMESET" );

   x_retl( ctx, x_fsTimeSet( x_parc( ctx, 1 ), x_parnint( ctx, 2 ) ) );
}

// Set the file length without changing its position.
// FileTrunc( pHandle, [nOffset := 0] ) --> lOK
X_FUNC( FILETRUNC )
{
   X_ARGCHECK( ctx, x_itemGetPtrGC( x_paritem( ctx, 1 ), &x_fileGC ) != NULL, "FILETRUNC" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISNUM( ctx, 2 ), "FILETRUNC" );

   x_retl( ctx, x_fileTrunc( x_paritem( ctx, 1 ), x_parnintdef( ctx, 2, 0 ) ) );
}

// Release a lock on a byte range.
// FileUnlock( pHandle, nStart, nLen ) --> lOK
X_FUNC( FILEUNLOCK )
{
   X_ARGCHECK( ctx, x_itemGetPtrGC( x_paritem( ctx, 1 ), &x_fileGC ) != NULL, "FILEUNLOCK" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 2 ), "FILEUNLOCK" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 3 ), "FILEUNLOCK" );

   x_retl( ctx, x_fileLock( x_paritem( ctx, 1 ), x_parnint( ctx, 2 ), x_parnint( ctx, 3 ), 0, 1 ) );
}

// Test if a string contains a valid file name
// FileValid( cFileName, [nMaxName], [nMaxExtension], [lNoExtension], [lSpaces] ) --> lIsValid
X_FUNC( FILEVALID )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "FILEVALID" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISNUM( ctx, 2 ), "FILEVALID" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISNUM( ctx, 3 ), "FILEVALID" );
   X_ARGCHECK_OPT( ctx, 4, X_ISNIL( ctx, 4 ) || X_ISLOG( ctx, 4 ), "FILEVALID" );
   X_ARGCHECK_OPT( ctx, 5, X_ISNIL( ctx, 5 ) || X_ISLOG( ctx, 5 ), "FILEVALID" );

   int64_t maxName = x_parnintdef( ctx, 2, 0 );
   int64_t maxExt = x_parnintdef( ctx, 3, 0 );
   int noExt = x_parl( ctx, 4 );
   int spaces = x_parldef( ctx, 5, 1 );

   /* The rules of the system; nMaxName and nMaxExtension limit the lengths only when given, spaces are allowed */
   x_retl( ctx, x_fsNameValid( x_parc( ctx, 1 ), x_parclen( ctx, 1 ), maxName, maxExt, noExt, spaces ) );
}

// Write bytes from a string at the current file position.
// FileWrite( pHandle, cBuffer, [nToWrite], [nTimeOut := -1] ) --> nWritten | -1
X_FUNC( FILEWRITE )
{
   X_ARGCHECK( ctx, x_itemGetPtrGC( x_paritem( ctx, 1 ), &x_fileGC ) != NULL, "FILEWRITE" );
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 2 ), "FILEWRITE" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISNUM( ctx, 3 ), "FILEWRITE" );
   X_ARGCHECK_OPT( ctx, 4, X_ISNIL( ctx, 4 ) || X_ISNUM( ctx, 4 ), "FILEWRITE" );

   x_fileWriteCall( ctx );
}

// Write bytes at an offset and preserve the file position.
// FileWriteAt( pHandle, cBuffer, nToWrite, nOffset ) --> nWritten | -1
X_FUNC( FILEWRITEAT )
{
   X_ARGCHECK( ctx, x_itemGetPtrGC( x_paritem( ctx, 1 ), &x_fileGC ) != NULL, "FILEWRITEAT" );
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 2 ), "FILEWRITEAT" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 3 ), "FILEWRITEAT" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 4 ), "FILEWRITEAT" );

   x_fileWriteAtCall( ctx );
}

// Write INI configuration from a hash
// FileWriteIni( cFileName, hData, [cCommentBegin], [cCommentEnd], [lAutoMain] ) --> lSuccess
X_FUNC( FILEWRITEINI )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) && X_ISHASH( ctx, 2 ), "FILEWRITEINI" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISCHAR( ctx, 3 ), "FILEWRITEINI" );
   X_ARGCHECK_OPT( ctx, 4, X_ISNIL( ctx, 4 ) || X_ISCHAR( ctx, 4 ), "FILEWRITEINI" );
   X_ARGCHECK_OPT( ctx, 5, X_ISNIL( ctx, 5 ) || X_ISLOG( ctx, 5 ), "FILEWRITEINI" );

   x_fileWriteIniCall( ctx );
}

// Return a new TStreamFileWriter object
// FileWriter( cFileName, [nFileAttr] ) --> oTStreamFileWriter
X_FUNC( FILEWRITER )
{
   x_errorRaiseNamed( ctx->result, 1900, "FILEWRITER" );
}

// Return function key name
// FKLabel( nFunctionKey ) --> cKeyLabel
X_FUNC( FKLABEL )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "FKLABEL" );

   int64_t key = x_parnint( ctx, 1 );
   char label[4];

   if( key >= 1 && key <= 40 )
   {
      snprintf( label, sizeof( label ), "F%d", ( int )key );
      x_retc( ctx, label );
   }
   else
      x_retc( ctx, "" );
}

// Return number of function keys as a constant
// FKMax( ) --> nFunctionKeys
X_FUNC( FKMAX )
{
   x_retni( ctx, 40 );
}

// Count the lines in a text file
// FLineCount( cFileName ) --> nLineCount
X_FUNC( FLINECOUNT )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "FLINECOUNT" );

   x_fsCountCall( ctx, 0 );
}

// Round a number toward negative infinity
// Floor( nValue ) --> nInteger
X_FUNC( FLOOR )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "FLOOR" );

   x_mathRetnd( ctx, floor( x_parnd( ctx, 1 ) ), "FLOOR" );
}

// Compose a file name from individual components
// FNameMerge( [cPath], [cFileName], [cExtension] ) --> cResult
X_FUNC( FNAMEMERGE )
{
   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISCHAR( ctx, 1 ), "FNAMEMERGE" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISCHAR( ctx, 2 ), "FNAMEMERGE" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISCHAR( ctx, 3 ), "FNAMEMERGE" );

   x_fsNameMergeCall( ctx );
}

// Split a file name into individual components
// FNameSplit( cString, [@cPath], [@cFileName], [@cExtension] ) --> NIL
X_FUNC( FNAMESPLIT )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "FNAMESPLIT" );

   x_fsFNameSplitCall( ctx );
}

// Open a binary file
// FOpen( cFile, [nMode] ) --> nHandle
X_FUNC( FOPEN )
{
   x_retnint( ctx, x_fsOpen( x_parc( ctx, 1 ), x_parnidef( ctx, 2, FO_READ | FO_COMPAT ) ) );
}

// Parse one line of delimited text into an array
// FParseLine( cTextLine, [cDelimiter] ) --> aTextFields
X_FUNC( FPARSELINE )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "FPARSELINE" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || ( X_ISCHAR( ctx, 2 ) && x_parclen( ctx, 2 ) == 1 ), "FPARSELINE" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || x_parc( ctx, 2 )[0] != '"', "FPARSELINE" );

   x_fsParseLineCall( ctx );
}

// Read characters from a binary file into a buffer variable
// FRead( nHandle, @cBufferVar, nBytes ) --> nBytes
X_FUNC( FREAD )
{
   x_fsFReadCall( ctx );
}

// Extract the next line from a text file
// FReadLine( nFileHandle, @cLine, [caEndOfLine], [nLineLength] ) --> nReturn
X_FUNC( FREADLINE )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ) && X_ISBYREF( ctx, 2 ), "FREADLINE" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISCHAR( ctx, 3 ) || X_ISARRAY( ctx, 3 ), "FREADLINE" );
   X_ARGCHECK_OPT( ctx, 3, !X_ISCHAR( ctx, 3 ) || x_parclen( ctx, 3 ) > 0, "FREADLINE" );
   X_ARGCHECK_OPT( ctx, 3, !X_ISARRAY( ctx, 3 ) || x_arrayLen( x_paritem( ctx, 3 ) ) > 0, "FREADLINE" );
   X_ARGCHECK_OPT( ctx, 4, X_ISNIL( ctx, 4 ) || ( X_ISNUM( ctx, 4 ) && x_parnd( ctx, 4 ) >= 1 ), "FREADLINE" );
   X_ARGCHECK_OPT( ctx, 4, X_ISNIL( ctx, 4 ) || x_parnd( ctx, 4 ) < 0x1p63, "FREADLINE" );
   X_ARGCHECK_OPT( ctx, 4, X_ISNIL( ctx, 4 ) || x_parnd( ctx, 4 ) < ( double )SIZE_MAX, "FREADLINE" );

   x_fsReadLineCall( ctx );
}

// Read characters from a binary file
// FReadStr( nHandle, nBytes ) --> cString
X_FUNC( FREADSTR )
{
   x_fsFReadStrCall( ctx );
}

// Release a dynamically loaded external DLL from memory
// FreeLibrary( nDllHandle ) --> lSuccess
X_FUNC( FREELIBRARY )
{
   x_errorRaiseNamed( ctx->result, 1900, "FREELIBRARY" );
}

// Change the name of a file
// FRename( cOldFile, cNewFile ) --> nSuccess
X_FUNC( FRENAME )
{
   x_retni( ctx, IIF( x_fsRename( x_parc( ctx, 1 ), x_parc( ctx, 2 ) ), 0, F_ERROR ) );
}

// Set a binary file pointer to a new position
// FSeek( nHandle, nOffset, [nOrigin] ) --> nPosition
X_FUNC( FSEEK )
{
   x_retnint( ctx, x_fsSeek( x_parnint( ctx, 1 ), x_parnint( ctx, 2 ), x_parnidef( ctx, 3, FS_SET ) ) );
}

// Create and open a temporary file
// FTempCreate( [cTempDir], [cPrefix], [nFileAttr], [@cFileName] ) --> nFileHandle
X_FUNC( FTEMPCREATE )
{
   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISCHAR( ctx, 1 ), "FTEMPCREATE" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISCHAR( ctx, 2 ), "FTEMPCREATE" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISNUM( ctx, 3 ), "FTEMPCREATE" );
   X_ARGCHECK_OPT( ctx, 4, X_ISNIL( ctx, 4 ) || X_ISBYREF( ctx, 4 ), "FTEMPCREATE" );

   x_fsTempCreateCall( ctx );
}

// Convert a floating point number to an 8 byte binary string
// FToC( nFloat ) --> cFloat
X_FUNC( FTOC )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "FTOC" );

   x_binRetFloat64( ctx, x_parnd( ctx, 1 ) );
}

// Calculate the future value of constant, periodic investments
// FV( nInvestment, nInterestRate, nPeriods ) --> nFutureValue
X_FUNC( FV )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ) && X_ISNUM( ctx, 2 ) && X_ISNUM( ctx, 3 ), "FV" );

   double payment = x_parnd( ctx, 1 ), rate = x_parnd( ctx, 2 ), periods = x_parnd( ctx, 3 );

   double value = IIF( rate == 0.0, payment * periods, payment * ( pow( 1.0 + rate, periods ) - 1.0 ) / rate );

   x_mathRetnd( ctx, value, "FV" );
}

// Count the words in a text file
// FWordCount( cFileName ) --> nWordCount
X_FUNC( FWORDCOUNT )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "FWORDCOUNT" );

   x_fsCountCall( ctx, 2 );
}

// Write to an open binary file
// FWrite( nHandle, cBuffer, [nBytes] ) --> nBytesWritten
X_FUNC( FWRITE )
{
   x_fsFWriteCall( ctx );
}

// Display a bitmap ( .BMP ) file on screen
// GBMPDISP( aBmpArray | cFile, nX, nY, [nTransColor] ) --> NIL
X_FUNC( GBMPDISP )
{
   x_errorRaiseNamed( ctx->result, 1900, "GBMPDISP" );
}

// Load a bitmap ( .bmp ) or icon ( .ico ) file into memory
// GBMPLOAD( cFileName ) --> aBmpArray
X_FUNC( GBMPLOAD )
{
   x_errorRaiseNamed( ctx->result, 1900, "GBMPLOAD" );
}

// Scan the memory and release all garbage memory blocks
// GCAll( [lForce] ) --> NIL
X_FUNC( GCALL )
{
   x_errorRaiseNamed( ctx->result, 1900, "GCALL" );
}

// Invoke the garbage collector for one collection cycle
// GCStep( ) --> NIL
X_FUNC( GCSTEP )
{
   x_errorRaiseNamed( ctx->result, 1900, "GCSTEP" );
}

// Draw an ellipse or circle
// GELLIPSE( nXc, nYc, nRadiusX, nRadiusY, [nDegStart], [nDegEnd], [nStyle], [nColor], [nMode], [nOutLineColor], [nHeight3D] ) --> NIL
X_FUNC( GELLIPSE )
{
   x_errorRaiseNamed( ctx->result, 1900, "GELLIPSE" );
}

// (Windows only) Return an instantiated OLE Automation object
// GetActiveObject( cProgID ) --> oOleAuto
X_FUNC( GETACTIVEOBJECT )
{
   x_errorRaiseNamed( ctx->result, 1900, "GETACTIVEOBJECT" );
}

// Return the default color attribute for clearing the screen
// GetClearA( ) --> nClearAttribute
X_FUNC( GETCLEARA )
{
   x_retni( ctx, x_clearColor );
}

// Return the default character for clearing the screen
// GetClearB( ) --> nClearCharacter
X_FUNC( GETCLEARB )
{
   x_retni( ctx, x_clearChar );
}

// Return the background color of a color value
// GetClrBack( cColor ) --> cBackgroundColor
X_FUNC( GETCLRBACK )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "GETCLRBACK" );

   const char *text = x_parc( ctx, 1 );
   const char *slash = strchr( text, '/' );

   x_retc( ctx, IIF( slash != NULL, slash + 1, "" ) );
}

// Return the foreground color of a color value
// GetClrFore( cColor ) --> cForegroundColor
X_FUNC( GETCLRFORE )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "GETCLRFORE" );

   const char *text = x_parc( ctx, 1 );
   const char *slash = strchr( text, '/' );

   x_retclen( ctx, text, IIF( slash != NULL, ( size_t )( slash - text ), x_parclen( ctx, 1 ) ) );
}

// Extract a color value from a color string
// GetClrPair( cColorString, nPos ) --> cColorValue
X_FUNC( GETCLRPAIR )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) && X_ISNUM( ctx, 2 ), "GETCLRPAIR" );

   const char *text = x_parc( ctx, 1 );
   size_t start, length;

   if( x_colorPair( text, x_parclen( ctx, 1 ), x_parnint( ctx, 2 ), &start, &length ) )
      x_retclen( ctx, text + start, length );
   else
      x_retc( ctx, "" );
}

// Return the handle of the current thread
// GetCurrentThread( ) --> pThreadHandle
X_FUNC( GETCURRENTTHREAD )
{
   x_errorRaiseNamed( ctx->result, 1900, "GETCURRENTTHREAD" );
}

// (Windows only) Return the name of the default printer
// GetDefaultPrinter( ) --> cPrinterName
X_FUNC( GETDEFAULTPRINTER )
{
   x_errorRaiseNamed( ctx->result, 1900, "GETDEFAULTPRINTER" );
}

// Retrieve an operating system environment variable
// GetE( cEnvVar ) --> cString
X_FUNC( GETE )
{
   x_errorRaiseNamed( ctx->result, 1900, "GETE" );
}

// Retrieve the contents of a DOS environment variable
// GetEnv( cEnvironmentVariable, [cDefault] ) --> cString
X_FUNC( GETENV )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "GETENV" );
   X_ARGCHECK_OPT( ctx, 2, X_ISCHAR( ctx, 2 ), "GETENV" );

   const char *value = getenv( x_parc( ctx, 1 ) );

   x_retc( ctx, IIF( value != NULL, value, x_parc( ctx, 2 ) ) );
}

// Return the error code of the last dynamically called DLL function
// GetLastError( ) --> nErrorCode
X_FUNC( GETLASTERROR )
{
   x_errorRaiseNamed( ctx->result, 1900, "GETLASTERROR" );
}

// Retrieve the length of data in a serialized binary string
// GetLen8( cBinaryData ) --> nBytes
X_FUNC( GETLEN8 )
{
   x_errorRaiseNamed( ctx->result, 1900, "GETLEN8" );
}

// Return the length of a color value within a color string
// GetPairLen( cColorString, nPos ) --> nLength
X_FUNC( GETPAIRLEN )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) && X_ISNUM( ctx, 2 ), "GETPAIRLEN" );

   size_t start, length;
   int found;

   found = x_colorPair( x_parc( ctx, 1 ), x_parclen( ctx, 1 ), x_parnint( ctx, 2 ), &start, &length );
   x_retnint( ctx, IIF( found, ( int64_t )length, 0 ) );
}

// Return the absolute position of a color value in a color string
// GetPairPos( cColorString, nPos ) --> nAbsPos
X_FUNC( GETPAIRPOS )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) && X_ISNUM( ctx, 2 ), "GETPAIRPOS" );

   size_t start, length;
   int found;

   found = x_colorPair( x_parc( ctx, 1 ), x_parclen( ctx, 1 ), x_parnint( ctx, 2 ), &start, &length );
   x_retnint( ctx, IIF( found, ( int64_t )start + 1, 0 ) );
}

// Return the computing precision for trigonometric functions
// GetPrec( ) --> nDecimalPlaces
// To be checked whether GetPrec() stays; the precision could be handled by the compiler.
X_FUNC( GETPREC )
{
   x_retni( ctx, x_mathPrecision() );
}

// (Windows only) Return information about the available printers
// GetPrinters( [lPortInfo], [lLocalPrinters] ) --> aPrinterInfo
X_FUNC( GETPRINTERS )
{
   x_errorRaiseNamed( ctx->result, 1900, "GETPRINTERS" );
}

// Return the memory address of a function in a dynamically loaded DLL
// GetProcAddress( nDllHandle, cFuncName | nOrdinal ) --> pAddress
X_FUNC( GETPROCADDRESS )
{
   x_errorRaiseNamed( ctx->result, 1900, "GETPROCADDRESS" );
}

// (Windows only) Return the value of a registry entry
// GetRegistry( nHKEY, cRegPath, cRegKey ) --> xRegValue
X_FUNC( GETREGISTRY )
{
   x_errorRaiseNamed( ctx->result, 1900, "GETREGISTRY" );
}

// Return the numeric system thread ID of a thread
// GetSystemThreadID( [pThreadHandle] ) --> nSystemTID
X_FUNC( GETSYSTEMTHREADID )
{
   x_errorRaiseNamed( ctx->result, 1900, "GETSYSTEMTHREADID" );
}

// Return the numeric application thread ID of a thread
// GetThreadID( [pThreadHandle] ) --> nApplicationTID
X_FUNC( GETTHREADID )
{
   x_errorRaiseNamed( ctx->result, 1900, "GETTHREADID" );
}

// Erase a font from memory
// GFNTERASE( aFont ) --> NIL
X_FUNC( GFNTERASE )
{
   x_errorRaiseNamed( ctx->result, 1900, "GFNTERASE" );
}

// Load a font file into memory
// GFNTLOAD( cFontFile ) --> aFont
X_FUNC( GFNTLOAD )
{
   x_errorRaiseNamed( ctx->result, 1900, "GFNTLOAD" );
}

// Set an already loaded font as active
// GFNTSET( aFont, [nClipTop, nClipBottom] ) --> aClipInfo
X_FUNC( GFNTSET )
{
   x_errorRaiseNamed( ctx->result, 1900, "GFNTSET" );
}

// Draw a frame with a 3-D look
// GFRAME( nXStart, nYStart, nXEnd, nYEnd, nColorBack, nColorLight, nColorDark, nWidthUp, nWidthRight, nWidthDown, nWidthLeft, [nMode], [nStyle] ) --> NIL
X_FUNC( GFRAME )
{
   x_errorRaiseNamed( ctx->result, 1900, "GFRAME" );
}

// Draw a graphics primitive through the active GT driver
// GfxPrimitive( nType, [nTop], [nLeft], [nBottom], [nRight], [nColor] ) --> nResult
X_FUNC( GFXPRIMITIVE )
{
   X_GTCHECK( ctx, "GFXPRIMITIVE" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "GFXPRIMITIVE" );
   for( int index = 2; index <= 6; ++index )
      X_ARGCHECK_OPT( ctx, index, X_ISNUM( ctx, index ), "GFXPRIMITIVE" );

   int top = x_parni( ctx, 2 );
   int left = x_parni( ctx, 3 );
   int bottom = x_parni( ctx, 4 );
   int right = x_parni( ctx, 5 );
   int color = x_parni( ctx, 6 );

   x_retni( ctx, x_gtGfxPrimitive( x_parni( ctx, 1 ), top, left, bottom, right, color ) );
}

// Draw UTF-8 text through the active GT graphics driver
// GfxText( nTop, nLeft, cText, nColor, nSize, nWidth ) --> NIL
X_FUNC( GFXTEXT )
{
   X_GTCHECK( ctx, "GFXTEXT" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "GFXTEXT" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 2 ), "GFXTEXT" );
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 3 ), "GFXTEXT" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 4 ), "GFXTEXT" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 5 ), "GFXTEXT" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 6 ), "GFXTEXT" );

   int top = x_parni( ctx, 1 );
   int left = x_parni( ctx, 2 );
   int color = x_parni( ctx, 4 );
   int size = x_parni( ctx, 5 );
   int width = x_parni( ctx, 6 );

   x_gtGfxText( top, left, x_parc( ctx, 3 ), color, size, width );
}

// Get color information for a pixel
// GGETPIXEL( nX, nY ) --> nColor
X_FUNC( GGETPIXEL )
{
   x_errorRaiseNamed( ctx->result, 1900, "GGETPIXEL" );
}

// Draw a line in graphic mode
// GLINE( nXStart, nYStart, nXEnd, nYEnd, [nColor], [nMode] ) --> NIL
X_FUNC( GLINE )
{
   x_errorRaiseNamed( ctx->result, 1900, "GLINE" );
}

// Switch video mode
// GMODE( [nMode] ) --> aOldState
X_FUNC( GMODE )
{
   x_errorRaiseNamed( ctx->result, 1900, "GMODE" );
}

// Draw a polygon on screen
// GPOLYGON( aVertex, lFilled, nMode, nOutLine, nFillColor ) --> NIL
X_FUNC( GPOLYGON )
{
   x_errorRaiseNamed( ctx->result, 1900, "GPOLYGON" );
}

// Draw a pixel on the screen
// GPUTPIXEL( nX, nY, nColor, nMode ) --> NIL
X_FUNC( GPUTPIXEL )
{
   x_errorRaiseNamed( ctx->result, 1900, "GPUTPIXEL" );
}

// Draw a rectangle in graphic mode
// GRECT( nXStart, nYStart, nXEnd, nYEnd, [nStyle], [nColor], [nMode] ) --> NIL
X_FUNC( GRECT )
{
   x_errorRaiseNamed( ctx->result, 1900, "GRECT" );
}

// Define the allowed display area
// GSETCLIP( [nX1, nY1, nX2, nY2] ) --> aOldRegion
X_FUNC( GSETCLIP )
{
   x_errorRaiseNamed( ctx->result, 1900, "GSETCLIP" );
}

// Define a screen region to be excluded from display
// GSETEXCL( [nExclArea]|[nTop, nLeft, nBottom, nRight, nType] ) --> NIL
X_FUNC( GSETEXCL )
{
   x_errorRaiseNamed( ctx->result, 1900, "GSETEXCL" );
}

// Change components of a color
// GSETPAL( nColor, nRedValue, nGreenValue, nBlueValue ) --> aOldPalette
X_FUNC( GSETPAL )
{
   x_errorRaiseNamed( ctx->result, 1900, "GSETPAL" );
}

// Query or change an active GT driver property
// GtInfo( nSelector, [xValue], [xValue2] ) --> xResult
X_FUNC_ALIAS( GTINFO, XDEV_GTINFO )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "GTINFO" );

   X_GTCHECK( ctx, "GTINFO" );

   x_gtInfo( x_parni( ctx, 1 ), ( X_ITEM * )x_paritem( ctx, 2 ), ( X_ITEM * )x_paritem( ctx, 3 ), ctx->result );
}

// Return information about the active GT driver
// GtVersion( [nType] ) --> cVersion
X_FUNC( GTVERSION )
{
   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISNUM( ctx, 1 ), "GTVERSION" );

   x_retc( ctx, x_gtVersion( x_parnidef( ctx, 1, 0 ) ) );
}

// Draw graphic text without background
// GWRITEAT( nColStart, nLnStart, cString, [nColor], [nMode], [aFont] ) --> nWidth
X_FUNC( GWRITEAT )
{
   x_errorRaiseNamed( ctx->result, 1900, "GWRITEAT" );
}

// Remove a key/value pair from an associative array
// HaaDelAt( hArray, nPos ) --> NIL
X_FUNC( HAADELAT )
{
   X_ARGCHECK( ctx, X_ISHASH( ctx, 1 ) && X_ISNUM( ctx, 2 ), "HAADELAT" );

   if( !x_hashDelAt( ( X_ITEM * )x_paritem( ctx, 1 ), ( size_t )x_parnint( ctx, 2 ) ) )
      ( void )x_errorRaise( ctx->result, 1187, x_paritem( ctx, 1 ), x_paritem( ctx, 2 ) );
}

// Return the key of an associative array at an ordinal position
// HaaGetKeyAt( hArray, nPos ) --> xKey
X_FUNC( HAAGETKEYAT )
{
   X_ARGCHECK( ctx, X_ISHASH( ctx, 1 ) && X_ISNUM( ctx, 2 ), "HAAGETKEYAT" );

   x_hashHGetKeyAt( ctx );
}

// Return the ordinal position of a key in an associative array
// HaaGetPos( hArray, xKey ) --> nPos
X_FUNC( HAAGETPOS )
{
   X_ARGCHECK( ctx, X_ISHASH( ctx, 1 ) && x_hashIsKey( x_paritem( ctx, 2 ) ), "HAAGETPOS" );

   x_retnint( ctx, ( int64_t )x_hashPos( x_paritem( ctx, 1 ), x_paritem( ctx, 2 ) ) );
}

// Return the sort order of a key in an associative array; a hash keeps the order of adding, so it is nPos
// HaaGetRealPos( hArray, nPos ) --> nSortOrder
X_FUNC( HAAGETREALPOS )
{
   X_ARGCHECK( ctx, X_ISHASH( ctx, 1 ) && X_ISNUM( ctx, 2 ), "HAAGETREALPOS" );

   size_t pos = ( size_t )x_parnint( ctx, 2 );

   x_retnint( ctx, ( int64_t )IIF( pos >= 1 && pos <= x_hashLen( x_paritem( ctx, 1 ) ), pos, 0 ) );
}

// Return the value of an associative array at an ordinal position
// HaaGetValueAt( hArray, nPos ) --> xValue
X_FUNC( HAAGETVALUEAT )
{
   X_ARGCHECK( ctx, X_ISHASH( ctx, 1 ) && X_ISNUM( ctx, 2 ), "HAAGETVALUEAT" );

   x_hashHGetValueAt( ctx );
}

// Change the value of an associative array at an ordinal position
// HaaSetValueAt( hArray, nPos, xValue ) --> NIL
X_FUNC( HAASETVALUEAT )
{
   X_ARGCHECK( ctx, X_ISHASH( ctx, 1 ) && X_ISNUM( ctx, 2 ), "HAASETVALUEAT" );

   x_hashHSetValueAt( ctx );
}

// Pre-allocate memory for a large hash; the hash grows by itself, so nothing is needed
// HAllocate( hHash, nCount ) --> NIL
X_FUNC( HALLOCATE )
{
   X_ARGCHECK( ctx, X_ISHASH( ctx, 1 ) && X_ISNUM( ctx, 2 ), "HALLOCATE" );
}

// Replace all soft carriage returns with hard carriage returns
// HardCR( cString ) --> cConvertedString
X_FUNC( HARDCR )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "HARDCR" );

   size_t i;

   const char *text = x_parc( ctx, 1 );
   size_t length = x_parclen( ctx, 1 );
   char *buffer = ( char * )x_xgrab( length + 1 );

   for( i = 0; i < length; ++i )
      buffer[i] = IIF( ( unsigned char )text[i] == 0x8D, ( char )0x0D, text[i] );

   buffer[length] = '\0';

   x_retclen_buffer( ctx, buffer, length );
}

// Create a new hash
// Hash( [xKey1, xValue1 [, xKeyN, xValueN]] ) --> hHash
X_FUNC( HASH )
{
   int count = x_pcount( ctx );
   int i;

   X_ARGCHECK( ctx, count % 2 == 0, "HASH" );
   for( i = 1; i < count; i += 2 )
      X_ARGCHECK( ctx, x_hashIsKey( x_paritem( ctx, i ) ), "HASH" );

   x_hashHash( ctx );
}

// Create an entire copy of a hash
// HClone( hHash ) --> hClone
X_FUNC( HCLONE )
{
   X_ARGCHECK( ctx, X_ISHASH( ctx, 1 ), "HCLONE" );

   x_hashHClone( ctx );
}

// Copy key/value pairs from a hash into another hash; xMode is HASH_UNION (default), HASH_INTERSECT,
// HASH_DIFFERENCE, HASH_REMOVE from hash.ch or a block {| xKey, xValue, nPos | lCopy }
// HCopy( hSource, hTarget, [nStart], [nCount], [xMode] ) --> hTarget
X_FUNC( HCOPY )
{
   X_ARGCHECK( ctx, X_ISHASH( ctx, 1 ) && X_ISHASH( ctx, 2 ), "HCOPY" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNUM( ctx, 3 ) || X_ISNIL( ctx, 3 ), "HCOPY" );
   X_ARGCHECK_OPT( ctx, 4, X_ISNUM( ctx, 4 ) || X_ISNIL( ctx, 4 ), "HCOPY" );
   X_ARGCHECK_OPT( ctx, 5, X_ISNUM( ctx, 5 ) || X_ISBLOCK( ctx, 5 ) || X_ISNIL( ctx, 5 ), "HCOPY" );

   size_t start = ( size_t )IIF( X_ISNUM( ctx, 3 ), x_parnint( ctx, 3 ), 1 );
   int64_t count = IIF( X_ISNUM( ctx, 4 ), x_parnint( ctx, 4 ), -1 );
   int mode = IIF( X_ISNUM( ctx, 5 ), x_parni( ctx, 5 ), X_HASH_UNION );
   const X_ITEM *code = IIF( X_ISBLOCK( ctx, 5 ), x_paritem( ctx, 5 ), NULL );

   x_hashMerge( ctx, ( X_ITEM * )x_paritem( ctx, 2 ), x_paritem( ctx, 1 ), start, count, mode, code );
   x_itemReturn( ctx, x_paritem( ctx, 2 ) );
}

// Remove a key/value pair from the hash by its key
// HDel( hHash, xKey ) --> NIL
X_FUNC( HDEL )
{
   X_ARGCHECK( ctx, X_ISHASH( ctx, 1 ) && x_hashIsKey( x_paritem( ctx, 2 ) ), "HDEL" );

   x_hashDel( ( X_ITEM * )x_paritem( ctx, 1 ), x_paritem( ctx, 2 ) );
}

// Remove a key/value pair from the hash by its ordinal position
// HDelAt( hHash, nPos ) --> NIL
X_FUNC( HDELAT )
{
   X_ARGCHECK( ctx, X_ISHASH( ctx, 1 ) && X_ISNUM( ctx, 2 ), "HDELAT" );

   if( !x_hashDelAt( ( X_ITEM * )x_paritem( ctx, 1 ), ( size_t )x_parnint( ctx, 2 ) ) )
      ( void )x_errorRaise( ctx->result, 1187, x_paritem( ctx, 1 ), x_paritem( ctx, 2 ) );
}

// Evaluate a code block with each hash element; the block gets the key, the value and the position
// HEval( hHash, bBlock, [nStart], [nCount] ) --> hHash
X_FUNC( HEVAL )
{
   X_ARGCHECK( ctx, X_ISHASH( ctx, 1 ) && X_ISBLOCK( ctx, 2 ), "HEVAL" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNUM( ctx, 3 ) || X_ISNIL( ctx, 3 ), "HEVAL" );
   X_ARGCHECK_OPT( ctx, 4, X_ISNUM( ctx, 4 ) || X_ISNIL( ctx, 4 ), "HEVAL" );

   x_hashHEval( ctx );
}

// Convert a Hex string to a numeric value
// HexToNum( cHexString ) --> nNumber
X_FUNC( HEXTONUM )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "HEXTONUM" );

   x_hexToNum( ctx );
}

// Convert a Hex encoded character string to an ASCII string
// HexToStr( cHexString ) --> cASCIIstring
X_FUNC( HEXTOSTR )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "HEXTOSTR" );

   size_t length = 0;
   char *result = x_hexDecode( x_parc( ctx, 1 ), x_parclen( ctx, 1 ), &length );

   x_retclen_buffer( ctx, result, length );
}

// Copy the same value into all key/value pairs
// HFill( Hash, xValue ) --> NIL
X_FUNC( HFILL )
{
   X_ARGCHECK( ctx, X_ISHASH( ctx, 1 ), "HFILL" );

   size_t i;

   for( i = 1; i <= x_hashLen( x_paritem( ctx, 1 ) ); ++i )
      x_itemCopy( x_hashValueAt( x_paritem( ctx, 1 ), i ), x_paritem( ctx, 2 ) );
}

// Retrieve the value associated with a specified key
// HGet( hHash, xKey ) --> xValue
X_FUNC( HGET )
{
   X_ARGCHECK( ctx, X_ISHASH( ctx, 1 ) && x_hashIsKey( x_paritem( ctx, 2 ) ), "HGET" );

   x_hashHGet( ctx );
}

// Check if a hash is compatible with an associative array; a hash always keeps the order of adding
// HGetAACompatibility( hHash ) --> lIsAssocArray
X_FUNC( HGETAACOMPATIBILITY )
{
   X_ARGCHECK( ctx, X_ISHASH( ctx, 1 ), "HGETAACOMPATIBILITY" );

   x_retl( ctx, 1 );
}

// Retrieve the AutoAdd attribute of a hash
// HGetAutoAdd( hHash ) --> lIsAutoAdd
X_FUNC( HGETAUTOADD )
{
   X_ARGCHECK( ctx, X_ISHASH( ctx, 1 ), "HGETAUTOADD" );

   x_retl( ctx, ( x_hashFlags( x_paritem( ctx, 1 ) ) & X_HASH_AUTOADD ) != 0 );
}

// Retrieve the case sensitivity attribute of a hash
// HGetCaseMatch( hHash ) --> lIsCaseSensitive
X_FUNC( HGETCASEMATCH )
{
   X_ARGCHECK( ctx, X_ISHASH( ctx, 1 ), "HGETCASEMATCH" );

   x_retl( ctx, ( x_hashFlags( x_paritem( ctx, 1 ) ) & X_HASH_CASEMATCH ) != 0 );
}

// Retrieve the key from a hash by its ordinal position
// HGetKeyAt( hHash, nPos ) --> xKey
X_FUNC( HGETKEYAT )
{
   X_ARGCHECK( ctx, X_ISHASH( ctx, 1 ) && X_ISNUM( ctx, 2 ), "HGETKEYAT" );

   x_hashHGetKeyAt( ctx );
}

// Collect all keys from a hash in an array
// HGetKeys( hHash ) --> aKeys
X_FUNC( HGETKEYS )
{
   X_ARGCHECK( ctx, X_ISHASH( ctx, 1 ), "HGETKEYS" );

   x_hashHGetList( ctx, 0 );
}

// Retrieve a key/value pair from a hash by its ordinal position
// HGetPairAt( hHash, nPos ) --> { xKey, xValue }
X_FUNC( HGETPAIRAT )
{
   X_ARGCHECK( ctx, X_ISHASH( ctx, 1 ) && X_ISNUM( ctx, 2 ), "HGETPAIRAT" );

   x_hashHGetPairAt( ctx );
}

// Check if a hash is partitioned; a hash is never partitioned, it has one table of slots
// HGetPartition( hHash, [@nPageSize], [@nLevel] ) --> lIsPartitioned
X_FUNC( HGETPARTITION )
{
   X_ARGCHECK( ctx, X_ISHASH( ctx, 1 ), "HGETPARTITION" );

   x_retl( ctx, 0 );
}

// Retrieve the ordinal position of a key in a hash
// HGetPos( hHash, xKey ) --> nPos
X_FUNC( HGETPOS )
{
   X_ARGCHECK( ctx, X_ISHASH( ctx, 1 ) && x_hashIsKey( x_paritem( ctx, 2 ) ), "HGETPOS" );

   x_retnint( ctx, ( int64_t )x_hashPos( x_paritem( ctx, 1 ), x_paritem( ctx, 2 ) ) );
}

// Retrieve the sort order of all keys in an associative array; it is the order of adding: 1, 2, 3...
// HGetVaaPos( hArray ) --> aSortOrder
X_FUNC( HGETVAAPOS )
{
   X_ARGCHECK( ctx, X_ISHASH( ctx, 1 ), "HGETVAAPOS" );

   x_hashHGetVaaPos( ctx );
}

// Retrieve the value from a hash by its ordinal position
// HGetValueAt( hHash, nPos ) --> xValue
X_FUNC( HGETVALUEAT )
{
   X_ARGCHECK( ctx, X_ISHASH( ctx, 1 ) && X_ISNUM( ctx, 2 ), "HGETVALUEAT" );

   x_hashHGetValueAt( ctx );
}

// Collect all values from a hash in an array
// HGetValues( hHash ) --> aValues
X_FUNC( HGETVALUES )
{
   X_ARGCHECK( ctx, X_ISHASH( ctx, 1 ), "HGETVALUES" );

   x_hashHGetList( ctx, 1 );
}

// Determine if a key is present in a hash
// HHasKey( hHash, xKey ) --> lExists
X_FUNC( HHASKEY )
{
   X_ARGCHECK( ctx, X_ISHASH( ctx, 1 ) && x_hashIsKey( x_paritem( ctx, 2 ) ), "HHASKEY" );

   x_retl( ctx, x_hashPos( x_paritem( ctx, 1 ), x_paritem( ctx, 2 ) ) != 0 );
}

// Merge the contents of an entire hash into another hash; xMode as in HCopy()
// HMerge( hTarget, hSource, [xMode] ) --> hTarget
X_FUNC( HMERGE )
{
   X_ARGCHECK( ctx, X_ISHASH( ctx, 1 ) && X_ISHASH( ctx, 2 ), "HMERGE" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNUM( ctx, 3 ) || X_ISBLOCK( ctx, 3 ) || X_ISNIL( ctx, 3 ), "HMERGE" );

   int mode = IIF( X_ISNUM( ctx, 3 ), x_parni( ctx, 3 ), X_HASH_UNION );
   const X_ITEM *code = IIF( X_ISBLOCK( ctx, 3 ), x_paritem( ctx, 3 ), NULL );

   x_hashMerge( ctx, ( X_ITEM * )x_paritem( ctx, 1 ), x_paritem( ctx, 2 ), 1, -1, mode, code );
   x_itemReturn( ctx, x_paritem( ctx, 1 ) );
}

// Extract the hour from a DateTime value
// Hour( dDateTime ) --> nHour
X_FUNC( HOUR )
{
   x_errorRaiseNamed( ctx->result, 1900, "HOUR" );
}

// Search a value in a hash, compared as in AScan(); a block gets the key, the value and the position
// HScan( hHash, xValue, [nStart], [nCount], [lExact], [lASCII] ) --> nPos
X_FUNC( HSCAN )
{
   X_ARGCHECK( ctx, X_ISHASH( ctx, 1 ), "HSCAN" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNUM( ctx, 3 ) || X_ISNIL( ctx, 3 ), "HSCAN" );
   X_ARGCHECK_OPT( ctx, 4, X_ISNUM( ctx, 4 ) || X_ISNIL( ctx, 4 ), "HSCAN" );

   X_ITEM nil = X_ITEM_NIL_INIT;
   const X_ITEM *search = IIF( x_paritem( ctx, 2 ) != NULL, x_paritem( ctx, 2 ), &nil );
   size_t start = ( size_t )IIF( X_ISNUM( ctx, 3 ), x_parnint( ctx, 3 ), 1 );
   int64_t count = IIF( X_ISNUM( ctx, 4 ), x_parnint( ctx, 4 ), -1 );
   int exact = X_ISLOG( ctx, 5 ) && x_parl( ctx, 5 );

   x_retnint( ctx, ( int64_t )x_hashScan( ctx, x_paritem( ctx, 1 ), search, start, count, exact ) );
}

// Associate a value with a key in a hash
// HSet( hHash, xKey, xValue ) --> NIL
X_FUNC( HSET )
{
   X_ARGCHECK( ctx, X_ISHASH( ctx, 1 ) && x_hashIsKey( x_paritem( ctx, 2 ) ), "HSET" );

   x_hashAdd( ( X_ITEM * )x_paritem( ctx, 1 ), x_paritem( ctx, 2 ), x_paritem( ctx, 3 ) );
}

// Enable or disable associative array compatibility for an empty hash; a hash always keeps the order of
// adding, so it succeeds when it is turned on
// HSetAACompatibility( hHash, lToggle ) --> lSuccess
X_FUNC( HSETAACOMPATIBILITY )
{
   X_ARGCHECK( ctx, X_ISHASH( ctx, 1 ) && X_ISLOG( ctx, 2 ), "HSETAACOMPATIBILITY" );

   x_retl( ctx, x_parl( ctx, 2 ) );
}

// Change the AutoAdd attribute of a hash
// HSetAutoAdd( hHash, lOnOff ) --> hHash
X_FUNC( HSETAUTOADD )
{
   X_ARGCHECK( ctx, X_ISHASH( ctx, 1 ) && X_ISLOG( ctx, 2 ), "HSETAUTOADD" );

   x_hashSetFlags( ( X_ITEM * )x_paritem( ctx, 1 ), X_HASH_AUTOADD, x_parl( ctx, 2 ) );
   x_itemReturn( ctx, x_paritem( ctx, 1 ) );
}

// Change the case sensitivity attribute of a hash
// HSetCaseMatch( hHash, lOnOff ) --> hHash
X_FUNC( HSETCASEMATCH )
{
   X_ARGCHECK( ctx, X_ISHASH( ctx, 1 ) && X_ISLOG( ctx, 2 ), "HSETCASEMATCH" );

   x_hashSetFlags( ( X_ITEM * )x_paritem( ctx, 1 ), X_HASH_CASEMATCH, x_parl( ctx, 2 ) );
   x_itemReturn( ctx, x_paritem( ctx, 1 ) );
}

// Partition a linear hash for improved performance; the table of slots grows by itself, so nothing is needed
// HSetPartition( hHash, nPageSize, [nLevel] ) --> NIL
X_FUNC( HSETPARTITION )
{
   X_ARGCHECK( ctx, X_ISHASH( ctx, 1 ), "HSETPARTITION" );
}

// Change the value in a hash by its ordinal position
// HSetValueAt( hHash, nPos, xValue ) --> NIL
X_FUNC( HSETVALUEAT )
{
   X_ARGCHECK( ctx, X_ISHASH( ctx, 1 ) && X_ISNUM( ctx, 2 ), "HSETVALUEAT" );

   x_hashHSetValueAt( ctx );
}

// Convert a xDev numeric to a 16-bit binary integer
// I2Bin( nInteger ) --> cBinaryInteger
X_FUNC( I2BIN )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "I2BIN" );

   char buffer[2];

   uint16_t value = ( uint16_t )x_parni( ctx, 1 );

   buffer[0] = ( char )( value & 0xFF );
   buffer[1] = ( char )( ( value >> 8 ) & 0xFF );

   x_retclen( ctx, buffer, sizeof( buffer ) );
}

// Add a background task for being executed during idle states
// IdleAdd( bAction ) --> nTaskHandle
X_FUNC( IDLEADD )
{
   x_errorRaiseNamed( ctx->result, 1900, "IDLEADD" );
}

// Remove a task from the list of idle tasks
// IdleDel( nTaskHandle ) --> bAction
X_FUNC( IDLEDEL )
{
   x_errorRaiseNamed( ctx->result, 1900, "IDLEDEL" );
}

// Reset the internal counter of idle tasks
// IdleReset( ) --> NIL
X_FUNC( IDLERESET )
{
   x_errorRaiseNamed( ctx->result, 1900, "IDLERESET" );
}

// Halt idle task processing for a number of seconds
// IdleSleep( nSeconds ) --> NIL
X_FUNC( IDLESLEEP )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "IDLESLEEP" );
   x_sysSleep( x_parnd( ctx, 1 ) );
}

// Query or change the default time interval for idle task processing
// IdleSleepMSec( [nNewInterval] ) --> nOldInterval
X_FUNC( IDLESLEEPMSEC )
{
   x_errorRaiseNamed( ctx->result, 1900, "IDLESLEEPMSEC" );
}

// Signal an idle state
// IdleState( ) --> NIL
X_FUNC( IDLESTATE )
{
   x_errorRaiseNamed( ctx->result, 1900, "IDLESTATE" );
}

// Toggle the mode for CPU usage in Idle wait states
// IdleWaitNoCPU( [lNewMode] ) --> lOldMode
X_FUNC( IDLEWAITNOCPU )
{
   x_errorRaiseNamed( ctx->result, 1900, "IDLEWAITNOCPU" );
}

// Wait for an incoming connection on a server side socket
// INetAccept( pServerSocket ) --> pClientSocket
X_FUNC( INETACCEPT )
{
   x_errorRaiseNamed( ctx->result, 1900, "INETACCEPT" );
}

// Determine the internet address of a remote station
// INetAddress( pSocket ) --> cInternetAddress
X_FUNC( INETADDRESS )
{
   x_errorRaiseNamed( ctx->result, 1900, "INETADDRESS" );
}

// Release memory resources for sockets
// INetCleanup( ) --> NIL
X_FUNC( INETCLEANUP )
{
   x_errorRaiseNamed( ctx->result, 1900, "INETCLEANUP" );
}

// Reset the last error code of a socket
// INetClearError( pSocket ) --> NIL
X_FUNC( INETCLEARERROR )
{
   x_errorRaiseNamed( ctx->result, 1900, "INETCLEARERROR" );
}

// Void a callback function for a socket
// INetClearPeriodCallback( pSocket ) --> NIL
X_FUNC( INETCLEARPERIODCALLBACK )
{
   x_errorRaiseNamed( ctx->result, 1900, "INETCLEARPERIODCALLBACK" );
}

// Void a timout value for a socket
// INetClearTimeout( pSocket ) --> NIL
X_FUNC( INETCLEARTIMEOUT )
{
   x_errorRaiseNamed( ctx->result, 1900, "INETCLEARTIMEOUT" );
}

// Close a connection on a socket
// INetClose( pSocket ) --> nError
X_FUNC( INETCLOSE )
{
   x_errorRaiseNamed( ctx->result, 1900, "INETCLOSE" );
}

// Establish a sockets connection to a server
// INetConnect( cURL, nPort ) --> pSocket
X_FUNC( INETCONNECT )
{
   x_errorRaiseNamed( ctx->result, 1900, "INETCONNECT" );
}

// Establish a sockets connection to a server using the IP address
// INetConnectIP( cIPAddress, nPort ) --> pSocket
X_FUNC( INETCONNECTIP )
{
   x_errorRaiseNamed( ctx->result, 1900, "INETCONNECTIP" );
}

// Return the number of bytes transferred in the last sockets operation
// INetCount( pSocket ) --> nBytes
X_FUNC( INETCOUNT )
{
   x_errorRaiseNamed( ctx->result, 1900, "INETCOUNT" );
}

// Create a raw, unconnected socket
// INetCreate( ) --> pSocket
X_FUNC( INETCREATE )
{
   x_errorRaiseNamed( ctx->result, 1900, "INETCREATE" );
}

// Return new line characters used in internet communications
// INetCRLF( ) --> cCRLF
X_FUNC( INETCRLF )
{
   x_errorRaiseNamed( ctx->result, 1900, "INETCRLF" );
}

// Test if incoming data is available to be read
// INetDataReady( pSocket, [nMilliSecs] ) --> nResult
X_FUNC( INETDATAREADY )
{
   x_errorRaiseNamed( ctx->result, 1900, "INETDATAREADY" );
}

// Create an unbound datagram oriented socket
// INetDGram( [lBroadcast] ) --> pSocket
X_FUNC( INETDGRAM )
{
   x_errorRaiseNamed( ctx->result, 1900, "INETDGRAM" );
}

// Create a bound datagram oriented socket
// INetDGramBind( nPort, [cIPAddress], [lBroadcast], [cMulticast] ) --> pSocket
X_FUNC( INETDGRAMBIND )
{
   x_errorRaiseNamed( ctx->result, 1900, "INETDGRAMBIND" );
}

// Read data from a datagram socket
// INetDGramRecv( pSocket, @cBuffer, [nBytes] ) --> nReceivedBytes
X_FUNC( INETDGRAMRECV )
{
   x_errorRaiseNamed( ctx->result, 1900, "INETDGRAMRECV" );
}

// Send data to a datagram socket
// INetDGramSend( pSocket, cIPAddress, nPort, cData, [nBytes] ) --> nSentBytes
X_FUNC( INETDGRAMSEND )
{
   x_errorRaiseNamed( ctx->result, 1900, "INETDGRAMSEND" );
}

// Return the last sockets error code
// INetErrorCode( pSocket ) --> nErrorCode
X_FUNC( INETERRORCODE )
{
   x_errorRaiseNamed( ctx->result, 1900, "INETERRORCODE" );
}

// Return a descriptive error message
// INetErrorDesc( pSocket ) --> cErrorMessage
X_FUNC( INETERRORDESC )
{
   x_errorRaiseNamed( ctx->result, 1900, "INETERRORDESC" );
}

// Retrieve alias names from a server
// INetGetAlias( cURL ) --> aAliasNames
X_FUNC( INETGETALIAS )
{
   x_errorRaiseNamed( ctx->result, 1900, "INETGETALIAS" );
}

// Query IP addresses associated with a name
// INetGetHosts( cHostName ) --> aIPAddresses
X_FUNC( INETGETHOSTS )
{
   x_errorRaiseNamed( ctx->result, 1900, "INETGETHOSTS" );
}

// Query a callback associated with a socket
// INetGetPeriodCallback( pSocket ) --> aExecutableArray
X_FUNC( INETGETPERIODCALLBACK )
{
   x_errorRaiseNamed( ctx->result, 1900, "INETGETPERIODCALLBACK" );
}

// Query a timout value for a socket
// INetGetTimeout( pSocket ) --> nMilliSecs
X_FUNC( INETGETTIMEOUT )
{
   x_errorRaiseNamed( ctx->result, 1900, "INETGETTIMEOUT" );
}

// Initialize the sockets subsystem
// INetInit( ) --> NIL
X_FUNC( INETINIT )
{
   x_errorRaiseNamed( ctx->result, 1900, "INETINIT" );
}

// Determine the port number a socket is connected to
// INetPort( pSocket ) --> nPortNumber
X_FUNC( INETPORT )
{
   x_errorRaiseNamed( ctx->result, 1900, "INETPORT" );
}

// Read data from a socket
// INetRecv( pSocket, @cBuffer, [nBytes] ) --> nBytesRead
X_FUNC( INETRECV )
{
   x_errorRaiseNamed( ctx->result, 1900, "INETRECV" );
}

// Read all data from a socket
// INetRecvAll( pSocket, @cBuffer, [nBytes] ) --> nBytesRead
X_FUNC( INETRECVALL )
{
   x_errorRaiseNamed( ctx->result, 1900, "INETRECVALL" );
}

// Read one block of data until an end-of-block marker is detected
// INetRecvEndblock( pSocket, [cEndOfBlock], [@nBytesRead], [nMaxLength], [nIncrement] ) --> cBlock
X_FUNC( INETRECVENDBLOCK )
{
   x_errorRaiseNamed( ctx->result, 1900, "INETRECVENDBLOCK" );
}

// Read one line of data until CRLF is detected
// INetRecvLine( pSocket, [@nBytesRead], [nMaxLength], [nIncrement] ) --> cLine
X_FUNC( INETRECVLINE )
{
   x_errorRaiseNamed( ctx->result, 1900, "INETRECVLINE" );
}

// Send data to a socket
// INetSend( pSocket, cData, [nBytes] ) --> nBytesSent
X_FUNC( INETSEND )
{
   x_errorRaiseNamed( ctx->result, 1900, "INETSEND" );
}

// Send all data to a socket
// INetSendAll( pSocket, cData, [nBytes] ) --> nBytesSent
X_FUNC( INETSENDALL )
{
   x_errorRaiseNamed( ctx->result, 1900, "INETSENDALL" );
}

// Create a server side socket
// INetServer( nPort, [pRawSocket], [cIPAddress], [nMaxConnections] ) --> pServerSocket
X_FUNC( INETSERVER )
{
   x_errorRaiseNamed( ctx->result, 1900, "INETSERVER" );
}

// Associate callback information with a socket
// INetSetPeriodCallback( pSocket, aExecutableArray ) --> NIL
X_FUNC( INETSETPERIODCALLBACK )
{
   x_errorRaiseNamed( ctx->result, 1900, "INETSETPERIODCALLBACK" );
}

// Set a timout value in milliseconds for a socket
// INetSetTimeout( pSocket, nMilliSeconds ) --> NIL
X_FUNC( INETSETTIMEOUT )
{
   x_errorRaiseNamed( ctx->result, 1900, "INETSETTIMEOUT" );
}

// Return the largest number
// Infinity( [l32BitOS] ) --> nLargestNumber
X_FUNC_ALIAS( INFINITY, XDEV_INFINITY )
{
}

// Extract a character from the keyboard buffer or a mouse event
// Inkey( [nSeconds], [nEventMask] ) --> nInkeyCode
X_FUNC( INKEY )
{
   X_GTCHECK( ctx, "INKEY" );

   X_ARGCHECK_OPT( ctx, 1, X_ISNUM( ctx, 1 ) || X_ISNIL( ctx, 1 ), "INKEY" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNUM( ctx, 2 ), "INKEY" );

   int argc = x_pcount( ctx );
   int bWait = argc == 1 || ( argc > 1 && X_ISNUM( ctx, 1 ) );

   x_retni( ctx, x_inkey( bWait, x_parnd( ctx, 1 ), IIF( argc > 1, x_parni( ctx, 2 ), INKEY_ALL ) ) );
}

// Convert a numeric value to an integer
// Int( nExp ) --> nInteger
X_FUNC( INT )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "INT" );

   x_mathInt( ctx );
}

// Convert an unsigned 16-bit or 32-bit integer to a signed one; 0 for an invalid argument
// IntNeg( nUnsigned | cHexUnsigned, [l32Bit] ) --> nSigned
X_FUNC( INTNEG )
{
   x_numSigned( ctx );
}

// Convert a signed 16-bit or 32-bit integer to an unsigned one; 0 for an invalid argument
// IntPos( nSigned | cHexSigned, [l32Bit] ) --> nUnsigned
X_FUNC( INTPOS )
{
   x_numUnsigned( ctx );
}

// Exchange the foreground and background color
// InvertAttr( xColor ) --> nInvertedAttribute
X_FUNC( INVERTATTR )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ) || X_ISCHAR( ctx, 1 ), "INVERTATTR" );

   x_retni( ctx, x_colorInvert( x_colorArg( ctx, 1, 0 ) ) );
}

// Exchange the foreground and background color on the screen
// InvertWin( [nTop], [nLeft], [nBottom], [nRight] ) --> cNull
X_FUNC( INVERTWIN )
{
   X_GTCHECK( ctx, "INVERTWIN" );

   x_screenInvertWin( ctx );
}

// Convert "Yes" in a national language to a logical value
// IsAffirm( cChar ) --> lIsYes
X_FUNC( ISAFFIRM )
{
   x_errorRaiseNamed( ctx->result, 1900, "ISAFFIRM" );
}

// Check if the first character of a string is alpha-numeric
// IsAlNum( cString ) --> lIsAlphaNumeric
X_FUNC( ISALNUM )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "ISALNUM" );

   x_retl( ctx, x_utf8IsAlnum( x_parc( ctx, 1 ), x_parclen( ctx, 1 ) ) );
}

// Determine if the leftmost character in a string is alphabetic
// IsAlpha( cString ) --> lBoolean
X_FUNC( ISALPHA )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "ISALPHA" );

   x_retl( ctx, x_utf8IsAlpha( x_parc( ctx, 1 ), x_parclen( ctx, 1 ) ) );
}

// Test if the value returned by an expression is an array
// IsArray( expression ) --> lIsArray
X_FUNC( ISARRAY )
{
   const X_ITEM *item = x_paritem( ctx, 1 );

   x_retl( ctx, item != NULL && X_IS_ARRAY( item ) && !X_IS_OBJECT( item ) );
}

// Check if the first character of a string is a 7-bit ASCII character
// IsAscii( cString ) --> lIs7BitChar
X_FUNC( ISASCII )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "ISASCII" );

   x_retl( ctx, x_parclen( ctx, 1 ) > 0 && ( unsigned char )x_parc( ctx, 1 )[0] < 128 );
}

// Test a bit of a number; nBitPos 1 is the lowest bit, .F. for an invalid argument
// IsBit( nLong | cHexLong, [nBitPos] ) --> lSet
X_FUNC( ISBIT )
{
   x_numIsBit( ctx );
}

// Test if the value returned by an expression is a Code block
// IsBlock( expression ) --> lIsCodeblock
X_FUNC( ISBLOCK )
{
   const X_ITEM *item = x_paritem( ctx, 1 );

   x_retl( ctx, item != NULL && X_IS_BLOCK( item ) );
}

// Test if the value returned by an expression is a character string
// IsCharacter( expression ) --> lIsCharacter
X_FUNC( ISCHARACTER )
{
   const X_ITEM *item = x_paritem( ctx, 1 );

   x_retl( ctx, item != NULL && X_IS_STRING( item ) );
}

// Check if the first character of a string is a control character
// IsCntrl( cString ) --> lIsCtrlChar
X_FUNC( ISCNTRL )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "ISCNTRL" );

   unsigned char c = IIF( x_parclen( ctx, 1 ) > 0, ( unsigned char )x_parc( ctx, 1 )[0], 0 );

   x_retl( ctx, ( c >= 1 && c <= 31 ) || c == 127 );
}

// Determine if the current computer has color capability
// IsColor( ) --> lBoolean
X_FUNC( ISCOLOR )
{
   X_GTCHECK( ctx, "ISCOLOR" );

   x_retl( ctx, x_gtIsColor() );
}

// Test if the value returned by an expression is a Date
// IsDate( expression ) --> lIsDate
X_FUNC( ISDATE )
{
   const X_ITEM *item = x_paritem( ctx, 1 );

   x_retl( ctx, item != NULL && X_IS_DATE( item ) );
}

// Test if the value returned by an expression is a DateTime value
// IsDateTime( expression ) --> lIsDateTime
X_FUNC( ISDATETIME )
{
   x_errorRaiseNamed( ctx->result, 1900, "ISDATETIME" );
}

// Check if the default color is set
// IsDefColor( ) --> lIsDefaultColor
X_FUNC( ISDEFCOLOR )
{
   X_GTCHECK( ctx, "ISDEFCOLOR" );

   unsigned char colors[X_CLRSTR_LEN + 1];

   x_gtGetColorStr( colors );
   x_retl( ctx, strcmp( ( const char * )colors, "W/N,N/W,N/N,N/N,N/W" ) == 0 );
}

// Determine if the leftmost character in a string is a digit
// IsDigit( cString ) --> lBoolean
X_FUNC( ISDIGIT )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "ISDIGIT" );

   x_retl( ctx, x_parclen( ctx, 1 ) > 0 && isdigit( ( unsigned char )x_parc( ctx, 1 )[0] ) );
}

// Check if a character string contains the name of an existing directory
// IsDir( cDirSpec ) --> lIsDirectory
X_FUNC( ISDIR )
{
   x_errorRaiseNamed( ctx->result, 1900, "ISDIR" );
}

// Check if a character string contains the name of an existing directory
// IsDirectory( cDirName ) --> lIsDirectory
X_FUNC( ISDIRECTORY )
{
   x_errorRaiseNamed( ctx->result, 1900, "ISDIRECTORY" );
}

// (Windows only) Verify if a drive is ready
// IsDisk( cDrive ) --> lDriveIsReady
X_FUNC( ISDISK )
{
   x_errorRaiseNamed( ctx->result, 1900, "ISDISK" );
}

// Check if the first character of a string is a 7-bit graphical ASCII character
// IsGraph( cString ) --> lIs7BitGraphicalChar
X_FUNC( ISGRAPH )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "ISGRAPH" );

   x_retl( ctx, x_parclen( ctx, 1 ) > 0 && isgraph( ( unsigned char )x_parc( ctx, 1 )[0] ) );
}

// Test if the value returned by an expression is a Hash
// IsHash( expression ) --> lIsHash
X_FUNC( ISHASH )
{
   const X_ITEM *item = x_paritem( ctx, 1 );

   x_retl( ctx, item != NULL && X_IS_HASH( item ) );
}

// Check if a Date value belongs to a leap year
// IsLeap( [dDate] ) --> lIsLeapYear
X_FUNC( ISLEAP )
{
   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISDATE( ctx, 1 ), "ISLEAP" );

   long julian = x_dateParam( ctx, 1 );
   int year, month, day;

   if( julian == 0 )
   {
      x_retl( ctx, 0 );
      return;
   }

   x_dateDecode( julian, &year, &month, &day );
   x_retl( ctx, x_dateIsLeap( year ) );
}

// Test if the value returned by an expression is a logical value
// IsLogical( expression ) --> lIsLogical
X_FUNC( ISLOGICAL )
{
   const X_ITEM *item = x_paritem( ctx, 1 );

   x_retl( ctx, item != NULL && X_IS_LOGICAL( item ) );
}

// Determine if the leftmost character is a lowercase letter
// IsLower( cString ) --> lBoolean
X_FUNC( ISLOWER )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "ISLOWER" );

   x_retl( ctx, x_utf8IsLower( x_parc( ctx, 1 ), x_parclen( ctx, 1 ) ) );
}

// Test if the value returned by an expression is a Memo value
// IsMemo( expression ) --> lIsMemo
X_FUNC( ISMEMO )
{
   x_retl( ctx, 0 );
}

// Convert "No" in a national language to a logical value
// IsNegative( cChar ) --> lIsNo
X_FUNC( ISNEGATIVE )
{
   x_errorRaiseNamed( ctx->result, 1900, "ISNEGATIVE" );
}

// Test if the value returned by an expression is NIL
// IsNIL( expression ) --> lIsNIL
X_FUNC( ISNIL )
{
   const X_ITEM *item = x_paritem( ctx, 1 );

   x_retl( ctx, item == NULL || X_IS_NIL( item ) );
}

// Test if the value returned by an expression is empty
// IsNull( expression ) --> lIsNull
X_FUNC( ISNULL )
{
   x_errorRaiseNamed( ctx->result, 1900, "ISNULL" );
}

// Test if the value returned by an expression is a numeric value
// IsNumber( expression ) --> lIsNumber
X_FUNC( ISNUMBER )
{
   const X_ITEM *item = x_paritem( ctx, 1 );

   x_retl( ctx, item != NULL && X_IS_NUMERIC( item ) );
}

// Test if the value returned by an expression is an object
// IsObject( expression ) --> lIsObject
X_FUNC( ISOBJECT )
{
   const X_ITEM *item = x_paritem( ctx, 1 );

   x_retl( ctx, item != NULL && X_IS_OBJECT( item ) );
}

// Check if the first character of a string is a printable 7-bit ASCII character
// IsPrint( cString ) --> lIs7BitPrintableChar
X_FUNC( ISPRINT )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "ISPRINT" );

   x_retl( ctx, x_parclen( ctx, 1 ) > 0 && isprint( ( unsigned char )x_parc( ctx, 1 )[0] ) );
}

// Determine whether LPT1, or the given printer port, is ready
// IsPrinter( [cPort] ) --> lReady
X_FUNC( ISPRINTER )
{
   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISCHAR( ctx, 1 ), "ISPRINTER" );

   x_retl( ctx, x_sysPrinterReady( IIF( X_ISCHAR( ctx, 1 ), x_parc( ctx, 1 ), NULL ) ) );
}

// Check if the first character of a string is a punctuation character
// IsPunct( cString ) --> lIsPunctuationChar
X_FUNC( ISPUNCT )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "ISPUNCT" );

   x_retl( ctx, x_parclen( ctx, 1 ) > 0 && ispunct( ( unsigned char )x_parc( ctx, 1 )[0] ) );
}

// Check if a character string is a compiled regular expression
// IsRegExString( cString ) --> lIsRegExComp
X_FUNC( ISREGEXSTRING )
{
   x_errorRaiseNamed( ctx->result, 1900, "ISREGEXSTRING" );
}

// Compare two thread handles
// IsSameThread( pThreadHandle1, pThreadHandle2 ) --> lIsSameThread
X_FUNC( ISSAMETHREAD )
{
   x_errorRaiseNamed( ctx->result, 1900, "ISSAMETHREAD" );
}

// Check if the first character of a string is a white-space character
// IsSpace( cString ) --> lIsWhiteSpaceChar
X_FUNC( ISSPACE )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "ISSPACE" );

   x_retl( ctx, x_parclen( ctx, 1 ) > 0 && isspace( ( unsigned char )x_parc( ctx, 1 )[0] ) );
}

// Determine if the leftmost character in a string is uppercase
// IsUpper( cString ) --> lBoolean
X_FUNC( ISUPPER )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "ISUPPER" );

   x_retl( ctx, x_utf8IsUpper( x_parc( ctx, 1 ), x_parclen( ctx, 1 ) ) );
}

// Check if an expression is the thread handle of a running thread
// IsValidThread( xValue ) --> lIsValid
X_FUNC( ISVALIDTHREAD )
{
   x_errorRaiseNamed( ctx->result, 1900, "ISVALIDTHREAD" );
}

// Check if the first character of a string is a hexadecimal digit
// IsXDigit( cString ) --> lIsHexDigit
X_FUNC( ISXDIGIT )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "ISXDIGIT" );

   x_retl( ctx, x_parclen( ctx, 1 ) > 0 && isxdigit( ( unsigned char )x_parc( ctx, 1 )[0] ) );
}

// Suspend the current thread until a second thread has terminated
// JoinThread( pThreadHandle ) --> NIL
X_FUNC( JOINTHREAD )
{
   x_errorRaiseNamed( ctx->result, 1900, "JOINTHREAD" );
}

// Justify the characters of a string to the left
// JustLeft( cString, [xChar] ) --> cResult
X_FUNC( JUSTLEFT )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "JUSTLEFT" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISCHAR( ctx, 2 ) || X_ISNUM( ctx, 2 ), "JUSTLEFT" );

   x_textJustLeft( ctx );
}

// Justify the characters of a string to the right
// JustRight( cString, [xChar] ) --> cResult
X_FUNC( JUSTRIGHT )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "JUSTRIGHT" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISCHAR( ctx, 2 ) || X_ISNUM( ctx, 2 ), "JUSTRIGHT" );

   x_textJustRight( ctx );
}

// Determine the state of special keys like Ctrl or Shift keys
// KbdStat( ) --> nKeyState
X_FUNC( KBDSTAT )
{
   X_GTCHECK( ctx, "KBDSTAT" );

   x_screenKbdStat( ctx );
}

// Stuff a string into the keyboard buffer, clearing any pending keystrokes
// hidden runtime target of the KEYBOARD <cString> command
// Keyboard( cString )
X_FUNC( KEYBOARD )
{
   X_GTCHECK( ctx, "KEYBOARD" );

   x_inkeyReset();
   if( X_ISCHAR( ctx, 1 ) )
      x_inkeySetText( x_parc( ctx, 1 ), x_parclen( ctx, 1 ), 0 );
}

// Return the character of a key code as a UTF-8 string, "" when the key is not a character
// KeyChar( nKey ) --> cChar
X_FUNC( KEYCHAR )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "KEYCHAR" );

   char text[4];

   x_retclen( ctx, text, x_inkeyKeyText( x_parni( ctx, 1 ), text ) );
}

// Return the KF_* flags of Shift, Ctrl, Alt and keypad of an extended key code, 0 for a classic code
// KeyMod( nKey ) --> nFlags
X_FUNC( KEYMOD )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "KEYMOD" );

   int key = x_parni( ctx, 1 );

   x_retni( ctx, IIF( X_INKEY_ISEXT( key ) && !X_INKEY_ISMOUSEPOS( key ), X_INKEY_FLAGS( key ), 0 ) );
}

// Put an inkey code into the keyboard buffer
// KeyPut( nInkeyCode ) --> NIL
X_FUNC( KEYPUT )
{
   x_errorRaiseNamed( ctx->result, 1900, "KEYPUT" );
}

// Start a timer to write a key code into the keyboard buffer
// KeySec( nKey, nTime, [nCounter], [lMode] ) --> lSuccess
X_FUNC( KEYSEC )
{
   x_errorRaiseNamed( ctx->result, 1900, "KEYSEC" );
}

// Convert an extended key code to the classic code of inkey.ch; a classic code is returned unchanged
// KeyStd( nKey ) --> nKey
X_FUNC( KEYSTD )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "KEYSTD" );

   x_retni( ctx, x_inkeyKeyStd( x_parni( ctx, 1 ) ) );
}

// Write a key code into the keyboard buffer at a specified time
// KeyTime( nKey, cTime ) --> lSuccess
X_FUNC( KEYTIME )
{
   x_errorRaiseNamed( ctx->result, 1900, "KEYTIME" );
}

// Kill all running threads except for the main thread
// KillAllThreads( ) --> NIL
X_FUNC( KILLALLTHREADS )
{
   x_errorRaiseNamed( ctx->result, 1900, "KILLALLTHREADS" );
}

// Kill a running thread
// KillThread( pThreadHandle ) --> NIL
X_FUNC( KILLTHREAD )
{
   x_errorRaiseNamed( ctx->result, 1900, "KILLTHREAD" );
}

// Query or change the status of the Caps lock key
// KSetCaps( [lNewMode] ) --> lOldMode
X_FUNC( KSETCAPS )
{
   X_GTCHECK( ctx, "KSETCAPS" );
   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISLOG( ctx, 1 ), "KSETCAPS" );

   x_screenKSet( ctx, X_GTI_KBD_CAPSLOCK );
}

// Query or change the status of the Insert/Overwrite key
// KSetIns( [lNewMode] ) --> lOldMode
X_FUNC( KSETINS )
{
   X_GTCHECK( ctx, "KSETINS" );
   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISLOG( ctx, 1 ), "KSETINS" );

   x_screenKSet( ctx, X_GTI_KBD_INSERT );
}

// Query or change the status of the Num lock key
// KSetNum( [lNewMode] ) --> lOldMode
X_FUNC( KSETNUM )
{
   X_GTCHECK( ctx, "KSETNUM" );
   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISLOG( ctx, 1 ), "KSETNUM" );

   x_screenKSet( ctx, X_GTI_KBD_NUMLOCK );
}

// Query or change the status of the Scroll lock key
// KSetScroll( [lNewMode] ) --> lOldMode
X_FUNC( KSETSCROLL )
{
   X_GTCHECK( ctx, "KSETSCROLL" );
   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISLOG( ctx, 1 ), "KSETSCROLL" );

   x_screenKSet( ctx, X_GTI_KBD_SCROLOCK );
}

// Convert a xDev numeric value to a 32-bit binary integer
// L2Bin( nExp ) --> cBinaryInteger
X_FUNC( L2BIN )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "L2BIN" );

   char buffer[4];
   size_t i;

   uint32_t value = ( uint32_t )x_parnint( ctx, 1 );

   for( i = 0; i < 4; ++i )
      buffer[i] = ( char )( ( value >> ( i * 8 ) ) & 0xFF );

   x_retclen( ctx, buffer, sizeof( buffer ) );
}

// Return language specific error messages
// LangErrMsg( nGenericError ) --> cErrorMessage
X_FUNC( LANGERRMSG )
{
   x_errorRaiseNamed( ctx->result, 1900, "LANGERRMSG" );
}

// Return language specific messages or text strings
// LangMessage( nMessageID ) --> cMessage
X_FUNC( LANGMESSAGE )
{
   x_errorRaiseNamed( ctx->result, 1900, "LANGMESSAGE" );
}

// Return the currently selected language
// LangName( ) --> cLanguageID
X_FUNC( LANGNAME )
{
   x_errorRaiseNamed( ctx->result, 1900, "LANGNAME" );
}

// Query or change the current national language
// LangSelect( [cLanguageID] ) --> cOldLanguageID
X_FUNC( LANGSELECT )
{
   x_errorRaiseNamed( ctx->result, 1900, "LANGSELECT" );
}

// Return the number of days in a month
// LastDayOM( dDate | nMonth ) --> nDaysInMonth
X_FUNC( LASTDAYOM )
{
   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISDATE( ctx, 1 ) || X_ISNUM( ctx, 1 ), "LASTDAYOM" );

   x_dateLastDayOM( ctx );
}

// Return INKEY( ) value of last key extracted from the keyboard
// LastKey( ) --> nInkeyCode
X_FUNC( LASTKEY )
{
   x_retni( ctx, x_inkeyLast( INKEY_ALLEVENTS ) );
}

// Extract substring beginning with first character in a string
// Left( cString, nCount ) --> cSubString
X_FUNC( LEFT )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "LEFT" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 2 ), "LEFT" );

   x_textLeft( ctx );
}

// Return the length of a string or number of array elements
// Len( cString | aTarget ) --> nCount
X_FUNC( LEN )
{
   x_textLen( ctx );
}

// Return the number of characters a numeric value needs for display
// LenNum( nNumber ) --> nLength
X_FUNC( LENNUM )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "LENNUM" );

   x_retni( ctx, x_mathDisplayLength( x_paritem( ctx, 1 ) ) );
}

// Execute a function located in a dynamically loaded xDev DLL
// LibDo( cFuncName [, xParams, ...] ) --> xResult
X_FUNC( LIBDO )
{
   x_errorRaiseNamed( ctx->result, 1900, "LIBDO" );
}

// Release a dynamically loaded xDev DLL from memory
// LibFree( pDllHandle ) --> lSuccess
X_FUNC( LIBFREE )
{
   x_errorRaiseNamed( ctx->result, 1900, "LIBFREE" );
}

// Load an xDev DLL file into memory
// LibLoad( cDllFile ) --> pDLL
X_FUNC( LIBLOAD )
{
   x_errorRaiseNamed( ctx->result, 1900, "LIBLOAD" );
}

// Load an external DLL file into memory
// LoadLibrary( cDLLFile ) --> nDllHandle
X_FUNC( LOADLIBRARY )
{
   x_errorRaiseNamed( ctx->result, 1900, "LOADLIBRARY" );
}

// Calculate the natural logarithm of a numeric value
// Log( nExp ) --> nNaturalLog
X_FUNC( LOG )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "LOG" );

   x_retnd( ctx, log( x_parnd( ctx, 1 ) ) );
}

// Calculate the base-10 logarithm
// Log10( nValue ) --> nLogarithm
X_FUNC( LOG10 )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "LOG10" );

   x_mathRetnd( ctx, log10( x_parnd( ctx, 1 ) ), "LOG10" );
}

// Return a character string holding a date stamp
// LogDateStamp( ) --> cDateStamp
X_FUNC( LOGDATESTAMP )
{
   x_errorRaiseNamed( ctx->result, 1900, "LOGDATESTAMP" );
}

// Convert uppercase characters to lowercase
// Lower( cString ) --> cLowerString
X_FUNC( LOWER )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "LOWER" );

   const char *text = x_parc( ctx, 1 );
   size_t length = x_parclen( ctx, 1 );
   char *result = ( char * )x_xgrab( length + 1 );

   length = x_utf8Case( result, text, length, 0 );
   result[length] = '\0';

   x_retclen_buffer( ctx, result, length );
}

// Convert a logical value to a character
// LToC( [lLogic] ) --> cChar
X_FUNC( LTOC )
{
   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISLOG( ctx, 1 ), "LTOC" );

   x_retc( ctx, IIF( x_parl( ctx, 1 ), "T", "F" ) );
}

// Convert a logical value to a numeric value
// LToN( [lLogic] ) --> nLogic
X_FUNC( LTON )
{
   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISLOG( ctx, 1 ), "LTON" );

   x_retni( ctx, IIF( x_parl( ctx, 1 ), 1, 0 ) );
}

// Remove leading spaces from a character string
// LTrim( cString ) --> cTrimString
X_FUNC( LTRIM )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "LTRIM" );

   const char *text = x_parc( ctx, 1 );
   size_t length = x_parclen( ctx, 1 );

   while( length > 0 && *text == ' ' )
   {
      ++text;
      --length;
   }

   x_retclen( ctx, text, length );
}

// Create a new directory
// MakeDir( cDirectory ) --> nOSError
X_FUNC( MAKEDIR )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "MAKEDIR" );

   x_retni( ctx, IIF( x_fsDirMake( x_parc( ctx, 1 ) ), 0, x_fsError() ) );
}

// Calculate the mantissa of a floating point number
// Mantissa( nFloat ) --> nMantissa
X_FUNC( MANTISSA )
{
   int exponent;

   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "MANTISSA" );

   x_retnd( ctx, frexp( x_parnd( ctx, 1 ), &exponent ) * 2.0 );
}

// Return the larger of two numeric or date values
// Max( nExp1, nExp2 ) --> nLarger
X_FUNC( MAX )
{
   X_ARGCHECK( ctx, ( X_ISNUM( ctx, 1 ) && X_ISNUM( ctx, 2 ) ) || ( X_ISDATE( ctx, 1 ) && X_ISDATE( ctx, 2 ) ), "MAX" );

   x_mathMax( ctx );
}

// Determine the maximum visible screen column
// MaxCol( ) --> nColumn
X_FUNC( MAXCOL )
{
   X_GTCHECK( ctx, "MAXCOL" );

   x_retni( ctx, x_gtMaxCol() );
}

// Return the longest line in an ASCII formatted character string
// MaxLine( cText ) --> nMaxLineLength
X_FUNC( MAXLINE )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "MAXLINE" );

   x_retnint( ctx, ( int64_t )x_textLines( x_parc( ctx, 1 ), x_parclen( ctx, 1 ), 80, 1 ) );
}

// Determine the maximum visible screen row
// MaxRow( ) --> nRow
X_FUNC( MAXROW )
{
   X_GTCHECK( ctx, "MAXROW" );

   x_retni( ctx, x_gtMaxRow() );
}

// Determine the mouse cursor's screen column position
// MCol( ) --> nCurrentMouseColumn
X_FUNC( MCOL )
{
   X_GTCHECK( ctx, "MCOL" );

   x_retni( ctx, x_mouseCol() );
}

// Calculate a message digest for a stream of data using the MD5 algorithm
// MD5( cString ) --> cMD5
X_FUNC( MD5 )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "MD5" );

   X_MD5 md5;
   char hex[33];

   x_md5Init( &md5 );
   x_md5Update( &md5, x_parc( ctx, 1 ), x_parclen( ctx, 1 ) );
   x_md5Final( &md5, hex );
   x_retclen( ctx, hex, 32 );
}

// Calculate a message digest for a file using the MD5 algorithm
// MD5File( cFileName ) --> cMD5
X_FUNC( MD5FILE )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "MD5FILE" );

   x_fsMD5FileCall( ctx );
}

// Determine the double-click speed threshold of the mouse
// MDblClk( [nNewSpeed] ) --> nSpeed
X_FUNC( MDBLCLK )
{
   X_GTCHECK( ctx, "MDBLCLK" );
   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISNUM( ctx, 1 ), "MDBLCLK" );

   int previous = x_mouseGetDoubleClickSpeed();

   if( X_ISNUM( ctx, 1 ) )
      x_mouseSetDoubleClickSpeed( x_parni( ctx, 1 ) );

   x_retni( ctx, previous );
}

// Format a date as "Month dd, yy"
// MDY( [dDate] ) --> cDate
X_FUNC( MDY )
{
   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISDATE( ctx, 1 ), "MDY" );

   x_dateMDY( ctx );
}

// Extract a line of text from a character string or memo field
// MemoLine( cString, [nLineLength], [nLineNumber], [nTabSize], [lWrap] ) --> cLine
X_FUNC( MEMOLINE )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "MEMOLINE" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISNUM( ctx, 2 ), "MEMOLINE" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISNUM( ctx, 3 ), "MEMOLINE" );
   X_ARGCHECK_OPT( ctx, 4, X_ISNIL( ctx, 4 ) || X_ISNUM( ctx, 4 ), "MEMOLINE" );
   X_ARGCHECK_OPT( ctx, 5, X_ISNIL( ctx, 5 ) || X_ISLOG( ctx, 5 ), "MEMOLINE" );

   x_textMemoLine( ctx );
}

// Return the contents of a disk file as a character string
// MemoRead( cFile, [lKeepEof] ) --> cString
X_FUNC( MEMOREAD )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "MEMOREAD" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISLOG( ctx, 2 ), "MEMOREAD" );

   x_fsMemoReadCall( ctx );
}

// Determine the amount of available free pool memory
// Memory( nExp ) --> nKbytes
X_FUNC( MEMORY )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "MEMORY" );

   /* xDev has no fixed pool: character values (0), the largest block (1) and RUN (2) can use
      the free physical memory */
   int pool = x_parni( ctx, 1 );

   x_retnint( ctx, IIF( pool >= 0 && pool <= 2, ( int64_t )( x_sysMemoryAvail() / 1024 ), 0 ) );
}

// Replace carriage return/linefeeds in character strings
// MemoTran( cString, [cReplaceHardCR], [cReplaceSoftCR] ) --> cNewString
X_FUNC( MEMOTRAN )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "MEMOTRAN" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISCHAR( ctx, 2 ), "MEMOTRAN" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISCHAR( ctx, 3 ), "MEMOTRAN" );

   x_textMemoTran( ctx );
}

// Write a character string or memo field to a disk file
// MemoWrit( cFile, cString, [lAddEof := .T.] ) --> lSuccess
X_FUNC( MEMOWRIT )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "MEMOWRIT" );
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 2 ), "MEMOWRIT" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISLOG( ctx, 3 ), "MEMOWRIT" );

   x_fsMemoWritCall( ctx );
}

// Hide the mouse pointer
// MHide( ) --> NIL
X_FUNC( MHIDE )
{
   x_errorRaiseNamed( ctx->result, 1900, "MHIDE" );
}

// Define a time delay in milliseconds
// MilliSec( nMilliSeconds ) --> cNull
X_FUNC( MILLISEC )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "MILLISEC" );

   x_sysSleep( x_parnd( ctx, 1 ) / 1000.0 );
   x_retc( ctx, "" );
}

// Return the current UTC time as a Julian timestamp in milliseconds
// MilliSeconds( ) --> nMilliSeconds
X_FUNC( MILLISECONDS )
{
   x_retnint( ctx, x_dateMilliSeconds() );
}

// Return the smaller of two numeric or date values
// Min( nExp1, nExp2 ) --> nSmaller
X_FUNC( MIN )
{
   X_ARGCHECK( ctx, ( X_ISNUM( ctx, 1 ) && X_ISNUM( ctx, 2 ) ) || ( X_ISDATE( ctx, 1 ) && X_ISDATE( ctx, 2 ) ), "MIN" );

   x_mathMin( ctx );
}

// Extract the minute from a DateTime value
// Minute( dDateTime ) --> nMinute
X_FUNC( MINUTE )
{
   x_errorRaiseNamed( ctx->result, 1900, "MINUTE" );
}

// Count the number of lines in a character string or memo field
// MLCount( cString, [nLineLength], [nTabSize], [lWrap] ) --> nLines
X_FUNC( MLCOUNT )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "MLCOUNT" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISNUM( ctx, 2 ), "MLCOUNT" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISNUM( ctx, 3 ), "MLCOUNT" );
   X_ARGCHECK_OPT( ctx, 4, X_ISNIL( ctx, 4 ) || X_ISLOG( ctx, 4 ), "MLCOUNT" );

   x_textMLCount( ctx );
}

// Return position of a string based on line and column position
// MLCToPos( cText, nWidth, nLine, nCol, [nTabSize], [lWrap] ) --> nPosition
X_FUNC( MLCTOPOS )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "MLCTOPOS" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 2 ), "MLCTOPOS" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 3 ), "MLCTOPOS" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 4 ), "MLCTOPOS" );
   X_ARGCHECK_OPT( ctx, 5, X_ISNIL( ctx, 5 ) || X_ISNUM( ctx, 5 ), "MLCTOPOS" );
   X_ARGCHECK_OPT( ctx, 6, X_ISNIL( ctx, 6 ) || X_ISLOG( ctx, 6 ), "MLCTOPOS" );

   x_textMLCToPos( ctx );
}

// Determine the press status of the left mouse button
// MLeftDown( ) --> lIsPressed
X_FUNC( MLEFTDOWN )
{
   X_GTCHECK( ctx, "MLEFTDOWN" );

   x_retl( ctx, x_mouseButtonState( 0 ) );
}

// Determine the position of a line in a string or memo field
// MLPos( cString, nLineLength, nLine, [nTabSize], [lWrap] ) --> nPosition
X_FUNC( MLPOS )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "MLPOS" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 2 ), "MLPOS" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 3 ), "MLPOS" );
   X_ARGCHECK_OPT( ctx, 4, X_ISNIL( ctx, 4 ) || X_ISNUM( ctx, 4 ), "MLPOS" );
   X_ARGCHECK_OPT( ctx, 5, X_ISNIL( ctx, 5 ) || X_ISLOG( ctx, 5 ), "MLPOS" );

   x_textMLPos( ctx );
}

// Return the dBASE III PLUS modulus of two numbers
// Mod( nDividend, nDivisor ) --> nRemainder
X_FUNC( MOD )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "MOD" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 2 ), "MOD" );

   x_mathMod( ctx );
}

// Convert a date value to the number of the month
// Month( dDate ) --> nMonth
X_FUNC( MONTH )
{
   X_ARGCHECK( ctx, X_ISDATE( ctx, 1 ), "MONTH" );

   int year, month, day;

   long julian = x_pardl( ctx, 1 );

   if( julian == 0 )
   {
      x_retni( ctx, 0 );
      return;
   }

   x_dateDecode( julian, &year, &month, &day );
   x_retni( ctx, month );
}

// Return line/ column position of a string based on a position
// MPosToLC( cText, nWidth, nPos, [nTabSize], [lWrap] ) --> aLineColumn
X_FUNC( MPOSTOLC )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "MPOSTOLC" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 2 ), "MPOSTOLC" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 3 ), "MPOSTOLC" );
   X_ARGCHECK_OPT( ctx, 4, X_ISNIL( ctx, 4 ) || X_ISNUM( ctx, 4 ), "MPOSTOLC" );
   X_ARGCHECK_OPT( ctx, 5, X_ISNIL( ctx, 5 ) || X_ISLOG( ctx, 5 ), "MPOSTOLC" );

   x_textMPosToLC( ctx );
}

// Determine if a mouse is present
// MPresent( ) --> lIsPresent
X_FUNC( MPRESENT )
{
   X_GTCHECK( ctx, "MPRESENT" );

   x_retl( ctx, x_mouseIsPresent() );
}

// Re-establish the previous state of a mouse
// MRestState( cSaveState ) --> NIL
X_FUNC( MRESTSTATE )
{
   x_errorRaiseNamed( ctx->result, 1900, "MRESTSTATE" );
}

// Determine the status of the right mouse button
// MRightDown( ) --> lIsPressed
X_FUNC( MRIGHTDOWN )
{
   X_GTCHECK( ctx, "MRIGHTDOWN" );

   x_retl( ctx, x_mouseButtonState( 1 ) );
}

// Determine a mouse cursor's screen row position
// MRow( ) --> nCurrentMouseRow
X_FUNC( MROW )
{
   X_GTCHECK( ctx, "MROW" );

   x_retni( ctx, x_mouseRow() );
}

// Save the current state of a mouse
// MSaveState( ) --> cSaveState
X_FUNC( MSAVESTATE )
{
   x_errorRaiseNamed( ctx->result, 1900, "MSAVESTATE" );
}

// Set screen boundaries for the mouse cursor
// MSetBounds( [nTop], [nLeft], [nBottom], [nRight] ) --> NIL
X_FUNC( MSETBOUNDS )
{
   x_errorRaiseNamed( ctx->result, 1900, "MSETBOUNDS" );
}

// Define an inclusion region
// MSETCLIP( [nCoord list], [nMode] )
X_FUNC( MSETCLIP )
{
   x_errorRaiseNamed( ctx->result, 1900, "MSETCLIP" );
}

// Determine a mouse's visibility
// MSetCursor( [lVisible] ) --> lIsVisible
X_FUNC( MSETCURSOR )
{
   x_errorRaiseNamed( ctx->result, 1900, "MSETCURSOR" );
}

// Set a new position for the mouse cursor
// MSetPos( nRow, nCol ) --> NIL
X_FUNC( MSETPOS )
{
   X_GTCHECK( ctx, "MSETPOS" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "MSETPOS" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 2 ), "MSETPOS" );

   x_mouseSetPos( x_parni( ctx, 1 ), x_parni( ctx, 2 ) );
}

// Display the mouse pointer
// MShow( [nCol, nRow, nStyle] ) --> nOldCursorShape
X_FUNC( MSHOW )
{
   x_errorRaiseNamed( ctx->result, 1900, "MSHOW" );
}

// Return the current mouse state
// MSTATE( ) --> aState | 0
X_FUNC( MSTATE )
{
   x_errorRaiseNamed( ctx->result, 1900, "MSTATE" );
}

// Check if an application is created for multi-threading
// MultiThread( ) --> lIsMultiThread
X_FUNC( MULTITHREAD )
{
   x_errorRaiseNamed( ctx->result, 1900, "MULTITHREAD" );
}

// Create a Mutex
// MutexCreate( ) --> pMutexHandle
X_FUNC( MUTEXCREATE )
{
   x_errorRaiseNamed( ctx->result, 1900, "MUTEXCREATE" );
}

// Obtain a permanent lock on a Mutex
// MutexLock( pMutexHandle ) --> lSuccess
X_FUNC( MUTEXLOCK )
{
   x_errorRaiseNamed( ctx->result, 1900, "MUTEXLOCK" );
}

// Try to obtain a lock on a Mutex with timeout
// MutexTimeoutLock( pMutexHandle, nWaitMilliSecs ) --> lSuccess
X_FUNC( MUTEXTIMEOUTLOCK )
{
   x_errorRaiseNamed( ctx->result, 1900, "MUTEXTIMEOUTLOCK" );
}

// Try to obtain a permanent lock on a Mutex
// MutexTryLock( pMutexHandle ) --> lSuccess
X_FUNC( MUTEXTRYLOCK )
{
   x_errorRaiseNamed( ctx->result, 1900, "MUTEXTRYLOCK" );
}

// Unlock a Mutex
// MutexUnlock( pMutexHandle ) --> NIL
X_FUNC( MUTEXUNLOCK )
{
   x_errorRaiseNamed( ctx->result, 1900, "MUTEXUNLOCK" );
}

// Return application specific messages
// NationMsg( nMessageID ) --> cMessage
X_FUNC( NATIONMSG )
{
   x_errorRaiseNamed( ctx->result, 1900, "NATIONMSG" );
}

// Return the current workstation identification
// NetName( ) --> cWorkstationName
X_FUNC( NETNAME )
{
   char *name = x_sysNetName();

   x_retc( ctx, name );
   x_xfree( name );
}

// Read the pending key in the keyboard buffer
// NextKey( ) --> nInkeyCode
X_FUNC( NEXTKEY )
{
   X_GTCHECK( ctx, "NEXTKEY" );

   x_retni( ctx, x_inkeyNext( INKEY_ALL ) );
}

// Toggle snow suppression
// NoSnow( lToggle ) --> NIL
X_FUNC( NOSNOW )
{
   X_ARGCHECK( ctx, X_ISLOG( ctx, 1 ), "NOSNOW" );

   x_gtSetSnowFlag( x_parl( ctx, 1 ) );
   x_ret( ctx );
}

// Resume a single thread blocked by a particular Mutex
// Notify( pMutexHandle, [xValue] ) --> NIL
X_FUNC( NOTIFY )
{
   x_errorRaiseNamed( ctx->result, 1900, "NOTIFY" );
}

// Resume all threads blocked by a particular Mutex
// NotifyAll( pMutexHandle ) --> NIL
X_FUNC( NOTIFYALL )
{
   x_errorRaiseNamed( ctx->result, 1900, "NOTIFYALL" );
}

// Convert an integer to a string of digits for a specified number base
// NToC( nInteger, [nBase], [nLength], [cPad] ) --> cString
X_FUNC( NTOC )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "NTOC" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISNUM( ctx, 2 ), "NTOC" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || ( X_ISNUM( ctx, 3 ) && x_parnint( ctx, 3 ) >= 0 ), "NTOC" );
   X_ARGCHECK_OPT( ctx, 4, X_ISNIL( ctx, 4 ) || X_ISCHAR( ctx, 4 ), "NTOC" );

   x_mathNToC( ctx );
}

// Convert a numeric week day to its name
// NToCDoW( nDayOfWeek ) --> cDayName
X_FUNC( NTOCDOW )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "NTOCDOW" );

   int64_t number = x_parnint( ctx, 1 );

   x_retc( ctx, IIF( number >= 1 && number <= 7, x_dateCDOW( ( int )number ), "" ) );
}

// Convert a numeric month to its name
// NToCMonth( nMonth ) --> cMonthName
X_FUNC( NTOCMONTH )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "NTOCMONTH" );

   int64_t number = x_parnint( ctx, 1 );

   x_retc( ctx, IIF( number >= 1 && number <= 12, x_dateCMonth( ( int )number ), "" ) );
}

// Convert a numeric color attribute to a color string
// NToColor( nColor, [lAsChar] ) --> cColor
X_FUNC( NTOCOLOR )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "NTOCOLOR" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISLOG( ctx, 2 ), "NTOCOLOR" );

   x_screenNToColor( ctx );
}

// Return a null string ("")
// Nul( [xValue] ) --> cNullString
X_FUNC( NUL )
{
   x_errorRaiseNamed( ctx->result, 1900, "NUL" );
}

// Join numbers with a bitwise AND; -1 for an invalid argument
// NumAnd( nWord1 | cHexWord1, nWord2 | cHexWord2, [nWordN | cHexWordN, ...] ) --> nWordAnd
X_FUNC( NUMAND )
{
   x_numLogic( ctx, '&' );
}

// Count multiple occurrences of a substring within a string
// NumAt( cSearch, cString, [nSkipChars] ) --> nCount
X_FUNC( NUMAT )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) && X_ISCHAR( ctx, 2 ), "NUMAT" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISNUM( ctx, 3 ), "NUMAT" );

   x_textNumAt( ctx );
}

// Return the number of mouse buttons
// NumButtons( ) --> nButtonCount
X_FUNC( NUMBUTTONS )
{
   x_errorRaiseNamed( ctx->result, 1900, "NUMBUTTONS" );
}

// Install and/or increments an internal counter value
// NumCount( [nValue], [lIsStart] ) --> nCounter
X_FUNC( NUMCOUNT )
{
   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISNUM( ctx, 1 ), "NUMCOUNT" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISLOG( ctx, 2 ), "NUMCOUNT" );

   x_mathCounter( ctx );
}

// Return the high byte of a 16-bit integer; -1 for an invalid argument
// NumHigh( nInteger | cHex ) --> nHighByte
X_FUNC( NUMHIGH )
{
   x_numByte( ctx, 1 );
}

// Return the number of lines in an ASCII formatted character string
// NumLine( cText, [nLineLength] ) --> nLineCount
X_FUNC( NUMLINE )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "NUMLINE" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || ( X_ISNUM( ctx, 2 ) && x_parnd( ctx, 2 ) >= 1 ), "NUMLINE" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || x_parnd( ctx, 2 ) < 0x1p63, "NUMLINE" );

   size_t width = ( size_t )x_parnintdef( ctx, 2, 80 );

   x_retnint( ctx, ( int64_t )x_textLines( x_parc( ctx, 1 ), x_parclen( ctx, 1 ), width, 0 ) );
}

// Return the low byte of a 16-bit integer; -1 for an invalid argument
// NumLow( nInteger | cHex ) --> nLowByte
X_FUNC( NUMLOW )
{
   x_numByte( ctx, 0 );
}

// Mirror 8 or 16 bits of a 16-bit integer; -1 for an invalid argument
// NumMirr( nInteger | cHex, [lEightBits] ) --> nInteger
X_FUNC( NUMMIRR )
{
   x_numMirror( ctx, 1, IIF( x_parl( ctx, 2 ), 8, 16 ), 0xFFFF );
}

// Mirror bits of a numeric integer value; -1 for an invalid argument
// NumMirrX( nMaxBits, nInteger | cHex ) --> nInteger
X_FUNC( NUMMIRRX )
{
   x_numMirror( ctx, 2, IIF( X_ISNUM( ctx, 1 ), x_parni( ctx, 1 ), 0 ), UINT32_MAX );
}

// Return the 16-bit complement of a number; -1 for an invalid argument
// NumNot( nWord | cHexWord ) --> nNegatedWord
X_FUNC( NUMNOT )
{
   x_numNot( ctx );
}

// Join numbers with a bitwise OR; -1 for an invalid argument
// NumOr( nWord1 | cHexWord1, nWord2 | cHexWord2, [nWordN | cHexWordN, ...] ) --> nWordOr
X_FUNC( NUMOR )
{
   x_numLogic( ctx, '|' );
}

// Rotate the bits of a 16-bit value, or of its low byte, to the left; -1 for an invalid argument
// NumRol( nWord | cHexWord, nCount | cHexCount, [lLowByte] ) --> nWordRotated
X_FUNC( NUMROL )
{
   x_numRol( ctx );
}

// Convert a numeric value or a pointer to a Hex string
// NumToHex( nNum | pPointer, [nLen] ) --> cHexString
X_FUNC( NUMTOHEX )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ) || X_ISPOINTER( ctx, 1 ), "NUMTOHEX" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNUM( ctx, 2 ) || X_ISNIL( ctx, 2 ), "NUMTOHEX" );

   uint64_t value;
   size_t length = 0;
   char *result;

   if( X_ISPOINTER( ctx, 1 ) )
      value = ( uint64_t )( uintptr_t )x_parptr( ctx, 1 );
   else
      value = ( uint64_t )x_parnint( ctx, 1 );

   result = x_numToHex( value, x_parnidef( ctx, 2, 0 ), &length );
   x_retclen_buffer( ctx, result, length );
}

// Return the number of tokens in a string
// NumToken( cString, [cDelimiter], [nSkipWidth] ) --> nCount
X_FUNC( NUMTOKEN )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "NUMTOKEN" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISCHAR( ctx, 2 ) || X_ISNUM( ctx, 2 ), "NUMTOKEN" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISNUM( ctx, 3 ), "NUMTOKEN" );

   /* A number as the second argument is already nSkipWidth: NumToken( cString, 1 ) */
   size_t skip = ( size_t )x_parnintdef( ctx, IIF( X_ISNUM( ctx, 2 ), 2, 3 ), 0 );
   unsigned char isDelimiter[256];

   x_tokenDelimiters( IIF( X_ISCHAR( ctx, 2 ), x_parc( ctx, 2 ), NULL ), x_parclen( ctx, 2 ), isDelimiter );
   x_retnint( ctx, ( int64_t )x_tokenScan( x_parc( ctx, 1 ), x_parclen( ctx, 1 ), isDelimiter, skip, NULL ) );
}

// Join numbers with a bitwise exclusive OR; -1 for an invalid argument
// NumXor( nWord1 | cHexWord1, nWord2 | cHexWord2 ) --> nWordXor
X_FUNC( NUMXOR )
{
   x_numLogic( ctx, '^' );
}

// Retrieve the pointer to a method
// ObjMsgPtr( oObject, cMethod ) --> nMethodPointer
X_FUNC( OBJMSGPTR )
{
   x_errorRaiseNamed( ctx->result, 1900, "OBJMSGPTR" );
}

// Count the occurrance of a substring in a character string
// Occurs( cSubStr, cString ) --> nCount
X_FUNC( OCCURS )
{
   x_errorRaiseNamed( ctx->result, 1900, "OCCURS" );
}

// (Windows only) Return the last OLE error code as character string
// Ole2TxtError( ) --> cErrorCode
X_FUNC( OLE2TXTERROR )
{
   x_errorRaiseNamed( ctx->result, 1900, "OLE2TXTERROR" );
}

// (Windows only) Return the last OLE error code
// OleError( ) --> nErrorCode
X_FUNC( OLEERROR )
{
   x_errorRaiseNamed( ctx->result, 1900, "OLEERROR" );
}

// Open a child process
// OpenProcess( xExeFile, [@nStdIN], [@nStdOUT], [@nStdERR], [lBackGround], [@nProcessID] ) --> nProcessHandle
X_FUNC( OPENPROCESS )
{
   x_errorRaiseNamed( ctx->result, 1900, "OPENPROCESS" );
}

// Specify conditions for ordering
// ordCondSet( [cForCondition], [bForCondition], [lAll], [bWhileCondition], [bEval], [nInterval], [nStart], [nNext], [nRecord], [lRest], [lDescend], [lCompound], [lAdditive], [lCurrent], [lCustom], [lNoOptimize], [cWhileCondition], [lTemporary], [lUseFilter], [lExclusive] ) --> lSuccess
X_FUNC( ORDCOND )
{
   x_errorRaiseNamed( ctx->result, 1900, "ORDCOND" );
}

// Return the operating system name
// OS( ) --> cOsName
X_FUNC( OS )
{
   x_retc( ctx, X_VER_PLATFORM );
}

// Return the operating specific drive separator character
// OsDriveSeparator( ) --> cDriveSeparator
X_FUNC( OSDRIVESEPARATOR )
{
   x_retc( ctx, X_OS_DRIVE_SEPARATOR );
}

// Return the operating specific error code of the last low-level file operation
// OsError( ) --> nLastErrorCode
X_FUNC( OSERROR )
{
   x_errorRaiseNamed( ctx->result, 1900, "OSERROR" );
}

// Return the mask that matches all files of the operating system
// OsFileMask( ) --> cMask
X_FUNC( OSFILEMASK )
{
   x_retc( ctx, "*" );
}

// Return the end-of-line character(s) to use with the current operating system
// OsNewLine( ) --> cEndOfLine
X_FUNC( OSNEWLINE )
{
   x_errorRaiseNamed( ctx->result, 1900, "OSNEWLINE" );
}

// Return the operating specific characters for paths
// OsPathDelimiters( ) --> cDelimiters
X_FUNC( OSPATHDELIMITERS )
{
   x_retc( ctx, X_OS_PATH_DELIMITERS );
}

// Return the operating specific separator character for a path list
// OsPathListSeparator( ) --> cSeparator
X_FUNC( OSPATHLISTSEPARATOR )
{
   x_retc( ctx, X_OS_PATH_LIST_SEPARATOR );
}

// Return the operating specific separator character for a path
// OsPathSeparator( ) --> cDelimiter
X_FUNC( OSPATHSEPARATOR )
{
   const char separator = X_OS_PATH_SEPARATOR;

   x_retclen( ctx, &separator, 1 );
}

// (Windows only) Retrieve specific version information about the operating system
// Os_VersionInfo( ) --> aVersionInfo
X_FUNC( OS_VERSIONINFO )
{
   x_errorRaiseNamed( ctx->result, 1900, "OS_VERSIONINFO" );
}

// Write a list of values to the standard error device
// OutErr( exp list ) --> NIL
X_FUNC( OUTERR )
{
   x_consoleOut( ctx, 0, 2 );
   x_ret( ctx );
}

// Write a list of values to the standard output device
// OutStd( exp list ) --> NIL
X_FUNC( OUTSTD )
{
   x_consoleOut( ctx, 0, 1 );
   x_ret( ctx );
}

// Pad character, date, and numeric values with a fill character on the right, like PadR()
// Pad( exp, nLength, [cFillChar] ) --> cPaddedString
X_FUNC( PAD )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "PAD" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 2 ), "PAD" );
   X_ARGCHECK_OPT( ctx, 3, X_ISCHAR( ctx, 3 ), "PAD" );

   int64_t width = x_parnint( ctx, 2 );

   /* Pad() is PadR(); nLength counts characters and the fill character may be any UTF-8 character */
   x_utf8Pad( ctx, x_parc( ctx, 1 ), x_parclen( ctx, 1 ), width, x_parc( ctx, 3 ), x_parclen( ctx, 3 ), 0, 1, "PAD" );
}

// PadC character, date, and numeric values with a fill character
// PadC( exp, nLength, [cFillChar] ) --> cPaddedString
X_FUNC( PADC )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "PADC" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 2 ), "PADC" );
   X_ARGCHECK_OPT( ctx, 3, X_ISCHAR( ctx, 3 ), "PADC" );

   int64_t width = x_parnint( ctx, 2 );

   /* nLength counts characters; the fill character may be any UTF-8 character */
   x_utf8Pad( ctx, x_parc( ctx, 1 ), x_parclen( ctx, 1 ), width, x_parc( ctx, 3 ), x_parclen( ctx, 3 ), 1, 1, "PADC" );
}

// PadL character, date, and numeric values with a fill character
// PadL( exp, nLength, [cFillChar] ) --> cPaddedString
X_FUNC( PADL )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "PADL" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 2 ), "PADL" );
   X_ARGCHECK_OPT( ctx, 3, X_ISCHAR( ctx, 3 ), "PADL" );

   int64_t width = x_parnint( ctx, 2 );

   /* nLength counts characters; the fill character may be any UTF-8 character */
   x_utf8Pad( ctx, x_parc( ctx, 1 ), x_parclen( ctx, 1 ), width, x_parc( ctx, 3 ), x_parclen( ctx, 3 ), 1, 0, "PADL" );
}

// Pad a character string on the left
// PadLeft( cString, nLength, [xChar] ) --> cResult
X_FUNC( PADLEFT )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) && X_ISNUM( ctx, 2 ), "PADLEFT" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISCHAR( ctx, 3 ) || X_ISNUM( ctx, 3 ), "PADLEFT" );

   x_textPadLeft( ctx );
}

// PadR character, date, and numeric values with a fill character
// PadR( exp, nLength, [cFillChar] ) --> cPaddedString
X_FUNC( PADR )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "PADR" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 2 ), "PADR" );
   X_ARGCHECK_OPT( ctx, 3, X_ISCHAR( ctx, 3 ), "PADR" );

   int64_t width = x_parnint( ctx, 2 );

   /* nLength counts characters; the fill character may be any UTF-8 character */
   x_utf8Pad( ctx, x_parc( ctx, 1 ), x_parclen( ctx, 1 ), width, x_parc( ctx, 3 ), x_parclen( ctx, 3 ), 0, 1, "PADR" );
}

// Pad a character string on the right
// PadRight( cString, nLength, [xChar] ) --> cResult
X_FUNC( PADRIGHT )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) && X_ISNUM( ctx, 2 ), "PADRIGHT" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISCHAR( ctx, 3 ) || X_ISNUM( ctx, 3 ), "PADRIGHT" );

   X_UCHAR c;

   x_ucharArg( ctx, 3, &c );

   /* nLength counts characters: a shorter one keeps the first characters, a negative one gives "" */
   x_utf8Pad( ctx, x_parc( ctx, 1 ), x_parclen( ctx, 1 ), x_parnint( ctx, 2 ), c.bytes, c.size, 0, 1, "PADRIGHT" );
}

// Calculate a periodical payment for loans
// Payment( nLoan, nInterestRate, nPeriods ) --> nPayment
X_FUNC( PAYMENT )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ) && X_ISNUM( ctx, 2 ) && X_ISNUM( ctx, 3 ), "PAYMENT" );

   double loan = x_parnd( ctx, 1 ), rate = x_parnd( ctx, 2 ), periods = x_parnd( ctx, 3 );

   double value = IIF( rate == 0.0, loan / periods, loan * rate / ( 1.0 - pow( 1.0 + rate, -periods ) ) );

   x_mathRetnd( ctx, value, "PAYMENT" );
}

// Return the current column position of the printhead
// PCol( ) --> nColumn
X_FUNC( PCOL )
{
   x_retni( ctx, x_prnCol );
}

// Calculate the duration for paying back a loan at fixed interest rate and payments
// Periods( nLoan, nPayBack, nInterestRate ) --> nDuration
X_FUNC( PERIODS )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ) && X_ISNUM( ctx, 2 ) && X_ISNUM( ctx, 3 ), "PERIODS" );

   x_mathPeriods( ctx );
}

// Return the mathematical constant pi
// Pi( ) --> nPi
X_FUNC( PI )
{
   x_retnd( ctx, X_MATH_PI );
}

// Read bytes from a pointer into a character string
// Pointer2String( pPointer, nBytes ) --> cString
X_FUNC( POINTER2STRING )
{
   x_errorRaiseNamed( ctx->result, 1900, "POINTER2STRING" );
}

// Return the position of the first alphabetic characters in a character string
// PosAlpha( cString, [lNoAlpha], [nSkipChars] ) --> nPos
X_FUNC( POSALPHA )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "POSALPHA" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISLOG( ctx, 2 ), "POSALPHA" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISNUM( ctx, 3 ), "POSALPHA" );

   int64_t ignored = x_parnintdef( ctx, 3, 0 );
   int negate = X_ISLOG( ctx, 2 ) && x_parl( ctx, 2 );

   x_retnint( ctx, x_posFind( x_parc( ctx, 1 ), x_parclen( ctx, 1 ), ignored, x_utf8IsAlpha, negate ) );
}

// Replace a single character at a specified position in a string
// PosChar( cString, xChar, [nPos] ) --> cResult
X_FUNC( POSCHAR )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) && ( X_ISCHAR( ctx, 2 ) || X_ISNUM( ctx, 2 ) ), "POSCHAR" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISNUM( ctx, 3 ), "POSCHAR" );

   x_textPosChar( ctx );
}

// Delete character(s) at a specified position in a string
// PosDel( cString, [nPos], [nCount] ) --> cResult
X_FUNC( POSDEL )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "POSDEL" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISNUM( ctx, 2 ), "POSDEL" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISNUM( ctx, 3 ), "POSDEL" );

   x_textPosDel( ctx );
}

// Search the first position where two character strings differ
// PosDiff( cString1, cString2, [nSkipChars] ) --> nPos
X_FUNC( POSDIFF )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) && X_ISCHAR( ctx, 2 ), "POSDIFF" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISNUM( ctx, 3 ), "POSDIFF" );

   x_textPosDiff( ctx );
}

// Search the first position where two character strings are equal
// PosEqual( cString1, cString2, [nCompare], [nSkipChars] ) --> nPos
X_FUNC( POSEQUAL )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) && X_ISCHAR( ctx, 2 ), "POSEQUAL" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISNUM( ctx, 3 ), "POSEQUAL" );
   X_ARGCHECK_OPT( ctx, 4, X_ISNIL( ctx, 4 ) || X_ISNUM( ctx, 4 ), "POSEQUAL" );

   x_textPosEqual( ctx );
}

// Insert character(s) at a specified position in a string
// PosIns( cString, cInsert, [nPos] ) --> cResult
X_FUNC( POSINS )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) && X_ISCHAR( ctx, 2 ), "POSINS" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISNUM( ctx, 3 ), "POSINS" );

   x_textPosIns( ctx );
}

// Return the position of the first lower case letter in a character string
// PosLower( cString, [lNoLower], [nSkipChars] ) --> nPos
X_FUNC( POSLOWER )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "POSLOWER" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISLOG( ctx, 2 ), "POSLOWER" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISNUM( ctx, 3 ), "POSLOWER" );

   int64_t ignored = x_parnintdef( ctx, 3, 0 );
   int negate = X_ISLOG( ctx, 2 ) && x_parl( ctx, 2 );

   x_retnint( ctx, x_posFind( x_parc( ctx, 1 ), x_parclen( ctx, 1 ), ignored, x_utf8IsLower, negate ) );
}

// Retrieve the position of the first character out of a range found in a string
// PosRange( cRangeStart, cRangeEnd, cString, [lNotInRange], [nSkipChars] ) --> nPos
X_FUNC( POSRANGE )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) || X_ISNUM( ctx, 1 ), "POSRANGE" );
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 2 ) || X_ISNUM( ctx, 2 ), "POSRANGE" );
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 3 ), "POSRANGE" );
   X_ARGCHECK_OPT( ctx, 4, X_ISNIL( ctx, 4 ) || X_ISLOG( ctx, 4 ), "POSRANGE" );
   X_ARGCHECK_OPT( ctx, 5, X_ISNIL( ctx, 5 ) || X_ISNUM( ctx, 5 ), "POSRANGE" );

   x_textPosRange( ctx );
}

// Replace characters in a string beginning at a specified position
// PosRepl( cString, cReplace, [nPos] ) --> cResult
X_FUNC( POSREPL )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) && X_ISCHAR( ctx, 2 ), "POSREPL" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISNUM( ctx, 3 ), "POSREPL" );

   x_textPosRepl( ctx );
}

// Return the position of the first upper case letter in a character string
// PosUpper( cString, [lNoUpper], [nSkipChars] ) --> nPos
X_FUNC( POSUPPER )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "POSUPPER" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISLOG( ctx, 2 ), "POSUPPER" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISNUM( ctx, 3 ), "POSUPPER" );

   int64_t ignored = x_parnintdef( ctx, 3, 0 );
   int negate = X_ISLOG( ctx, 2 ) && x_parl( ctx, 2 );

   x_retnint( ctx, x_posFind( x_parc( ctx, 1 ), x_parclen( ctx, 1 ), ignored, x_utf8IsUpper, negate ) );
}

// (Windows only) Check if a particular printer is installed
// PrinterExists( cPrinterName ) --> lInstalled
X_FUNC( PRINTEREXISTS )
{
   x_errorRaiseNamed( ctx->result, 1900, "PRINTEREXISTS" );
}

// (Windows only) Print a file to a Windows printer in RAW mode
// PrintFileRaw( cPrinterName, cFileName, [cJobTitle] ) --> nErrorCode
X_FUNC( PRINTFILERAW )
{
   x_errorRaiseNamed( ctx->result, 1900, "PRINTFILERAW" );
}

// Retrieve the return value of a child process
// ProcessValue( nProcessHandle, [lWaitForTermination] ) --> nErrorLevel
X_FUNC( PROCESSVALUE )
{
   x_errorRaiseNamed( ctx->result, 1900, "PROCESSVALUE" );
}

// Determine the current PRG source code file
// ProcFile( [nCallStack] ) --> cPrgFilename
X_FUNC( PROCFILE )
{
   x_errorRaiseNamed( ctx->result, 1900, "PROCFILE" );
}

// Return the current row position of the printhead
// PRow( ) --> nRow
X_FUNC( PROW )
{
   x_retni( ctx, x_prnRow );
}

// Return the operating specific separator character for a path, a short name of OsPathSeparator()
// PS( ) --> cDelimiter
X_FUNC( PS )
{
   const char separator = X_OS_PATH_SEPARATOR;

   x_retclen( ctx, &separator, 1 );
}

// Calculate the present value of future capital, based on periodic investments
// PV( nInvestment, nInterestRate, nPeriods ) --> nPresentValue
X_FUNC( PV )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ) && X_ISNUM( ctx, 2 ) && X_ISNUM( ctx, 3 ), "PV" );

   double payment = x_parnd( ctx, 1 ), rate = x_parnd( ctx, 2 ), periods = x_parnd( ctx, 3 );

   double value = IIF( rate == 0.0, payment * periods, payment * ( 1.0 - pow( 1.0 + rate, -periods ) ) / rate );

   x_mathRetnd( ctx, value, "PV" );
}

// Retrieve the value of a parameter passed to a function, method or procedure
// PValue( nParamPos ) --> xParamValue
X_FUNC( PVALUE )
{
   x_errorRaiseNamed( ctx->result, 1900, "PVALUE" );
}

// Display a list of expressions to the console
// QOut( [exp list] ) --> NIL
X_FUNC( QOUT )
{
   x_consoleOut( ctx, 1, 0 );
   x_ret( ctx );
}

// Display a list of expressions to the console
// QQOut( [exp list] ) --> NIL
X_FUNC( QQOUT )
{
   x_consoleOut( ctx, 0, 0 );
   x_ret( ctx );
}

// Retrieve the self object during method execution
// QSelf( ) --> self
X_FUNC( QSELF )
{
   x_errorRaiseNamed( ctx->result, 1900, "QSELF" );
}

// Return the quarter a date belongs to
// Quarter( [dDate] ) --> nQuarter
X_FUNC( QUARTER )
{
   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISDATE( ctx, 1 ), "QUARTER" );

   long julian = x_dateParam( ctx, 1 );
   int year, month, day;

   if( julian == 0 )
   {
      x_retni( ctx, 0 );
      return;
   }

   x_dateDecode( julian, &year, &month, &day );
   x_retni( ctx, ( month - 1 ) / 3 + 1 );
}

// (Windows only) Check if a particular registry key with specified value exists
// QueryRegistry( nHKEY, cRegPath, cRegKey, xValue, [lCreate] ) --> lExists
X_FUNC( QUERYREGISTRY )
{
   x_errorRaiseNamed( ctx->result, 1900, "QUERYREGISTRY" );
}

// Terminate the program and run EXIT procedures, target of QUIT and CANCEL
// Quit( ) --> NIL
X_FUNC( QUIT )
{
   x_quitRaise();
}

// Retrieve the with object during WITH OBJECT execution
// QWith( ) --> oObject | NIL
X_FUNC( QWITH )
{
   x_errorRaiseNamed( ctx->result, 1900, "QWITH" );
}

// Generate a random number between two boundaries
// Random( [nMaxLimit] ) --> nRandomNumber or Random( nMinLimit, nMaxLimit ) --> nRandomNumber
X_FUNC( RANDOM )
{
   X_ARGCHECK_OPT( ctx, 1, X_ISNUM( ctx, 1 ), "RANDOM" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNUM( ctx, 2 ), "RANDOM" );

   x_mathRandom( ctx );
}

// Generate a random integer number between two boundaries
// RandomInt( [nMaxLimit] ) --> nRandomInteger or RandomInt( nMinLimit, nMaxLimit ) --> nRandomInteger
X_FUNC( RANDOMINT )
{
   X_ARGCHECK_OPT( ctx, 1, X_ISNUM( ctx, 1 ), "RANDOMINT" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNUM( ctx, 2 ), "RANDOMINT" );

   x_mathRandomInt( ctx );
}

// Set the seed for generating random numbers
// RandomSeed( [nSeed] ) --> NIL
X_FUNC( RANDOMSEED )
{
   X_ARGCHECK_OPT( ctx, 1, X_ISNUM( ctx, 1 ), "RANDOMSEED" );

   if( X_ISNUM( ctx, 1 ) )
      x_randomSeed( ( uint64_t )x_parnint( ctx, 1 ) );
   else
      x_randomSeedTime();

   x_ret( ctx );
}

// Delete character(s) within a specified range of characters
// RangeRem( xRangeStart, xRangeEnd, cString ) --> cResult
X_FUNC( RANGEREM )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) || X_ISNUM( ctx, 1 ), "RANGEREM" );
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 2 ) || X_ISNUM( ctx, 2 ), "RANGEREM" );
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 3 ), "RANGEREM" );

   x_textRangeRem( ctx );
}

// Replace character(s) within a specified range of characters
// RangeRepl( xRangeStart, xRangeEnd, cString, cReplace ) --> cResult
X_FUNC( RANGEREPL )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) || X_ISNUM( ctx, 1 ), "RANGEREPL" );
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 2 ) || X_ISNUM( ctx, 2 ), "RANGEREPL" );
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 3 ), "RANGEREPL" );
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 4 ) && x_parclen( ctx, 4 ) > 0, "RANGEREPL" );

   x_textRangeRepl( ctx );
}

// Search a value in an array beginning with the last element
// RAScan( aArray, xbSearch, [nStart], [nCount], [lExact], [lASCII] ) --> nElement
X_FUNC( RASCAN )
{
   X_ARGCHECK( ctx, X_ISARRAY( ctx, 1 ) && x_pcount( ctx ) >= 2, "RASCAN" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISNUM( ctx, 3 ), "RASCAN" );
   X_ARGCHECK_OPT( ctx, 4, X_ISNIL( ctx, 4 ) || X_ISNUM( ctx, 4 ), "RASCAN" );
   X_ARGCHECK_OPT( ctx, 5, X_ISNIL( ctx, 5 ) || X_ISLOG( ctx, 5 ), "RASCAN" );
   X_ARGCHECK_OPT( ctx, 6, X_ISNIL( ctx, 6 ) || X_ISLOG( ctx, 6 ), "RASCAN" );

   x_arrayReverseScanCall( ctx );
}

// Return the position of the last occurrence of a substring
// RAt( cSearch, cTarget, [nStart], [nEnd] ) --> nPosition
X_FUNC( RAT )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "RAT" );
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 2 ), "RAT" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNUM( ctx, 3 ), "RAT" );
   X_ARGCHECK_OPT( ctx, 4, X_ISNUM( ctx, 4 ), "RAT" );

   x_textRAt( ctx );
}

// Calculate the interest rate for a loan
// Rate( nLoan, nPayment, nPeriods ) --> nInterestRate
X_FUNC( RATE )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ) && X_ISNUM( ctx, 2 ) && X_ISNUM( ctx, 3 ), "RATE" );

   x_retnd( ctx, x_mathRate( x_parnd( ctx, 1 ), x_parnd( ctx, 2 ), x_parnd( ctx, 3 ) ) );
}

// Scan a formatted character string or memo field for text lines
// ReadLine( cText, [cDelim | aDelim], nLineLen, [nTabWidth], lWrap, [nStartOffset], @lFound, @lEOF, @nEndOffSet, @nEndOfLine ) --> NIL
X_FUNC( READLINE )
{
   int i, whole;

   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "READLINE" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISCHAR( ctx, 2 ) || X_ISARRAY( ctx, 2 ), "READLINE" );
   X_ARGCHECK_OPT( ctx, 2, !X_ISCHAR( ctx, 2 ) || x_parclen( ctx, 2 ) > 0, "READLINE" );
   X_ARGCHECK_OPT( ctx, 2, !X_ISARRAY( ctx, 2 ) || x_arrayLen( x_paritem( ctx, 2 ) ) > 0, "READLINE" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 3 ) && x_parnd( ctx, 3 ) >= 1 && x_parnd( ctx, 3 ) < 0x1p63, "READLINE" );
   X_ARGCHECK_OPT( ctx, 4, X_ISNIL( ctx, 4 ) || ( X_ISNUM( ctx, 4 ) && x_parnd( ctx, 4 ) >= 1 ), "READLINE" );
   X_ARGCHECK_OPT( ctx, 4, X_ISNIL( ctx, 4 ) || x_parnd( ctx, 4 ) < 0x1p63, "READLINE" );
   X_ARGCHECK_OPT( ctx, 5, X_ISNIL( ctx, 5 ) || X_ISLOG( ctx, 5 ), "READLINE" );
   X_ARGCHECK_OPT( ctx, 6, X_ISNIL( ctx, 6 ) || X_ISNUM( ctx, 6 ), "READLINE" );

   whole = X_ISNIL( ctx, 6 ) || !X_IS_DOUBLE( x_paritem( ctx, 6 ) );
   X_ARGCHECK_OPT( ctx, 6, whole || x_parnd( ctx, 6 ) >= -0x1p63, "READLINE" );
   X_ARGCHECK_OPT( ctx, 6, whole || x_parnd( ctx, 6 ) < 0x1p63, "READLINE" );
   for( i = 7; i <= 10; ++i )
      X_ARGCHECK( ctx, X_ISBYREF( ctx, i ), "READLINE" );

   x_textReadLineCall( ctx );
}

// Search a string using a regular expression
// RegEx( cRegEx, cString, [lCaseSensitive], [lNewLine] ) --> aMatches
X_FUNC( REGEX )
{
   x_errorRaiseNamed( ctx->result, 1900, "REGEX" );
}

// Parse a string and fill an array with parsing information
// RegExAll( cRegEx, cString, [lCaseSensitive], [lNewLine], [nMaxMatches], [nWichMatch], [lMatchOnly] ) --> aAllRegexMatches
X_FUNC( REGEXALL )
{
   x_errorRaiseNamed( ctx->result, 1900, "REGEXALL" );
}

// Parse a string and fill an array with parsing information
// RegExAtX( cRegEx, cString, [lCaseSensitive], [lNewLine] ) --> aMatches
X_FUNC( REGEXATX )
{
   x_errorRaiseNamed( ctx->result, 1900, "REGEXATX" );
}

// Compile a regular expression into a binary search pattern
// RegExComp( cRegEx, [lCaseSensitive], [lNewLine] ) --> cBinaryRegEx
X_FUNC( REGEXCOMP )
{
   x_errorRaiseNamed( ctx->result, 1900, "REGEXCOMP" );
}

// Test if a string contains a substring using a regular expression
// RegExMatch( cRegEx, cString, [lCaseSensitive], [lNewLine] ) --> lFound
X_FUNC( REGEXMATCH )
{
   x_errorRaiseNamed( ctx->result, 1900, "REGEXMATCH" );
}

// Search and replace characters within a character string using a regular expression
// RegExReplace( cRegEx, cString, cReplace, [lCaseSensitive], [lNewLine], [nMaxMatches], [nWichMatch] ) --> cNewString
X_FUNC( REGEXREPLACE )
{
   x_errorRaiseNamed( ctx->result, 1900, "REGEXREPLACE" );
}

// Parse a string using a regular expression and fill an array
// RegExSplit( cRegEx, cString, [lCaseSensitive], [lNewLine], [nMaxMatches] ) --> aSplitString
X_FUNC( REGEXSPLIT )
{
   x_errorRaiseNamed( ctx->result, 1900, "REGEXSPLIT" );
}

// Delete a specified character from both sides of a string
// RemAll( cString, [xChar] ) --> cResult
X_FUNC( REMALL )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "REMALL" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISCHAR( ctx, 2 ) || X_ISNUM( ctx, 2 ), "REMALL" );

   x_charRemove( ctx, 1, 1 );
}

// Delete a specified character from the left side of a string
// RemLeft( cString, [xChar] ) --> cResult
X_FUNC( REMLEFT )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "REMLEFT" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISCHAR( ctx, 2 ) || X_ISNUM( ctx, 2 ), "REMLEFT" );

   x_charRemove( ctx, 1, 0 );
}

// Delete a specified character from the right side of a string
// RemRight( cString, [xChar] ) --> cResult
X_FUNC( REMRIGHT )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "REMRIGHT" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISCHAR( ctx, 2 ) || X_ISNUM( ctx, 2 ), "REMRIGHT" );

   x_charRemove( ctx, 0, 1 );
}

// Rename a file and handle errors
// RenameFile( cOldFile, cNewFile ) --> nErrorCode
X_FUNC( RENAMEFILE )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) && X_ISCHAR( ctx, 2 ), "RENAMEFILE" );

   x_fsRenameFileCall( ctx );
}

// Replace a specified character on both sides of a string
// ReplAll( cString, xReplace, [xSearch] ) --> cResult
X_FUNC( REPLALL )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) && ( X_ISCHAR( ctx, 2 ) || X_ISNUM( ctx, 2 ) ), "REPLALL" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISCHAR( ctx, 3 ) || X_ISNUM( ctx, 3 ), "REPLALL" );

   x_charReplaceSides( ctx, 1, 1 );
}

// Return a string repeated a specified number of times
// Replicate( cString, nCount ) --> cRepeatedString
X_FUNC( REPLICATE )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "REPLICATE" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 2 ), "REPLICATE" );

   x_textReplicate( ctx );
}

// Replace a specified character on the left side of a string
// ReplLeft( cString, xReplace, [xSearch] ) --> cResult
X_FUNC( REPLLEFT )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) && ( X_ISCHAR( ctx, 2 ) || X_ISNUM( ctx, 2 ) ), "REPLLEFT" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISCHAR( ctx, 3 ) || X_ISNUM( ctx, 3 ), "REPLLEFT" );

   x_charReplaceSides( ctx, 1, 0 );
}

// Replace a specified character on the right side of a string
// ReplRight( cString, xReplace, [xSearch] ) --> cResult
X_FUNC( REPLRIGHT )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) && ( X_ISCHAR( ctx, 2 ) || X_ISNUM( ctx, 2 ) ), "REPLRIGHT" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISCHAR( ctx, 3 ) || X_ISNUM( ctx, 3 ), "REPLRIGHT" );

   x_charReplaceSides( ctx, 0, 1 );
}

// Replace the with object during WITH OBJECT execution
// ResetWith( oObject ) --> NIL
X_FUNC( RESETWITH )
{
   x_errorRaiseNamed( ctx->result, 1900, "RESETWITH" );
}

// Restore shape and position of the screen cursor
// RestCursor( nSaved ) --> cNull
X_FUNC( RESTCURSOR )
{
   x_errorRaiseNamed( ctx->result, 1900, "RESTCURSOR" );
}

// Convert binary information back to a code block
// RestoreBlock( aCodeblockInfo ) --> bCodeblock
X_FUNC( RESTOREBLOCK )
{
   x_errorRaiseNamed( ctx->result, 1900, "RESTOREBLOCK" );
}

// Display a saved screen region to a specified location
// RestScreen( [nTop], [nLeft], [nBottom], [nRight], cScreen ) --> NIL
X_FUNC( RESTSCREEN )
{
   X_GTCHECK( ctx, "RESTSCREEN" );
   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISNUM( ctx, 1 ), "RESTSCREEN" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISNUM( ctx, 2 ), "RESTSCREEN" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISNUM( ctx, 3 ), "RESTSCREEN" );
   X_ARGCHECK_OPT( ctx, 4, X_ISNIL( ctx, 4 ) || X_ISNUM( ctx, 4 ), "RESTSCREEN" );
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 5 ), "RESTSCREEN" );

   x_screenRestScreen( ctx );
}

// Restore SetKey() settings and associated code blocks
// RestSetKey( aSavedSetKey ) --> lRestored
X_FUNC( RESTSETKEY )
{
   x_errorRaiseNamed( ctx->result, 1900, "RESTSETKEY" );
}

// Restore the global environment of the incremental tokenizer
// RestToken( cSavedTokenEnv ) --> cNullString
X_FUNC( RESTTOKEN )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "RESTTOKEN" );

   x_tokenRestToken( ctx );
}

// Return a substring beginning with the rightmost character
// Right( cString, nCount ) --> cSubString
X_FUNC( RIGHT )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "RIGHT" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 2 ), "RIGHT" );

   x_textRight( ctx );
}

// Return a numeric value rounded to a specified number of digits
// Round( nNumber, nDecimals ) --> nRounded
X_FUNC( ROUND )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "ROUND" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 2 ), "ROUND" );

   x_mathRound( ctx );
}

// Return the screen row position of the cursor
// Row( ) --> nRow
X_FUNC( ROW )
{
   X_GTCHECK( ctx, "ROW" );

   unsigned short row = 0, col = 0;

   x_gtGetPos( &row, &col );
   x_retni( ctx, row );
}

// Convert radians to degrees
// RToD( nRadians ) --> nDegrees
X_FUNC( RTOD )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "RTOD" );

   x_mathRetnd( ctx, x_mathRadToDeg( x_parnd( ctx, 1 ) ), "RTOD" );
}

// Remove trailing spaces from a character string
// RTrim( cString ) --> cTrimString
X_FUNC( RTRIM )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "RTRIM" );

   const char *text = x_parc( ctx, 1 );
   size_t length = x_strRTrimLen( text, x_parclen( ctx, 1 ), 0 );

   x_retclen( ctx, text, length );
}

// Execute an operating system command, target of the RUN command
// Run( cCommand ) --> NIL
X_FUNC( RUN )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "RUN" );

   x_sysRun( ctx );
}

// Execute an operating system command and capture its standard output and error output
// RunCapture( cCommand, [@nExitCode] ) --> cOutput
X_FUNC( RUNCAPTURE )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "RUNCAPTURE" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISNUM( ctx, 2 ), "RUNCAPTURE" );

   x_sysRunCapture( ctx );
}

// Prepare a code block for serialization
// SaveBlock( bCodeblock ) --> aCodeBlockInfo
X_FUNC( SAVEBLOCK )
{
   x_errorRaiseNamed( ctx->result, 1900, "SAVEBLOCK" );
}

// Save the current cursor shape and position
// SaveCursor( ) --> nSaved
X_FUNC( SAVECURSOR )
{
   x_errorRaiseNamed( ctx->result, 1900, "SAVECURSOR" );
}

// Save a screen region for later display
// SaveScreen( [nTop], [nLeft], [nBottom], [nRight] ) --> cScreen
X_FUNC( SAVESCREEN )
{
   X_GTCHECK( ctx, "SAVESCREEN" );
   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISNUM( ctx, 1 ), "SAVESCREEN" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISNUM( ctx, 2 ), "SAVESCREEN" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISNUM( ctx, 3 ), "SAVESCREEN" );
   X_ARGCHECK_OPT( ctx, 4, X_ISNIL( ctx, 4 ) || X_ISNUM( ctx, 4 ), "SAVESCREEN" );

   x_screenSaveScreen( ctx );
}

// Save SetKey() settings and associated code blocks
// SaveSetKey( ) --> aSavedSetKeys
X_FUNC( SAVESETKEY )
{
   x_errorRaiseNamed( ctx->result, 1900, "SAVESETKEY" );
}

// Save the global environment of the incremental tokenizer
// SaveToken( ) --> cGlobalTokenEnv
X_FUNC( SAVETOKEN )
{
   size_t length;
   const char *env = x_tokenEnvGlobal( &length );

   if( env != NULL )
      x_retclen( ctx, env, length );
   else
      x_retc( ctx, "" );
}

// Display a value at a screen position, optionally formatted by a picture and in a color
// Say( nRow, nCol, exp, [cPicture], [cColorString] ) --> NIL
X_FUNC( SAY )
{
   X_GTCHECK( ctx, "SAY" );

   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "SAY" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 2 ), "SAY" );
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 3 ) || X_ISNUM( ctx, 3 ) || X_ISDATE( ctx, 3 ) || X_ISLOG( ctx, 3 ), "SAY" );
   X_ARGCHECK_OPT( ctx, 4, X_ISNIL( ctx, 4 ) || X_ISCHAR( ctx, 4 ), "SAY" );
   X_ARGCHECK_OPT( ctx, 5, X_ISNIL( ctx, 5 ) || X_ISCHAR( ctx, 5 ), "SAY" );

   x_gtSetPos( ( unsigned short )x_parni( ctx, 1 ), ( unsigned short )x_parni( ctx, 2 ) );
   x_dispOut( ctx, 3, IIF( X_ISCHAR( ctx, 4 ), x_parc( ctx, 4 ), "" ), x_parclen( ctx, 4 ), 5 );
}

// Output a string vertically to the bottom of the screen
// SayDown( cString, [nDelay], [nRow], [nCol] ) --> cNull
X_FUNC( SAYDOWN )
{
   x_errorRaiseNamed( ctx->result, 1900, "SAYDOWN" );
}

// Output a string on the screen using a "move in" effect
// SayMoveIn( cString [nDelay], [nRow], [nCol], [lRight] ) --> cNull
X_FUNC( SAYMOVEIN )
{
   x_errorRaiseNamed( ctx->result, 1900, "SAYMOVEIN" );
}

// Display a string on screen keeping existing color attributes
// SayScreen( cString, [nRow], [nCol] ) --> cNull
X_FUNC( SAYSCREEN )
{
   x_errorRaiseNamed( ctx->result, 1900, "SAYSCREEN" );
}

// Output a string on the screen using a "spread" effect
// SaySpread( cString, [nMilliSecondsDelay], [nRow], [nCol] ) --> cNull
X_FUNC( SAYSPREAD )
{
   x_errorRaiseNamed( ctx->result, 1900, "SAYSPREAD" );
}

// Return the numeric color attribute for a specified coordinate on the screen
// ScreenAttr( [nRow], [nCol] ) --> nAttribute
X_FUNC( SCREENATTR )
{
   X_GTCHECK( ctx, "SCREENATTR" );

   unsigned short row, col;
   int color = 0;
   uint8_t attr;
   uint16_t character;

   x_gtGetPos( &row, &col );
   x_gtGetCell( ( int )x_parnintdef( ctx, 1, row ), ( int )x_parnintdef( ctx, 2, col ), &color, &attr, &character );
   x_retni( ctx, color );
}

// Write the contents of the current screen to a file
// ScreenFile( cFileName, [lAppend], [nOffSet], [lTruncate] ) --> nBytesWritten
X_FUNC( SCREENFILE )
{
   x_errorRaiseNamed( ctx->result, 1900, "SCREENFILE" );
}

// Search strings on the screen and change their color
// ScreenMark( cSearch, xColor, lCase, [lAll], [cPre], [cPost] ) --> lFound
X_FUNC( SCREENMARK )
{
   x_errorRaiseNamed( ctx->result, 1900, "SCREENMARK" );
}

// Mix a character string with color attributes
// ScreenMix( cString, cColorAttr, [nRow], [nCol] ) --> cNull
X_FUNC( SCREENMIX )
{
   x_errorRaiseNamed( ctx->result, 1900, "SCREENMIX" );
}

// Return the screen contents beginning at a specified position
// ScreenStr( [nRow], [nCol], [nCount] ) --> cScreenString
X_FUNC( SCREENSTR )
{
   x_errorRaiseNamed( ctx->result, 1900, "SCREENSTR" );
}

// Scroll a screen region up or down, right or left
// Scroll( [nTop], [nLeft], [nBottom], [nRight], [nVert], [nHoriz], [xColor], [xChar] ) --> NIL
X_FUNC( SCROLL )
{
   X_GTCHECK( ctx, "SCROLL" );
   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISNUM( ctx, 1 ), "SCROLL" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISNUM( ctx, 2 ), "SCROLL" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISNUM( ctx, 3 ), "SCROLL" );
   X_ARGCHECK_OPT( ctx, 4, X_ISNIL( ctx, 4 ) || X_ISNUM( ctx, 4 ), "SCROLL" );
   X_ARGCHECK_OPT( ctx, 5, X_ISNIL( ctx, 5 ) || X_ISNUM( ctx, 5 ), "SCROLL" );
   X_ARGCHECK_OPT( ctx, 6, X_ISNIL( ctx, 6 ) || X_ISNUM( ctx, 6 ), "SCROLL" );

   X_ARGCHECK_OPT( ctx, 7, X_ISNIL( ctx, 7 ) || X_ISCHAR( ctx, 7 ) || X_ISNUM( ctx, 7 ), "SCROLL" );
   X_ARGCHECK_OPT( ctx, 7, !X_ISNUM( ctx, 7 ) || ( x_parnd( ctx, 7 ) >= -1 && x_parnd( ctx, 7 ) <= 255 ), "SCROLL" );
   X_ARGCHECK_OPT( ctx, 8, X_ISNIL( ctx, 8 ) || X_ISCHAR( ctx, 8 ) || X_ISNUM( ctx, 8 ), "SCROLL" );
   X_ARGCHECK_OPT( ctx, 8, !X_ISNUM( ctx, 8 ) || ( x_parnd( ctx, 8 ) >= -1 && x_parnd( ctx, 8 ) <= 65535 ), "SCROLL" );

   x_screenScroll( ctx );
}

// Scroll a screen region horizontally and/or vertically
// Scrollfixed( [nTop], [nLeft], [nBottom], [nRight], [nRows], [nColumns] ) --> NIL
X_FUNC( SCROLLFIXED )
{
   x_errorRaiseNamed( ctx->result, 1900, "SCROLLFIXED" );
}

// Return the number of seconds elapsed since midnight
// Seconds( ) --> nSeconds
X_FUNC( SECONDS )
{
   struct timespec now;

   timespec_get( &now, TIME_UTC );

   struct tm *local = localtime( &now.tv_sec );

   x_retnd( ctx, local->tm_hour * 3600 + local->tm_min * 60 + local->tm_sec + ( now.tv_nsec / 10000000 ) / 100.0 );
}

// Return the CPU time used by the current process
// SecondsCPU( [nWhichTime] ) --> nSeconds
X_FUNC( SECONDSCPU )
{
   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISNUM( ctx, 1 ), "SECONDSCPU" );

   x_retnd( ctx, x_sysCpuTime( ( int )x_parnintdef( ctx, 1, 3 ) ) );
}

// Suspend thread execution for a number of seconds
// SecondsSleep( nSeconds ) --> NIL
X_FUNC( SECONDSSLEEP )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "SECONDSSLEEP" );

   x_sysSleep( x_parnd( ctx, 1 ) );
}

// Calculate the number of seconds from a time string
// Secs( cTime | dDateTime ) --> nSeconds
X_FUNC( SECS )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) || X_ISDATE( ctx, 1 ), "SECS" );

   double seconds = IIF( X_ISCHAR( ctx, 1 ), x_timeToSeconds( x_parc( ctx, 1 ), x_parclen( ctx, 1 ) ), 0 );

   x_retnint( ctx, IIF( seconds < 0, 0, ( int64_t )seconds ) );
}

// Convert numeric seconds into a time formatted character string
// SecToTime( [nSeconds], [lHundredth] ) --> cTime
X_FUNC( SECTOTIME )
{
   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISNUM( ctx, 1 ), "SECTOTIME" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISLOG( ctx, 2 ), "SECTOTIME" );

   x_timeSecToTime( ctx );
}

// Convert an arbitrary value to a binary string
// Serialize( xValue ) --> cBinary
X_FUNC( SERIALIZE )
{
   x_errorRaiseNamed( ctx->result, 1900, "SERIALIZE" );
}

// Serialize values of simple data types
// SerializeSimple( xValue ) --> cBinaryData
X_FUNC( SERIALIZESIMPLE )
{
   x_errorRaiseNamed( ctx->result, 1900, "SERIALIZESIMPLE" );
}

// Return the position of the next chunk of binary data to retrieve
// SerialNext( cBinaryData ) --> nPos
X_FUNC( SERIALNEXT )
{
   x_errorRaiseNamed( ctx->result, 1900, "SERIALNEXT" );
}

// Inspect or change a system setting
// Set( nSpecifier, [expNewSetting], [lOpenMode] ) --> CurrentSetting
X_FUNC( SET )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "SET" );

   x_setSet( ctx );
}

// Set the search mode for At***() functions
// SetAtLike( [nNewMode], [cWildCard] ) --> nOldMode
X_FUNC( SETATLIKE )
{
   x_errorRaiseNamed( ctx->result, 1900, "SETATLIKE" );
}

// Set bits of a number; nBitPos 1 is the lowest bit, -1 for an invalid argument
// SetBit( nLong | cHexLong, nBitPos1, [nBitPosN, ...] ) --> nNewValue
X_FUNC( SETBIT )
{
   x_numBit( ctx, 1 );
}

// Toggle * interpretation between blinking/background intensity
// SetBlink( [lToggle] ) --> lCurrentSetting
X_FUNC( SETBLINK )
{
   X_GTCHECK( ctx, "SETBLINK" );

   int blink;

   x_gtGetBlink( &blink );
   if( X_ISLOG( ctx, 1 ) )
      x_gtSetBlink( x_parl( ctx, 1 ) );

   x_retl( ctx, blink );
}

// Toggle Alt+C and Ctrl+Break as program termination keys
// SetCancel( [lToggle] ) --> lCurrentSetting
X_FUNC( SETCANCEL )
{
   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISLOG( ctx, 1 ), "SETCANCEL" );

   x_retl( ctx, x_setCancel );
   if( X_ISLOG( ctx, 1 ) )
      x_setCancel = x_parl( ctx, 1 );
}

// Show or hide the century digits of dates
// SetCentury( [lToggle | cOnOff] ) --> lPreviousSetting
X_FUNC( SETCENTURY )
{
   int on = X_ISCHAR( ctx, 1 ) && x_stricmp( x_parc( ctx, 1 ), "ON" ) == 0;
   int off = X_ISCHAR( ctx, 1 ) && x_stricmp( x_parc( ctx, 1 ), "OFF" ) == 0;

   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISLOG( ctx, 1 ) || on || off, "SETCENTURY" );

   x_dateSetCentury( ctx );
}

// Set the default color attribute for clearing the screen
// SetClearA( [xColor] ) --> cNull
X_FUNC( SETCLEARA )
{
   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISNUM( ctx, 1 ) || X_ISCHAR( ctx, 1 ), "SETCLEARA" );

   x_clearColor = x_colorArg( ctx, 1, 7 );
   x_retc( ctx, "" );
}

// Set the default character attribute for clearing the screen
// SetClearB( [xChar] ) --> cNull
X_FUNC( SETCLEARB )
{
   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISNUM( ctx, 1 ) || X_ISCHAR( ctx, 1 ), "SETCLEARB" );

   x_clearChar = x_cellChar( ctx, 1, ' ' );
   x_retc( ctx, "" );
}

// Replace a color value in a color string
// SetClrPair( cColorString, nPos, cColor ) --> cNewColorString
X_FUNC( SETCLRPAIR )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) && X_ISNUM( ctx, 2 ) && X_ISCHAR( ctx, 3 ), "SETCLRPAIR" );

   x_screenSetClrPair( ctx );
}

// Return the current colors and optionally set new colors
// SetColor( [cColorString] ) --> cColorString
X_FUNC( SETCOLOR )
{
   X_GTCHECK( ctx, "SETCOLOR" );

   unsigned char colors[X_CLRSTR_LEN];

   x_gtGetColorStr( colors );
   if( X_ISCHAR( ctx, 1 ) )
      x_gtSetColorStr( ( unsigned char * )x_parc( ctx, 1 ) );

   x_retc( ctx, ( const char * )colors );
}

// Return the current 24-bit colors and optionally set a new foreground and background pair
// SetColorExt( [nForeground], [nBackground] ) --> aPrevious
X_FUNC( SETCOLOREXT )
{
   uint32_t foreground = 0, background = 0;

   if( !x_trueColorCheck( ctx, "SETCOLOREXT" ) )
      return;

   X_ARGCHECK( ctx, x_pcount( ctx ) == 0 || x_pcount( ctx ) == 2, "SETCOLOREXT" );
   if( x_pcount( ctx ) == 2 && !x_rgb( ctx, 1, "SETCOLOREXT", &foreground ) )
      return;

   if( x_pcount( ctx ) == 2 && !x_rgb( ctx, 2, "SETCOLOREXT", &background ) )
      return;

   x_screenSetColorExt( ctx, foreground, background );
}

// Set the cursor shape
// SetCursor( [nCursorShape] ) --> nCurrentSetting
X_FUNC( SETCURSOR )
{
   X_GTCHECK( ctx, "SETCURSOR" );

   X_ARGCHECK_OPT( ctx, 1, X_ISNUM( ctx, 1 ), "SETCURSOR" );

   unsigned short shape = 0;

   x_gtGetCursor( &shape );
   x_retni( ctx, shape );

   if( x_pcount( ctx ) >= 1 )
      x_gtSetCursor( ( unsigned short )x_parni( ctx, 1 ) );
}

// Change the system date from a Date value
// SetDate( dDate ) --> lSuccess
X_FUNC( SETDATE )
{
   x_errorRaiseNamed( ctx->result, 1900, "SETDATE" );
}

// Query or change the behavior with operating system errors
// SetErrorMode( [nNewErrorMode] ) --> nOldErrorMode
X_FUNC( SETERRORMODE )
{
   x_errorRaiseNamed( ctx->result, 1900, "SETERRORMODE" );
}

// Set file attributes
// SetFAttr( cFileName, [nAttributes] ) --> nErrorCode
X_FUNC( SETFATTR )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "SETFATTR" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISNUM( ctx, 2 ), "SETFATTR" );

   x_fsSetFAttrCall( ctx );
}

// Set the last change date and time of a file
// SetFDaTi( cFileName, [dFileDate], [cFileTime] ) --> lSuccess
X_FUNC( SETFDATI )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "SETFDATI" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISDATE( ctx, 2 ) || X_ISCHAR( ctx, 2 ), "SETFDATI" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISCHAR( ctx, 3 ), "SETFDATI" );

   x_fsSetFDaTiCall( ctx );
}

// Assign an action block to a key
// SetKey( nInkeyCode, [bAction], [bCondition] ) --> bCurrentAction
X_FUNC( SETKEY )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "SETKEY" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISBLOCK( ctx, 2 ), "SETKEY" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISBLOCK( ctx, 3 ), "SETKEY" );

   x_screenSetKey( ctx );
}

// Associate a code block with multiple keys
// SetKeyArray( aInkey, [bCodeblock] ) --> NIL
X_FUNC( SETKEYARRAY )
{
   x_errorRaiseNamed( ctx->result, 1900, "SETKEYARRAY" );
}

// Evaluate a code block associated with a key
// SetKeyCheck( nInkey, [param1], [param2], [param3] ) --> lIsSetkeyBlock
X_FUNC( SETKEYCHECK )
{
   x_errorRaiseNamed( ctx->result, 1900, "SETKEYCHECK" );
}

// Retrieve code blocks associated with a key
// SetKeyGet( nInkeyCode, [@bCondition] ) --> bCodeblock
X_FUNC( SETKEYGET )
{
   x_errorRaiseNamed( ctx->result, 1900, "SETKEYGET" );
}

// Query or change all SetKey() code blocks
// SetKeySave( [aNewKeyBlock] ) --> aOldKeyBlock
X_FUNC( SETKEYSAVE )
{
   x_errorRaiseNamed( ctx->result, 1900, "SETKEYSAVE" );
}

// Set a numeric value as last error code
// SetLastError( nNewErrorCode ) --> nOldErrorCode
X_FUNC( SETLASTERROR )
{
   x_errorRaiseNamed( ctx->result, 1900, "SETLASTERROR" );
}

// Change the return value of function LastKey()
// SetLastKey( nKey ) --> cNullString
X_FUNC( SETLASTKEY )
{
   x_errorRaiseNamed( ctx->result, 1900, "SETLASTKEY" );
}

// Change display mode to a specified number of rows and columns
// SetMode( nRows, nCols ) --> lSuccess
X_FUNC( SETMODE )
{
   X_GTCHECK( ctx, "SETMODE" );

   x_screenSetMode( ctx );
}

// Determine the visibility of the mouse cursor
// SetMouse( [lOnOff], [nRow], [nCol] ) --> lIsMouseVisible
X_FUNC( SETMOUSE )
{
   x_errorRaiseNamed( ctx->result, 1900, "SETMOUSE" );
}

// Query or change the color for displaying failure messages of Net*() functions
// SetNetMsgColor( [cNewColor] ) --> cOldColor
X_FUNC( SETNETMSGCOLOR )
{
   x_errorRaiseNamed( ctx->result, 1900, "SETNETMSGCOLOR" );
}

// Change the system date from Numeric values
// SetNewDate( nYear, nMonth, nDay ) --> lSuccess
X_FUNC( SETNEWDATE )
{
   x_errorRaiseNamed( ctx->result, 1900, "SETNEWDATE" );
}

// Change the system time from Numeric values
// SetNewTime( nHour, nMinute, nSecond ) --> lSuccess
X_FUNC( SETNEWTIME )
{
   x_errorRaiseNamed( ctx->result, 1900, "SETNEWTIME" );
}

// Move the cursor to a new position
// SetPos( nRow, nCol ) --> nRow
X_FUNC( SETPOS )
{
   X_GTCHECK( ctx, "SETPOS" );

   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "SETPOS" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 2 ), "SETPOS" );

   int row = x_parni( ctx, 1 );

   x_gtSetPos( ( unsigned short )row, ( unsigned short )x_parni( ctx, 2 ) );
   x_retni( ctx, row );
}

// Move the screen cursor (text mode) one column to the right
// SetPosBS( ) --> NIL
X_FUNC( SETPOSBS )
{
   x_errorRaiseNamed( ctx->result, 1900, "SETPOSBS" );
}

// Set PROW( ) and PCOL( ) values
// SetPRC( nRow, nCol ) --> NIL
X_FUNC( SETPRC )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "SETPRC" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 2 ), "SETPRC" );

   x_prnRow = x_parni( ctx, 1 );
   x_prnCol = x_parni( ctx, 2 );
}

// Specify the computing precision for trigonometric functions
// SetPrec( nDecimalPlaces ) --> cNullString
// To be checked whether SetPrec() stays; the precision could be handled by the compiler.
X_FUNC( SETPREC )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "SETPREC" );

   if( x_parnint( ctx, 1 ) >= 1 && x_parnint( ctx, 1 ) <= 16 )
      x_mathSetPrecision( ( int )x_parnint( ctx, 1 ) );

   x_retc( ctx, "" );
}

// (Windows only) Create a key/value pair in the registry
// SetRegistry( nHKEY, cRegPath, cRegKey, xValue ) --> lSuccess
X_FUNC( SETREGISTRY )
{
   x_errorRaiseNamed( ctx->result, 1900, "SETREGISTRY" );
}

// Change the system time from a Time string
// SetTime( cTime ) --> lSuccess
X_FUNC( SETTIME )
{
   x_errorRaiseNamed( ctx->result, 1900, "SETTIME" );
}

// Change the with object during WITH OBJECT execution
// SetWith( [oNewObject] ) --> oOldObject
X_FUNC( SETWITH )
{
   x_errorRaiseNamed( ctx->result, 1900, "SETWITH" );
}

// Display a shadow around a rectangular area on the screen
// Shadow( nTop, nLeft, nBottom, nRight, [nColorAttr] ) --> NIL
X_FUNC( SHADOW )
{
   X_GTCHECK( ctx, "SHADOW" );

   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ) && fabs( x_parnd( ctx, 1 ) ) <= USHRT_MAX, "SHADOW" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 2 ) && fabs( x_parnd( ctx, 2 ) ) <= USHRT_MAX, "SHADOW" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 3 ) && fabs( x_parnd( ctx, 3 ) ) <= USHRT_MAX, "SHADOW" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 4 ) && fabs( x_parnd( ctx, 4 ) ) <= USHRT_MAX, "SHADOW" );
   X_ARGCHECK_OPT( ctx, 5, X_ISNIL( ctx, 5 ) || ( X_ISNUM( ctx, 5 ) && x_parnd( ctx, 5 ) >= 0 ), "SHADOW" );
   X_ARGCHECK_OPT( ctx, 5, X_ISNIL( ctx, 5 ) || x_parnd( ctx, 5 ) <= 255, "SHADOW" );

   int color = x_parnidef( ctx, 5, 7 );

   x_screenShadow( x_parni( ctx, 1 ), x_parni( ctx, 2 ), x_parni( ctx, 3 ), x_parni( ctx, 4 ), color );
   x_ret( ctx );
}

// Display the system time continuously at a specified screen position
// ShowTime( [nRow], [nCol], [lHideSecs], [cColor], [l12h], [lAmPm] ) --> cNull
X_FUNC( SHOWTIME )
{
   x_errorRaiseNamed( ctx->result, 1900, "SHOWTIME" );
}

// Convert the sign of a number to a numeric value
// Sign( nNumber ) --> nSign
X_FUNC( SIGN )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "SIGN" );

   x_mathSign( ctx );
}

// Calculate the sine of an angle in radians
// Sin( nRadians ) --> nSine
X_FUNC( SIN )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "SIN" );

   x_mathRetnd( ctx, sin( x_parnd( ctx, 1 ) ), "SIN" );
}

// Calculate the hyperbolic sine
// SinH( nValue ) --> nHyperbolicSine
X_FUNC( SINH )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "SINH" );

   x_mathRetnd( ctx, sinh( x_parnd( ctx, 1 ) ), "SINH" );
}

// Calculate the amount of memory required to store a C structure
// SizeofCStructure( aTypes, [nAlign] ) --> nBytes
X_FUNC( SIZEOFCSTRUCTURE )
{
   x_errorRaiseNamed( ctx->result, 1900, "SIZEOFCSTRUCTURE" );
}

// Convert a character string to "soundex" form
// SoundEx( cString ) --> cSoundexString
X_FUNC( SOUNDEX )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "SOUNDEX" );

   x_textSoundEx( ctx );
}

// Return a string of spaces
// Space( nCount ) --> cSpaces
X_FUNC( SPACE )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "SPACE" );

   int64_t count = x_parnint( ctx, 1 );

   if( count < 0 )
      count = 0;

   char *buffer = ( char * )x_xgrab( ( size_t )count + 1 );

   memset( buffer, ' ', ( size_t )count );
   buffer[count] = '\0';

   x_retclen_buffer( ctx, buffer, ( size_t )count );
}

// Return the square root of a positive number
// Sqrt( nNumber ) --> nRoot
X_FUNC( SQRT )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "SQRT" );

   x_retnd( ctx, sqrt( x_parnd( ctx, 1 ) ) );
}

// Select the standard color of SetColor()
// Standard( ) --> cNull
X_FUNC( STANDARD )
{
   X_GTCHECK( ctx, "STANDARD" );

   x_gtColorSelect( 0 );
   x_retc( ctx, "" );
}

// Start a new thread
// StartThread( bBlock | cFuncName | pFuncPtr [, xParams, ...] ) --> pThreadHandle or
X_FUNC( STARTTHREAD )
{
   x_errorRaiseNamed( ctx->result, 1900, "STARTTHREAD" );
}

// Convert a "yyyymmdd" formatted string to a Date value
// SToD( [cDate] ) --> dDate
X_FUNC( STOD )
{
   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISCHAR( ctx, 1 ), "STOD" );

   x_dateSToD( ctx );
}

// Stop a thread from outside
// StopThread( pThreadHandle, [pMutexHandle] ) --> NIL
X_FUNC( STOPTHREAD )
{
   x_errorRaiseNamed( ctx->result, 1900, "STOPTHREAD" );
}

// Convert a "YYYYMMDDhhmmss.ccc" formatted string to a DateTime value
// SToT( [cDateTime] ) --> dDateTime
X_FUNC( STOT )
{
   x_errorRaiseNamed( ctx->result, 1900, "STOT" );
}

// Convert a numeric expression to a character string
// Str( nNumber, [nLength], [nDecimals] ) --> cNumber
X_FUNC( STR )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "STR" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNUM( ctx, 2 ), "STR" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNUM( ctx, 3 ), "STR" );

   x_mathStr( ctx );
}

// Delete characters from a string based on a mask string
// StrDel( cString, cMask ) --> cNewString
X_FUNC( STRDEL )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) && X_ISCHAR( ctx, 2 ), "STRDEL" );

   x_strDelete( ctx );
}

// Calculate the similarity of two strings
// StrDiff( cString1, cString2, [nReplace], [nDelete], [nInsert] ) --> nSimilarity
X_FUNC( STRDIFF )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) && X_ISCHAR( ctx, 2 ), "STRDIFF" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISNUM( ctx, 3 ), "STRDIFF" );
   X_ARGCHECK_OPT( ctx, 4, X_ISNIL( ctx, 4 ) || X_ISNUM( ctx, 4 ), "STRDIFF" );
   X_ARGCHECK_OPT( ctx, 5, X_ISNIL( ctx, 5 ) || X_ISNUM( ctx, 5 ), "STRDIFF" );

   x_textStrDiff( ctx );
}

// Write a string to a file starting at a specified position
// StrFile( cString, cFileName, [lUseExisting], [nOffset], [lTruncate] ) --> nBytesWritten
X_FUNC( STRFILE )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) && X_ISCHAR( ctx, 2 ), "STRFILE" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISLOG( ctx, 3 ), "STRFILE" );
   X_ARGCHECK_OPT( ctx, 4, X_ISNIL( ctx, 4 ) || ( X_ISNUM( ctx, 4 ) && x_parnint( ctx, 4 ) >= 0 ), "STRFILE" );
   X_ARGCHECK_OPT( ctx, 5, X_ISNIL( ctx, 5 ) || X_ISLOG( ctx, 5 ), "STRFILE" );

   x_fsStrFileCall( ctx );
}

// Obtain the pointer for a character string
// String2Pointer( cString ) --> pPointer
X_FUNC( STRING2POINTER )
{
   x_errorRaiseNamed( ctx->result, 1900, "STRING2POINTER" );
}

// Create a literal character string from a string
// StringToLiteral( cString ) --> cLiteralString
X_FUNC( STRINGTOLITERAL )
{
   size_t length;
   char *result;

   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "STRINGTOLITERAL" );

   result = x_valToPrgExp( x_paritem( ctx, 1 ), &length );
   x_retclen_buffer( ctx, result, length );
}

// Check whether a string is valid UTF-8
// StrIsUtf8( cString ) --> lValid
X_FUNC( STRISUTF8 )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "STRISUTF8" );

   x_retl( ctx, x_utf8Validate( x_parc( ctx, 1 ), x_parclen( ctx, 1 ) ) );
}

// Determine the ASCII code of a specified character in a string
// StrPeek( cString, nPos ) --> nASCII
X_FUNC( STRPEEK )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) && X_ISNUM( ctx, 2 ), "STRPEEK" );

   x_stringByte( ctx, 0 );
}

// Change the ASCII code of a specified character in a string
// StrPoke( cString, nPos, nASCII ) --> cString
X_FUNC( STRPOKE )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) && X_ISNUM( ctx, 2 ), "STRPOKE" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 3 ) && x_parnd( ctx, 3 ) >= 0 && x_parnd( ctx, 3 ) <= 255, "STRPOKE" );

   x_stringByte( ctx, 1 );
}

// Display a screen string at the specified position
// StrScreen( cScreen, [nRow], [nCol] ) --> cNull
X_FUNC( STRSCREEN )
{
   x_errorRaiseNamed( ctx->result, 1900, "STRSCREEN" );
}

// Exchange characters between two strings
// StrSwap( [@]cString1, [@]cString2 ) --> cNull
X_FUNC( STRSWAP )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) && X_ISCHAR( ctx, 2 ), "STRSWAP" );

   x_textStrSwap( ctx );
}

// Convert a character string to a Hex string
// StrToHex( cString, [cSeparator] ) --> cHexString
X_FUNC( STRTOHEX )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "STRTOHEX" );

   const char *separator = IIF( X_ISCHAR( ctx, 2 ), x_parc( ctx, 2 ), "" );
   size_t separatorLength = IIF( X_ISCHAR( ctx, 2 ), x_parclen( ctx, 2 ), 0 );
   size_t length = 0;

   char *result = x_hexEncode( x_parc( ctx, 1 ), x_parclen( ctx, 1 ), separator, separatorLength, &length );
   x_retclen_buffer( ctx, result, length );
}

// Search and replace characters within a string or memo field
// StrTran( cString, cSearch, [cReplace], [nStart], [nCount] ) --> cNewString
X_FUNC( STRTRAN )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "STRTRAN" );
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 2 ), "STRTRAN" );
   X_ARGCHECK_OPT( ctx, 3, X_ISCHAR( ctx, 3 ), "STRTRAN" );
   X_ARGCHECK_OPT( ctx, 4, X_ISNUM( ctx, 4 ), "STRTRAN" );
   X_ARGCHECK_OPT( ctx, 5, X_ISNUM( ctx, 5 ), "STRTRAN" );

   x_textStrTran( ctx );
}

// Convert values contained in a binary C structure string to an array
// StructureToArray( cBinary, aTypes, [nAlign], [lRecurse], aStructure ) --> aStructure
X_FUNC( STRUCTURETOARRAY )
{
   x_errorRaiseNamed( ctx->result, 1900, "STRUCTURETOARRAY" );
}

// Convert a numeric value to a character string, padded with zeros
// StrZero( nNumber, [nLength], [nDecimals] ) --> cString
X_FUNC( STRZERO )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "STRZERO" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNUM( ctx, 2 ), "STRZERO" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNUM( ctx, 3 ), "STRZERO" );

   x_mathStrZero( ctx );
}

// Delete and insert characters in a string
// Stuff( cString, nStart, nDelete, cInsert ) --> cNewString
X_FUNC( STUFF )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "STUFF" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 2 ), "STUFF" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 3 ), "STUFF" );
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 4 ), "STUFF" );

   x_textStuff( ctx );
}

// Subscribe for notifications on a Mutex
// Subscribe( pMutexHandle, nWaitMilliSecs, [@lNotified] ) --> xReturn
X_FUNC( SUBSCRIBE )
{
   x_errorRaiseNamed( ctx->result, 1900, "SUBSCRIBE" );
}

// Subscribe for notifications on a Mutex and discard pending notifications
// SubscribeNow( pMutexHandle, nWaitMilliSecs, [@lNotified] ) --> xReturn
X_FUNC( SUBSCRIBENOW )
{
   x_errorRaiseNamed( ctx->result, 1900, "SUBSCRIBENOW" );
}

// Extract a substring from a character string
// SubStr( cString, nStart, [nCount] ) --> cSubstring
X_FUNC( SUBSTR )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "SUBSTR" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 2 ), "SUBSTR" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNUM( ctx, 3 ), "SUBSTR" );

   x_textSubStr( ctx );
}

// Close the log file of the operating system
// SysLogClose( cAppName ) --> lSuccess
X_FUNC( SYSLOGCLOSE )
{
   x_errorRaiseNamed( ctx->result, 1900, "SYSLOGCLOSE" );
}

// Add a new entry to the log file of the operating system
// SysLogMessage( cMessage, nPriority, nSysID ) --> lSuccess
X_FUNC( SYSLOGMESSAGE )
{
   x_errorRaiseNamed( ctx->result, 1900, "SYSLOGMESSAGE" );
}

// Open the log file of the operating system
// SysLogOpen( cAppName ) --> lSuccess
X_FUNC( SYSLOGOPEN )
{
   x_errorRaiseNamed( ctx->result, 1900, "SYSLOGOPEN" );
}

// Replace a tab with a specified number of another character
// TabExpand( cString, [nTabWidth], [xFillChar] ) --> cResultString
X_FUNC( TABEXPAND )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "TABEXPAND" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISCHAR( ctx, 2 ) || X_ISNUM( ctx, 2 ), "TABEXPAND" );
   X_ARGCHECK_OPT( ctx, 2, !X_ISNUM( ctx, 2 ) || x_parnint( ctx, 2 ) >= 1, "TABEXPAND" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISCHAR( ctx, 3 ) || X_ISNUM( ctx, 3 ), "TABEXPAND" );

   x_textTabExpand( ctx );
}

// Insert a Tab (Chr(9)) for multiple occurrences of a character
// TabPack( cString, [nTabWidth], [xFillChar] ) --> cResult
X_FUNC( TABPACK )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "TABPACK" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISCHAR( ctx, 2 ) || X_ISNUM( ctx, 2 ), "TABPACK" );
   X_ARGCHECK_OPT( ctx, 2, !X_ISNUM( ctx, 2 ) || x_parnint( ctx, 2 ) >= 1, "TABPACK" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISCHAR( ctx, 3 ) || X_ISNUM( ctx, 3 ), "TABPACK" );

   x_textTabPack( ctx );
}

// Calculate the tangent of an angle in radians
// Tan( nRadians ) --> nTangent
X_FUNC( TAN )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "TAN" );

   x_mathRetnd( ctx, tan( x_parnd( ctx, 1 ) ), "TAN" );
}

// Calculate the hyperbolic tangent
// TanH( nValue ) --> nHyperbolicTangent
X_FUNC( TANH )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "TANH" );

   x_mathRetnd( ctx, tanh( x_parnd( ctx, 1 ) ), "TANH" );
}

// Retrieve an array or object from its pointer
// ThisArray( pID ) --> aArray | oObject
X_FUNC( THISARRAY )
{
   x_errorRaiseNamed( ctx->result, 1900, "THISARRAY" );
}

// Put a thread to sleep
// ThreadSleep( nMilliSeconds, [lNoCpu] ) --> NIL
X_FUNC( THREADSLEEP )
{
   x_errorRaiseNamed( ctx->result, 1900, "THREADSLEEP" );
}

// Raise an Error object as a runtime error: the block of ErrorBlock() receives this object
// Throw( oError ) --> NIL
X_FUNC( THROW )
{
   X_ARGCHECK( ctx, X_ISOBJECT( ctx, 1 ) && x_objIsKindOf( x_paritem( ctx, 1 ), "ERROR" ), "THROW" );

   x_errorRaiseObject( ctx->result, x_paritem( ctx, 1 ) );
}

// Release memory tables required for HTML classes
// THtmlCleanup( ) --> lSuccess
X_FUNC( THTMLCLEANUP )
{
   x_errorRaiseNamed( ctx->result, 1900, "THTMLCLEANUP" );
}

// Initialize memory tables required for HTML classes
// THtmlInit( ) --> lSuccess
X_FUNC( THTMLINIT )
{
   x_errorRaiseNamed( ctx->result, 1900, "THTMLINIT" );
}

// Validate a HTML tag name and attribute
// THtmlIsValid( cTagName, [cAttrName] ) --> lIsValid
X_FUNC( THTMLISVALID )
{
   x_errorRaiseNamed( ctx->result, 1900, "THTMLISVALID" );
}

// Return the system time
// Time( ) --> cTimeString
X_FUNC( TIME )
{
   time_t now = time( NULL );
   char text[9];

   strftime( text, sizeof( text ), "%H:%M:%S", localtime( &now ) );
   x_retclen( ctx, text, 8 );
}

// Calculate the number of seconds since midnight
// TimeToSec( [cTime] ) --> nSeconds
X_FUNC( TIMETOSEC )
{
   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISCHAR( ctx, 1 ), "TIMETOSEC" );

   if( X_ISCHAR( ctx, 1 ) )
   {
      double seconds = x_timeToSeconds( x_parc( ctx, 1 ), x_parclen( ctx, 1 ) );

      x_retnd( ctx, IIF( seconds < 0, 0, seconds ) );
   }
   else
   {
      time_t now = time( NULL );
      struct tm *local = localtime( &now );

      x_retnd( ctx, local->tm_hour * 3600 + local->tm_min * 60 + local->tm_sec );
   }
}

// Check if a character string is a valid time string
// TimeValid( cTime ) --> lIsTimeString
X_FUNC( TIMEVALID )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "TIMEVALID" );

   x_retl( ctx, x_timeToSeconds( x_parc( ctx, 1 ), x_parclen( ctx, 1 ) ) >= 0 );
}

// Retrieve the n-th token from a string
// Token( cString, [cDelimiter], [nTokenPos], [nSkipWidth], [@cPreTokenSep], [@cPostTokenSep] ) --> cToken
X_FUNC( TOKEN )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "TOKEN" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISCHAR( ctx, 2 ) || X_ISNUM( ctx, 2 ), "TOKEN" );

   /* A number as the second argument is already nTokenPos: Token( cString, 1 ) */
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISNUM( ctx, 3 ), "TOKEN" );
   X_ARGCHECK_OPT( ctx, 4, X_ISNUM( ctx, 2 ) || X_ISNIL( ctx, 4 ) || X_ISNUM( ctx, 4 ), "TOKEN" );

   x_tokenToken( ctx );
}

// Return the start and end position of a token
// TokenAt( [lNextDelimiter], [nCount], [@cTokenEnv] ) --> nPosition
X_FUNC( TOKENAT )
{
   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISLOG( ctx, 1 ), "TOKENAT" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISNUM( ctx, 2 ), "TOKENAT" );

   x_tokenTokenAt( ctx );
}

// Test if tokens can still be found with TokenNext()
// TokenEnd( [@cTokenEnv] ) --> lTokenEnd
X_FUNC( TOKENEND )
{
   x_tokenTokenEnd( ctx );
}

// Release memory resources of the global tokenizer environment
// TokenExit( ) --> lSuccess
X_FUNC( TOKENEXIT )
{
   x_tokenEnvSetGlobal( NULL, 0 );
   x_retl( ctx, 1 );
}

// Initialize the environment for the incremental tokenizer
// TokenInit( [@cString], [cDelimiter], [nSkipWidth], [@cTokenEnv] ) --> lSuccess
X_FUNC( TOKENINIT )
{
   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISCHAR( ctx, 1 ), "TOKENINIT" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISCHAR( ctx, 2 ), "TOKENINIT" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISNUM( ctx, 3 ), "TOKENINIT" );

   x_tokenTokenInit( ctx );
}

// Change the first character of tokens to lower case
// TokenLower( cString, [cDelimiter], [nTokenPos], [nSkipWidth] ) --> cResult
X_FUNC( TOKENLOWER )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "TOKENLOWER" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISCHAR( ctx, 2 ) || X_ISNUM( ctx, 2 ), "TOKENLOWER" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISNUM( ctx, 3 ), "TOKENLOWER" );
   X_ARGCHECK_OPT( ctx, 4, X_ISNIL( ctx, 4 ) || X_ISNUM( ctx, 4 ), "TOKENLOWER" );

   x_tokenTokenLower( ctx );
}

// Retrieve the next token from a string
// TokenNext( [@]cString, [nTokenPos], [@cTokenEnv] ) --> cToken
X_FUNC( TOKENNEXT )
{
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISNUM( ctx, 2 ), "TOKENNEXT" );

   x_tokenTokenNext( ctx );
}

// Return the number of tokens in a tokenizer environment
// TokenNum( [@cTokenEnv] ) --> nTokenCount
X_FUNC( TOKENNUM )
{
   size_t length;
   int owned;
   char *env = x_tokenEnvLoad( ctx, 1, &length, &owned );

   x_retnint( ctx, IIF( env != NULL, ( int64_t )x_tokenEnvCount( env ), 0 ) );
   x_tokenEnvRelease( env, owned );
}

// Retrieve the separating characters of a token
// TokenSep( [lRightSeparator] ) --> cSeparator
X_FUNC( TOKENSEP )
{
   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISLOG( ctx, 1 ), "TOKENSEP" );

   const char *separator;
   size_t length = x_tokenSep( x_parl( ctx, 1 ), &separator );

   x_retclen( ctx, separator, length );
}

// Change the first character of tokens to upper case
// TokenUpper( cString, [cDelimiter], [nTokenPos], [nSkipWidth] ) --> cResult
X_FUNC( TOKENUPPER )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "TOKENUPPER" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISCHAR( ctx, 2 ) || X_ISNUM( ctx, 2 ), "TOKENUPPER" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISNUM( ctx, 3 ), "TOKENUPPER" );
   X_ARGCHECK_OPT( ctx, 4, X_ISNIL( ctx, 4 ) || X_ISNUM( ctx, 4 ), "TOKENUPPER" );

   x_tokenTokenUpper( ctx );
}

// Sound a speaker tone for a specified frequency and duration
// Tone( nFrequency, [nDuration] ) --> NIL
X_FUNC( TONE )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "TONE" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISNUM( ctx, 2 ), "TONE" );

   /* Both values are truncated; the duration is in 1/18 second, 1 by default */
   x_sysTone( trunc( x_parnd( ctx, 1 ) ), IIF( X_ISNUM( ctx, 2 ), trunc( x_parnd( ctx, 2 ) ), 1.0 ) );
   x_ret( ctx );
}

// Trace and log the contents of one or more variables
// TraceLog( xVariables, ... ) --> lTrue
X_FUNC( TRACELOG )
{
   x_errorRaiseNamed( ctx->result, 1900, "TRACELOG" );
}

// Convert any value into a formatted character string
// Transform( exp, cSayPicture ) --> cFormatString
X_FUNC( TRANSFORM )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) || X_ISNUM( ctx, 1 ) || X_ISDATE( ctx, 1 ) || X_ISLOG( ctx, 1 ), "TRANSFORM" );
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 2 ), "TRANSFORM" );

   x_transform( ctx->result, x_paritem( ctx, 1 ), x_parc( ctx, 2 ), x_parclen( ctx, 2 ) );
}

// Remove trailing spaces from a character string
// Trim( cString ) --> cTrimString
X_FUNC( TRIM )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "TRIM" );

   const char *text = x_parc( ctx, 1 );
   size_t length = x_strRTrimLen( text, x_parclen( ctx, 1 ), 0 );

   x_retclen( ctx, text, length );
}

// Complete a relative path to include the root directory
// TrueName( cPath ) --> cRoot
X_FUNC( TRUENAME )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "TRUENAME" );

   x_fsTrueNameCall( ctx );
}

// Convert numeric seconds into a time formatted character string
// TString( nSeconds ) --> cTimeString
X_FUNC( TSTRING )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "TSTRING" );

   double seconds = fmod( x_parnd( ctx, 1 ), 86400.0 );

   x_timeReturn( ctx, IIF( seconds < 0, seconds + 86400.0, seconds ), 0 );
}

// Convert a time in milliseconds since the Unix epoch, as FileTimeGet() gives it, to the local
// "date hh:mm:ss"; the date in the SET DATE format. xDev has no DateTime type, so the time is a number.
// TToC( nTimeMs ) --> cDateTimeString
X_FUNC( TTOC )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "TTOC" );

   char buffer[32];

   x_retc( ctx, IIF( x_fsTimeText( x_parnint( ctx, 1 ), buffer, sizeof( buffer ) ), buffer, "" ) );
}

// Convert a Date value to a character string in YYYYMMDDhhmmss.ccc format
// TToS( dDateTime ) --> cYYYYMMDDhhmmss.ccc
X_FUNC( TTOS )
{
   x_errorRaiseNamed( ctx->result, 1900, "TTOS" );
}

// Display the contents of a file, target of the TYPE command
// TypeFile( cFile, [lPrint] ) --> NIL
X_FUNC( TYPEFILE )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "TYPEFILE" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISLOG( ctx, 2 ), "TYPEFILE" );

   x_fsTypeCommandCall( ctx );
}

// Convert a numeric value to an unsigned long binary integer (4 bytes)
// U2bin( nNumber ) --> cInteger
X_FUNC( U2BIN )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "U2BIN" );

   x_binRetUInt32( ctx, ( uint32_t )x_parnint( ctx, 1 ) );
}

// Search a UTF-8 string for a substring; the positions are counted in characters
// UAt( cSearch, cTarget, [nStart], [nEnd] ) --> nPosition
X_FUNC( UAT )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "UAT" );
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 2 ), "UAT" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNUM( ctx, 3 ), "UAT" );
   X_ARGCHECK_OPT( ctx, 4, X_ISNUM( ctx, 4 ), "UAT" );

   const char *search = x_parc( ctx, 1 );
   size_t searchLength = x_parclen( ctx, 1 );
   const char *text = x_parc( ctx, 2 );
   size_t length = x_parclen( ctx, 2 );
   int64_t start = x_parnint( ctx, 3 );
   int64_t end = x_parnint( ctx, 4 );
   int hasStart = x_pcount( ctx ) >= 3;
   int hasEnd = x_pcount( ctx ) >= 4;

   x_retnint( ctx, x_utf8At( search, searchLength, text, length, start, hasStart, end, hasEnd ) );
}

// Split a UTF-8 string into an array of code points, one string each; a combining mark is an element
// of its own, and so is every invalid byte, kept unchanged like in USubStr()
// UChars( cString ) --> aChars
X_FUNC( UCHARS )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "UCHARS" );

   x_textUChars( ctx );
}

// Extract characters from the beginning of a UTF-8 string
// ULeft( cString, nCount ) --> cSubString
X_FUNC( ULEFT )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "ULEFT" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 2 ), "ULEFT" );

   x_utf8Left( ctx, x_parc( ctx, 1 ), x_parclen( ctx, 1 ), x_parnint( ctx, 2 ) );
}

// Return the number of characters of a UTF-8 string
// ULen( cString ) --> nCount
X_FUNC( ULEN )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "ULEN" );

   x_retnint( ctx, ( int64_t )x_utf8Length( x_parc( ctx, 1 ), x_parclen( ctx, 1 ) ) );
}

// Uncompress a compressed character string (ZIP)
// Uncompress( nBytes, cCompressed ) --> cUncompressed
X_FUNC( UNCOMPRESS )
{
   x_errorRaiseNamed( ctx->result, 1900, "UNCOMPRESS" );
}

// Select the unselected color of SetColor()
// Unselected( ) --> cNull
X_FUNC( UNSELECTED )
{
   X_GTCHECK( ctx, "UNSELECTED" );

   x_gtColorSelect( 4 );
   x_retc( ctx, "" );
}

// Remove all text characters from the screen
// UntextWin( nTop, nLeft, nBottom, nRight, xReplace, [xExceptFrom], [cExceptTo] ) --> cNull
X_FUNC( UNTEXTWIN )
{
   x_errorRaiseNamed( ctx->result, 1900, "UNTEXTWIN" );
}

// Pad a UTF-8 string on both sides with a fill character; the length is counted in characters
// UPadC( cString, nLength, [cFillChar] ) --> cPaddedString
X_FUNC( UPADC )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "UPADC" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 2 ), "UPADC" );
   X_ARGCHECK_OPT( ctx, 3, X_ISCHAR( ctx, 3 ), "UPADC" );

   int64_t width = x_parnint( ctx, 2 );

   x_utf8Pad( ctx, x_parc( ctx, 1 ), x_parclen( ctx, 1 ), width, x_parc( ctx, 3 ), x_parclen( ctx, 3 ), 1, 1, "UPADC" );
}

// Pad a UTF-8 string on the left with a fill character; the length is counted in characters
// UPadL( cString, nLength, [cFillChar] ) --> cPaddedString
X_FUNC( UPADL )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "UPADL" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 2 ), "UPADL" );
   X_ARGCHECK_OPT( ctx, 3, X_ISCHAR( ctx, 3 ), "UPADL" );

   int64_t width = x_parnint( ctx, 2 );

   x_utf8Pad( ctx, x_parc( ctx, 1 ), x_parclen( ctx, 1 ), width, x_parc( ctx, 3 ), x_parclen( ctx, 3 ), 1, 0, "UPADL" );
}

// Pad a UTF-8 string on the right with a fill character; the length is counted in characters
// UPadR( cString, nLength, [cFillChar] ) --> cPaddedString
X_FUNC( UPADR )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "UPADR" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 2 ), "UPADR" );
   X_ARGCHECK_OPT( ctx, 3, X_ISCHAR( ctx, 3 ), "UPADR" );

   int64_t width = x_parnint( ctx, 2 );

   x_utf8Pad( ctx, x_parc( ctx, 1 ), x_parclen( ctx, 1 ), width, x_parc( ctx, 3 ), x_parclen( ctx, 3 ), 0, 1, "UPADR" );
}

// Return the code point of the UTF-8 character at a position; 0 outside the string
// UPeek( cString, nPos ) --> nCodePoint
X_FUNC( UPEEK )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) && X_ISNUM( ctx, 2 ), "UPEEK" );

   x_utf8Peek( ctx, x_parc( ctx, 1 ), x_parclen( ctx, 1 ), x_parnd( ctx, 2 ) );
}

// Replace the UTF-8 character at a position with the character of a code point
// UPoke( cString, nPos, nCodePoint ) --> cResult
X_FUNC( UPOKE )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) && X_ISNUM( ctx, 2 ) && X_ISNUM( ctx, 3 ), "UPOKE" );

   x_utf8Poke( ctx, x_parc( ctx, 1 ), x_parclen( ctx, 1 ), x_parnd( ctx, 2 ), x_parnd( ctx, 3 ) );
}

// Convert lowercase characters to uppercase
// Upper( cString ) --> cUpperString
X_FUNC( UPPER )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "UPPER" );

   const char *text = x_parc( ctx, 1 );
   size_t length = x_parclen( ctx, 1 );
   char *result = ( char * )x_xgrab( length + 1 );

   length = x_utf8Case( result, text, length, 1 );
   result[length] = '\0';

   x_retclen_buffer( ctx, result, length );
}

// Return the position of the last occurrence of a UTF-8 string; the positions are counted in characters
// URAt( cSearch, cTarget, [nStart], [nEnd] ) --> nPosition
X_FUNC( URAT )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) && X_ISCHAR( ctx, 2 ), "URAT" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNUM( ctx, 3 ), "URAT" );
   X_ARGCHECK_OPT( ctx, 4, X_ISNUM( ctx, 4 ), "URAT" );

   const char *search = x_parc( ctx, 1 );
   size_t searchLength = x_parclen( ctx, 1 );
   const char *text = x_parc( ctx, 2 );
   size_t length = x_parclen( ctx, 2 );
   int64_t start = x_parnint( ctx, 3 );
   int64_t end = x_parnint( ctx, 4 );
   int hasStart = x_pcount( ctx ) >= 3;
   int hasEnd = x_pcount( ctx ) >= 4;

   x_utf8RAt( ctx, search, searchLength, text, length, start, hasStart, end, hasEnd );
}

// Extract characters from the end of a UTF-8 string
// URight( cString, nCount ) --> cSubString
X_FUNC( URIGHT )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "URIGHT" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 2 ), "URIGHT" );

   x_utf8Right( ctx, x_parc( ctx, 1 ), x_parclen( ctx, 1 ), x_parnint( ctx, 2 ) );
}

// Return the name of the user who runs the program; "" when the system does not give it
// UserName( ) --> cUserName
X_FUNC( USERNAME )
{
   x_retc_buffer( ctx, x_sysUserName() );
}

// Delete and insert characters in a UTF-8 string; the positions are counted in characters
// UStuff( cString, nStart, nDelete, cInsert ) --> cNewString
X_FUNC( USTUFF )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "USTUFF" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 2 ), "USTUFF" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 3 ), "USTUFF" );
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 4 ), "USTUFF" );

   int64_t start = x_parnint( ctx, 2 );
   int64_t count = x_parnint( ctx, 3 );

   x_utf8Stuff( ctx, x_parc( ctx, 1 ), x_parclen( ctx, 1 ), start, count, x_parc( ctx, 4 ), x_parclen( ctx, 4 ) );
}

// Extract a substring from a UTF-8 string; the positions are counted in characters
// USubStr( cString, nStart, [nCount] ) --> cSubstring
X_FUNC( USUBSTR )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "USUBSTR" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 2 ), "USUBSTR" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNUM( ctx, 3 ), "USUBSTR" );

   int hasCount = x_pcount( ctx ) >= 3;

   x_utf8SubStr( ctx, x_parc( ctx, 1 ), x_parclen( ctx, 1 ), x_parnint( ctx, 2 ), hasCount, x_parnint( ctx, 3 ) );
}

// Return the code point of the first UTF-8 character of a string
// UTF8Asc( cString ) --> nCodePoint
X_FUNC( UTF8ASC )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "UTF8ASC" );

   x_utf8Asc( ctx, x_parc( ctx, 1 ), x_parclen( ctx, 1 ) );
}

// Convert a code point to a UTF-8 character
// UTF8Chr( nCodePoint ) --> cChar
X_FUNC( UTF8CHR )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "UTF8CHR" );

   x_utf8Chr( ctx, x_parnd( ctx, 1 ) );
}

// Decode a UUEncoded character string
// UUDecode( cUUEncode ) --> cString
X_FUNC( UUDECODE )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "UUDECODE" );

   x_uuResult( ctx, 1 );
}

// Decode a UUEncoded file
// UUDecodeFile( cUUEFile, cTargetFile ) --> NIL
X_FUNC( UUDECODEFILE )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) && X_ISCHAR( ctx, 2 ), "UUDECODEFILE" );

   x_fsUuCall( ctx, 1 );
}

// UUEncode a character string
// UUEncode( cString, [nBytes] ) --> cUUEncoded
X_FUNC( UUENCODE )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "UUENCODE" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || ( X_ISNUM( ctx, 2 ) && x_parnd( ctx, 2 ) >= 0 ), "UUENCODE" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || x_parnd( ctx, 2 ) <= ( double )x_parclen( ctx, 1 ), "UUENCODE" );

   x_uuResult( ctx, 0 );
}

// UUEncode a file
// UUEncodeFile( cFilename, cUUEFile ) --> NIL
X_FUNC( UUENCODEFILE )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) && X_ISCHAR( ctx, 2 ), "UUENCODEFILE" );

   x_fsUuCall( ctx, 0 );
}

// Convert a character number to numeric type
// Val( cNumber ) --> nNumber
X_FUNC( VAL )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "VAL" );

   x_mathVal( ctx );
}

// Return the numeric value of a digit at a specified position in a string
// ValPos( cString, [nPos] ) --> nValue
X_FUNC( VALPOS )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ), "VALPOS" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISNUM( ctx, 2 ), "VALPOS" );

   const char *text = x_parc( ctx, 1 );
   size_t length = x_parclen( ctx, 1 );
   int64_t position = x_parnintdef( ctx, 2, ( int64_t )length );

   /* The digit at nPos, the last character without it; 0 for any other character */
   if( position >= 1 && ( size_t )position <= length && isdigit( ( unsigned char )text[position - 1] ) )
      x_retni( ctx, text[position - 1] - '0' );
   else
      x_retni( ctx, 0 );
}

// Convert a value to PRG code
// ValToPrg( xValue ) --> cPRGcode
X_FUNC( VALTOPRG )
{
   x_errorRaiseNamed( ctx->result, 1900, "VALTOPRG" );
}

// Convert a value to a character string holding a macro-expression
// ValToPrgExp( xValue ) --> cMacroExpression
X_FUNC( VALTOPRGEXP )
{
   X_ARGCHECK( ctx, x_pcount( ctx ) >= 1, "VALTOPRGEXP" );

   size_t length;
   char *text = x_valToPrgExp( x_paritem( ctx, 1 ), &length );

   x_retclen_buffer( ctx, text, length );
}

// Convert values of simple data types to character string
// ValToStr( xValue ) --> cString
X_FUNC( VALTOSTR )
{
   x_errorRaiseNamed( ctx->result, 1900, "VALTOSTR" );
}

// Determine the data type returned by an expression
// ValType( exp ) --> cType
X_FUNC( VALTYPE )
{
   x_itemValType( ctx );
}

// Return xDev version information
// Version( [nSelector := VERSION_XDEV] ) --> xVersion
X_FUNC( VERSION )
{
   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISNUM( ctx, 1 ), "VERSION" );

   x_sysVersion( ctx );
}

// Convert a numeric value to an unsigned short binary integer (2 bytes)
// W2bin( nNumber ) --> cInteger
X_FUNC( W2BIN )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ) && x_parnd( ctx, 1 ) >= 0 && x_parnd( ctx, 1 ) <= 65535, "W2BIN" );

   x_binRetUInt16( ctx, ( uint16_t )x_parnint( ctx, 1 ) );
}

// Close all windows
// WAClose( ) --> nErrorCode
X_FUNC( WACLOSE )
{
   X_GTCHECK( ctx, "WACLOSE" );

   x_retni( ctx, x_ctCloseAll() );
}

// Wait for a keypress, target of the WAIT command
// Wait( [xPrompt] ) --> cKey
X_FUNC( WAIT )
{
   X_GTCHECK( ctx, "WAIT" );

   x_screenWait( ctx );
}

// Suspend the current thread until all other threads have terminated
// WaitForThreads( ) --> NIL
X_FUNC( WAITFORTHREADS )
{
   x_errorRaiseNamed( ctx->result, 1900, "WAITFORTHREADS" );
}

// Define a wait period and allow for time controlled loops
// WaitPeriod( [nTime] ) --> lPeriodIsActive
X_FUNC( WAITPERIOD )
{
   x_errorRaiseNamed( ctx->result, 1900, "WAITPERIOD" );
}

// Determine the area that can be used for displaying windows
// WBoard( [nTop], [nLeft], [nBottom], [nRight] ) --> nErrorCode
X_FUNC( WBOARD )
{
   X_GTCHECK( ctx, "WBOARD" );

   if( x_pcount( ctx ) == 0 )
   {
      x_retni( ctx, x_ctBoard( 0, 0, 0, 0, 0 ) );
      return;
   }

   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ) && fabs( x_parnd( ctx, 1 ) ) <= USHRT_MAX, "WBOARD" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 2 ) && fabs( x_parnd( ctx, 2 ) ) <= USHRT_MAX, "WBOARD" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 3 ) && fabs( x_parnd( ctx, 3 ) ) <= USHRT_MAX, "WBOARD" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 4 ) && fabs( x_parnd( ctx, 4 ) ) <= USHRT_MAX, "WBOARD" );

   x_retni( ctx, x_ctBoard( 1, x_parni( ctx, 1 ), x_parni( ctx, 2 ), x_parni( ctx, 3 ), x_parni( ctx, 4 ) ) );
}

// Draw a frame around the current window
// WBox( [cBoxChars | nBoxType] ) --> nWindowID
X_FUNC( WBOX )
{
   X_GTCHECK( ctx, "WBOX" );

   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISNUM( ctx, 1 ) || X_ISCHAR( ctx, 1 ), "WBOX" );

   if( X_ISCHAR( ctx, 1 ) && x_parclen( ctx, 1 ) > 0 )
      x_retni( ctx, x_ctBox( x_parc( ctx, 1 ) ) );
   else
      x_retni( ctx, x_ctBoxType( x_parni( ctx, 1 ) ) );
}

// Center a window on the screen or make it entirely visible
// WCenter( [lCenter] ) --> nWindowID
X_FUNC( WCENTER )
{
   X_GTCHECK( ctx, "WCENTER" );

   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISLOG( ctx, 1 ), "WCENTER" );

   x_retni( ctx, x_ctCenter( x_parl( ctx, 1 ) ) );
}

// Close the current window and select the open window with the highest ID
// WClose( ) --> nNextWindowID
X_FUNC( WCLOSE )
{
   X_GTCHECK( ctx, "WCLOSE" );

   x_retni( ctx, x_ctClose() );
}

// Return the left column position of the selected window
// WCol( [lCentered] ) --> nLeftColumn
X_FUNC( WCOL )
{
   int top, left, bottom, right;

   X_GTCHECK( ctx, "WCOL" );

   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISLOG( ctx, 1 ), "WCOL" );

   x_ctCoords( x_parl( ctx, 1 ), &top, &left, &bottom, &right );
   x_retni( ctx, left );
}

// Calculate the numeric calendar week from a date
// Week( [dDate] ) --> nWeekOfYear
X_FUNC( WEEK )
{
   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISDATE( ctx, 1 ), "WEEK" );

   x_dateWeek( ctx );
}

// Return the left column position of the usable area in a formatted window
// WFCol( [lRelative] ) --> nLeftColumn
X_FUNC( WFCOL )
{
   int top, left, bottom, right;

   X_GTCHECK( ctx, "WFCOL" );

   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISLOG( ctx, 1 ), "WFCOL" );

   x_ctFormatCoords( x_parl( ctx, 1 ), &top, &left, &bottom, &right );
   x_retni( ctx, left );
}

// Return the right column position of the usable area in a formatted window
// WFLastCol( [lRelative] ) --> nRightColumn
X_FUNC( WFLASTCOL )
{
   int top, left, bottom, right;

   X_GTCHECK( ctx, "WFLASTCOL" );

   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISLOG( ctx, 1 ), "WFLASTCOL" );

   x_ctFormatCoords( x_parl( ctx, 1 ), &top, &left, &bottom, &right );
   x_retni( ctx, right );
}

// Return the bottom row position of the usable area in a formatted window
// WFLastRow( [lRelative] ) --> nBottomRow
X_FUNC( WFLASTROW )
{
   int top, left, bottom, right;

   X_GTCHECK( ctx, "WFLASTROW" );

   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISLOG( ctx, 1 ), "WFLASTROW" );

   x_ctFormatCoords( x_parl( ctx, 1 ), &top, &left, &bottom, &right );
   x_retni( ctx, bottom );
}

// Define the usable display area inside the current window; without parameters remove all margins
// WFormat( [nTop], [nLeft], [nBottom], [nRight] ) --> nWindowID
X_FUNC( WFORMAT )
{
   int top, left, bottom, right;

   X_GTCHECK( ctx, "WFORMAT" );

   if( x_pcount( ctx ) == 0 )
   {
      x_ctFormatCoords( 1, &top, &left, &bottom, &right );
      x_retni( ctx, x_ctFormat( -top, -left, -bottom, -right ) );
      return;
   }

   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ) && fabs( x_parnd( ctx, 1 ) ) <= USHRT_MAX, "WFORMAT" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 2 ) && fabs( x_parnd( ctx, 2 ) ) <= USHRT_MAX, "WFORMAT" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 3 ) && fabs( x_parnd( ctx, 3 ) ) <= USHRT_MAX, "WFORMAT" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 4 ) && fabs( x_parnd( ctx, 4 ) ) <= USHRT_MAX, "WFORMAT" );

   x_retni( ctx, x_ctFormat( x_parni( ctx, 1 ), x_parni( ctx, 2 ), x_parni( ctx, 3 ), x_parni( ctx, 4 ) ) );
}

// Return the top row position of the usable area in a formatted window
// WFRow( [lRelative] ) --> nTopRow
X_FUNC( WFROW )
{
   int top, left, bottom, right;

   X_GTCHECK( ctx, "WFROW" );

   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISLOG( ctx, 1 ), "WFROW" );

   x_ctFormatCoords( x_parl( ctx, 1 ), &top, &left, &bottom, &right );
   x_retni( ctx, top );
}

// Convert a character string including wild card characters to a regular expression
// Wild2RegEx( cWildCard [, lCaseSensitive] ) --> cRegEx
X_FUNC( WILD2REGEX )
{
   x_errorRaiseNamed( ctx->result, 1900, "WILD2REGEX" );
}

// Test if a string begins with a search pattern
// WildMatch( cPattern, cString, [lFullMatch] ) --> lFound
X_FUNC( WILDMATCH )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) && X_ISCHAR( ctx, 2 ), "WILDMATCH" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISLOG( ctx, 3 ), "WILDMATCH" );

   int full = x_parl( ctx, 3 );

   x_retl( ctx, x_wildMatch( x_parc( ctx, 1 ), x_parclen( ctx, 1 ), x_parc( ctx, 2 ), x_parclen( ctx, 2 ), full ) );
}

// Return all coordinates of the current window
// WInfo( ) --> aWindowInfo
X_FUNC( WINFO )
{
   x_errorRaiseNamed( ctx->result, 1900, "WINFO" );
}

// Determine the nesting level of WITH OBJECT statements
// WithObjectCounter( ) --> nNestingLevel
X_FUNC( WITHOBJECTCOUNTER )
{
   x_errorRaiseNamed( ctx->result, 1900, "WITHOBJECTCOUNTER" );
}

// Return the right column position of the selected window
// WLastCol( [lCentered] ) --> nRightColumn
X_FUNC( WLASTCOL )
{
   int top, left, bottom, right;

   X_GTCHECK( ctx, "WLASTCOL" );

   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISLOG( ctx, 1 ), "WLASTCOL" );

   x_ctCoords( x_parl( ctx, 1 ), &top, &left, &bottom, &right );
   x_retni( ctx, right );
}

// Return the bottom row position of the selected window
// WLastRow( [lCentered] ) --> nBottomRow
X_FUNC( WLASTROW )
{
   int top, left, bottom, right;

   X_GTCHECK( ctx, "WLASTROW" );

   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISLOG( ctx, 1 ), "WLASTROW" );

   x_ctCoords( x_parl( ctx, 1 ), &top, &left, &bottom, &right );
   x_retni( ctx, bottom );
}

// Determine on which side windows are allowed to be moved off the screen; NIL keeps a side
// WMode( [lTop], [lLeft], [lBottom], [lRight] ) --> Zero
X_FUNC( WMODE )
{
   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISLOG( ctx, 1 ), "WMODE" );
   X_ARGCHECK_OPT( ctx, 2, X_ISNIL( ctx, 2 ) || X_ISLOG( ctx, 2 ), "WMODE" );
   X_ARGCHECK_OPT( ctx, 3, X_ISNIL( ctx, 3 ) || X_ISLOG( ctx, 3 ), "WMODE" );
   X_ARGCHECK_OPT( ctx, 4, X_ISNIL( ctx, 4 ) || X_ISLOG( ctx, 4 ), "WMODE" );

   int top = IIF( X_ISLOG( ctx, 1 ), x_parl( ctx, 1 ), -1 );
   int left = IIF( X_ISLOG( ctx, 2 ), x_parl( ctx, 2 ), -1 );
   int bottom = IIF( X_ISLOG( ctx, 3 ), x_parl( ctx, 3 ), -1 );
   int right = IIF( X_ISLOG( ctx, 4 ), x_parl( ctx, 4 ), -1 );

   x_retni( ctx, x_ctMode( top, left, bottom, right ) );
}

// Change the upper left coordinate for the current window within WBoard() and WMode()
// WMove( nTop, nLeft ) --> nWindowID
X_FUNC( WMOVE )
{
   X_GTCHECK( ctx, "WMOVE" );

   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ) && fabs( x_parnd( ctx, 1 ) ) <= USHRT_MAX, "WMOVE" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 2 ) && fabs( x_parnd( ctx, 2 ) ) <= USHRT_MAX, "WMOVE" );

   x_retni( ctx, x_ctMove( x_parni( ctx, 1 ), x_parni( ctx, 2 ) ) );
}

// Move the mouse cursor to a new position in a window
// WMSetPos( nRow, nCol ) --> NIL
X_FUNC( WMSETPOS )
{
   x_errorRaiseNamed( ctx->result, 1900, "WMSETPOS" );
}

// Return the largest window ID of all windows
// WNum( ) --> nMaxWindowID
X_FUNC( WNUM )
{
   x_retni( ctx, x_ctMaxWindow() );
}

// Calculate the week number in a month
// WoM( [dDate] ) --> nWeekOfMonth
X_FUNC( WOM )
{
   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISDATE( ctx, 1 ), "WOM" );

   x_dateWoM( ctx );
}

// Create a new window and select it
// WOpen( nTop, nLeft, nBottom, nRight, [lClear] ) --> nWindowID
X_FUNC( WOPEN )
{
   X_GTCHECK( ctx, "WOPEN" );

   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ) && fabs( x_parnd( ctx, 1 ) ) <= USHRT_MAX, "WOPEN" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 2 ) && fabs( x_parnd( ctx, 2 ) ) <= USHRT_MAX, "WOPEN" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 3 ) && fabs( x_parnd( ctx, 3 ) ) <= USHRT_MAX, "WOPEN" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 4 ) && fabs( x_parnd( ctx, 4 ) ) <= USHRT_MAX, "WOPEN" );
   X_ARGCHECK_OPT( ctx, 5, X_ISNIL( ctx, 5 ) || X_ISLOG( ctx, 5 ), "WOPEN" );

   int top = x_parni( ctx, 1 );
   int left = x_parni( ctx, 2 );
   int bottom = x_parni( ctx, 3 );
   int right = x_parni( ctx, 4 );

   x_retni( ctx, x_ctOpen( top, left, bottom, right, x_parl( ctx, 5 ) ) );
}

// Convert CALL command numeric parameters from double to int
// Word( nNumber ) --> nInteger
X_FUNC( WORD )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ), "WORD" );

   x_retni( ctx, x_parni( ctx, 1 ) );
}

// Reduce a word of cWordList that repeats next to itself to one occurrence: "a a b" gives "a b"
// WordOne( cWordList, cString ) --> cResult
// To be reworked for words of text; it worked on 2-byte sequences.
X_FUNC( WORDONE )
{
   x_errorRaiseNamed( ctx->result, 1900, "WORDONE" );
}

// Remove all words of a string except the words of cWordList
// WordOnly( cWordList, cString ) --> cResult
// To be reworked for words of text; it worked on 2-byte sequences.
X_FUNC( WORDONLY )
{
   x_errorRaiseNamed( ctx->result, 1900, "WORDONLY" );
}

// Delete specified words (2-byte sequence) from a string
// WordRem( cWordList, cString ) --> cResult
X_FUNC( WORDREM )
{
   x_errorRaiseNamed( ctx->result, 1900, "WORDREM" );
}

// Replace the whole words of cWordList in a string with the matching words of cReplace
// WordRepl( cWordList, cString, cReplace ) --> cResult
// To be reworked for words of text; it worked on 2-byte sequences.
X_FUNC( WORDREPL )
{
   x_errorRaiseNamed( ctx->result, 1900, "WORDREPL" );
}

// Exchange each pair of adjacent words in a string: "a b c d" gives "b a d c"
// WordSwap( cString ) --> cResult
// To be reworked for words of text; it worked on 2-byte sequences.
X_FUNC( WORDSWAP )
{
   x_errorRaiseNamed( ctx->result, 1900, "WORDSWAP" );
}

// Replace the words of cWordList with the matching characters of cCharList: "and" with "&"
// WordToChar( cWordList, cString, cCharList ) --> cResult
// To be reworked for words of text; it worked on 2-byte sequences.
X_FUNC( WORDTOCHAR )
{
   x_errorRaiseNamed( ctx->result, 1900, "WORDTOCHAR" );
}

// Return the top row position of the selected window
// WRow( [lCentered] ) --> nTopRow
X_FUNC( WROW )
{
   int top, left, bottom, right;

   X_GTCHECK( ctx, "WROW" );

   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISLOG( ctx, 1 ), "WROW" );

   x_ctCoords( x_parl( ctx, 1 ), &top, &left, &bottom, &right );
   x_retni( ctx, top );
}

// Return and/or select an open window as the current window; -1 for a window that is not open
// WSelect( [nWindowID] ) --> nCurrentWindow
X_FUNC( WSELECT )
{
   X_GTCHECK( ctx, "WSELECT" );

   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISNUM( ctx, 1 ), "WSELECT" );
   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || fabs( x_parnd( ctx, 1 ) ) <= USHRT_MAX, "WSELECT" );

   x_retni( ctx, IIF( X_ISNUM( ctx, 1 ), x_ctSelect( x_parni( ctx, 1 ) ), x_ctCurrent() ) );
}

// Determine the visibility and/or position of the mouse cursor
// WSetMouse( [lOnOff], [nRow], [nCol] ) --> lIsMouseVisible
X_FUNC( WSETMOUSE )
{
   x_errorRaiseNamed( ctx->result, 1900, "WSETMOUSE" );
}

// Toggle the setting for moving windows interactively
// WSetMove( [lNewMoveMode] ) --> lOldMoveMode
X_FUNC( WSETMOVE )
{
   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISLOG( ctx, 1 ), "WSETMOVE" );

   x_retl( ctx, x_ctSetMove( IIF( X_ISLOG( ctx, 1 ), x_parl( ctx, 1 ), -1 ) ) );
}

// Define the shadow color for windows opened later; -1 turns the shadow off
// WSetShadow( [cColor | nColor] ) --> nOldColor
X_FUNC( WSETSHADOW )
{
   X_ARGCHECK_OPT( ctx, 1, X_ISNIL( ctx, 1 ) || X_ISCHAR( ctx, 1 ) || X_ISNUM( ctx, 1 ), "WSETSHADOW" );
   X_ARGCHECK_OPT( ctx, 1, !X_ISNUM( ctx, 1 ) || x_parnd( ctx, 1 ) >= -1, "WSETSHADOW" );
   X_ARGCHECK_OPT( ctx, 1, !X_ISNUM( ctx, 1 ) || x_parnd( ctx, 1 ) <= 255, "WSETSHADOW" );

   x_ctWSetShadow( ctx );
}

// Return window IDs of all open windows
// WStack( ) --> aWindowIDs
X_FUNC( WSTACK )
{
   x_errorRaiseNamed( ctx->result, 1900, "WSTACK" );
}

// Set the increments for vertical and horizontal interactive window movement; only without open windows
// WStep( nVertical, nHorizontal ) --> nErrorCode
X_FUNC( WSTEP )
{
   X_ARGCHECK( ctx, X_ISNUM( ctx, 1 ) && fabs( x_parnd( ctx, 1 ) ) <= USHRT_MAX, "WSTEP" );
   X_ARGCHECK( ctx, X_ISNUM( ctx, 2 ) && fabs( x_parnd( ctx, 2 ) ) <= USHRT_MAX, "WSTEP" );

   x_retni( ctx, x_ctStep( x_parni( ctx, 1 ), x_parni( ctx, 2 ) ) );
}

// Retrieve a textual error description for XML file parsing errors
// XmlErrorDesc( nErrorCode ) --> cErrorMessage
X_FUNC( XMLERRORDESC )
{
   x_errorRaiseNamed( ctx->result, 1900, "XMLERRORDESC" );
}

// Convert values of data type C, D, L, M, N to a string
// XToC( xValue ) --> cValue
X_FUNC( XTOC )
{
   X_ARGCHECK( ctx, X_ISCHAR( ctx, 1 ) || X_ISLOG( ctx, 1 ) || X_ISNUM( ctx, 1 ) || X_ISDATE( ctx, 1 ), "XTOC" );

   x_textXToC( ctx );
}

// Convert a date value to the year as a numeric value
// Year( dDate ) --> nYear
X_FUNC( YEAR )
{
   X_ARGCHECK( ctx, X_ISDATE( ctx, 1 ), "YEAR" );

   int year, month, day;

   long julian = x_pardl( ctx, 1 );

   if( julian == 0 )
   {
      x_retni( ctx, 0 );
      return;
   }

   x_dateDecode( julian, &year, &month, &day );
   x_retni( ctx, year );
}
