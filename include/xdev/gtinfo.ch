#ifndef GTINFO_CH
#define GTINFO_CH

#define GTI_ISGRAPHIC         0
#define GTI_SCREENWIDTH       1
#define GTI_SCREENHEIGHT      2
#define GTI_SCREENDEPTH       3
#define GTI_FONTSIZE          4
#define GTI_FONTWIDTH         5
#define GTI_DESKTOPWIDTH      6
#define GTI_DESKTOPHEIGHT     7
#define GTI_DESKTOPDEPTH      8
#define GTI_COMPATBUFFER      9
#define GTI_KBDSHIFTS         10
#define GTI_KBDALT            12
#define GTI_ISSCREENPOS       13
#define GTI_FULLSCREEN        GTI_ISSCREENPOS
#define GTI_KBDSUPPORT        14
#define GTI_CLIPBOARDDATA     15
#define GTI_CLIPBOARDPASTE    16
#define GTI_CURSORBLINKRATE   19
#define GTI_DESKTOPROWS       20
#define GTI_DESKTOPCOLS       21
#define GTI_FONTWEIGHT        22
#define GTI_FONTQUALITY       23
#define GTI_FONTNAME          24
#define GTI_WINTITLE          26
#define GTI_ICONFILE          27
#define GTI_ICONRES           28
#define GTI_MOUSESTATUS       29
#define GTI_INPUTFD           30
#define GTI_OUTPUTFD          31
#define GTI_ERRORFD           32
#define GTI_ESCDELAY          33
#define GTI_VIEWMAXHEIGHT     34
#define GTI_VIEWMAXWIDTH      35
#define GTI_VIEWPORTHEIGHT    36
#define GTI_VIEWPORTWIDTH     37
#define GTI_STDOUTCON         38
#define GTI_STDERRCON         39
#define GTI_ISCTWIN           40
#define GTI_ISMULTIWIN        41
#define GTI_GETWIN            42
#define GTI_SETWIN            43
#define GTI_NEWWIN            44
#define GTI_ADDKEYMAP         45
#define GTI_DELKEYMAP         46
#define GTI_ISUNICODE         47
#define GTI_SELECTCOPY        48
#define GTI_RESIZABLE         49
#define GTI_CLOSABLE          50
#define GTI_SCREENSIZE        52
#define GTI_PALETTE           53
#define GTI_RESIZEFONT        54
#define GTI_SETPOS_XY         55
#define GTI_SETPOS_ROWCOL     56
#define GTI_CARGO             58
#define GTI_FONTSEL           59
#define GTI_INKEYFILTER       60
#define GTI_INKEYREAD         61
#define GTI_ALTENTER          62
#define GTI_ISFULLSCREEN      63
#define GTI_ONLINE            64
#define GTI_VERSION           65
#define GTI_MAXIMIZED         66
#define GTI_FONTATTRIBUTE     67
#define GTI_WINHANDLE         69
#define GTI_MOUSEPOS_XY       70
#define GTI_DISPIMAGE         71
#define GTI_REDRAWMAX         72
#define GTI_RESIZESTEP        73
#define GTI_CLOSEMODE         74
#define GTI_MINIMIZED         75
#define GTI_SYSMENUADD        77
#define GTI_MSGBOX            78
#define GTI_SOUND             79
#define GTI_WINDOWWIDTH       80
#define GTI_WINDOWHEIGHT      81
#define GTI_OUTPUTWIDTH       82
#define GTI_OUTPUTHEIGHT      83
#define GTI_LOGICALWIDTH      84
#define GTI_LOGICALHEIGHT     85
#define GTI_PIXELDENSITY      86
#define GTI_DISPLAYSCALE      87
#define GTI_CONTENTSCALE      88
#define GTI_RENDERERNAME      89
#define GTI_SYSTEMTHEME       90
#define GTI_OPENURL           91
#define GTI_FILEDIALOG        92
#define GTI_WINDOWOPACITY     93
#define GTI_WINDOWALWAYSONTOP 94
#define GTI_WINDOWBORDERED    95
#define GTI_RESIZEBLOCK       96
#define GTI_EXPOSEAREA        97

/* Font weights */
#define GTI_FONTW_THIN        1
#define GTI_FONTW_NORMAL      2
#define GTI_FONTW_BOLD        3

/* Font qualities */
#define GTI_FONTQ_DRAFT       1
#define GTI_FONTQ_NORMAL      2
#define GTI_FONTQ_HIGH        3

/* Font attributes */
#define GTI_FONTA_FIXMETRIC   0x0001
#define GTI_FONTA_CLRBKG      0x0002
#define GTI_FONTA_DRAWBOX     0x0004
#define GTI_FONTA_CTRLCHARS   0x0008
#define GTI_FONTA_NOSTRETCH   0x0010

/* Keyboard shifts states */
#define GTI_KBD_SHIFT        0x000001
#define GTI_KBD_CTRL         0x000002
#define GTI_KBD_ALT          0x000004
#define GTI_KBD_KEYPAD       0x000008
#define GTI_KBD_SCROLOCK     0x000010
#define GTI_KBD_NUMLOCK      0x000020
#define GTI_KBD_CAPSLOCK     0x000040
#define GTI_KBD_INSERT       0x000080
#define GTI_KBD_LSHIFT       0x000100
#define GTI_KBD_RSHIFT       0x000200
#define GTI_KBD_LCTRL        0x000400
#define GTI_KBD_RCTRL        0x000800
#define GTI_KBD_LALT         0x001000
#define GTI_KBD_RALT         0x002000
#define GTI_KBD_LWIN         0x004000
#define GTI_KBD_RWIN         0x008000
#define GTI_KBD_MENU         0x010000

#endif /* GTINFO_CH */
