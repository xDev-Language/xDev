#ifndef MYTIFF_CH
#define MYTIFF_CH

/* Resources and callbacks, the names of Motif with Mt for Xm; pairs in aArgs of MtCreate...(),
   MtSetValues() and MtGetValues() */

/* Core: every widget */
#define MtNx                      1
#define MtNy                      2
#define MtNwidth                  3
#define MtNheight                 4
#define MtNbackground             5
#define MtNsensitive              6
#define MtNdestroyCallback        7

/* Primitive: Label and PushButton */
#define MtNforeground            11
#define MtNshadowThickness       12
#define MtNhighlightThickness    13
#define MtNtopShadowColor        14
#define MtNbottomShadowColor     15
#define MtNhighlightColor        16
#define MtNtraversalOn           17

/* Label: Label and PushButton */
#define MtNlabelString           21
#define MtNalignment             22
#define MtNmarginWidth           23
#define MtNmarginHeight          24
#define MtNrecomputeSize         25

/* PushButton */
#define MtNarmColor              31
#define MtNfillOnArm             32
#define MtNactivateCallback      33
#define MtNarmCallback           34
#define MtNdisarmCallback        35

/* Shell; MtNdocked is of xDev, Motif has no docking */
#define MtNtitle                 41
#define MtNmwmDecorations        42
#define MtNdeleteResponse        43
#define MtNdocked                44

/* MtNalignment */
#define MtALIGNMENT_BEGINNING     1
#define MtALIGNMENT_CENTER        2
#define MtALIGNMENT_END           3

/* MtNmwmDecorations, the bits of the Motif window manager */
#define MWM_DECOR_BORDER          2
#define MWM_DECOR_RESIZEH         4
#define MWM_DECOR_TITLE           8
#define MWM_DECOR_MENU           16
#define MWM_DECOR_MAXIMIZE       64

/* MtNdeleteResponse: what the menu button of the frame does */
#define MtDESTROY                 1   // The window is destroyed
#define MtUNMAP                   2   // The window is hidden
#define MtDO_NOTHING              3

/* Reasons of a callback, element MtCD_REASON of its call data */
#define MtCR_ACTIVATE             1
#define MtCR_ARM                  2
#define MtCR_DISARM               3

/* Call data of a callback: { nReason, nKey, nClickCount }; MtNdestroyCallback gets NIL */
#define MtCD_REASON               1
#define MtCD_KEY                  2
#define MtCD_CLICKCOUNT           3

/* MtGetColors() --> { nForeground, nTopShadow, nBottomShadow, nSelect } */
#define MtCOLOR_FOREGROUND        1
#define MtCOLOR_TOPSHADOW         2
#define MtCOLOR_BOTTOMSHADOW      3
#define MtCOLOR_SELECT            4

#endif /* MYTIFF_CH */
