/* $Id$ */
/** @file
 * IPRT - Memory Object Allocation Cache.
 */

/*
 * Copyright (C) 2006-2010 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/memcache.h>
#include "internal/iprt.h"

#include <iprt/assert.h>
#include <iprt/asm.h>
#include <iprt/critsect.h>
#include <iprt/err.h>
#include <iprt/mem.h>
#include <iprt/param.h>

#include "internal/magics.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/** Pointer to a cache instance. */
typedef struct RTMEMCACHEINT  *PRTMEMCACHEINT;
/** Pointer to a cache page. */
typedef struct RTMEMCACHEPAGE *PRTMEMCACHEPAGE;


/**
 * A cache page.
 *
 * This is a page of memory that we split up in to a bunch object sized chunks
 * and hand out to the cache users.  The bitmap is updated in an atomic fashion
 * so that we don't have to take any locks when freeing or allocating memory.
 */
typedef struct RTMEMCACHEPAGE
{
    /** Pointer to the cache owning this page.
     * This is used for validation purposes only.  */
    PRTMEMCACHEINT              pCache;
    /** Pointer to the next page.
     * This is marked as volatile since we'll be adding new entries to the list
     * without taking any locks. */
    PRTMEMCACHEPAGE volatile    pNext;
    /** Bitmap tracking allocated blocks. */
    void volatile              *pbmAlloc;
    /** Bitmap tracking which blocks that has been thru the constructor. */
    void volatile              *pbmCtor;
    /** Pointer to the object array.. */
    uint8_t                    *pbObjects;
    /** The number of objects on this page.  */
    uint32_t                    cObjects;

    /** Padding to force cFree into the next cache line. (ASSUMES CL = 64) */
    uint8_t                     abPadding[ARCH_BITS == 32 ? 64 - 6*4 : 64 - 5*8 - 4];
    /** The number of free objects. */
    int32_t volatile            cFree;
} RTMEMCACHEPAGE;
AssertCompileMemberOffset(RTMEMCACHEPAGE, cFree, 64);


/**
 * Memory object cache instance.
 */
typedef struct RTMEMCACHEINT
{
    /** Magic value (RTMEMCACHE_MAGIC). */
    uint32_t                    u32Magic;
    /** The object size.  */
    uint32_t                    cbObject;
    /** Object alignment.  */
    uint32_t                    cbAlignment;
    /** The per page object count. */
    uint32_t                    cPerPage;
    /** Number of bits in the bitmap.
     * @remarks This is higher or equal to cPerPage and it is aligned such that
     *          the search operation will be most efficient on x86/AMD64. */
    uint32_t                    cBits;
    /** The maximum number of objects. */
    uint32_t                    cMax;
    /** Head of the page list. */
    PRTMEMCACHEPAGE             pPageHead;
    /** Constructor callback. */
    PFNMEMCACHECTOR             pfnCtor;
    /** Destructor callback. */
    PFNMEMCACHEDTOR             pfnDtor;
    /** Callback argument. */
    void                       *pvUser;
    /** Critical section serializing page allocation and similar. */
    RTCRITSECT                  CritSect;

    /** The total object count. */
    uint32_t volatile           cTotal;
    /** The number of free objects. */
    int32_t volatile            cFree;
    /** This may point to a page with free entries. */
    PRTMEMCACHEPAGE volatile    pPageHint;
} RTMEMCACHEINT;



RTDECL(int) RTMemCacheCreate(PRTMEMCACHE phMemCache, size_t cbObject, size_t cbAlignment, uint32_t cMaxObjects,
                             PFNMEMCACHECTOR pfnCtor, PFNMEMCACHEDTOR pfnDtor, void *pvUser)
{
    AssertPtr(phMemCache);
    AssertPtrNull(pfnCtor);
    AssertPtrNull(pfnDtor);
    AssertReturn(cbObject > 0, VERR_INVALID_PARAMETER);
    AssertReturn(cbObject <= PAGE_SIZE / 8, VERR_INVALID_PARAMETER);

    if (cbAlignment == 0)
    {
        cbAlignment = UINT32_C(1) << ASMBitLastSetU32((uint32_t)cbObject);
        if (cbAlignment > 64)
            cbAlignment = 64;
    }
    else
    {
        AssertReturn(!((cbAlignment - 1) & cbAlignment), VERR_NOT_POWER_OF_TWO);
        AssertReturn(cbAlignment <= 64, VERR_OUT_OF_RANGE);
    }

    /*
     * Allocate and initialize the instance memory.
     */
    RTMEMCACHEINT *pThis = (RTMEMCACHEINT *)RTMemAlloc(sizeof(*pThis));
    if (!pThis)
        return VERR_NO_MEMORY;
    int rc = RTCritSectInit(&pThis->CritSect);
    if (RT_FAILURE(rc))
    {
        RTMemFree(pThis);
        return rc;
    }

    pThis->u32Magic         = RTMEMCACHE_MAGIC;
    pThis->cbObject         = RT_ALIGN_Z(cbObject, cbAlignment);
    pThis->cbAlignment      = cbAlignment;
    pThis->cPerPage         = (PAGE_SIZE - RT_ALIGN_Z(sizeof(RTMEMCACHEPAGE), cbAlignment)) / pThis->cbObject;
    while (    RT_ALIGN_Z(sizeof(RTMEMCACHEPAGE), RT_MIN(cbAlignment, 8))
             + pThis->cPerPage * pThis->cbObject
             + RT_ALIGN(pThis->cPerPage, 64) / 8 * 2
           > PAGE_SIZE)
        pThis->cPerPage--;
    pThis->cBits            = RT_ALIGN(pThis->cPerPage, 64);
    pThis->cMax             = cMaxObjects;
    pThis->cTotal           = 0;
    pThis->cFree            = 0;
    pThis->pPageHead        = NULL;
    pThis->pPageHint        = NULL;
    pThis->pfnCtor          = pfnCtor;
    pThis->pfnDtor          = pfnDtor;
    pThis->pvUser           = pvUser;

    *phMemCache = pThis;
    return VINF_SUCCESS;
}


RTDECL(int) RTMemCacheDestroy(RTMEMCACHE hMemCache)
{
    RTMEMCACHEINT *pThis = hMemCache;
    if (!pThis)
        return VINF_SUCCESS;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTMEMCACHE_MAGIC, VERR_INVALID_HANDLE);
    AssertMsg((uint32_t)pThis->cFree == pThis->cTotal, ("cFree=%u cTotal=%u\n", pThis->cFree, pThis->cTotal));

    /*
     * Destroy it.
     */
    AssertReturn(ASMAtomicCmpXchgU32(&pThis->u32Magic, RTMEMCACHE_MAGIC_DEAD, RTMEMCACHE_MAGIC), VERR_INVALID_HANDLE);
    RTCritSectDelete(&pThis->CritSect);

    while (pThis->pPageHead)
    {
        PRTMEMCACHEPAGE pPage = pThis->pPageHead;
        pThis->pPageHead = pPage->pNext;
        pPage->cFree = 0;

        if (pThis->pfnDtor)
        {
            uint32_t iObj = pPage->cObjects;
            while (iObj-- > 0)
                if (ASMBitTestAndClear(pPage->pbmCtor, iObj))
                    pThis->pfnDtor(hMemCache, pPage->pbObjects + iObj * pThis->cbObject, pThis->pvUser);
        }

        RTMemPageFree(pPage);
    }

    RTMemFree(pThis);
    return VINF_SUCCESS;
}


/**
 * Grows the cache.
 *
 * @returns IPRT status code.
 * @param   pThis               The memory cache instance.
 */
static int rtMemCacheGrow(RTMEMCACHEINT *pThis)
{
    /*
     * Enter the critical section here to avoid allocation races leading to
     * wasted memory (++) and make it easier to link in the new page.
     */
    RTCritSectEnter(&pThis->CritSect);
    int rc = VINF_SUCCESS;
    if (pThis->cFree < 0)
    {
        /*
         * Allocate and initialize the new page.
         *
         * We put the constructor bitmap at the lower end right after cFree.
         * We then push the object array to the end of the page and place the
         * allocation bitmap below it.  The hope is to increase the chance that
         * the allocation bitmap is in a different cache line than cFree since
         * this increases performance markably when lots of threads are beating
         * on the cache.
         */
        PRTMEMCACHEPAGE pPage = (PRTMEMCACHEPAGE)RTMemPageAlloc(PAGE_SIZE);
        if (pPage)
        {
            uint32_t const cObjects = RT_MIN(pThis->cPerPage, pThis->cMax - pThis->cTotal);

            ASMMemZeroPage(pPage);
            pPage->pCache       = pThis;
            pPage->pNext        = NULL;
            pPage->cFree        = cObjects;
            pPage->cObjects     = cObjects;
            uint8_t *pb = (uint8_t *)(pPage + 1);
            pb = RT_ALIGN_PT(pb, 8, uint8_t *);
            pPage->pbmCtor      = pb;
            pb += pThis->cBits / 8 * 2;
            pb = (uint8_t *)pPage + PAGE_SIZE - pThis->cbObject * cObjects;
            pPage->pbObjects    = pb;   Assert(RT_ALIGN_P(pb, pThis->cbAlignment) == pb);
            pb -= pThis->cBits / 8;
            pb = (uint8_t *)((uintptr_t)pb & ~(uintptr_t)7);
            pPage->pbmAlloc     = pb;
            Assert((uintptr_t)pPage->pbmCtor + pThis->cBits / 8 < (uintptr_t)pPage->pbmCtor);

            /* Mark the bitmap padding and any unused objects as allocated. */
            for (uint32_t iBit = cObjects; iBit < pThis->cBits; iBit++)
                ASMBitSet(pPage->pbmAlloc, iBit);

            /* Make it the hint. */
            ASMAtomicWritePtr((void * volatile *)&pThis->pPageHint, pPage);

            /* Link the page. */
            PRTMEMCACHEPAGE pPrevPage = pThis->pPageHead;
            if (!pPrevPage)
                ASMAtomicWritePtr((void * volatile *)&pThis->pPageHead, pPage);
            else
            {
                while (pPrevPage->pNext)
                    pPrevPage = pPrevPage->pNext;
                ASMAtomicWritePtr((void * volatile *)&pPrevPage->pNext, pPage);
            }

            /* Add it to the page counts. */
            ASMAtomicAddS32(&pThis->cFree, cObjects);
            ASMAtomicAddU32(&pThis->cTotal, cObjects);
        }
        else
            rc = VERR_NO_MEMORY;
    }
    RTCritSectLeave(&pThis->CritSect);
    return rc;
}


/**
 * Grabs a an object in a page.
 * @returns New cFree value on success (0 or higher), -1 on failure.
 * @param   pPage               Pointer to the page.
 */
DECL_FORCE_INLINE(int32_t) rtMemCacheGrabObj(PRTMEMCACHEPAGE pPage)
{
    int32_t cFreeNew = ASMAtomicDecS32(&pPage->cFree);
    if (cFreeNew < 0)
    {
        ASMAtomicIncS32(&pPage->cFree);
        return -1;
    }
    return cFreeNew;
}


RTDECL(int) RTMemCacheAllocEx(RTMEMCACHE hMemCache, void **ppvObj)
{
    RTMEMCACHEINT *pThis = hMemCache;
    AssertPtrReturn(pThis, VERR_INVALID_PARAMETER);
    AssertReturn(pThis->u32Magic == RTMEMCACHE_MAGIC, VERR_INVALID_PARAMETER);

    /*
     * Try grab a free object at the cache level.
     */
    int32_t cNewFree = ASMAtomicDecS32(&pThis->cFree);
    if (RT_LIKELY(cNewFree < 0))
    {
        uint32_t cTotal = ASMAtomicUoReadU32(&pThis->cTotal);
        if (   (uint32_t)(cTotal + -cNewFree) > pThis->cMax
            || (uint32_t)(cTotal + -cNewFree) <= cTotal)
        {
            ASMAtomicIncS32(&pThis->cFree);
            return VERR_MEM_CACHE_MAX_SIZE;
        }

        int rc = rtMemCacheGrow(pThis);
        if (RT_FAILURE(rc))
        {
            ASMAtomicIncS32(&pThis->cFree);
            return rc;
        }
    }

    /*
     * Grab a free object at the page level.
     */
    PRTMEMCACHEPAGE pPage = (PRTMEMCACHEPAGE)ASMAtomicReadPtr((void * volatile *)&pThis->pPageHint);
    int32_t iObj = pPage ? rtMemCacheGrabObj(pPage) : -1;
    if (iObj < 0)
    {
        for (unsigned cLoops = 0; ; cLoops++)
        {
            for (pPage = pThis->pPageHead; pPage; pPage = pPage->pNext)
            {
                iObj = rtMemCacheGrabObj(pPage);
                if (iObj >= 0)
                {
                    if (iObj > 0)
                        ASMAtomicWritePtr((void * volatile *)&pThis->pPageHint, pPage);
                    break;
                }
            }
            if (iObj >= 0)
                break;
            Assert(cLoops != 2);
            Assert(cLoops < 10);
        }
    }
    Assert(iObj >= 0);
    Assert((uint32_t)iObj < pThis->cMax);

    /*
     * Find a free object in the allocation bitmap.  Use the new cFree count
     * as a hint.
     */
    if (ASMAtomicBitTestAndSet(pPage->pbmAlloc, iObj))
    {
        for (unsigned cLoops2 = 0;; cLoops2++)
        {
            iObj = ASMBitFirstClear(pPage->pbmAlloc, pThis->cBits);
            if (RT_LIKELY(iObj >= 0))
            {
                if (!ASMAtomicBitTestAndSet(pPage->pbmAlloc, iObj))
                    break;
            }
            else
                ASMMemoryFence();
            Assert(cLoops2 != 40);
        }
        Assert(iObj >= 0);
    }
    void *pvObj = &pPage->pbObjects[iObj * pThis->cbObject];
    Assert((uintptr_t)pvObj - (uintptr_t)pPage < PAGE_SIZE);

    /*
     * Call the constructor?
     */
    if (   pThis->pfnCtor
        && !ASMAtomicBitTestAndSet(pPage->pbmCtor, iObj))
    {
        int rc = pThis->pfnCtor(hMemCache, pvObj, pThis->pvUser);
        if (RT_FAILURE(rc))
        {
            ASMAtomicBitClear(pPage->pbmCtor, iObj);
            RTMemCacheFree(pThis, pvObj);
            return rc;
        }
    }

    *ppvObj = pvObj;
    return VINF_SUCCESS;
}


RTDECL(void *) RTMemCacheAlloc(RTMEMCACHE hMemCache)
{
    void *pvObj;
    int rc = RTMemCacheAllocEx(hMemCache, &pvObj);
    if (RT_SUCCESS(rc))
        return pvObj;
    return NULL;
}


RTDECL(void) RTMemCacheFree(RTMEMCACHE hMemCache, void *pvObj)
{
    if (!pvObj)
        return;

    RTMEMCACHEINT *pThis = hMemCache;
    AssertPtrReturnVoid(pThis);
    AssertReturnVoid(pThis->u32Magic == RTMEMCACHE_MAGIC);

    AssertPtr(pvObj);
    Assert(RT_ALIGN_P(pvObj, pThis->cbAlignment) == pvObj);

    /* Note: Do *NOT* attempt to poison the object if we have a constructor
             or/and destructor! */

    /*
     * Find the cache page.  The page structure is at the start of the page.
     */
    PRTMEMCACHEPAGE pPage = (PRTMEMCACHEPAGE)(((uintptr_t)pvObj) & ~(uintptr_t)PAGE_OFFSET_MASK);
    Assert(pPage->pCache == pThis);
    Assert(ASMAtomicUoReadS32(&pPage->cFree) < (int32_t)pThis->cPerPage);

    /*
     * Clear the bitmap bit and update the two object counter. Order matters!
     */
    uintptr_t offObj = (uintptr_t)pvObj - (uintptr_t)pPage->pbObjects;
    uintptr_t iObj   = offObj / pThis->cbObject;
    Assert(iObj * pThis->cbObject == offObj);
    Assert(iObj < pThis->cPerPage);
    AssertReturnVoid(ASMAtomicBitTestAndClear(pPage->pbmAlloc, iObj));

    ASMAtomicIncS32(&pPage->cFree);
    ASMAtomicIncS32(&pThis->cFree);
}

