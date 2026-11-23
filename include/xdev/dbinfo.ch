/*
 * Copyright (c) 2024-2026 xDev-Language
 *
 * Library...: xDev RDD
 * File......: dbinfo.ch
 *
 */

#ifndef DBINFO_CH
#define DBINFO_CH

#define RDDI_ISDBF                1
#define RDDI_CANPUTREC            2
#define RDDI_DELIMITER            3
#define RDDI_SEPARATOR            4

#define RDDI_TABLEEXT             5
#define RDDI_MEMOEXT              6
#define RDDI_ORDBAGEXT            7
#define RDDI_ORDEREXT             8
#define RDDI_ORDSTRUCTEXT         9

#define RDDI_LOCAL               10
#define RDDI_REMOTE              11
#define RDDI_CONNECTION          12
#define RDDI_TABLETYPE           13
#define RDDI_MEMOTYPE            14
#define RDDI_LARGEFILE           15
#define RDDI_LOCKSCHEME          16
#define RDDI_RECORDMAP           17
#define RDDI_ENCRYPTION          18
#define RDDI_TRIGGER             19
#define RDDI_AUTOLOCK            20

#define RDDI_STRUCTORD           21
#define RDDI_STRICTREAD          22
#define RDDI_STRICTSTRUCT        23
#define RDDI_OPTIMIZE            24
#define RDDI_FORCEOPT            25
#define RDDI_AUTOOPEN            26
#define RDDI_AUTOORDER           27
#define RDDI_AUTOSHARE           28
#define RDDI_MULTITAG            29
#define RDDI_SORTRECNO           30
#define RDDI_MULTIKEY            31

#define RDDI_MEMOBLOCKSIZE       32
#define RDDI_MEMOVERSION         33
#define RDDI_MEMOGCTYPE          34
#define RDDI_MEMOREADLOCK        35
#define RDDI_MEMOREUSE           36
#define RDDI_BLOB_SUPPORT        37

#define RDDI_PENDINGTRIGGER      40
#define RDDI_PENDINGPASSWORD     41
#define RDDI_PASSWORD            42
#define RDDI_LOCKRETRY           43
#define RDDI_DIRTYREAD           44
#define RDDI_INDEXPAGESIZE       45
#define RDDI_DECIMALS            46
#define RDDI_SETHEADER           47

#define RDDI_CONNECT             61
#define RDDI_DISCONNECT          62
#define RDDI_EXECUTE             63
#define RDDI_ERROR               64
#define RDDI_ERRORNO             65
#define RDDI_INSERTID            66
#define RDDI_AFFECTEDROWS        67
#define RDDI_QUERY               68

#define DBOI_CONDITION            1
#define DBOI_EXPRESSION           2
#define DBOI_POSITION             3
#define DBOI_RECNO                4
#define DBOI_NAME                 5
#define DBOI_NUMBER               6
#define DBOI_BAGNAME              7
#define DBOI_BAGEXT               8
#define DBOI_INDEXEXT            DBOI_BAGEXT
#define DBOI_INDEXNAME           DBOI_BAGNAME
#define DBOI_ORDERCOUNT           9
#define DBOI_FILEHANDLE          10
#define DBOI_ISCOND              11
#define DBOI_ISDESC              12
#define DBOI_UNIQUE              13

#define DBOI_FULLPATH            20
#define DBOI_KEYTYPE             24
#define DBOI_KEYSIZE             25
#define DBOI_KEYCOUNT            26
#define DBOI_SETCODEBLOCK        27
#define DBOI_KEYDEC              28
#define DBOI_HPLOCKING           29
#define DBOI_LOCKOFFSET          35

#define DBOI_KEYADD              36
#define DBOI_KEYDELETE           37
#define DBOI_KEYVAL              38
#define DBOI_SCOPETOP            39
#define DBOI_SCOPEBOTTOM         40
#define DBOI_SCOPETOPCLEAR       41
#define DBOI_SCOPEBOTTOMCLEAR    42
#define DBOI_CUSTOM              45
#define DBOI_SKIPUNIQUE          46

#define DBOI_KEYSINCLUDED        50

#define DBOI_KEYGOTO             DBOI_POSITION
#define DBOI_KEYGOTORAW          DBOI_KEYNORAW
#define DBOI_KEYNO               DBOI_POSITION
#define DBOI_KEYNORAW            51
#define DBOI_KEYCOUNTRAW         52

#define DBOI_OPTLEVEL            53

#define DBOI_STRICTREAD          60
#define DBOI_OPTIMIZE            61
#define DBOI_AUTOOPEN            62
#define DBOI_AUTOORDER           63
#define DBOI_AUTOSHARE           64

#define DBOI_SKIPEVAL           100
#define DBOI_SKIPEVALBACK       101
#define DBOI_SKIPREGEX          102
#define DBOI_SKIPREGEXBACK      103
#define DBOI_SKIPWILD           104
#define DBOI_SKIPWILDBACK       105
#define DBOI_SCOPEEVAL          106
#define DBOI_FINDREC            107
#define DBOI_FINDRECCONT        108
#define DBOI_SCOPESET           109
#define DBOI_SCOPECLEAR         110

#define DBOI_BAGCOUNT           111
#define DBOI_BAGNUMBER          112
#define DBOI_BAGORDER           113

#define DBOI_ISMULTITAG         114
#define DBOI_ISSORTRECNO        115
#define DBOI_LARGEFILE          116
#define DBOI_TEMPLATE           117
#define DBOI_MULTIKEY           118
#define DBOI_CHGONLY            119
#define DBOI_PARTIAL            120
#define DBOI_SHARED             121
#define DBOI_ISREADONLY         122
#define DBOI_READLOCK           123
#define DBOI_WRITELOCK          124
#define DBOI_UPDATECOUNTER      125

#define DBOI_EVALSTEP           126
#define DBOI_ISREINDEX          127
#define DBOI_I_BAGNAME          128
#define DBOI_I_TAGNAME          129

#define DBOI_RELKEYPOS          130
#define DBOI_USECURRENT         131
#define DBOI_INDEXTYPE          132
#define DBOI_RESETPOS           133
#define DBOI_INDEXPAGESIZE      134

#define DBOI_OPTIMIZED_NONE       0
#define DBOI_OPTIMIZED_PART       1
#define DBOI_OPTIMIZED_FULL       2

#define DBOI_TYPE_UNDEF          -1
#define DBOI_TYPE_NONE            0
#define DBOI_TYPE_NONCOMPACT      1
#define DBOI_TYPE_COMPACT         2
#define DBOI_TYPE_COMPOUND        3

#define DBRMI_FUNCTION            1
#define DBRMI_PARAM               2
#define DBRMI_LOVAL               3
#define DBRMI_HIVAL               4
#define DBRMI_RESULT              5
#define DBRMI_SIZE                5

#define DBRI_DELETED              1
#define DBRI_LOCKED               2
#define DBRI_RECSIZE              3
#define DBRI_RECNO                4
#define DBRI_UPDATED              5
#define DBRI_ENCRYPTED            6
#define DBRI_RAWRECORD            7
#define DBRI_RAWMEMOS             8
#define DBRI_RAWDATA              9

#define DBI_ISDBF                 1
#define DBI_CANPUTREC             2
#define DBI_GETHEADERSIZE         3
#define DBI_LASTUPDATE            4
#define DBI_GETDELIMITER          5
#define DBI_SETDELIMITER          6
#define DBI_GETRECSIZE            7
#define DBI_GETLOCKARRAY          8
#define DBI_TABLEEXT              9
#define DBI_FULLPATH             10

#define DBI_ISFLOCK              20
#define DBI_CHILDCOUNT           22
#define DBI_FILEHANDLE           23
#define DBI_BOF                  26
#define DBI_EOF                  27
#define DBI_DBFILTER             28
#define DBI_FOUND                29
#define DBI_FCOUNT               30
#define DBI_LOCKCOUNT            31
#define DBI_VALIDBUFFER          32
#define DBI_ALIAS                33
#define DBI_GETSCOPE             34
#define DBI_LOCKOFFSET           35
#define DBI_SHARED               36
#define DBI_MEMOEXT              37
#define DBI_MEMOHANDLE           38
#define DBI_MEMOBLOCKSIZE        39

#define DBI_DB_VERSION          101
#define DBI_RDD_VERSION         102

#define DBI_LOCKSCHEME          128
#define DBI_ISREADONLY          129
#define DBI_ROLLBACK            130
#define DBI_PASSWORD            131
#define DBI_ISENCRYPTED         132
#define DBI_MEMOTYPE            133
#define DBI_SEPARATOR           134
#define DBI_MEMOVERSION         135
#define DBI_TABLETYPE           136
#define DBI_SCOPEDRELATION      137
#define DBI_TRIGGER             138
#define DBI_OPENINFO            139
#define DBI_ENCRYPT             140
#define DBI_DECRYPT             141
#define DBI_MEMOPACK            142
#define DBI_DIRTYREAD           143
#define DBI_POSITIONED          144
#define DBI_ISTEMPORARY         145
#define DBI_LOCKTEST            146
#define DBI_TRANSREC            148
#define DBI_SETHEADER           149

#define DBI_RM_SUPPORTED        150
#define DBI_RM_CREATE           151
#define DBI_RM_REMOVE           152
#define DBI_RM_CLEAR            153
#define DBI_RM_FILL             154
#define DBI_RM_ADD              155
#define DBI_RM_DROP             156
#define DBI_RM_TEST             157
#define DBI_RM_COUNT            158
#define DBI_RM_HANDLE           159

#define DBI_QUERY               170

#define DBI_BLOB_DIRECT_EXPORT  201
#define DBI_BLOB_DIRECT_GET     202
#define DBI_BLOB_DIRECT_IMPORT  203
#define DBI_BLOB_DIRECT_PUT     204
#define DBI_BLOB_ROOT_GET       205
#define DBI_BLOB_ROOT_PUT       206
#define DBI_BLOB_ROOT_LOCK      207
#define DBI_BLOB_ROOT_UNLOCK    208

#define DBI_BLOB_DIRECT_LEN     209
#define DBI_BLOB_DIRECT_TYPE    210
#define DBI_BLOB_INTEGRITY      211
#define DBI_BLOB_OFFSET         212
#define DBI_BLOB_RECOVER        213

#define DBI_USER               1000

#define DBS_ISNULL              101
#define DBS_COUNTER             102
#define DBS_STEP                103
#define DBS_BLOB_GET            201
#define DBS_BLOB_LEN            202
#define DBS_BLOB_OFFSET         203
#define DBS_BLOB_POINTER        204
#define DBS_BLOB_TYPE           205

#define BLOB_EXPORT_APPEND      1
#define BLOB_EXPORT_OVERWRITE   0

#define BLOB_IMPORT_COMPRESS    1
#define BLOB_IMPORT_ENCRYPT     2

#define FILEGET_APPEND          BLOB_EXPORT_APPEND
#define FILEGET_OVERWRITE       BLOB_EXPORT_OVERWRITE

#define FILEPUT_COMPRESS        BLOB_IMPORT_COMPRESS
#define FILEPUT_ENCRYPT         BLOB_IMPORT_ENCRYPT

#define DB_DBF_STD              1
#define DB_DBF_VFP              2

#define DB_MEMO_NONE            0
#define DB_MEMO_DBT             1
#define DB_MEMO_FPT             2
#define DB_MEMO_SMT             3

#define DB_MEMOVER_STD          1
#define DB_MEMOVER_SIX          2
#define DB_MEMOVER_FLEX         3
#define DB_MEMOVER_CLIP         4

#define DB_CRYPT_NONE           0
#define DB_CRYPT_SIX            1

#define DB_DBFLOCK_DEFAULT      0
#define DB_DBFLOCK_CLIPPER      1
#define DB_DBFLOCK_COMIX        2
#define DB_DBFLOCK_VFP          3
#define DB_DBFLOCK_HB32         4
#define DB_DBFLOCK_HB64         5
#define DB_DBFLOCK_CLIPPER2     6

#ifdef X_LEGACY_LEVEL5
   #define DB_DBFLOCK_CLIP         DB_DBFLOCK_CLIPPER
   #define DB_DBFLOCK_CL53         DB_DBFLOCK_COMIX
   #define DB_DBFLOCK_CL53EXT      DB_DBFLOCK_HB32
#endif

#define DB_SETHEADER_CLOSE    0x00
#define DB_SETHEADER_COMMIT   0x01
#define DB_SETHEADER_WRITE    0x02
#define DB_SETHEADER_APPEND   0x00
#define DB_SETHEADER_REPLACE  0x04
#define DB_SETHEADER_YYEAR    0x10
#define DB_SETHEADER_EOL      0x20
#define DB_SETHEADER_MASK     0x37

#define DB_SETHEADER_MINIMAL     DB_SETHEADER_CLOSE

#define DB_SETHEADER_COMMITSYNC  DB_SETHEADER_COMMIT

#define DB_SETHEADER_APPENDSYNC  DB_SETHEADER_WRITE

#define DB_SETHEADER_CHANGE      ( DB_SETHEADER_CLOSE + DB_SETHEADER_REPLACE )

#define DB_SETHEADER_CLIPPER     ( DB_SETHEADER_COMMIT + DB_SETHEADER_REPLACE )

#define DB_SETHEADER_FULL        ( DB_SETHEADER_WRITE + DB_SETHEADER_REPLACE )

#endif /* DBINFO_CH */
