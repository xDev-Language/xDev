#ifndef GETEXIT_CH
#define GETEXIT_CH

/* Values of Get:exitState */
#define GE_NOEXIT   0   // no exit attempted, prepare for editing
#define GE_UP       1   // go to the previous Get
#define GE_DOWN     2   // go to the next Get
#define GE_TOP      3   // go to the first Get
#define GE_BOTTOM   4   // go to the last Get
#define GE_ENTER    5   // normal end of editing
#define GE_WRITE    6   // end READ and keep the values
#define GE_ESCAPE   7   // end READ without keeping the values
#define GE_WHEN     8   // the WHEN clause was not satisfied

#endif /* GETEXIT_CH */
