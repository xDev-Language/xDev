#ifndef XDEV_DBF_ORDER_H
#define XDEV_DBF_ORDER_H

#include "dbf-internal.h"

struct _X_DBF_INDEX_BAG
{
   DBF      area;
   X_ITEM   file;
   char    *path;
   size_t   references;
   uint32_t version;
   uint32_t cdxFreePage;
   int      cdx;
   int      compound;
   int      temporary;
   int      exclusive;
   size_t   readLocks;
   size_t   writeLocks;
   uint32_t ntxFreePage;
   uint32_t ntxNextPage;
   int      structural;
   int      lockMode;
   size_t   lockDepth;
   size_t   unbound;
   int      largeFile;
   int      initializing;
   int      cdxRebuild;
   void    *cdxPages;
};

enum
{
   NTX_PAGE_SIZE = 1024,
   X_DBF_KEY_SIZE = 256
};

typedef struct
{
   uint32_t      record;
   unsigned char value[X_DBF_KEY_SIZE];
} X_DBF_INDEX_KEY;

typedef struct
{
   X_DBF_INDEX_KEY key;
   uint32_t        page;
   uint32_t        version;
   unsigned int    slot;
   uint32_t        path[64];
   unsigned int    slots[64];
   unsigned int    depth;
   int             valid;
   int             keyValid;
} X_DBF_INDEX_CURSOR;

typedef struct _X_DBF_ORDER_PAGE
{
   struct _X_DBF_ORDER_PAGE **children;
   unsigned char             *keys;
   uint32_t                   number;
   uint64_t                   total;
   int                        count;
} X_DBF_ORDER_PAGE;

struct _X_DBF_ORDER
{
   X_DBF_ORDER       *next;
   DBF                area;
   X_DBF_INDEX_BAG   *bag;
   X_ITEM             block;
   X_ITEM             forBlock;
   X_ITEM             scopes[2];
   X_DBF_INDEX_KEY    scopeKeys[2];
   size_t             scopeLengths[2];
   X_DBF_ORDER_PAGE **pages;
   X_DBF_ORDER_PAGE  *root;
   size_t             pageCount;
   size_t             pageCapacity;
   size_t             stride;
   uint32_t           freePage;
   uint16_t           length;
   uint16_t           decimals;
   uint16_t           maximum;
   char               type;
   int                unique;
   int                userUnique;
   int                custom;
   int                partial;
   int template;
   int                multiKey;
   int                headerChanged;
   int                descending;
   int                reversed;
   int                broken;
   int                changed;
   int                opening;
   int                sortRecords;
   char               name[31];
   char               diskName[11];
   char               expression[256];
   char               condition[256];
   uint32_t           ntxRoot;
   uint32_t           ntxHeader;
   uint32_t           ntxVersion;
   void              *ntxPages;
   uint32_t           cdxHeader;
   uint32_t           cdxRoot;
   X_DBF_INDEX_CURSOR cursor;
   X_DBF_INDEX_KEY    oldKey;
   X_DBF_INDEX_KEY    newKey;
   int                oldIncluded;
   int                newIncluded;
   /* Results of OrdKeyCount() and OrdKeyNo(), valid while the bag version and generation stay;
      generation changes with the scope, the direction and the unique setting */
   uint64_t        generation;
   uint64_t        countCache;
   uint64_t        countGeneration;
   uint32_t        countVersion;
   int             countValid;
   X_DBF_INDEX_KEY positionKey;
   uint64_t        positionCache;
   uint64_t        positionGeneration;
   uint32_t        positionVersion;
   int             positionValid;
};

void         x_dbfNtxDiscard( X_DBF_ORDER *index );
X_DBF_STATUS x_dbfNtxDirectory( X_DBF_INDEX_BAG *bag, int force );
X_DBF_STATUS x_dbfNtxRelease( X_DBF_ORDER *index, uint32_t page, unsigned int depth );
X_DBF_STATUS x_dbfNtxFind( X_DBF_ORDER *index, const X_DBF_INDEX_KEY *key, size_t length, int records, int strict,
                           int backwards, X_DBF_INDEX_CURSOR *cursor );
X_DBF_STATUS x_dbfNtxStep( X_DBF_ORDER *index, X_DBF_INDEX_CURSOR *cursor, int backwards );
X_DBF_STATUS x_dbfNtxChange( X_DBF_ORDER *index, const X_DBF_INDEX_KEY *removed, const X_DBF_INDEX_KEY *added );
X_DBF_STATUS x_dbfNtxSync( X_DBF_ORDER *index );
X_DBF_STATUS x_dbfCdxSync( X_DBF_INDEX_BAG *bag );
X_DBF_STATUS x_dbfNtxLoad( X_DBF_ORDER *index, int force );
X_DBF_STATUS x_dbfCdxLoad( X_DBF_INDEX_BAG *bag, int force );
X_DBF_STATUS x_dbfCdxChange( X_DBF_ORDER *index, const X_DBF_INDEX_KEY *removed, const X_DBF_INDEX_KEY *added );
void         x_dbfCdxDiscard( X_DBF_INDEX_BAG *bag );
X_DBF_STATUS x_dbfCdxFind( X_DBF_ORDER *index, const X_DBF_INDEX_KEY *key, size_t length, int records, int strict,
                           int backwards, X_DBF_INDEX_CURSOR *cursor );
X_DBF_STATUS x_dbfCdxStep( X_DBF_ORDER *index, X_DBF_INDEX_CURSOR *cursor, int backwards );
void         x_dbfOrderPagesClear( X_DBF_ORDER *index );
int          x_dbfOrderKeyAt( const X_DBF_ORDER *index, uint64_t rank, X_DBF_INDEX_KEY *key );
int          x_dbfOrderCompare( const X_DBF_ORDER *index, const X_DBF_INDEX_KEY *left, const X_DBF_INDEX_KEY *right,
                                size_t length, int records );

#endif
