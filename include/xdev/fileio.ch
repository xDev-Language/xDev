#ifndef FILEIO_CH
#define FILEIO_CH

#define FC_NORMAL      0
#define FC_READONLY    1
#define FC_HIDDEN      2
#define FC_SYSTEM      4

#define FA_READONLY    0x00000001
#define FA_HIDDEN      0x00000002
#define FA_SYSTEM      0x00000004
#define FA_DIRECTORY   0x00000010
#define FA_ARCHIVE     0x00000020
#define FA_NORMAL      0x00000080

#define FO_READ        0
#define FO_WRITE       1
#define FO_READWRITE   2
#define FO_COMPAT      0
#define FO_EXCLUSIVE   16
#define FO_DENYWRITE   32
#define FO_DENYREAD    48
#define FO_DENYNONE    64
#define FO_SHARED      FO_DENYNONE

#define FS_SET         0
#define FS_RELATIVE    1
#define FS_END         2

#define FLX_EXCLUSIVE  0x0000
#define FLX_SHARED     0x0100
#define FLX_NO_WAIT    0x0000
#define FLX_WAIT       0x0200

#define VF_TIMEOUT     1
#define VF_RDHANDLE    5
#define VF_WRHANDLE    6
#define VF_IONAME      7

#define DISK_AVAIL     0
#define DISK_FREE      1
#define DISK_USED      2
#define DISK_TOTAL     3

#define F_ERROR        ( -1 )

#endif /* FILEIO_CH */
