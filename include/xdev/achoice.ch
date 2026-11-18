#ifndef ACHOICE_CH
#define ACHOICE_CH

/* Modes passed to the AChoice() user block */
#define AC_IDLE       0   // no more keys to process
#define AC_HITTOP     1   // attempt to move past the top of the list
#define AC_HITBOTTOM  2   // attempt to move past the bottom of the list
#define AC_EXCEPT     3   // a key AChoice() does not handle itself
#define AC_NOITEM     4   // no selectable items

/* Values the user block returns */
#define AC_ABORT      0   // abort the selection, AChoice() returns 0
#define AC_SELECT     1   // select the current item
#define AC_CONT       2   // continue
#define AC_GOTO       3   // go to the next item starting with the last key

#endif
