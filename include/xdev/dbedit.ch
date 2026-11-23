/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev TUI
 * File......: dbedit.ch
 *
 */

#ifndef DBEDIT_CH
#define DBEDIT_CH

/* Modes passed to the user function of DBEdit() */
#define DE_IDLE        0   // the keys are handled and no key is pending
#define DE_HITTOP      1   // attempt to move past the top of the file
#define DE_HITBOTTOM   2   // attempt to move past the bottom of the file
#define DE_EMPTY       3   // no records in the work area
#define DE_EXCEPT      4   // a key DBEdit() does not handle

/* Results of the user function of DBEdit() */
#define DE_ABORT       0   // end DBEdit()
#define DE_CONT        1   // continue DBEdit()
#define DE_REFRESH     2   // read and draw the rows again, then continue

#endif /* DBEDIT_CH */
