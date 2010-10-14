/** @file
 * IPRT - Assembly Functions.
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
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
 */

#ifndef ___iprt_asm_h
#define ___iprt_asm_h

#include <iprt/cdefs.h>
#include <iprt/types.h>
#include <iprt/assert.h>
/** @def RT_INLINE_ASM_USES_INTRIN
 * Defined as 1 if we're using a _MSC_VER 1400.
 * Otherwise defined as 0.
 */

/* Solaris 10 header ugliness */
#ifdef u
# undef u
#endif

#if defined(_MSC_VER) && RT_INLINE_ASM_USES_INTRIN
# include <intrin.h>
  /* Emit the intrinsics at all optimization levels. */
# pragma intrinsic(_ReadWriteBarrier)
# pragma intrinsic(__cpuid)
# pragma intrinsic(__stosd)
# pragma intrinsic(__stosw)
# pragma intrinsic(__stosb)
# pragma intrinsic(_BitScanForward)
# pragma intrinsic(_BitScanReverse)
# pragma intrinsic(_bittest)
# pragma intrinsic(_bittestandset)
# pragma intrinsic(_bittestandreset)
# pragma intrinsic(_bittestandcomplement)
# pragma intrinsic(_byteswap_ushort)
# pragma intrinsic(_byteswap_ulong)
# pragma intrinsic(_interlockedbittestandset)
# pragma intrinsic(_interlockedbittestandreset)
# pragma intrinsic(_InterlockedAnd)
# pragma intrinsic(_InterlockedOr)
# pragma intrinsic(_InterlockedIncrement)
# pragma intrinsic(_InterlockedDecrement)
# pragma intrinsic(_InterlockedExchange)
# pragma intrinsic(_InterlockedExchangeAdd)
# pragma intrinsic(_InterlockedCompareExchange)
# pragma intrinsic(_InterlockedCompareExchange64)
# ifdef RT_ARCH_AMD64
#  pragma intrinsic(__stosq)
#  pragma intrinsic(_byteswap_uint64)
#  pragma intrinsic(_InterlockedExchange64)
#  pragma intrinsic(_InterlockedExchangeAdd64)
#  pragma intrinsic(_InterlockedAnd64)
#  pragma intrinsic(_InterlockedOr64)
#  pragma intrinsic(_InterlockedIncrement64)
#  pragma intrinsic(_InterlockedDecrement64)
# endif
#endif


/** @defgroup grp_rt_asm    ASM - Assembly Routines
 * @ingroup grp_rt
 *
 * @remarks The difference between ordered and unordered atomic operations are that
 *          the former will complete outstanding reads and writes before continuing
 *          while the latter doesn't make any promisses about the order. Ordered
 *          operations doesn't, it seems, make any 100% promise wrt to whether
 *          the operation will complete before any subsequent memory access.
 *          (please, correct if wrong.)
 *
 *          ASMAtomicSomething operations are all ordered, while ASMAtomicUoSomething
 *          are unordered (note the Uo).
 *
 * @remarks Some remarks about __volatile__: Without this keyword gcc is allowed to reorder
 *          or even optimize assembler instructions away. For instance, in the following code
 *          the second rdmsr instruction is optimized away because gcc treats that instruction
 *          as deterministic:
 *
 *            @code
 *            static inline uint64_t rdmsr_low(int idx)
 *            {
 *              uint32_t low;
 *              __asm__ ("rdmsr" : "=a"(low) : "c"(idx) : "edx");
 *            }
 *            ...
 *            uint32_t msr1 = rdmsr_low(1);
 *            foo(msr1);
 *            msr1 = rdmsr_low(1);
 *            bar(msr1);
 *            @endcode
 *
 *          The input parameter of rdmsr_low is the same for both calls and therefore gcc will
 *          use the result of the first call as input parameter for bar() as well. For rdmsr this
 *          is not acceptable as this instruction is _not_ deterministic. This applies to reading
 *          machine status information in general.
 *
 * @{
 */


/** @def RT_INLINE_ASM_GCC_4_3_X_X86
 * Used to work around some 4.3.x register allocation issues in this version of
 * the compiler. So far this workaround is still required for 4.4 and 4.5. */
#ifdef __GNUC__
# define RT_INLINE_ASM_GCC_4_3_X_X86 (__GNUC__ == 4 && __GNUC_MINOR__ >= 3 && defined(__i386__))
#endif
#ifndef RT_INLINE_ASM_GCC_4_3_X_X86
# define RT_INLINE_ASM_GCC_4_3_X_X86 0
#endif

/** @def RT_INLINE_DONT_USE_CMPXCHG8B
 * i686-apple-darwin9-gcc-4.0.1 (GCC) 4.0.1 (Apple Inc. build 5493) screws up
 * RTSemRWRequestWrite semsemrw-lockless-generic.cpp in release builds. PIC
 * mode, x86.
 *
 * Some gcc 4.3.x versions may have register allocation issues with cmpxchg8b
 * when in PIC mode on x86.
 */
#ifndef RT_INLINE_DONT_MIX_CMPXCHG8B_AND_PIC
# define RT_INLINE_DONT_MIX_CMPXCHG8B_AND_PIC \
    (   (defined(PIC) || defined(__PIC__)) \
     && defined(RT_ARCH_X86) \
     && (   RT_INLINE_ASM_GCC_4_3_X_X86 \
         || defined(RT_OS_DARWIN)) )
#endif


/** @def ASMReturnAddress
 * Gets the return address of the current (or calling if you like) function or method.
 */
#ifdef _MSC_VER
# ifdef __cplusplus
extern "C"
# endif
void * _ReturnAddress(void);
# pragma intrinsic(_ReturnAddress)
# define ASMReturnAddress() _ReturnAddress()
#elif defined(__GNUC__) || defined(DOXYGEN_RUNNING)
# define ASMReturnAddress() __builtin_return_address(0)
#else
# error "Unsupported compiler."
#endif


/**
 * Compiler memory barrier.
 *
 * Ensure that the compiler does not use any cached (register/tmp stack) memory
 * values or any outstanding writes when returning from this function.
 *
 * This function must be used if non-volatile data is modified by a
 * device or the VMM. Typical cases are port access, MMIO access,
 * trapping instruction, etc.
 */
#if RT_INLINE_ASM_GNU_STYLE
# define ASMCompilerBarrier()   do { __asm__ __volatile__("" : : : "memory"); } while (0)
#elif RT_INLINE_ASM_USES_INTRIN
# define ASMCompilerBarrier()   do { _ReadWriteBarrier(); } while (0)
#else /* 2003 should have _ReadWriteBarrier() but I guess we're at 2002 level then... */
DECLINLINE(void) ASMCompilerBarrier(void)
{
    __asm
    {
    }
}
#endif


/** @def ASMBreakpoint
 * Debugger Breakpoint.
 * @remark  In the gnu world we add a nop instruction after the int3 to
 *          force gdb to remain at the int3 source line.
 * @remark  The L4 kernel will try make sense of the breakpoint, thus the jmp.
 * @internal
 */
#if RT_INLINE_ASM_GNU_STYLE
# if defined(RT_ARCH_AMD64) || defined(RT_ARCH_X86)
#  ifndef __L4ENV__
#   define ASMBreakpoint()      do { __asm__ __volatile__("int3\n\tnop"); } while (0)
#  else
#   define ASMBreakpoint()      do { __asm__ __volatile__("int3; jmp 1f; 1:"); } while (0)
#  endif
# elif defined(RT_ARCH_SPARC64)
#  define ASMBreakpoint()       do { __asm__ __volatile__("illtrap 0\n\t") } while (0)  /** @todo Sparc64: this is just a wild guess. */
# elif defined(RT_ARCH_SPARC)
#  define ASMBreakpoint()       do { __asm__ __volatile__("unimp 0\n\t"); } while (0)   /** @todo Sparc: this is just a wild guess (same as Sparc64, just different name). */
# else
#  error "PORTME"
# endif
#else
# define ASMBreakpoint()        __debugbreak()
#endif


/**
 * Spinloop hint for platforms that have these, empty function on the other
 * platforms.
 *
 * x86 & AMD64: The PAUSE variant of NOP for helping hyperthreaded CPUs detecing
 * spin locks.
 */
#if RT_INLINE_ASM_EXTERNAL && (defined(RT_ARCH_AMD64) || defined(RT_ARCH_X86))
DECLASM(void) ASMNopPause(void);
#else
DECLINLINE(void) ASMNopPause(void)
{
# if defined(RT_ARCH_AMD64) || defined(RT_ARCH_X86)
#  if RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__(".byte 0xf3,0x90\n\t");
#  else
    __asm {
        _emit 0f3h
        _emit 090h
    }
#  endif
# else
    /* dummy */
# endif
}
#endif


/**
 * Atomically Exchange an unsigned 8-bit value, ordered.
 *
 * @returns Current *pu8 value
 * @param   pu8    Pointer to the 8-bit variable to update.
 * @param   u8     The 8-bit value to assign to *pu8.
 */
#if RT_INLINE_ASM_EXTERNAL
DECLASM(uint8_t) ASMAtomicXchgU8(volatile uint8_t *pu8, uint8_t u8);
#else
DECLINLINE(uint8_t) ASMAtomicXchgU8(volatile uint8_t *pu8, uint8_t u8)
{
# if RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__("xchgb %0, %1\n\t"
                         : "=m" (*pu8),
                           "=q" (u8) /* =r - busted on g++ (GCC) 3.4.4 20050721 (Red Hat 3.4.4-2) */
                         : "1" (u8),
                           "m" (*pu8));
# else
    __asm
    {
#  ifdef RT_ARCH_AMD64
        mov     rdx, [pu8]
        mov     al, [u8]
        xchg    [rdx], al
        mov     [u8], al
#  else
        mov     edx, [pu8]
        mov     al, [u8]
        xchg    [edx], al
        mov     [u8], al
#  endif
    }
# endif
    return u8;
}
#endif


/**
 * Atomically Exchange a signed 8-bit value, ordered.
 *
 * @returns Current *pu8 value
 * @param   pi8     Pointer to the 8-bit variable to update.
 * @param   i8      The 8-bit value to assign to *pi8.
 */
DECLINLINE(int8_t) ASMAtomicXchgS8(volatile int8_t *pi8, int8_t i8)
{
    return (int8_t)ASMAtomicXchgU8((volatile uint8_t *)pi8, (uint8_t)i8);
}


/**
 * Atomically Exchange a bool value, ordered.
 *
 * @returns Current *pf value
 * @param   pf      Pointer to the 8-bit variable to update.
 * @param   f       The 8-bit value to assign to *pi8.
 */
DECLINLINE(bool) ASMAtomicXchgBool(volatile bool *pf, bool f)
{
#ifdef _MSC_VER
    return !!ASMAtomicXchgU8((volatile uint8_t *)pf, (uint8_t)f);
#else
    return (bool)ASMAtomicXchgU8((volatile uint8_t *)pf, (uint8_t)f);
#endif
}


/**
 * Atomically Exchange an unsigned 16-bit value, ordered.
 *
 * @returns Current *pu16 value
 * @param   pu16    Pointer to the 16-bit variable to update.
 * @param   u16     The 16-bit value to assign to *pu16.
 */
#if RT_INLINE_ASM_EXTERNAL
DECLASM(uint16_t) ASMAtomicXchgU16(volatile uint16_t *pu16, uint16_t u16);
#else
DECLINLINE(uint16_t) ASMAtomicXchgU16(volatile uint16_t *pu16, uint16_t u16)
{
# if RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__("xchgw %0, %1\n\t"
                         : "=m" (*pu16),
                           "=r" (u16)
                         : "1" (u16),
                           "m" (*pu16));
# else
    __asm
    {
#  ifdef RT_ARCH_AMD64
        mov     rdx, [pu16]
        mov     ax, [u16]
        xchg    [rdx], ax
        mov     [u16], ax
#  else
        mov     edx, [pu16]
        mov     ax, [u16]
        xchg    [edx], ax
        mov     [u16], ax
#  endif
    }
# endif
    return u16;
}
#endif


/**
 * Atomically Exchange a signed 16-bit value, ordered.
 *
 * @returns Current *pu16 value
 * @param   pi16    Pointer to the 16-bit variable to update.
 * @param   i16     The 16-bit value to assign to *pi16.
 */
DECLINLINE(int16_t) ASMAtomicXchgS16(volatile int16_t *pi16, int16_t i16)
{
    return (int16_t)ASMAtomicXchgU16((volatile uint16_t *)pi16, (uint16_t)i16);
}


/**
 * Atomically Exchange an unsigned 32-bit value, ordered.
 *
 * @returns Current *pu32 value
 * @param   pu32    Pointer to the 32-bit variable to update.
 * @param   u32     The 32-bit value to assign to *pu32.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(uint32_t) ASMAtomicXchgU32(volatile uint32_t *pu32, uint32_t u32);
#else
DECLINLINE(uint32_t) ASMAtomicXchgU32(volatile uint32_t *pu32, uint32_t u32)
{
# if RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__("xchgl %0, %1\n\t"
                         : "=m" (*pu32),
                           "=r" (u32)
                         : "1" (u32),
                           "m" (*pu32));

# elif RT_INLINE_ASM_USES_INTRIN
   u32 = _InterlockedExchange((long *)pu32, u32);

# else
    __asm
    {
#  ifdef RT_ARCH_AMD64
        mov     rdx, [pu32]
        mov     eax, u32
        xchg    [rdx], eax
        mov     [u32], eax
#  else
        mov     edx, [pu32]
        mov     eax, u32
        xchg    [edx], eax
        mov     [u32], eax
#  endif
    }
# endif
    return u32;
}
#endif


/**
 * Atomically Exchange a signed 32-bit value, ordered.
 *
 * @returns Current *pu32 value
 * @param   pi32    Pointer to the 32-bit variable to update.
 * @param   i32     The 32-bit value to assign to *pi32.
 */
DECLINLINE(int32_t) ASMAtomicXchgS32(volatile int32_t *pi32, int32_t i32)
{
    return (int32_t)ASMAtomicXchgU32((volatile uint32_t *)pi32, (uint32_t)i32);
}


/**
 * Atomically Exchange an unsigned 64-bit value, ordered.
 *
 * @returns Current *pu64 value
 * @param   pu64    Pointer to the 64-bit variable to update.
 * @param   u64     The 64-bit value to assign to *pu64.
 */
#if (RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN) \
 || RT_INLINE_DONT_MIX_CMPXCHG8B_AND_PIC
DECLASM(uint64_t) ASMAtomicXchgU64(volatile uint64_t *pu64, uint64_t u64);
#else
DECLINLINE(uint64_t) ASMAtomicXchgU64(volatile uint64_t *pu64, uint64_t u64)
{
# if defined(RT_ARCH_AMD64)
#  if RT_INLINE_ASM_USES_INTRIN
   u64 = _InterlockedExchange64((__int64 *)pu64, u64);

#  elif RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__("xchgq %0, %1\n\t"
                         : "=m" (*pu64),
                           "=r" (u64)
                         : "1" (u64),
                           "m" (*pu64));
#  else
    __asm
    {
        mov     rdx, [pu64]
        mov     rax, [u64]
        xchg    [rdx], rax
        mov     [u64], rax
    }
#  endif
# else /* !RT_ARCH_AMD64 */
#  if RT_INLINE_ASM_GNU_STYLE
#   if defined(PIC) || defined(__PIC__)
    uint32_t u32EBX = (uint32_t)u64;
    __asm__ __volatile__(/*"xchgl %%esi, %5\n\t"*/
                         "xchgl %%ebx, %3\n\t"
                         "1:\n\t"
                         "lock; cmpxchg8b (%5)\n\t"
                         "jnz 1b\n\t"
                         "movl %3, %%ebx\n\t"
                         /*"xchgl %%esi, %5\n\t"*/
                         : "=A" (u64),
                           "=m" (*pu64)
                         : "0" (*pu64),
                           "m" ( u32EBX ),
                           "c" ( (uint32_t)(u64 >> 32) ),
                           "S" (pu64));
#   else /* !PIC */
    __asm__ __volatile__("1:\n\t"
                         "lock; cmpxchg8b %1\n\t"
                         "jnz 1b\n\t"
                         : "=A" (u64),
                           "=m" (*pu64)
                         : "0" (*pu64),
                           "b" ( (uint32_t)u64 ),
                           "c" ( (uint32_t)(u64 >> 32) ));
#   endif
#  else
    __asm
    {
        mov     ebx, dword ptr [u64]
        mov     ecx, dword ptr [u64 + 4]
        mov     edi, pu64
        mov     eax, dword ptr [edi]
        mov     edx, dword ptr [edi + 4]
    retry:
        lock cmpxchg8b [edi]
        jnz retry
        mov     dword ptr [u64], eax
        mov     dword ptr [u64 + 4], edx
    }
#  endif
# endif /* !RT_ARCH_AMD64 */
    return u64;
}
#endif


/**
 * Atomically Exchange an signed 64-bit value, ordered.
 *
 * @returns Current *pi64 value
 * @param   pi64    Pointer to the 64-bit variable to update.
 * @param   i64     The 64-bit value to assign to *pi64.
 */
DECLINLINE(int64_t) ASMAtomicXchgS64(volatile int64_t *pi64, int64_t i64)
{
    return (int64_t)ASMAtomicXchgU64((volatile uint64_t *)pi64, (uint64_t)i64);
}


/**
 * Atomically Exchange a pointer value, ordered.
 *
 * @returns Current *ppv value
 * @param   ppv    Pointer to the pointer variable to update.
 * @param   pv     The pointer value to assign to *ppv.
 */
DECLINLINE(void *) ASMAtomicXchgPtr(void * volatile *ppv, const void *pv)
{
#if ARCH_BITS == 32
    return (void *)ASMAtomicXchgU32((volatile uint32_t *)(void *)ppv, (uint32_t)pv);
#elif ARCH_BITS == 64
    return (void *)ASMAtomicXchgU64((volatile uint64_t *)(void *)ppv, (uint64_t)pv);
#else
# error "ARCH_BITS is bogus"
#endif
}


/**
 * Convenience macro for avoiding the annoying casting with ASMAtomicXchgPtr.
 *
 * @returns Current *pv value
 * @param   ppv     Pointer to the pointer variable to update.
 * @param   pv      The pointer value to assign to *ppv.
 * @param   Type    The type of *ppv, sans volatile.
 */
#ifdef __GNUC__
# define ASMAtomicXchgPtrT(ppv, pv, Type) \
    __extension__ \
    ({\
        __typeof__(*(ppv)) volatile * const ppvTypeChecked = (ppv); \
        Type                          const pvTypeChecked  = (pv); \
        Type pvTypeCheckedRet = (__typeof__(*(ppv))) ASMAtomicXchgPtr((void * volatile *)ppvTypeChecked, (void *)pvTypeChecked); \
        pvTypeCheckedRet; \
     })
#else
# define ASMAtomicXchgPtrT(ppv, pv, Type) \
    (Type)ASMAtomicXchgPtr((void * volatile *)(ppv), (void *)(pv))
#endif


/**
 * Atomically Exchange a raw-mode context pointer value, ordered.
 *
 * @returns Current *ppv value
 * @param   ppvRC   Pointer to the pointer variable to update.
 * @param   pvRC    The pointer value to assign to *ppv.
 */
DECLINLINE(RTRCPTR) ASMAtomicXchgRCPtr(RTRCPTR volatile *ppvRC, RTRCPTR pvRC)
{
    return (RTRCPTR)ASMAtomicXchgU32((uint32_t volatile *)(void *)ppvRC, (uint32_t)pvRC);
}


/**
 * Atomically Exchange a ring-0 pointer value, ordered.
 *
 * @returns Current *ppv value
 * @param   ppvR0  Pointer to the pointer variable to update.
 * @param   pvR0   The pointer value to assign to *ppv.
 */
DECLINLINE(RTR0PTR) ASMAtomicXchgR0Ptr(RTR0PTR volatile *ppvR0, RTR0PTR pvR0)
{
#if R0_ARCH_BITS == 32
    return (RTR0PTR)ASMAtomicXchgU32((volatile uint32_t *)(void *)ppvR0, (uint32_t)pvR0);
#elif R0_ARCH_BITS == 64
    return (RTR0PTR)ASMAtomicXchgU64((volatile uint64_t *)(void *)ppvR0, (uint64_t)pvR0);
#else
# error "R0_ARCH_BITS is bogus"
#endif
}


/**
 * Atomically Exchange a ring-3 pointer value, ordered.
 *
 * @returns Current *ppv value
 * @param   ppvR3  Pointer to the pointer variable to update.
 * @param   pvR3   The pointer value to assign to *ppv.
 */
DECLINLINE(RTR3PTR) ASMAtomicXchgR3Ptr(RTR3PTR volatile *ppvR3, RTR3PTR pvR3)
{
#if R3_ARCH_BITS == 32
    return (RTR3PTR)ASMAtomicXchgU32((volatile uint32_t *)(void *)ppvR3, (uint32_t)pvR3);
#elif R3_ARCH_BITS == 64
    return (RTR3PTR)ASMAtomicXchgU64((volatile uint64_t *)(void *)ppvR3, (uint64_t)pvR3);
#else
# error "R3_ARCH_BITS is bogus"
#endif
}


/** @def ASMAtomicXchgHandle
 * Atomically Exchange a typical IPRT handle value, ordered.
 *
 * @param   ph          Pointer to the value to update.
 * @param   hNew        The new value to assigned to *pu.
 * @param   phRes       Where to store the current *ph value.
 *
 * @remarks This doesn't currently work for all handles (like RTFILE).
 */
#if HC_ARCH_BITS == 32
# define ASMAtomicXchgHandle(ph, hNew, phRes) \
   do { \
       AssertCompile(sizeof(*(ph))    == sizeof(uint32_t)); \
       AssertCompile(sizeof(*(phRes)) == sizeof(uint32_t)); \
       *(uint32_t *)(phRes) = ASMAtomicXchgU32((uint32_t volatile *)(ph), (const uint32_t)(hNew)); \
   } while (0)
#elif HC_ARCH_BITS == 64
# define ASMAtomicXchgHandle(ph, hNew, phRes) \
   do { \
       AssertCompile(sizeof(*(ph))    == sizeof(uint64_t)); \
       AssertCompile(sizeof(*(phRes)) == sizeof(uint64_t)); \
       *(uint64_t *)(phRes) = ASMAtomicXchgU64((uint64_t volatile *)(ph), (const uint64_t)(hNew)); \
   } while (0)
#else
# error HC_ARCH_BITS
#endif


/**
 * Atomically Exchange a value which size might differ
 * between platforms or compilers, ordered.
 *
 * @param   pu      Pointer to the variable to update.
 * @param   uNew    The value to assign to *pu.
 * @todo This is busted as its missing the result argument.
 */
#define ASMAtomicXchgSize(pu, uNew) \
    do { \
        switch (sizeof(*(pu))) { \
            case 1: ASMAtomicXchgU8((volatile uint8_t *)(void *)(pu), (uint8_t)(uNew)); break; \
            case 2: ASMAtomicXchgU16((volatile uint16_t *)(void *)(pu), (uint16_t)(uNew)); break; \
            case 4: ASMAtomicXchgU32((volatile uint32_t *)(void *)(pu), (uint32_t)(uNew)); break; \
            case 8: ASMAtomicXchgU64((volatile uint64_t *)(void *)(pu), (uint64_t)(uNew)); break; \
            default: AssertMsgFailed(("ASMAtomicXchgSize: size %d is not supported\n", sizeof(*(pu)))); \
        } \
    } while (0)

/**
 * Atomically Exchange a value which size might differ
 * between platforms or compilers, ordered.
 *
 * @param   pu      Pointer to the variable to update.
 * @param   uNew    The value to assign to *pu.
 * @param   puRes   Where to store the current *pu value.
 */
#define ASMAtomicXchgSizeCorrect(pu, uNew, puRes) \
    do { \
        switch (sizeof(*(pu))) { \
            case 1: *(uint8_t  *)(puRes) = ASMAtomicXchgU8((volatile uint8_t *)(void *)(pu), (uint8_t)(uNew)); break; \
            case 2: *(uint16_t *)(puRes) = ASMAtomicXchgU16((volatile uint16_t *)(void *)(pu), (uint16_t)(uNew)); break; \
            case 4: *(uint32_t *)(puRes) = ASMAtomicXchgU32((volatile uint32_t *)(void *)(pu), (uint32_t)(uNew)); break; \
            case 8: *(uint64_t *)(puRes) = ASMAtomicXchgU64((volatile uint64_t *)(void *)(pu), (uint64_t)(uNew)); break; \
            default: AssertMsgFailed(("ASMAtomicXchgSize: size %d is not supported\n", sizeof(*(pu)))); \
        } \
    } while (0)



/**
 * Atomically Compare and Exchange an unsigned 8-bit value, ordered.
 *
 * @returns true if xchg was done.
 * @returns false if xchg wasn't done.
 *
 * @param   pu8         Pointer to the value to update.
 * @param   u8New       The new value to assigned to *pu8.
 * @param   u8Old       The old value to *pu8 compare with.
 */
#if RT_INLINE_ASM_EXTERNAL || !RT_INLINE_ASM_GNU_STYLE
DECLASM(bool) ASMAtomicCmpXchgU8(volatile uint8_t *pu8, const uint8_t u8New, const uint8_t u8Old);
#else
DECLINLINE(bool) ASMAtomicCmpXchgU8(volatile uint8_t *pu8, const uint8_t u8New, uint8_t u8Old)
{
    uint8_t u8Ret;
    __asm__ __volatile__("lock; cmpxchgb %3, %0\n\t"
                         "setz  %1\n\t"
                         : "=m" (*pu8),
                           "=qm" (u8Ret),
                           "=a" (u8Old)
                         : "q" (u8New),
                           "2" (u8Old),
                           "m" (*pu8));
    return (bool)u8Ret;
}
#endif


/**
 * Atomically Compare and Exchange a signed 8-bit value, ordered.
 *
 * @returns true if xchg was done.
 * @returns false if xchg wasn't done.
 *
 * @param   pi8         Pointer to the value to update.
 * @param   i8New       The new value to assigned to *pi8.
 * @param   i8Old       The old value to *pi8 compare with.
 */
DECLINLINE(bool) ASMAtomicCmpXchgS8(volatile int8_t *pi8, const int8_t i8New, const int8_t i8Old)
{
    return ASMAtomicCmpXchgU8((volatile uint8_t *)pi8, (const uint8_t)i8New, (const uint8_t)i8Old);
}


/**
 * Atomically Compare and Exchange a bool value, ordered.
 *
 * @returns true if xchg was done.
 * @returns false if xchg wasn't done.
 *
 * @param   pf          Pointer to the value to update.
 * @param   fNew        The new value to assigned to *pf.
 * @param   fOld        The old value to *pf compare with.
 */
DECLINLINE(bool) ASMAtomicCmpXchgBool(volatile bool *pf, const bool fNew, const bool fOld)
{
    return ASMAtomicCmpXchgU8((volatile uint8_t *)pf, (const uint8_t)fNew, (const uint8_t)fOld);
}


/**
 * Atomically Compare and Exchange an unsigned 32-bit value, ordered.
 *
 * @returns true if xchg was done.
 * @returns false if xchg wasn't done.
 *
 * @param   pu32        Pointer to the value to update.
 * @param   u32New      The new value to assigned to *pu32.
 * @param   u32Old      The old value to *pu32 compare with.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(bool) ASMAtomicCmpXchgU32(volatile uint32_t *pu32, const uint32_t u32New, const uint32_t u32Old);
#else
DECLINLINE(bool) ASMAtomicCmpXchgU32(volatile uint32_t *pu32, const uint32_t u32New, uint32_t u32Old)
{
# if RT_INLINE_ASM_GNU_STYLE
    uint8_t u8Ret;
    __asm__ __volatile__("lock; cmpxchgl %3, %0\n\t"
                         "setz  %1\n\t"
                         : "=m" (*pu32),
                           "=qm" (u8Ret),
                           "=a" (u32Old)
                         : "r" (u32New),
                           "2" (u32Old),
                           "m" (*pu32));
    return (bool)u8Ret;

# elif RT_INLINE_ASM_USES_INTRIN
    return _InterlockedCompareExchange((long *)pu32, u32New, u32Old) == u32Old;

# else
    uint32_t u32Ret;
    __asm
    {
#  ifdef RT_ARCH_AMD64
        mov     rdx, [pu32]
#  else
        mov     edx, [pu32]
#  endif
        mov     eax, [u32Old]
        mov     ecx, [u32New]
#  ifdef RT_ARCH_AMD64
        lock cmpxchg [rdx], ecx
#  else
        lock cmpxchg [edx], ecx
#  endif
        setz    al
        movzx   eax, al
        mov     [u32Ret], eax
    }
    return !!u32Ret;
# endif
}
#endif


/**
 * Atomically Compare and Exchange a signed 32-bit value, ordered.
 *
 * @returns true if xchg was done.
 * @returns false if xchg wasn't done.
 *
 * @param   pi32        Pointer to the value to update.
 * @param   i32New      The new value to assigned to *pi32.
 * @param   i32Old      The old value to *pi32 compare with.
 */
DECLINLINE(bool) ASMAtomicCmpXchgS32(volatile int32_t *pi32, const int32_t i32New, const int32_t i32Old)
{
    return ASMAtomicCmpXchgU32((volatile uint32_t *)pi32, (uint32_t)i32New, (uint32_t)i32Old);
}


/**
 * Atomically Compare and exchange an unsigned 64-bit value, ordered.
 *
 * @returns true if xchg was done.
 * @returns false if xchg wasn't done.
 *
 * @param   pu64    Pointer to the 64-bit variable to update.
 * @param   u64New  The 64-bit value to assign to *pu64.
 * @param   u64Old  The value to compare with.
 */
#if (RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN) \
 || RT_INLINE_DONT_MIX_CMPXCHG8B_AND_PIC
DECLASM(bool) ASMAtomicCmpXchgU64(volatile uint64_t *pu64, const uint64_t u64New, const uint64_t u64Old);
#else
DECLINLINE(bool) ASMAtomicCmpXchgU64(volatile uint64_t *pu64, uint64_t u64New, uint64_t u64Old)
{
# if RT_INLINE_ASM_USES_INTRIN
   return _InterlockedCompareExchange64((__int64 *)pu64, u64New, u64Old) == u64Old;

# elif defined(RT_ARCH_AMD64)
#  if RT_INLINE_ASM_GNU_STYLE
    uint8_t u8Ret;
    __asm__ __volatile__("lock; cmpxchgq %3, %0\n\t"
                         "setz  %1\n\t"
                         : "=m" (*pu64),
                           "=qm" (u8Ret),
                           "=a" (u64Old)
                         : "r" (u64New),
                           "2" (u64Old),
                           "m" (*pu64));
    return (bool)u8Ret;
#  else
    bool fRet;
    __asm
    {
        mov     rdx, [pu32]
        mov     rax, [u64Old]
        mov     rcx, [u64New]
        lock cmpxchg [rdx], rcx
        setz    al
        mov     [fRet], al
    }
    return fRet;
#  endif
# else /* !RT_ARCH_AMD64 */
    uint32_t u32Ret;
#  if RT_INLINE_ASM_GNU_STYLE
#   if defined(PIC) || defined(__PIC__)
    uint32_t u32EBX = (uint32_t)u64New;
    uint32_t u32Spill;
    __asm__ __volatile__("xchgl %%ebx, %4\n\t"
                         "lock; cmpxchg8b (%6)\n\t"
                         "setz  %%al\n\t"
                         "movl  %4, %%ebx\n\t"
                         "movzbl %%al, %%eax\n\t"
                         : "=a" (u32Ret),
                           "=d" (u32Spill),
#    if (__GNUC__ * 100 + __GNUC_MINOR__) >= 403
                           "+m" (*pu64)
#    else
                           "=m" (*pu64)
#    endif
                         : "A" (u64Old),
                           "m" ( u32EBX ),
                           "c" ( (uint32_t)(u64New >> 32) ),
                           "S" (pu64));
#   else /* !PIC */
    uint32_t u32Spill;
    __asm__ __volatile__("lock; cmpxchg8b %2\n\t"
                         "setz  %%al\n\t"
                         "movzbl %%al, %%eax\n\t"
                         : "=a" (u32Ret),
                           "=d" (u32Spill),
                           "+m" (*pu64)
                         : "A" (u64Old),
                           "b" ( (uint32_t)u64New ),
                           "c" ( (uint32_t)(u64New >> 32) ));
#   endif
    return (bool)u32Ret;
#  else
    __asm
    {
        mov     ebx, dword ptr [u64New]
        mov     ecx, dword ptr [u64New + 4]
        mov     edi, [pu64]
        mov     eax, dword ptr [u64Old]
        mov     edx, dword ptr [u64Old + 4]
        lock cmpxchg8b [edi]
        setz    al
        movzx   eax, al
        mov     dword ptr [u32Ret], eax
    }
    return !!u32Ret;
#  endif
# endif /* !RT_ARCH_AMD64 */
}
#endif


/**
 * Atomically Compare and exchange a signed 64-bit value, ordered.
 *
 * @returns true if xchg was done.
 * @returns false if xchg wasn't done.
 *
 * @param   pi64    Pointer to the 64-bit variable to update.
 * @param   i64     The 64-bit value to assign to *pu64.
 * @param   i64Old  The value to compare with.
 */
DECLINLINE(bool) ASMAtomicCmpXchgS64(volatile int64_t *pi64, const int64_t i64, const int64_t i64Old)
{
    return ASMAtomicCmpXchgU64((volatile uint64_t *)pi64, (uint64_t)i64, (uint64_t)i64Old);
}


/**
 * Atomically Compare and Exchange a pointer value, ordered.
 *
 * @returns true if xchg was done.
 * @returns false if xchg wasn't done.
 *
 * @param   ppv         Pointer to the value to update.
 * @param   pvNew       The new value to assigned to *ppv.
 * @param   pvOld       The old value to *ppv compare with.
 */
DECLINLINE(bool) ASMAtomicCmpXchgPtrVoid(void * volatile *ppv, const void *pvNew, const void *pvOld)
{
#if ARCH_BITS == 32
    return ASMAtomicCmpXchgU32((volatile uint32_t *)(void *)ppv, (uint32_t)pvNew, (uint32_t)pvOld);
#elif ARCH_BITS == 64
    return ASMAtomicCmpXchgU64((volatile uint64_t *)(void *)ppv, (uint64_t)pvNew, (uint64_t)pvOld);
#else
# error "ARCH_BITS is bogus"
#endif
}


/**
 * Atomically Compare and Exchange a pointer value, ordered.
 *
 * @returns true if xchg was done.
 * @returns false if xchg wasn't done.
 *
 * @param   ppv         Pointer to the value to update.
 * @param   pvNew       The new value to assigned to *ppv.
 * @param   pvOld       The old value to *ppv compare with.
 *
 * @remarks This is relatively type safe on GCC platforms.
 */
#ifdef __GNUC__
# define ASMAtomicCmpXchgPtr(ppv, pvNew, pvOld) \
    __extension__ \
    ({\
        __typeof__(*(ppv)) volatile * const ppvTypeChecked   = (ppv); \
        __typeof__(*(ppv))            const pvNewTypeChecked = (pvNew); \
        __typeof__(*(ppv))            const pvOldTypeChecked = (pvOld); \
        bool fMacroRet = ASMAtomicCmpXchgPtrVoid((void * volatile *)ppvTypeChecked, \
                                                 (void *)pvNewTypeChecked, (void *)pvOldTypeChecked); \
        fMacroRet; \
     })
#else
# define ASMAtomicCmpXchgPtr(ppv, pvNew, pvOld) \
    ASMAtomicCmpXchgPtrVoid((void * volatile *)(ppv), (void *)(pvNew), (void *)(pvOld))
#endif


/** @def ASMAtomicCmpXchgHandle
 * Atomically Compare and Exchange a typical IPRT handle value, ordered.
 *
 * @param   ph          Pointer to the value to update.
 * @param   hNew        The new value to assigned to *pu.
 * @param   hOld        The old value to *pu compare with.
 * @param   fRc         Where to store the result.
 *
 * @remarks This doesn't currently work for all handles (like RTFILE).
 */
#if HC_ARCH_BITS == 32
# define ASMAtomicCmpXchgHandle(ph, hNew, hOld, fRc) \
   do { \
       AssertCompile(sizeof(*(ph)) == sizeof(uint32_t)); \
       (fRc) = ASMAtomicCmpXchgU32((uint32_t volatile *)(ph), (const uint32_t)(hNew), (const uint32_t)(hOld)); \
   } while (0)
#elif HC_ARCH_BITS == 64
# define ASMAtomicCmpXchgHandle(ph, hNew, hOld, fRc) \
   do { \
       AssertCompile(sizeof(*(ph)) == sizeof(uint64_t)); \
       (fRc) = ASMAtomicCmpXchgU64((uint64_t volatile *)(ph), (const uint64_t)(hNew), (const uint64_t)(hOld)); \
   } while (0)
#else
# error HC_ARCH_BITS
#endif


/** @def ASMAtomicCmpXchgSize
 * Atomically Compare and Exchange a value which size might differ
 * between platforms or compilers, ordered.
 *
 * @param   pu          Pointer to the value to update.
 * @param   uNew        The new value to assigned to *pu.
 * @param   uOld        The old value to *pu compare with.
 * @param   fRc         Where to store the result.
 */
#define ASMAtomicCmpXchgSize(pu, uNew, uOld, fRc) \
    do { \
        switch (sizeof(*(pu))) { \
            case 4: (fRc) = ASMAtomicCmpXchgU32((volatile uint32_t *)(void *)(pu), (uint32_t)(uNew), (uint32_t)(uOld)); \
                break; \
            case 8: (fRc) = ASMAtomicCmpXchgU64((volatile uint64_t *)(void *)(pu), (uint64_t)(uNew), (uint64_t)(uOld)); \
                break; \
            default: AssertMsgFailed(("ASMAtomicCmpXchgSize: size %d is not supported\n", sizeof(*(pu)))); \
                (fRc) = false; \
                break; \
        } \
    } while (0)


/**
 * Atomically Compare and Exchange an unsigned 32-bit value, additionally
 * passes back old value, ordered.
 *
 * @returns true if xchg was done.
 * @returns false if xchg wasn't done.
 *
 * @param   pu32        Pointer to the value to update.
 * @param   u32New      The new value to assigned to *pu32.
 * @param   u32Old      The old value to *pu32 compare with.
 * @param   pu32Old     Pointer store the old value at.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(bool) ASMAtomicCmpXchgExU32(volatile uint32_t *pu32, const uint32_t u32New, const uint32_t u32Old, uint32_t *pu32Old);
#else
DECLINLINE(bool) ASMAtomicCmpXchgExU32(volatile uint32_t *pu32, const uint32_t u32New, const uint32_t u32Old, uint32_t *pu32Old)
{
# if RT_INLINE_ASM_GNU_STYLE
    uint8_t u8Ret;
    __asm__ __volatile__("lock; cmpxchgl %3, %0\n\t"
                         "setz  %1\n\t"
                         : "=m" (*pu32),
                           "=qm" (u8Ret),
                           "=a" (*pu32Old)
                         : "r" (u32New),
                           "a" (u32Old),
                           "m" (*pu32));
    return (bool)u8Ret;

# elif RT_INLINE_ASM_USES_INTRIN
    return (*pu32Old =_InterlockedCompareExchange((long *)pu32, u32New, u32Old)) == u32Old;

# else
    uint32_t u32Ret;
    __asm
    {
#  ifdef RT_ARCH_AMD64
        mov     rdx, [pu32]
#  else
        mov     edx, [pu32]
#  endif
        mov     eax, [u32Old]
        mov     ecx, [u32New]
#  ifdef RT_ARCH_AMD64
        lock cmpxchg [rdx], ecx
        mov     rdx, [pu32Old]
        mov     [rdx], eax
#  else
        lock cmpxchg [edx], ecx
        mov     edx, [pu32Old]
        mov     [edx], eax
#  endif
        setz    al
        movzx   eax, al
        mov     [u32Ret], eax
    }
    return !!u32Ret;
# endif
}
#endif


/**
 * Atomically Compare and Exchange a signed 32-bit value, additionally
 * passes back old value, ordered.
 *
 * @returns true if xchg was done.
 * @returns false if xchg wasn't done.
 *
 * @param   pi32        Pointer to the value to update.
 * @param   i32New      The new value to assigned to *pi32.
 * @param   i32Old      The old value to *pi32 compare with.
 * @param   pi32Old     Pointer store the old value at.
 */
DECLINLINE(bool) ASMAtomicCmpXchgExS32(volatile int32_t *pi32, const int32_t i32New, const int32_t i32Old, int32_t *pi32Old)
{
    return ASMAtomicCmpXchgExU32((volatile uint32_t *)pi32, (uint32_t)i32New, (uint32_t)i32Old, (uint32_t *)pi32Old);
}


/**
 * Atomically Compare and exchange an unsigned 64-bit value, additionally
 * passing back old value, ordered.
 *
 * @returns true if xchg was done.
 * @returns false if xchg wasn't done.
 *
 * @param   pu64    Pointer to the 64-bit variable to update.
 * @param   u64New  The 64-bit value to assign to *pu64.
 * @param   u64Old  The value to compare with.
 * @param   pu64Old     Pointer store the old value at.
 */
#if (RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN) \
 || RT_INLINE_DONT_MIX_CMPXCHG8B_AND_PIC
DECLASM(bool) ASMAtomicCmpXchgExU64(volatile uint64_t *pu64, const uint64_t u64New, const uint64_t u64Old, uint64_t *pu64Old);
#else
DECLINLINE(bool) ASMAtomicCmpXchgExU64(volatile uint64_t *pu64, const uint64_t u64New, const uint64_t u64Old, uint64_t *pu64Old)
{
# if RT_INLINE_ASM_USES_INTRIN
   return (*pu64Old =_InterlockedCompareExchange64((__int64 *)pu64, u64New, u64Old)) == u64Old;

# elif defined(RT_ARCH_AMD64)
#  if RT_INLINE_ASM_GNU_STYLE
    uint8_t u8Ret;
    __asm__ __volatile__("lock; cmpxchgq %3, %0\n\t"
                         "setz  %1\n\t"
                         : "=m" (*pu64),
                           "=qm" (u8Ret),
                           "=a" (*pu64Old)
                         : "r" (u64New),
                           "a" (u64Old),
                           "m" (*pu64));
    return (bool)u8Ret;
#  else
    bool fRet;
    __asm
    {
        mov     rdx, [pu32]
        mov     rax, [u64Old]
        mov     rcx, [u64New]
        lock cmpxchg [rdx], rcx
        mov     rdx, [pu64Old]
        mov     [rdx], rax
        setz    al
        mov     [fRet], al
    }
    return fRet;
#  endif
# else /* !RT_ARCH_AMD64 */
#  if RT_INLINE_ASM_GNU_STYLE
    uint64_t u64Ret;
#   if defined(PIC) || defined(__PIC__)
    /* NB: this code uses a memory clobber description, because the clean
     * solution with an output value for *pu64 makes gcc run out of registers.
     * This will cause suboptimal code, and anyone with a better solution is
     * welcome to improve this. */
    __asm__ __volatile__("xchgl %%ebx, %1\n\t"
                         "lock; cmpxchg8b %3\n\t"
                         "xchgl %%ebx, %1\n\t"
                         : "=A" (u64Ret)
                         : "DS" ((uint32_t)u64New),
                           "c" ((uint32_t)(u64New >> 32)),
                           "m" (*pu64),
                           "0" (u64Old)
                         : "memory" );
#   else /* !PIC */
    __asm__ __volatile__("lock; cmpxchg8b %4\n\t"
                         : "=A" (u64Ret),
                           "=m" (*pu64)
                         : "b" ((uint32_t)u64New),
                           "c" ((uint32_t)(u64New >> 32)),
                           "m" (*pu64),
                           "0" (u64Old));
#   endif
    *pu64Old = u64Ret;
    return u64Ret == u64Old;
#  else
    uint32_t u32Ret;
    __asm
    {
        mov     ebx, dword ptr [u64New]
        mov     ecx, dword ptr [u64New + 4]
        mov     edi, [pu64]
        mov     eax, dword ptr [u64Old]
        mov     edx, dword ptr [u64Old + 4]
        lock cmpxchg8b [edi]
        mov     ebx, [pu64Old]
        mov     [ebx], eax
        setz    al
        movzx   eax, al
        add     ebx, 4
        mov     [ebx], edx
        mov     dword ptr [u32Ret], eax
    }
    return !!u32Ret;
#  endif
# endif /* !RT_ARCH_AMD64 */
}
#endif


/**
 * Atomically Compare and exchange a signed 64-bit value, additionally
 * passing back old value, ordered.
 *
 * @returns true if xchg was done.
 * @returns false if xchg wasn't done.
 *
 * @param   pi64    Pointer to the 64-bit variable to update.
 * @param   i64     The 64-bit value to assign to *pu64.
 * @param   i64Old  The value to compare with.
 * @param   pi64Old Pointer store the old value at.
 */
DECLINLINE(bool) ASMAtomicCmpXchgExS64(volatile int64_t *pi64, const int64_t i64, const int64_t i64Old, int64_t *pi64Old)
{
    return ASMAtomicCmpXchgExU64((volatile uint64_t *)pi64, (uint64_t)i64, (uint64_t)i64Old, (uint64_t *)pi64Old);
}

/** @def ASMAtomicCmpXchgExHandle
 * Atomically Compare and Exchange a typical IPRT handle value, ordered.
 *
 * @param   ph          Pointer to the value to update.
 * @param   hNew        The new value to assigned to *pu.
 * @param   hOld        The old value to *pu compare with.
 * @param   fRc         Where to store the result.
 * @param   phOldVal    Pointer to where to store the old value.
 *
 * @remarks This doesn't currently work for all handles (like RTFILE).
 */
#if HC_ARCH_BITS == 32
# define ASMAtomicCmpXchgExHandle(ph, hNew, hOld, fRc, phOldVal) \
    do { \
        AssertCompile(sizeof(*ph)       == sizeof(uint32_t)); \
        AssertCompile(sizeof(*phOldVal) == sizeof(uint32_t)); \
        (fRc) = ASMAtomicCmpXchgExU32((volatile uint32_t *)(pu), (uint32_t)(uNew), (uint32_t)(uOld), (uint32_t *)(puOldVal)); \
    } while (0)
#elif HC_ARCH_BITS == 64
# define ASMAtomicCmpXchgExHandle(ph, hNew, hOld, fRc, phOldVal) \
    do { \
        AssertCompile(sizeof(*(ph))       == sizeof(uint64_t)); \
        AssertCompile(sizeof(*(phOldVal)) == sizeof(uint64_t)); \
        (fRc) = ASMAtomicCmpXchgExU64((volatile uint64_t *)(pu), (uint64_t)(uNew), (uint64_t)(uOld), (uint64_t *)(puOldVal)); \
    } while (0)
#else
# error HC_ARCH_BITS
#endif


/** @def ASMAtomicCmpXchgExSize
 * Atomically Compare and Exchange a value which size might differ
 * between platforms or compilers. Additionally passes back old value.
 *
 * @param   pu          Pointer to the value to update.
 * @param   uNew        The new value to assigned to *pu.
 * @param   uOld        The old value to *pu compare with.
 * @param   fRc         Where to store the result.
 * @param   puOldVal    Pointer to where to store the old value.
 */
#define ASMAtomicCmpXchgExSize(pu, uNew, uOld, fRc, puOldVal) \
    do { \
        switch (sizeof(*(pu))) { \
            case 4: (fRc) = ASMAtomicCmpXchgExU32((volatile uint32_t *)(void *)(pu), (uint32_t)(uNew), (uint32_t)(uOld), (uint32_t *)(uOldVal)); \
                break; \
            case 8: (fRc) = ASMAtomicCmpXchgExU64((volatile uint64_t *)(void *)(pu), (uint64_t)(uNew), (uint64_t)(uOld), (uint64_t *)(uOldVal)); \
                break; \
            default: AssertMsgFailed(("ASMAtomicCmpXchgSize: size %d is not supported\n", sizeof(*(pu)))); \
                (fRc) = false; \
                (uOldVal) = 0; \
                break; \
        } \
    } while (0)


/**
 * Atomically Compare and Exchange a pointer value, additionally
 * passing back old value, ordered.
 *
 * @returns true if xchg was done.
 * @returns false if xchg wasn't done.
 *
 * @param   ppv         Pointer to the value to update.
 * @param   pvNew       The new value to assigned to *ppv.
 * @param   pvOld       The old value to *ppv compare with.
 * @param   ppvOld      Pointer store the old value at.
 */
DECLINLINE(bool) ASMAtomicCmpXchgExPtrVoid(void * volatile *ppv, const void *pvNew, const void *pvOld, void **ppvOld)
{
#if ARCH_BITS == 32
    return ASMAtomicCmpXchgExU32((volatile uint32_t *)(void *)ppv, (uint32_t)pvNew, (uint32_t)pvOld, (uint32_t *)ppvOld);
#elif ARCH_BITS == 64
    return ASMAtomicCmpXchgExU64((volatile uint64_t *)(void *)ppv, (uint64_t)pvNew, (uint64_t)pvOld, (uint64_t *)ppvOld);
#else
# error "ARCH_BITS is bogus"
#endif
}


/**
 * Atomically Compare and Exchange a pointer value, additionally
 * passing back old value, ordered.
 *
 * @returns true if xchg was done.
 * @returns false if xchg wasn't done.
 *
 * @param   ppv         Pointer to the value to update.
 * @param   pvNew       The new value to assigned to *ppv.
 * @param   pvOld       The old value to *ppv compare with.
 * @param   ppvOld      Pointer store the old value at.
 *
 * @remarks This is relatively type safe on GCC platforms.
 */
#ifdef __GNUC__
# define ASMAtomicCmpXchgExPtr(ppv, pvNew, pvOld, ppvOld) \
    __extension__ \
    ({\
        __typeof__(*(ppv)) volatile * const ppvTypeChecked    = (ppv); \
        __typeof__(*(ppv))            const pvNewTypeChecked  = (pvNew); \
        __typeof__(*(ppv))            const pvOldTypeChecked  = (pvOld); \
        __typeof__(*(ppv)) *          const ppvOldTypeChecked = (ppvOld); \
        bool fMacroRet = ASMAtomicCmpXchgExPtrVoid((void * volatile *)ppvTypeChecked, \
                                                   (void *)pvNewTypeChecked, (void *)pvOldTypeChecked, \
                                                   (void **)ppvOld); \
        fMacroRet; \
     })
#else
# define ASMAtomicCmpXchgExPtr(ppv, pvNew, pvOld, ppvOld) \
    ASMAtomicCmpXchgExPtrVoid((void * volatile *)(ppv), (void *)(pvNew), (void *)pvOld, (void **)ppvOld)
#endif


/**
 * Serialize Instruction.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(void) ASMSerializeInstruction(void);
#else
DECLINLINE(void) ASMSerializeInstruction(void)
{
# if RT_INLINE_ASM_GNU_STYLE
    RTCCUINTREG xAX = 0;
#  ifdef RT_ARCH_AMD64
    __asm__ ("cpuid"
             : "=a" (xAX)
             : "0" (xAX)
             : "rbx", "rcx", "rdx");
#  elif (defined(PIC) || defined(__PIC__)) && defined(__i386__)
    __asm__ ("push  %%ebx\n\t"
             "cpuid\n\t"
             "pop   %%ebx\n\t"
             : "=a" (xAX)
             : "0" (xAX)
             : "ecx", "edx");
#  else
    __asm__ ("cpuid"
             : "=a" (xAX)
             : "0" (xAX)
             : "ebx", "ecx", "edx");
#  endif

# elif RT_INLINE_ASM_USES_INTRIN
    int aInfo[4];
    __cpuid(aInfo, 0);

# else
    __asm
    {
        push    ebx
        xor     eax, eax
        cpuid
        pop     ebx
    }
# endif
}
#endif


/**
 * Memory fence, waits for any pending writes and reads to complete.
 */
DECLINLINE(void) ASMMemoryFence(void)
{
    /** @todo use mfence? check if all cpus we care for support it. */
    uint32_t volatile u32;
    ASMAtomicXchgU32(&u32, 0);
}


/**
 * Write fence, waits for any pending writes to complete.
 */
DECLINLINE(void) ASMWriteFence(void)
{
    /** @todo use sfence? check if all cpus we care for support it. */
    ASMMemoryFence();
}


/**
 * Read fence, waits for any pending reads to complete.
 */
DECLINLINE(void) ASMReadFence(void)
{
    /** @todo use lfence? check if all cpus we care for support it. */
    ASMMemoryFence();
}


/**
 * Atomically reads an unsigned 8-bit value, ordered.
 *
 * @returns Current *pu8 value
 * @param   pu8    Pointer to the 8-bit variable to read.
 */
DECLINLINE(uint8_t) ASMAtomicReadU8(volatile uint8_t *pu8)
{
    ASMMemoryFence();
    return *pu8;    /* byte reads are atomic on x86 */
}


/**
 * Atomically reads an unsigned 8-bit value, unordered.
 *
 * @returns Current *pu8 value
 * @param   pu8    Pointer to the 8-bit variable to read.
 */
DECLINLINE(uint8_t) ASMAtomicUoReadU8(volatile uint8_t *pu8)
{
    return *pu8;    /* byte reads are atomic on x86 */
}


/**
 * Atomically reads a signed 8-bit value, ordered.
 *
 * @returns Current *pi8 value
 * @param   pi8    Pointer to the 8-bit variable to read.
 */
DECLINLINE(int8_t) ASMAtomicReadS8(volatile int8_t *pi8)
{
    ASMMemoryFence();
    return *pi8;    /* byte reads are atomic on x86 */
}


/**
 * Atomically reads a signed 8-bit value, unordered.
 *
 * @returns Current *pi8 value
 * @param   pi8    Pointer to the 8-bit variable to read.
 */
DECLINLINE(int8_t) ASMAtomicUoReadS8(volatile int8_t *pi8)
{
    return *pi8;    /* byte reads are atomic on x86 */
}


/**
 * Atomically reads an unsigned 16-bit value, ordered.
 *
 * @returns Current *pu16 value
 * @param   pu16    Pointer to the 16-bit variable to read.
 */
DECLINLINE(uint16_t) ASMAtomicReadU16(volatile uint16_t *pu16)
{
    ASMMemoryFence();
    Assert(!((uintptr_t)pu16 & 1));
    return *pu16;
}


/**
 * Atomically reads an unsigned 16-bit value, unordered.
 *
 * @returns Current *pu16 value
 * @param   pu16    Pointer to the 16-bit variable to read.
 */
DECLINLINE(uint16_t) ASMAtomicUoReadU16(volatile uint16_t *pu16)
{
    Assert(!((uintptr_t)pu16 & 1));
    return *pu16;
}


/**
 * Atomically reads a signed 16-bit value, ordered.
 *
 * @returns Current *pi16 value
 * @param   pi16    Pointer to the 16-bit variable to read.
 */
DECLINLINE(int16_t) ASMAtomicReadS16(volatile int16_t *pi16)
{
    ASMMemoryFence();
    Assert(!((uintptr_t)pi16 & 1));
    return *pi16;
}


/**
 * Atomically reads a signed 16-bit value, unordered.
 *
 * @returns Current *pi16 value
 * @param   pi16    Pointer to the 16-bit variable to read.
 */
DECLINLINE(int16_t) ASMAtomicUoReadS16(volatile int16_t *pi16)
{
    Assert(!((uintptr_t)pi16 & 1));
    return *pi16;
}


/**
 * Atomically reads an unsigned 32-bit value, ordered.
 *
 * @returns Current *pu32 value
 * @param   pu32    Pointer to the 32-bit variable to read.
 */
DECLINLINE(uint32_t) ASMAtomicReadU32(volatile uint32_t *pu32)
{
    ASMMemoryFence();
    Assert(!((uintptr_t)pu32 & 3));
    return *pu32;
}


/**
 * Atomically reads an unsigned 32-bit value, unordered.
 *
 * @returns Current *pu32 value
 * @param   pu32    Pointer to the 32-bit variable to read.
 */
DECLINLINE(uint32_t) ASMAtomicUoReadU32(volatile uint32_t *pu32)
{
    Assert(!((uintptr_t)pu32 & 3));
    return *pu32;
}


/**
 * Atomically reads a signed 32-bit value, ordered.
 *
 * @returns Current *pi32 value
 * @param   pi32    Pointer to the 32-bit variable to read.
 */
DECLINLINE(int32_t) ASMAtomicReadS32(volatile int32_t *pi32)
{
    ASMMemoryFence();
    Assert(!((uintptr_t)pi32 & 3));
    return *pi32;
}


/**
 * Atomically reads a signed 32-bit value, unordered.
 *
 * @returns Current *pi32 value
 * @param   pi32    Pointer to the 32-bit variable to read.
 */
DECLINLINE(int32_t) ASMAtomicUoReadS32(volatile int32_t *pi32)
{
    Assert(!((uintptr_t)pi32 & 3));
    return *pi32;
}


/**
 * Atomically reads an unsigned 64-bit value, ordered.
 *
 * @returns Current *pu64 value
 * @param   pu64    Pointer to the 64-bit variable to read.
 *                  The memory pointed to must be writable.
 * @remark  This will fault if the memory is read-only!
 */
#if (RT_INLINE_ASM_EXTERNAL && !defined(RT_ARCH_AMD64)) \
 || RT_INLINE_DONT_MIX_CMPXCHG8B_AND_PIC
DECLASM(uint64_t) ASMAtomicReadU64(volatile uint64_t *pu64);
#else
DECLINLINE(uint64_t) ASMAtomicReadU64(volatile uint64_t *pu64)
{
    uint64_t u64;
# ifdef RT_ARCH_AMD64
    Assert(!((uintptr_t)pu64 & 7));
/*#  if RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__(  "mfence\n\t"
                           "movq %1, %0\n\t"
                         : "=r" (u64)
                         : "m" (*pu64));
#  else
    __asm
    {
        mfence
        mov     rdx, [pu64]
        mov     rax, [rdx]
        mov     [u64], rax
    }
#  endif*/
    ASMMemoryFence();
    u64 = *pu64;
# else /* !RT_ARCH_AMD64 */
#  if RT_INLINE_ASM_GNU_STYLE
#   if defined(PIC) || defined(__PIC__)
    uint32_t u32EBX = 0;
    Assert(!((uintptr_t)pu64 & 7));
    __asm__ __volatile__("xchgl %%ebx, %3\n\t"
                         "lock; cmpxchg8b (%5)\n\t"
                         "movl %3, %%ebx\n\t"
                         : "=A" (u64),
#    if (__GNUC__ * 100 + __GNUC_MINOR__) >= 403
                           "+m" (*pu64)
#    else
                           "=m" (*pu64)
#    endif
                         : "0" (0),
                           "m" (u32EBX),
                           "c" (0),
                           "S" (pu64));
#   else /* !PIC */
    __asm__ __volatile__("lock; cmpxchg8b %1\n\t"
                         : "=A" (u64),
                           "+m" (*pu64)
                         : "0" (0),
                           "b" (0),
                           "c" (0));
#   endif
#  else
    Assert(!((uintptr_t)pu64 & 7));
    __asm
    {
        xor     eax, eax
        xor     edx, edx
        mov     edi, pu64
        xor     ecx, ecx
        xor     ebx, ebx
        lock cmpxchg8b [edi]
        mov     dword ptr [u64], eax
        mov     dword ptr [u64 + 4], edx
    }
#  endif
# endif /* !RT_ARCH_AMD64 */
    return u64;
}
#endif


/**
 * Atomically reads an unsigned 64-bit value, unordered.
 *
 * @returns Current *pu64 value
 * @param   pu64    Pointer to the 64-bit variable to read.
 *                  The memory pointed to must be writable.
 * @remark  This will fault if the memory is read-only!
 */
#if (RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN) \
 || RT_INLINE_DONT_MIX_CMPXCHG8B_AND_PIC
DECLASM(uint64_t) ASMAtomicUoReadU64(volatile uint64_t *pu64);
#else
DECLINLINE(uint64_t) ASMAtomicUoReadU64(volatile uint64_t *pu64)
{
    uint64_t u64;
# ifdef RT_ARCH_AMD64
    Assert(!((uintptr_t)pu64 & 7));
/*#  if RT_INLINE_ASM_GNU_STYLE
    Assert(!((uintptr_t)pu64 & 7));
    __asm__ __volatile__("movq %1, %0\n\t"
                         : "=r" (u64)
                         : "m" (*pu64));
#  else
    __asm
    {
        mov     rdx, [pu64]
        mov     rax, [rdx]
        mov     [u64], rax
    }
#  endif */
    u64 = *pu64;
# else /* !RT_ARCH_AMD64 */
#  if RT_INLINE_ASM_GNU_STYLE
#   if defined(PIC) || defined(__PIC__)
    uint32_t u32EBX = 0;
    uint32_t u32Spill;
    Assert(!((uintptr_t)pu64 & 7));
    __asm__ __volatile__("xor   %%eax,%%eax\n\t"
                         "xor   %%ecx,%%ecx\n\t"
                         "xor   %%edx,%%edx\n\t"
                         "xchgl %%ebx, %3\n\t"
                         "lock; cmpxchg8b (%4)\n\t"
                         "movl %3, %%ebx\n\t"
                         : "=A" (u64),
#    if (__GNUC__ * 100 + __GNUC_MINOR__) >= 403
                           "+m" (*pu64),
#    else
                           "=m" (*pu64),
#    endif
                           "=c" (u32Spill)
                         : "m" (u32EBX),
                           "S" (pu64));
#   else /* !PIC */
    __asm__ __volatile__("lock; cmpxchg8b %1\n\t"
                         : "=A" (u64),
                           "+m" (*pu64)
                         : "0" (0),
                           "b" (0),
                           "c" (0));
#   endif
#  else
    Assert(!((uintptr_t)pu64 & 7));
    __asm
    {
        xor     eax, eax
        xor     edx, edx
        mov     edi, pu64
        xor     ecx, ecx
        xor     ebx, ebx
        lock cmpxchg8b [edi]
        mov     dword ptr [u64], eax
        mov     dword ptr [u64 + 4], edx
    }
#  endif
# endif /* !RT_ARCH_AMD64 */
    return u64;
}
#endif


/**
 * Atomically reads a signed 64-bit value, ordered.
 *
 * @returns Current *pi64 value
 * @param   pi64    Pointer to the 64-bit variable to read.
 *                  The memory pointed to must be writable.
 * @remark  This will fault if the memory is read-only!
 */
DECLINLINE(int64_t) ASMAtomicReadS64(volatile int64_t *pi64)
{
    return (int64_t)ASMAtomicReadU64((volatile uint64_t *)pi64);
}


/**
 * Atomically reads a signed 64-bit value, unordered.
 *
 * @returns Current *pi64 value
 * @param   pi64    Pointer to the 64-bit variable to read.
 *                  The memory pointed to must be writable.
 * @remark  This will fault if the memory is read-only!
 */
DECLINLINE(int64_t) ASMAtomicUoReadS64(volatile int64_t *pi64)
{
    return (int64_t)ASMAtomicUoReadU64((volatile uint64_t *)pi64);
}


/**
 * Atomically reads a pointer value, ordered.
 *
 * @returns Current *pv value
 * @param   ppv     Pointer to the pointer variable to read.
 *
 * @remarks Please use ASMAtomicReadPtrT, it provides better type safety and
 *          requires less typing (no casts).
 */
DECLINLINE(void *) ASMAtomicReadPtr(void * volatile *ppv)
{
#if ARCH_BITS == 32
    return (void *)ASMAtomicReadU32((volatile uint32_t *)(void *)ppv);
#elif ARCH_BITS == 64
    return (void *)ASMAtomicReadU64((volatile uint64_t *)(void *)ppv);
#else
# error "ARCH_BITS is bogus"
#endif
}

/**
 * Convenience macro for avoiding the annoying casting with ASMAtomicReadPtr.
 *
 * @returns Current *pv value
 * @param   ppv     Pointer to the pointer variable to read.
 * @param   Type    The type of *ppv, sans volatile.
 */
#ifdef __GNUC__
# define ASMAtomicReadPtrT(ppv, Type) \
    __extension__ \
    ({\
        __typeof__(*(ppv)) volatile *ppvTypeChecked = (ppv); \
        Type pvTypeChecked = (__typeof__(*(ppv))) ASMAtomicReadPtr((void * volatile *)ppvTypeChecked); \
        pvTypeChecked; \
     })
#else
# define ASMAtomicReadPtrT(ppv, Type) \
    (Type)ASMAtomicReadPtr((void * volatile *)(ppv))
#endif


/**
 * Atomically reads a pointer value, unordered.
 *
 * @returns Current *pv value
 * @param   ppv     Pointer to the pointer variable to read.
 *
 * @remarks Please use ASMAtomicUoReadPtrT, it provides better type safety and
 *          requires less typing (no casts).
 */
DECLINLINE(void *) ASMAtomicUoReadPtr(void * volatile *ppv)
{
#if ARCH_BITS == 32
    return (void *)ASMAtomicUoReadU32((volatile uint32_t *)(void *)ppv);
#elif ARCH_BITS == 64
    return (void *)ASMAtomicUoReadU64((volatile uint64_t *)(void *)ppv);
#else
# error "ARCH_BITS is bogus"
#endif
}


/**
 * Convenience macro for avoiding the annoying casting with ASMAtomicUoReadPtr.
 *
 * @returns Current *pv value
 * @param   ppv     Pointer to the pointer variable to read.
 * @param   Type    The type of *ppv, sans volatile.
 */
#ifdef __GNUC__
# define ASMAtomicUoReadPtrT(ppv, Type) \
    __extension__ \
    ({\
        __typeof__(*(ppv)) volatile * const ppvTypeChecked = (ppv); \
        Type pvTypeChecked = (__typeof__(*(ppv))) ASMAtomicUoReadPtr((void * volatile *)ppvTypeChecked); \
        pvTypeChecked; \
     })
#else
# define ASMAtomicUoReadPtrT(ppv, Type) \
    (Type)ASMAtomicUoReadPtr((void * volatile *)(ppv))
#endif


/**
 * Atomically reads a boolean value, ordered.
 *
 * @returns Current *pf value
 * @param   pf      Pointer to the boolean variable to read.
 */
DECLINLINE(bool) ASMAtomicReadBool(volatile bool *pf)
{
    ASMMemoryFence();
    return *pf;     /* byte reads are atomic on x86 */
}


/**
 * Atomically reads a boolean value, unordered.
 *
 * @returns Current *pf value
 * @param   pf      Pointer to the boolean variable to read.
 */
DECLINLINE(bool) ASMAtomicUoReadBool(volatile bool *pf)
{
    return *pf;     /* byte reads are atomic on x86 */
}


/**
 * Atomically read a typical IPRT handle value, ordered.
 *
 * @param   ph      Pointer to the handle variable to read.
 * @param   phRes   Where to store the result.
 *
 * @remarks This doesn't currently work for all handles (like RTFILE).
 */
#if HC_ARCH_BITS == 32
# define ASMAtomicReadHandle(ph, phRes) \
    do { \
        AssertCompile(sizeof(*(ph))    == sizeof(uint32_t)); \
        AssertCompile(sizeof(*(phRes)) == sizeof(uint32_t)); \
        *(uint32_t *)(phRes) = ASMAtomicReadU32((uint32_t volatile *)(ph)); \
    } while (0)
#elif HC_ARCH_BITS == 64
# define ASMAtomicReadHandle(ph, phRes) \
    do { \
        AssertCompile(sizeof(*(ph))    == sizeof(uint64_t)); \
        AssertCompile(sizeof(*(phRes)) == sizeof(uint64_t)); \
        *(uint64_t *)(phRes) = ASMAtomicReadU64((uint64_t volatile *)(ph)); \
    } while (0)
#else
# error HC_ARCH_BITS
#endif


/**
 * Atomically read a typical IPRT handle value, unordered.
 *
 * @param   ph      Pointer to the handle variable to read.
 * @param   phRes   Where to store the result.
 *
 * @remarks This doesn't currently work for all handles (like RTFILE).
 */
#if HC_ARCH_BITS == 32
# define ASMAtomicUoReadHandle(ph, phRes) \
    do { \
        AssertCompile(sizeof(*(ph))    == sizeof(uint32_t)); \
        AssertCompile(sizeof(*(phRes)) == sizeof(uint32_t)); \
        *(uint32_t *)(phRes) = ASMAtomicUoReadU32((uint32_t volatile *)(ph)); \
    } while (0)
#elif HC_ARCH_BITS == 64
# define ASMAtomicUoReadHandle(ph, phRes) \
    do { \
        AssertCompile(sizeof(*(ph))    == sizeof(uint64_t)); \
        AssertCompile(sizeof(*(phRes)) == sizeof(uint64_t)); \
        *(uint64_t *)(phRes) = ASMAtomicUoReadU64((uint64_t volatile *)(ph)); \
    } while (0)
#else
# error HC_ARCH_BITS
#endif


/**
 * Atomically read a value which size might differ
 * between platforms or compilers, ordered.
 *
 * @param   pu      Pointer to the variable to update.
 * @param   puRes   Where to store the result.
 */
#define ASMAtomicReadSize(pu, puRes) \
    do { \
        switch (sizeof(*(pu))) { \
            case 1: *(uint8_t  *)(puRes) = ASMAtomicReadU8( (volatile uint8_t  *)(void *)(pu)); break; \
            case 2: *(uint16_t *)(puRes) = ASMAtomicReadU16((volatile uint16_t *)(void *)(pu)); break; \
            case 4: *(uint32_t *)(puRes) = ASMAtomicReadU32((volatile uint32_t *)(void *)(pu)); break; \
            case 8: *(uint64_t *)(puRes) = ASMAtomicReadU64((volatile uint64_t *)(void *)(pu)); break; \
            default: AssertMsgFailed(("ASMAtomicReadSize: size %d is not supported\n", sizeof(*(pu)))); \
        } \
    } while (0)


/**
 * Atomically read a value which size might differ
 * between platforms or compilers, unordered.
 *
 * @param   pu      Pointer to the variable to read.
 * @param   puRes   Where to store the result.
 */
#define ASMAtomicUoReadSize(pu, puRes) \
    do { \
        switch (sizeof(*(pu))) { \
            case 1: *(uint8_t  *)(puRes) = ASMAtomicUoReadU8( (volatile uint8_t  *)(void *)(pu)); break; \
            case 2: *(uint16_t *)(puRes) = ASMAtomicUoReadU16((volatile uint16_t *)(void *)(pu)); break; \
            case 4: *(uint32_t *)(puRes) = ASMAtomicUoReadU32((volatile uint32_t *)(void *)(pu)); break; \
            case 8: *(uint64_t *)(puRes) = ASMAtomicUoReadU64((volatile uint64_t *)(void *)(pu)); break; \
            default: AssertMsgFailed(("ASMAtomicReadSize: size %d is not supported\n", sizeof(*(pu)))); \
        } \
    } while (0)


/**
 * Atomically writes an unsigned 8-bit value, ordered.
 *
 * @param   pu8     Pointer to the 8-bit variable.
 * @param   u8      The 8-bit value to assign to *pu8.
 */
DECLINLINE(void) ASMAtomicWriteU8(volatile uint8_t *pu8, uint8_t u8)
{
    ASMAtomicXchgU8(pu8, u8);
}


/**
 * Atomically writes an unsigned 8-bit value, unordered.
 *
 * @param   pu8     Pointer to the 8-bit variable.
 * @param   u8      The 8-bit value to assign to *pu8.
 */
DECLINLINE(void) ASMAtomicUoWriteU8(volatile uint8_t *pu8, uint8_t u8)
{
    *pu8 = u8;      /* byte writes are atomic on x86 */
}


/**
 * Atomically writes a signed 8-bit value, ordered.
 *
 * @param   pi8     Pointer to the 8-bit variable to read.
 * @param   i8      The 8-bit value to assign to *pi8.
 */
DECLINLINE(void) ASMAtomicWriteS8(volatile int8_t *pi8, int8_t i8)
{
    ASMAtomicXchgS8(pi8, i8);
}


/**
 * Atomically writes a signed 8-bit value, unordered.
 *
 * @param   pi8     Pointer to the 8-bit variable to read.
 * @param   i8      The 8-bit value to assign to *pi8.
 */
DECLINLINE(void) ASMAtomicUoWriteS8(volatile int8_t *pi8, int8_t i8)
{
    *pi8 = i8;      /* byte writes are atomic on x86 */
}


/**
 * Atomically writes an unsigned 16-bit value, ordered.
 *
 * @param   pu16    Pointer to the 16-bit variable.
 * @param   u16     The 16-bit value to assign to *pu16.
 */
DECLINLINE(void) ASMAtomicWriteU16(volatile uint16_t *pu16, uint16_t u16)
{
    ASMAtomicXchgU16(pu16, u16);
}


/**
 * Atomically writes an unsigned 16-bit value, unordered.
 *
 * @param   pu16    Pointer to the 16-bit variable.
 * @param   u16     The 16-bit value to assign to *pu16.
 */
DECLINLINE(void) ASMAtomicUoWriteU16(volatile uint16_t *pu16, uint16_t u16)
{
    Assert(!((uintptr_t)pu16 & 1));
    *pu16 = u16;
}


/**
 * Atomically writes a signed 16-bit value, ordered.
 *
 * @param   pi16    Pointer to the 16-bit variable to read.
 * @param   i16     The 16-bit value to assign to *pi16.
 */
DECLINLINE(void) ASMAtomicWriteS16(volatile int16_t *pi16, int16_t i16)
{
    ASMAtomicXchgS16(pi16, i16);
}


/**
 * Atomically writes a signed 16-bit value, unordered.
 *
 * @param   pi16    Pointer to the 16-bit variable to read.
 * @param   i16     The 16-bit value to assign to *pi16.
 */
DECLINLINE(void) ASMAtomicUoWriteS16(volatile int16_t *pi16, int16_t i16)
{
    Assert(!((uintptr_t)pi16 & 1));
    *pi16 = i16;
}


/**
 * Atomically writes an unsigned 32-bit value, ordered.
 *
 * @param   pu32    Pointer to the 32-bit variable.
 * @param   u32     The 32-bit value to assign to *pu32.
 */
DECLINLINE(void) ASMAtomicWriteU32(volatile uint32_t *pu32, uint32_t u32)
{
    ASMAtomicXchgU32(pu32, u32);
}


/**
 * Atomically writes an unsigned 32-bit value, unordered.
 *
 * @param   pu32    Pointer to the 32-bit variable.
 * @param   u32     The 32-bit value to assign to *pu32.
 */
DECLINLINE(void) ASMAtomicUoWriteU32(volatile uint32_t *pu32, uint32_t u32)
{
    Assert(!((uintptr_t)pu32 & 3));
    *pu32 = u32;
}


/**
 * Atomically writes a signed 32-bit value, ordered.
 *
 * @param   pi32    Pointer to the 32-bit variable to read.
 * @param   i32     The 32-bit value to assign to *pi32.
 */
DECLINLINE(void) ASMAtomicWriteS32(volatile int32_t *pi32, int32_t i32)
{
    ASMAtomicXchgS32(pi32, i32);
}


/**
 * Atomically writes a signed 32-bit value, unordered.
 *
 * @param   pi32    Pointer to the 32-bit variable to read.
 * @param   i32     The 32-bit value to assign to *pi32.
 */
DECLINLINE(void) ASMAtomicUoWriteS32(volatile int32_t *pi32, int32_t i32)
{
    Assert(!((uintptr_t)pi32 & 3));
    *pi32 = i32;
}


/**
 * Atomically writes an unsigned 64-bit value, ordered.
 *
 * @param   pu64    Pointer to the 64-bit variable.
 * @param   u64     The 64-bit value to assign to *pu64.
 */
DECLINLINE(void) ASMAtomicWriteU64(volatile uint64_t *pu64, uint64_t u64)
{
    ASMAtomicXchgU64(pu64, u64);
}


/**
 * Atomically writes an unsigned 64-bit value, unordered.
 *
 * @param   pu64    Pointer to the 64-bit variable.
 * @param   u64     The 64-bit value to assign to *pu64.
 */
DECLINLINE(void) ASMAtomicUoWriteU64(volatile uint64_t *pu64, uint64_t u64)
{
    Assert(!((uintptr_t)pu64 & 7));
#if ARCH_BITS == 64
    *pu64 = u64;
#else
    ASMAtomicXchgU64(pu64, u64);
#endif
}


/**
 * Atomically writes a signed 64-bit value, ordered.
 *
 * @param   pi64    Pointer to the 64-bit variable.
 * @param   i64     The 64-bit value to assign to *pi64.
 */
DECLINLINE(void) ASMAtomicWriteS64(volatile int64_t *pi64, int64_t i64)
{
    ASMAtomicXchgS64(pi64, i64);
}


/**
 * Atomically writes a signed 64-bit value, unordered.
 *
 * @param   pi64    Pointer to the 64-bit variable.
 * @param   i64     The 64-bit value to assign to *pi64.
 */
DECLINLINE(void) ASMAtomicUoWriteS64(volatile int64_t *pi64, int64_t i64)
{
    Assert(!((uintptr_t)pi64 & 7));
#if ARCH_BITS == 64
    *pi64 = i64;
#else
    ASMAtomicXchgS64(pi64, i64);
#endif
}


/**
 * Atomically writes a boolean value, unordered.
 *
 * @param   pf      Pointer to the boolean variable.
 * @param   f       The boolean value to assign to *pf.
 */
DECLINLINE(void) ASMAtomicWriteBool(volatile bool *pf, bool f)
{
    ASMAtomicWriteU8((uint8_t volatile *)pf, f);
}


/**
 * Atomically writes a boolean value, unordered.
 *
 * @param   pf      Pointer to the boolean variable.
 * @param   f       The boolean value to assign to *pf.
 */
DECLINLINE(void) ASMAtomicUoWriteBool(volatile bool *pf, bool f)
{
    *pf = f;    /* byte writes are atomic on x86 */
}


/**
 * Atomically writes a pointer value, ordered.
 *
 * @param   ppv     Pointer to the pointer variable.
 * @param   pv      The pointer value to assign to *ppv.
 */
DECLINLINE(void) ASMAtomicWritePtrVoid(void * volatile *ppv, const void *pv)
{
#if ARCH_BITS == 32
    ASMAtomicWriteU32((volatile uint32_t *)(void *)ppv, (uint32_t)pv);
#elif ARCH_BITS == 64
    ASMAtomicWriteU64((volatile uint64_t *)(void *)ppv, (uint64_t)pv);
#else
# error "ARCH_BITS is bogus"
#endif
}


/**
 * Atomically writes a pointer value, ordered.
 *
 * @param   ppv     Pointer to the pointer variable.
 * @param   pv      The pointer value to assign to *ppv. If NULL use
 *                  ASMAtomicWriteNullPtr or you'll land in trouble.
 *
 * @remarks This is relatively type safe on GCC platforms when @a pv isn't
 *          NULL.
 */
#ifdef __GNUC__
# define ASMAtomicWritePtr(ppv, pv) \
    do \
    { \
        __typeof__(*(ppv)) volatile * const ppvTypeChecked = (ppv); \
        __typeof__(*(ppv))            const pvTypeChecked  = (pv); \
        \
        AssertCompile(sizeof(*ppv) == sizeof(void *)); \
        AssertCompile(sizeof(pv) == sizeof(void *)); \
        Assert(!( (uintptr_t)ppv & ((ARCH_BITS / 8) - 1) )); \
        \
        ASMAtomicWritePtrVoid((void * volatile *)(ppvTypeChecked), (void *)(pvTypeChecked)); \
    } while (0)
#else
# define ASMAtomicWritePtr(ppv, pv) \
    do \
    { \
        AssertCompile(sizeof(*ppv) == sizeof(void *)); \
        AssertCompile(sizeof(pv) == sizeof(void *)); \
        Assert(!( (uintptr_t)ppv & ((ARCH_BITS / 8) - 1) )); \
        \
        ASMAtomicWritePtrVoid((void * volatile *)(ppv), (void *)(pv)); \
    } while (0)
#endif


/**
 * Atomically sets a pointer to NULL, ordered.
 *
 * @param   ppv     Pointer to the pointer variable that should be set to NULL.
 *
 * @remarks This is relatively type safe on GCC platforms.
 */
#ifdef __GNUC__
# define ASMAtomicWriteNullPtr(ppv) \
    do \
    { \
        __typeof__(*(ppv)) volatile * const ppvTypeChecked = (ppv); \
        AssertCompile(sizeof(*ppv) == sizeof(void *)); \
        Assert(!( (uintptr_t)ppv & ((ARCH_BITS / 8) - 1) )); \
        ASMAtomicWritePtrVoid((void * volatile *)(ppvTypeChecked), NULL); \
    } while (0)
#else
# define ASMAtomicWriteNullPtr(ppv) \
    do \
    { \
        AssertCompile(sizeof(*ppv) == sizeof(void *)); \
        Assert(!( (uintptr_t)ppv & ((ARCH_BITS / 8) - 1) )); \
        ASMAtomicWritePtrVoid((void * volatile *)(ppv), NULL); \
    } while (0)
#endif


/**
 * Atomically writes a pointer value, unordered.
 *
 * @returns Current *pv value
 * @param   ppv     Pointer to the pointer variable.
 * @param   pv      The pointer value to assign to *ppv. If NULL use
 *                  ASMAtomicUoWriteNullPtr or you'll land in trouble.
 *
 * @remarks This is relatively type safe on GCC platforms when @a pv isn't
 *          NULL.
 */
#ifdef __GNUC__
# define ASMAtomicUoWritePtr(ppv, pv) \
    do \
    { \
        __typeof__(*(ppv)) volatile * const ppvTypeChecked = (ppv); \
        __typeof__(*(ppv))            const pvTypeChecked  = (pv); \
        \
        AssertCompile(sizeof(*ppv) == sizeof(void *)); \
        AssertCompile(sizeof(pv) == sizeof(void *)); \
        Assert(!( (uintptr_t)ppv & ((ARCH_BITS / 8) - 1) )); \
        \
        *(ppvTypeChecked) = pvTypeChecked; \
    } while (0)
#else
# define ASMAtomicUoWritePtr(ppv, pv) \
    do \
    { \
        AssertCompile(sizeof(*ppv) == sizeof(void *)); \
        AssertCompile(sizeof(pv) == sizeof(void *)); \
        Assert(!( (uintptr_t)ppv & ((ARCH_BITS / 8) - 1) )); \
        *(ppv) = pv; \
    } while (0)
#endif


/**
 * Atomically sets a pointer to NULL, unordered.
 *
 * @param   ppv     Pointer to the pointer variable that should be set to NULL.
 *
 * @remarks This is relatively type safe on GCC platforms.
 */
#ifdef __GNUC__
# define ASMAtomicUoWriteNullPtr(ppv) \
    do \
    { \
        __typeof__(*(ppv)) volatile * const ppvTypeChecked = (ppv); \
        AssertCompile(sizeof(*ppv) == sizeof(void *)); \
        Assert(!( (uintptr_t)ppv & ((ARCH_BITS / 8) - 1) )); \
        *(ppvTypeChecked) = NULL; \
    } while (0)
#else
# define ASMAtomicUoWriteNullPtr(ppv) \
    do \
    { \
        AssertCompile(sizeof(*ppv) == sizeof(void *)); \
        Assert(!( (uintptr_t)ppv & ((ARCH_BITS / 8) - 1) )); \
        *(ppv) = NULL; \
    } while (0)
#endif


/**
 * Atomically write a typical IPRT handle value, ordered.
 *
 * @param   ph      Pointer to the variable to update.
 * @param   hNew    The value to assign to *ph.
 *
 * @remarks This doesn't currently work for all handles (like RTFILE).
 */
#if HC_ARCH_BITS == 32
# define ASMAtomicWriteHandle(ph, hNew) \
    do { \
        AssertCompile(sizeof(*(ph)) == sizeof(uint32_t)); \
        ASMAtomicWriteU32((uint32_t volatile *)(ph), (const uint32_t)(hNew)); \
    } while (0)
#elif HC_ARCH_BITS == 64
# define ASMAtomicWriteHandle(ph, hNew) \
    do { \
        AssertCompile(sizeof(*(ph)) == sizeof(uint64_t)); \
        ASMAtomicWriteU64((uint64_t volatile *)(ph), (const uint64_t)(hNew)); \
    } while (0)
#else
# error HC_ARCH_BITS
#endif


/**
 * Atomically write a typical IPRT handle value, unordered.
 *
 * @param   ph      Pointer to the variable to update.
 * @param   hNew    The value to assign to *ph.
 *
 * @remarks This doesn't currently work for all handles (like RTFILE).
 */
#if HC_ARCH_BITS == 32
# define ASMAtomicUoWriteHandle(ph, hNew) \
    do { \
        AssertCompile(sizeof(*(ph)) == sizeof(uint32_t)); \
        ASMAtomicUoWriteU32((uint32_t volatile *)(ph), (const uint32_t)hNew); \
    } while (0)
#elif HC_ARCH_BITS == 64
# define ASMAtomicUoWriteHandle(ph, hNew) \
    do { \
        AssertCompile(sizeof(*(ph)) == sizeof(uint64_t)); \
        ASMAtomicUoWriteU64((uint64_t volatile *)(ph), (const uint64_t)hNew); \
    } while (0)
#else
# error HC_ARCH_BITS
#endif


/**
 * Atomically write a value which size might differ
 * between platforms or compilers, ordered.
 *
 * @param   pu      Pointer to the variable to update.
 * @param   uNew    The value to assign to *pu.
 */
#define ASMAtomicWriteSize(pu, uNew) \
    do { \
        switch (sizeof(*(pu))) { \
            case 1: ASMAtomicWriteU8( (volatile uint8_t  *)(void *)(pu), (uint8_t )(uNew)); break; \
            case 2: ASMAtomicWriteU16((volatile uint16_t *)(void *)(pu), (uint16_t)(uNew)); break; \
            case 4: ASMAtomicWriteU32((volatile uint32_t *)(void *)(pu), (uint32_t)(uNew)); break; \
            case 8: ASMAtomicWriteU64((volatile uint64_t *)(void *)(pu), (uint64_t)(uNew)); break; \
            default: AssertMsgFailed(("ASMAtomicWriteSize: size %d is not supported\n", sizeof(*(pu)))); \
        } \
    } while (0)

/**
 * Atomically write a value which size might differ
 * between platforms or compilers, unordered.
 *
 * @param   pu      Pointer to the variable to update.
 * @param   uNew    The value to assign to *pu.
 */
#define ASMAtomicUoWriteSize(pu, uNew) \
    do { \
        switch (sizeof(*(pu))) { \
            case 1: ASMAtomicUoWriteU8( (volatile uint8_t  *)(void *)(pu), (uint8_t )(uNew)); break; \
            case 2: ASMAtomicUoWriteU16((volatile uint16_t *)(void *)(pu), (uint16_t)(uNew)); break; \
            case 4: ASMAtomicUoWriteU32((volatile uint32_t *)(void *)(pu), (uint32_t)(uNew)); break; \
            case 8: ASMAtomicUoWriteU64((volatile uint64_t *)(void *)(pu), (uint64_t)(uNew)); break; \
            default: AssertMsgFailed(("ASMAtomicWriteSize: size %d is not supported\n", sizeof(*(pu)))); \
        } \
    } while (0)



/**
 * Atomically exchanges and adds to a 32-bit value, ordered.
 *
 * @returns The old value.
 * @param   pu32        Pointer to the value.
 * @param   u32         Number to add.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(uint32_t) ASMAtomicAddU32(uint32_t volatile *pu32, uint32_t u32);
#else
DECLINLINE(uint32_t) ASMAtomicAddU32(uint32_t volatile *pu32, uint32_t u32)
{
# if RT_INLINE_ASM_USES_INTRIN
    u32 = _InterlockedExchangeAdd((long *)pu32, u32);
    return u32;

# elif RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__("lock; xaddl %0, %1\n\t"
                         : "=r" (u32),
                           "=m" (*pu32)
                         : "0" (u32),
                           "m" (*pu32)
                         : "memory");
    return u32;
# else
    __asm
    {
        mov     eax, [u32]
#  ifdef RT_ARCH_AMD64
        mov     rdx, [pu32]
        lock xadd [rdx], eax
#  else
        mov     edx, [pu32]
        lock xadd [edx], eax
#  endif
        mov     [u32], eax
    }
    return u32;
# endif
}
#endif


/**
 * Atomically exchanges and adds to a signed 32-bit value, ordered.
 *
 * @returns The old value.
 * @param   pi32        Pointer to the value.
 * @param   i32         Number to add.
 */
DECLINLINE(int32_t) ASMAtomicAddS32(int32_t volatile *pi32, int32_t i32)
{
    return (int32_t)ASMAtomicAddU32((uint32_t volatile *)pi32, (uint32_t)i32);
}


/**
 * Atomically exchanges and adds to a 64-bit value, ordered.
 *
 * @returns The old value.
 * @param   pu64        Pointer to the value.
 * @param   u64         Number to add.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(uint64_t) ASMAtomicAddU64(uint64_t volatile *pu64, uint64_t u64);
#else
DECLINLINE(uint64_t) ASMAtomicAddU64(uint64_t volatile *pu64, uint64_t u64)
{
# if RT_INLINE_ASM_USES_INTRIN && defined(RT_ARCH_AMD64)
    u64 = _InterlockedExchangeAdd64((__int64 *)pu64, u64);
    return u64;

# elif RT_INLINE_ASM_GNU_STYLE && defined(RT_ARCH_AMD64)
    __asm__ __volatile__("lock; xaddq %0, %1\n\t"
                         : "=r" (u64),
                           "=m" (*pu64)
                         : "0" (u64),
                           "m" (*pu64)
                         : "memory");
    return u64;
# else
    uint64_t u64New;
    for (;;)
    {
        uint64_t u64Old = ASMAtomicUoReadU64(pu64);
        u64New = u64Old + u64;
        if (ASMAtomicCmpXchgU64(pu64, u64New, u64Old))
            break;
        ASMNopPause();
    }
    return u64New;
# endif
}
#endif


/**
 * Atomically exchanges and adds to a signed 64-bit value, ordered.
 *
 * @returns The old value.
 * @param   pi64        Pointer to the value.
 * @param   i64         Number to add.
 */
DECLINLINE(int64_t) ASMAtomicAddS64(int64_t volatile *pi64, int64_t i64)
{
    return (int64_t)ASMAtomicAddU64((uint64_t volatile *)pi64, (uint64_t)i64);
}


/**
 * Atomically exchanges and subtracts to an unsigned 32-bit value, ordered.
 *
 * @returns The old value.
 * @param   pu32        Pointer to the value.
 * @param   u32         Number to subtract.
 */
DECLINLINE(uint32_t) ASMAtomicSubU32(uint32_t volatile *pu32, uint32_t u32)
{
    return ASMAtomicAddU32(pu32, (uint32_t)-(int32_t)u32);
}


/**
 * Atomically exchanges and subtracts to a signed 32-bit value, ordered.
 *
 * @returns The old value.
 * @param   pi32        Pointer to the value.
 * @param   i32         Number to subtract.
 */
DECLINLINE(int32_t) ASMAtomicSubS32(int32_t volatile *pi32, int32_t i32)
{
    return (int32_t)ASMAtomicAddU32((uint32_t volatile *)pi32, (uint32_t)-i32);
}


/**
 * Atomically exchanges and subtracts to an unsigned 64-bit value, ordered.
 *
 * @returns The old value.
 * @param   pu64        Pointer to the value.
 * @param   u64         Number to subtract.
 */
DECLINLINE(uint64_t) ASMAtomicSubU64(uint64_t volatile *pu64, uint64_t u64)
{
    return ASMAtomicAddU64(pu64, (uint64_t)-(int64_t)u64);
}


/**
 * Atomically exchanges and subtracts to a signed 64-bit value, ordered.
 *
 * @returns The old value.
 * @param   pi64        Pointer to the value.
 * @param   i64         Number to subtract.
 */
DECLINLINE(int64_t) ASMAtomicSubS64(int64_t volatile *pi64, int64_t i64)
{
    return (int64_t)ASMAtomicAddU64((uint64_t volatile *)pi64, (uint64_t)-i64);
}


/**
 * Atomically increment a 32-bit value, ordered.
 *
 * @returns The new value.
 * @param   pu32        Pointer to the value to increment.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(uint32_t) ASMAtomicIncU32(uint32_t volatile *pu32);
#else
DECLINLINE(uint32_t) ASMAtomicIncU32(uint32_t volatile *pu32)
{
    uint32_t u32;
# if RT_INLINE_ASM_USES_INTRIN
    u32 = _InterlockedIncrement((long *)pu32);
    return u32;

# elif RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__("lock; xaddl %0, %1\n\t"
                         : "=r" (u32),
                           "=m" (*pu32)
                         : "0" (1),
                           "m" (*pu32)
                         : "memory");
    return u32+1;
# else
    __asm
    {
        mov     eax, 1
#  ifdef RT_ARCH_AMD64
        mov     rdx, [pu32]
        lock xadd [rdx], eax
#  else
        mov     edx, [pu32]
        lock xadd [edx], eax
#  endif
        mov     u32, eax
    }
    return u32+1;
# endif
}
#endif


/**
 * Atomically increment a signed 32-bit value, ordered.
 *
 * @returns The new value.
 * @param   pi32        Pointer to the value to increment.
 */
DECLINLINE(int32_t) ASMAtomicIncS32(int32_t volatile *pi32)
{
    return (int32_t)ASMAtomicIncU32((uint32_t volatile *)pi32);
}


/**
 * Atomically increment a 64-bit value, ordered.
 *
 * @returns The new value.
 * @param   pu64        Pointer to the value to increment.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(uint64_t) ASMAtomicIncU64(uint64_t volatile *pu64);
#else
DECLINLINE(uint64_t) ASMAtomicIncU64(uint64_t volatile *pu64)
{
    uint64_t u64;
# if RT_INLINE_ASM_USES_INTRIN && defined(RT_ARCH_AMD64)
    u64 = _InterlockedIncrement64((__int64 *)pu64);
    return u64;

# elif RT_INLINE_ASM_GNU_STYLE && defined(RT_ARCH_AMD64)
    __asm__ __volatile__("lock; xaddq %0, %1\n\t"
                         : "=r" (u64),
                           "=m" (*pu64)
                         : "0" (1),
                           "m" (*pu64)
                         : "memory");
    return u64 + 1;
# else
    return ASMAtomicAddU64(pu64, 1) + 1;
# endif
}
#endif


/**
 * Atomically increment a signed 64-bit value, ordered.
 *
 * @returns The new value.
 * @param   pi64        Pointer to the value to increment.
 */
DECLINLINE(int64_t) ASMAtomicIncS64(int64_t volatile *pi64)
{
    return (int64_t)ASMAtomicIncU64((uint64_t volatile *)pi64);
}


/**
 * Atomically decrement an unsigned 32-bit value, ordered.
 *
 * @returns The new value.
 * @param   pu32        Pointer to the value to decrement.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(uint32_t) ASMAtomicDecU32(uint32_t volatile *pu32);
#else
DECLINLINE(uint32_t) ASMAtomicDecU32(uint32_t volatile *pu32)
{
    uint32_t u32;
# if RT_INLINE_ASM_USES_INTRIN
    u32 = _InterlockedDecrement((long *)pu32);
    return u32;

# elif RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__("lock; xaddl %0, %1\n\t"
                         : "=r" (u32),
                           "=m" (*pu32)
                         : "0" (-1),
                           "m" (*pu32)
                         : "memory");
    return u32-1;
# else
    __asm
    {
        mov     eax, -1
#  ifdef RT_ARCH_AMD64
        mov     rdx, [pu32]
        lock xadd [rdx], eax
#  else
        mov     edx, [pu32]
        lock xadd [edx], eax
#  endif
        mov     u32, eax
    }
    return u32-1;
# endif
}
#endif


/**
 * Atomically decrement a signed 32-bit value, ordered.
 *
 * @returns The new value.
 * @param   pi32        Pointer to the value to decrement.
 */
DECLINLINE(int32_t) ASMAtomicDecS32(int32_t volatile *pi32)
{
    return (int32_t)ASMAtomicDecU32((uint32_t volatile *)pi32);
}


/**
 * Atomically decrement an unsigned 64-bit value, ordered.
 *
 * @returns The new value.
 * @param   pu64        Pointer to the value to decrement.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(uint64_t) ASMAtomicDecU64(uint64_t volatile *pu64);
#else
DECLINLINE(uint64_t) ASMAtomicDecU64(uint64_t volatile *pu64)
{
# if RT_INLINE_ASM_USES_INTRIN && defined(RT_ARCH_AMD64)
    uint64_t u64 = _InterlockedDecrement64((__int64 volatile *)pu64);
    return u64;

# elif RT_INLINE_ASM_GNU_STYLE && defined(RT_ARCH_AMD64)
    uint64_t u64;
    __asm__ __volatile__("lock; xaddq %q0, %1\n\t"
                         : "=r" (u64),
                           "=m" (*pu64)
                         : "0" (~(uint64_t)0),
                           "m" (*pu64)
                         : "memory");
    return u64-1;
# else
    return ASMAtomicAddU64(pu64, UINT64_MAX) - 1;
# endif
}
#endif


/**
 * Atomically decrement a signed 64-bit value, ordered.
 *
 * @returns The new value.
 * @param   pi64        Pointer to the value to decrement.
 */
DECLINLINE(int64_t) ASMAtomicDecS64(int64_t volatile *pi64)
{
    return (int64_t)ASMAtomicDecU64((uint64_t volatile *)pi64);
}


/**
 * Atomically Or an unsigned 32-bit value, ordered.
 *
 * @param   pu32   Pointer to the pointer variable to OR u32 with.
 * @param   u32    The value to OR *pu32 with.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(void) ASMAtomicOrU32(uint32_t volatile *pu32, uint32_t u32);
#else
DECLINLINE(void) ASMAtomicOrU32(uint32_t volatile *pu32, uint32_t u32)
{
# if RT_INLINE_ASM_USES_INTRIN
    _InterlockedOr((long volatile *)pu32, (long)u32);

# elif RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__("lock; orl %1, %0\n\t"
                         : "=m" (*pu32)
                         : "ir" (u32),
                           "m" (*pu32));
# else
    __asm
    {
        mov     eax, [u32]
#  ifdef RT_ARCH_AMD64
        mov     rdx, [pu32]
        lock    or [rdx], eax
#  else
        mov     edx, [pu32]
        lock    or [edx], eax
#  endif
    }
# endif
}
#endif


/**
 * Atomically Or a signed 32-bit value, ordered.
 *
 * @param   pi32   Pointer to the pointer variable to OR u32 with.
 * @param   i32    The value to OR *pu32 with.
 */
DECLINLINE(void) ASMAtomicOrS32(int32_t volatile *pi32, int32_t i32)
{
    ASMAtomicOrU32((uint32_t volatile *)pi32, i32);
}


/**
 * Atomically Or an unsigned 64-bit value, ordered.
 *
 * @param   pu64   Pointer to the pointer variable to OR u64 with.
 * @param   u64    The value to OR *pu64 with.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(void) ASMAtomicOrU64(uint64_t volatile *pu64, uint64_t u64);
#else
DECLINLINE(void) ASMAtomicOrU64(uint64_t volatile *pu64, uint64_t u64)
{
# if RT_INLINE_ASM_USES_INTRIN && defined(RT_ARCH_AMD64)
    _InterlockedOr64((__int64 volatile *)pu64, (__int64)u64);

# elif RT_INLINE_ASM_GNU_STYLE && defined(RT_ARCH_AMD64)
    __asm__ __volatile__("lock; orq %1, %q0\n\t"
                         : "=m" (*pu64)
                         : "r" (u64),
                           "m" (*pu64));
# else
    for (;;)
    {
        uint64_t u64Old = ASMAtomicUoReadU64(pu64);
        uint64_t u64New = u64Old | u64;
        if (ASMAtomicCmpXchgU64(pu64, u64New, u64Old))
            break;
        ASMNopPause();
    }
# endif
}
#endif


/**
 * Atomically Or a signed 64-bit value, ordered.
 *
 * @param   pi64   Pointer to the pointer variable to OR u64 with.
 * @param   i64    The value to OR *pu64 with.
 */
DECLINLINE(void) ASMAtomicOrS64(int64_t volatile *pi64, int64_t i64)
{
    ASMAtomicOrU64((uint64_t volatile *)pi64, i64);
}
/**
 * Atomically And an unsigned 32-bit value, ordered.
 *
 * @param   pu32   Pointer to the pointer variable to AND u32 with.
 * @param   u32    The value to AND *pu32 with.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(void) ASMAtomicAndU32(uint32_t volatile *pu32, uint32_t u32);
#else
DECLINLINE(void) ASMAtomicAndU32(uint32_t volatile *pu32, uint32_t u32)
{
# if RT_INLINE_ASM_USES_INTRIN
    _InterlockedAnd((long volatile *)pu32, u32);

# elif RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__("lock; andl %1, %0\n\t"
                         : "=m" (*pu32)
                         : "ir" (u32),
                           "m" (*pu32));
# else
    __asm
    {
        mov     eax, [u32]
#  ifdef RT_ARCH_AMD64
        mov     rdx, [pu32]
        lock and [rdx], eax
#  else
        mov     edx, [pu32]
        lock and [edx], eax
#  endif
    }
# endif
}
#endif


/**
 * Atomically And a signed 32-bit value, ordered.
 *
 * @param   pi32   Pointer to the pointer variable to AND i32 with.
 * @param   i32    The value to AND *pi32 with.
 */
DECLINLINE(void) ASMAtomicAndS32(int32_t volatile *pi32, int32_t i32)
{
    ASMAtomicAndU32((uint32_t volatile *)pi32, (uint32_t)i32);
}


/**
 * Atomically And an unsigned 64-bit value, ordered.
 *
 * @param   pu64   Pointer to the pointer variable to AND u64 with.
 * @param   u64    The value to AND *pu64 with.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(void) ASMAtomicAndU64(uint64_t volatile *pu64, uint64_t u64);
#else
DECLINLINE(void) ASMAtomicAndU64(uint64_t volatile *pu64, uint64_t u64)
{
# if RT_INLINE_ASM_USES_INTRIN && defined(RT_ARCH_AMD64)
    _InterlockedAnd64((__int64 volatile *)pu64, u64);

# elif RT_INLINE_ASM_GNU_STYLE && defined(RT_ARCH_AMD64)
    __asm__ __volatile__("lock; andq %1, %0\n\t"
                         : "=m" (*pu64)
                         : "r" (u64),
                           "m" (*pu64));
# else
    for (;;)
    {
        uint64_t u64Old = ASMAtomicUoReadU64(pu64);
        uint64_t u64New = u64Old & u64;
        if (ASMAtomicCmpXchgU64(pu64, u64New, u64Old))
            break;
        ASMNopPause();
    }
# endif
}
#endif


/**
 * Atomically And a signed 64-bit value, ordered.
 *
 * @param   pi64   Pointer to the pointer variable to AND i64 with.
 * @param   i64    The value to AND *pi64 with.
 */
DECLINLINE(void) ASMAtomicAndS64(int64_t volatile *pi64, int64_t i64)
{
    ASMAtomicAndU64((uint64_t volatile *)pi64, (uint64_t)i64);
}



/** @def RT_ASM_PAGE_SIZE
 * We try avoid dragging in iprt/param.h here.
 * @internal
 */
#if defined(RT_ARCH_SPARC64)
# define RT_ASM_PAGE_SIZE   0x2000
# if defined(PAGE_SIZE) && !defined(NT_INCLUDED)
#  if PAGE_SIZE != 0x2000
#   error "PAGE_SIZE is not 0x2000!"
#  endif
# endif
#else
# define RT_ASM_PAGE_SIZE   0x1000
# if defined(PAGE_SIZE) && !defined(NT_INCLUDED)
#  if PAGE_SIZE != 0x1000
#   error "PAGE_SIZE is not 0x1000!"
#  endif
# endif
#endif

/**
 * Zeros a 4K memory page.
 *
 * @param   pv  Pointer to the memory block. This must be page aligned.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(void) ASMMemZeroPage(volatile void *pv);
# else
DECLINLINE(void) ASMMemZeroPage(volatile void *pv)
{
#  if RT_INLINE_ASM_USES_INTRIN
#   ifdef RT_ARCH_AMD64
    __stosq((unsigned __int64 *)pv, 0, RT_ASM_PAGE_SIZE / 8);
#   else
    __stosd((unsigned long *)pv, 0, RT_ASM_PAGE_SIZE / 4);
#   endif

#  elif RT_INLINE_ASM_GNU_STYLE
    RTCCUINTREG uDummy;
#   ifdef RT_ARCH_AMD64
    __asm__ __volatile__("rep stosq"
                         : "=D" (pv),
                           "=c" (uDummy)
                         : "0" (pv),
                           "c" (RT_ASM_PAGE_SIZE >> 3),
                           "a" (0)
                         : "memory");
#   else
    __asm__ __volatile__("rep stosl"
                         : "=D" (pv),
                           "=c" (uDummy)
                         : "0" (pv),
                           "c" (RT_ASM_PAGE_SIZE >> 2),
                           "a" (0)
                         : "memory");
#   endif
#  else
    __asm
    {
#   ifdef RT_ARCH_AMD64
        xor     rax, rax
        mov     ecx, 0200h
        mov     rdi, [pv]
        rep     stosq
#   else
        xor     eax, eax
        mov     ecx, 0400h
        mov     edi, [pv]
        rep     stosd
#   endif
    }
#  endif
}
# endif


/**
 * Zeros a memory block with a 32-bit aligned size.
 *
 * @param   pv  Pointer to the memory block.
 * @param   cb  Number of bytes in the block. This MUST be aligned on 32-bit!
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(void) ASMMemZero32(volatile void *pv, size_t cb);
#else
DECLINLINE(void) ASMMemZero32(volatile void *pv, size_t cb)
{
# if RT_INLINE_ASM_USES_INTRIN
#  ifdef RT_ARCH_AMD64
    if (!(cb & 7))
        __stosq((unsigned __int64 *)pv, 0, cb / 8);
    else
#  endif
        __stosd((unsigned long *)pv, 0, cb / 4);

# elif RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__("rep stosl"
                         : "=D" (pv),
                           "=c" (cb)
                         : "0" (pv),
                           "1" (cb >> 2),
                           "a" (0)
                         : "memory");
# else
    __asm
    {
        xor     eax, eax
#  ifdef RT_ARCH_AMD64
        mov     rcx, [cb]
        shr     rcx, 2
        mov     rdi, [pv]
#  else
        mov     ecx, [cb]
        shr     ecx, 2
        mov     edi, [pv]
#  endif
        rep stosd
    }
# endif
}
#endif


/**
 * Fills a memory block with a 32-bit aligned size.
 *
 * @param   pv  Pointer to the memory block.
 * @param   cb  Number of bytes in the block. This MUST be aligned on 32-bit!
 * @param   u32 The value to fill with.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(void) ASMMemFill32(volatile void *pv, size_t cb, uint32_t u32);
#else
DECLINLINE(void) ASMMemFill32(volatile void *pv, size_t cb, uint32_t u32)
{
# if RT_INLINE_ASM_USES_INTRIN
#  ifdef RT_ARCH_AMD64
    if (!(cb & 7))
        __stosq((unsigned __int64 *)pv, RT_MAKE_U64(u32, u32), cb / 8);
    else
#  endif
        __stosd((unsigned long *)pv, u32, cb / 4);

# elif RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__("rep stosl"
                         : "=D" (pv),
                           "=c" (cb)
                         : "0" (pv),
                           "1" (cb >> 2),
                           "a" (u32)
                         : "memory");
# else
    __asm
    {
#  ifdef RT_ARCH_AMD64
        mov     rcx, [cb]
        shr     rcx, 2
        mov     rdi, [pv]
#  else
        mov     ecx, [cb]
        shr     ecx, 2
        mov     edi, [pv]
#  endif
        mov     eax, [u32]
        rep stosd
    }
# endif
}
#endif


/**
 * Checks if a memory page is all zeros.
 *
 * @returns true / false.
 *
 * @param   pvPage      Pointer to the page.  Must be aligned on 16 byte
 *                      boundrary
 */
DECLINLINE(bool) ASMMemIsZeroPage(void const *pvPage)
{
# if 0 /*RT_INLINE_ASM_GNU_STYLE - this is actually slower... */
    union { RTCCUINTREG r; bool f; } uAX;
    RTCCUINTREG xCX, xDI;
   Assert(!((uintptr_t)pvPage & 15));
    __asm__ __volatile__("repe; "
#  ifdef RT_ARCH_AMD64
                         "scasq\n\t"
#  else
                         "scasl\n\t"
#  endif
                         "setnc %%al\n\t"
                         : "=&c" (xCX),
                           "=&D" (xDI),
                           "=&a" (uAX.r)
                         : "mr" (pvPage),
#  ifdef RT_ARCH_AMD64
                         "0" (RT_ASM_PAGE_SIZE/8),
#  else
                         "0" (RT_ASM_PAGE_SIZE/4),
#  endif
                         "1" (pvPage),
                         "2" (0));
    return uAX.f;
# else
   uintptr_t const *puPtr = (uintptr_t const *)pvPage;
   int              cLeft = RT_ASM_PAGE_SIZE / sizeof(uintptr_t) / 8;
   Assert(!((uintptr_t)pvPage & 15));
   for (;;)
   {
       if (puPtr[0])        return false;
       if (puPtr[4])        return false;

       if (puPtr[2])        return false;
       if (puPtr[6])        return false;

       if (puPtr[1])        return false;
       if (puPtr[5])        return false;

       if (puPtr[3])        return false;
       if (puPtr[7])        return false;

       if (!--cLeft)
           return true;
       puPtr += 8;
   }
   return true;
# endif
}


/**
 * Checks if a memory block is filled with the specified byte.
 *
 * This is a sort of inverted memchr.
 *
 * @returns Pointer to the byte which doesn't equal u8.
 * @returns NULL if all equal to u8.
 *
 * @param   pv      Pointer to the memory block.
 * @param   cb      Number of bytes in the block. This MUST be aligned on 32-bit!
 * @param   u8      The value it's supposed to be filled with.
 *
 * @todo Fix name, it is a predicate function but it's not returning boolean!
 */
DECLINLINE(void *) ASMMemIsAll8(void const *pv, size_t cb, uint8_t u8)
{
/** @todo rewrite this in inline assembly? */
    uint8_t const *pb = (uint8_t const *)pv;
    for (; cb; cb--, pb++)
        if (RT_UNLIKELY(*pb != u8))
            return (void *)pb;
    return NULL;
}


/**
 * Checks if a memory block is filled with the specified 32-bit value.
 *
 * This is a sort of inverted memchr.
 *
 * @returns Pointer to the first value which doesn't equal u32.
 * @returns NULL if all equal to u32.
 *
 * @param   pv      Pointer to the memory block.
 * @param   cb      Number of bytes in the block. This MUST be aligned on 32-bit!
 * @param   u32     The value it's supposed to be filled with.
 *
 * @todo Fix name, it is a predicate function but it's not returning boolean!
 */
DECLINLINE(uint32_t *) ASMMemIsAllU32(void const *pv, size_t cb, uint32_t u32)
{
/** @todo rewrite this in inline assembly? */
    uint32_t const *pu32 = (uint32_t const *)pv;
    for (; cb; cb -= 4, pu32++)
        if (RT_UNLIKELY(*pu32 != u32))
            return (uint32_t *)pu32;
    return NULL;
}


/**
 * Probes a byte pointer for read access.
 *
 * While the function will not fault if the byte is not read accessible,
 * the idea is to do this in a safe place like before acquiring locks
 * and such like.
 *
 * Also, this functions guarantees that an eager compiler is not going
 * to optimize the probing away.
 *
 * @param   pvByte      Pointer to the byte.
 */
#if RT_INLINE_ASM_EXTERNAL
DECLASM(uint8_t) ASMProbeReadByte(const void *pvByte);
#else
DECLINLINE(uint8_t) ASMProbeReadByte(const void *pvByte)
{
    /** @todo verify that the compiler actually doesn't optimize this away. (intel & gcc) */
    uint8_t u8;
# if RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__("movb (%1), %0\n\t"
                         : "=r" (u8)
                         : "r" (pvByte));
# else
    __asm
    {
#  ifdef RT_ARCH_AMD64
        mov     rax, [pvByte]
        mov     al, [rax]
#  else
        mov     eax, [pvByte]
        mov     al, [eax]
#  endif
        mov     [u8], al
    }
# endif
    return u8;
}
#endif

/**
 * Probes a buffer for read access page by page.
 *
 * While the function will fault if the buffer is not fully read
 * accessible, the idea is to do this in a safe place like before
 * acquiring locks and such like.
 *
 * Also, this functions guarantees that an eager compiler is not going
 * to optimize the probing away.
 *
 * @param   pvBuf       Pointer to the buffer.
 * @param   cbBuf       The size of the buffer in bytes. Must be >= 1.
 */
DECLINLINE(void) ASMProbeReadBuffer(const void *pvBuf, size_t cbBuf)
{
    /** @todo verify that the compiler actually doesn't optimize this away. (intel & gcc) */
    /* the first byte */
    const uint8_t *pu8 = (const uint8_t *)pvBuf;
    ASMProbeReadByte(pu8);

    /* the pages in between pages. */
    while (cbBuf > RT_ASM_PAGE_SIZE)
    {
        ASMProbeReadByte(pu8);
        cbBuf -= RT_ASM_PAGE_SIZE;
        pu8   += RT_ASM_PAGE_SIZE;
    }

    /* the last byte */
    ASMProbeReadByte(pu8 + cbBuf - 1);
}



/** @defgroup grp_inline_bits   Bit Operations
 * @{
 */


/**
 * Sets a bit in a bitmap.
 *
 * @param   pvBitmap    Pointer to the bitmap. This should be 32-bit aligned.
 * @param   iBit        The bit to set.
 *
 * @remarks The 32-bit aligning of pvBitmap is not a strict requirement.
 *          However, doing so will yield better performance as well as avoiding
 *          traps accessing the last bits in the bitmap.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(void) ASMBitSet(volatile void *pvBitmap, int32_t iBit);
#else
DECLINLINE(void) ASMBitSet(volatile void *pvBitmap, int32_t iBit)
{
# if RT_INLINE_ASM_USES_INTRIN
    _bittestandset((long *)pvBitmap, iBit);

# elif RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__("btsl %1, %0"
                         : "=m" (*(volatile long *)pvBitmap)
                         : "Ir" (iBit),
                           "m" (*(volatile long *)pvBitmap)
                         : "memory");
# else
    __asm
    {
#  ifdef RT_ARCH_AMD64
        mov     rax, [pvBitmap]
        mov     edx, [iBit]
        bts     [rax], edx
#  else
        mov     eax, [pvBitmap]
        mov     edx, [iBit]
        bts     [eax], edx
#  endif
    }
# endif
}
#endif


/**
 * Atomically sets a bit in a bitmap, ordered.
 *
 * @param   pvBitmap    Pointer to the bitmap. Must be 32-bit aligned, otherwise
 *                      the memory access isn't atomic!
 * @param   iBit        The bit to set.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(void) ASMAtomicBitSet(volatile void *pvBitmap, int32_t iBit);
#else
DECLINLINE(void) ASMAtomicBitSet(volatile void *pvBitmap, int32_t iBit)
{
    AssertMsg(!((uintptr_t)pvBitmap & 3), ("address %p not 32-bit aligned", pvBitmap));
# if RT_INLINE_ASM_USES_INTRIN
    _interlockedbittestandset((long *)pvBitmap, iBit);
# elif RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__("lock; btsl %1, %0"
                         : "=m" (*(volatile long *)pvBitmap)
                         : "Ir" (iBit),
                           "m" (*(volatile long *)pvBitmap)
                         : "memory");
# else
    __asm
    {
#  ifdef RT_ARCH_AMD64
        mov     rax, [pvBitmap]
        mov     edx, [iBit]
        lock bts [rax], edx
#  else
        mov     eax, [pvBitmap]
        mov     edx, [iBit]
        lock bts [eax], edx
#  endif
    }
# endif
}
#endif


/**
 * Clears a bit in a bitmap.
 *
 * @param   pvBitmap    Pointer to the bitmap.
 * @param   iBit        The bit to clear.
 *
 * @remarks The 32-bit aligning of pvBitmap is not a strict requirement.
 *          However, doing so will yield better performance as well as avoiding
 *          traps accessing the last bits in the bitmap.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(void) ASMBitClear(volatile void *pvBitmap, int32_t iBit);
#else
DECLINLINE(void) ASMBitClear(volatile void *pvBitmap, int32_t iBit)
{
# if RT_INLINE_ASM_USES_INTRIN
    _bittestandreset((long *)pvBitmap, iBit);

# elif RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__("btrl %1, %0"
                         : "=m" (*(volatile long *)pvBitmap)
                         : "Ir" (iBit),
                           "m" (*(volatile long *)pvBitmap)
                         : "memory");
# else
    __asm
    {
#  ifdef RT_ARCH_AMD64
        mov     rax, [pvBitmap]
        mov     edx, [iBit]
        btr     [rax], edx
#  else
        mov     eax, [pvBitmap]
        mov     edx, [iBit]
        btr     [eax], edx
#  endif
    }
# endif
}
#endif


/**
 * Atomically clears a bit in a bitmap, ordered.
 *
 * @param   pvBitmap    Pointer to the bitmap. Must be 32-bit aligned, otherwise
 *                      the memory access isn't atomic!
 * @param   iBit        The bit to toggle set.
 * @remarks No memory barrier, take care on smp.
 */
#if RT_INLINE_ASM_EXTERNAL
DECLASM(void) ASMAtomicBitClear(volatile void *pvBitmap, int32_t iBit);
#else
DECLINLINE(void) ASMAtomicBitClear(volatile void *pvBitmap, int32_t iBit)
{
    AssertMsg(!((uintptr_t)pvBitmap & 3), ("address %p not 32-bit aligned", pvBitmap));
# if RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__("lock; btrl %1, %0"
                         : "=m" (*(volatile long *)pvBitmap)
                         : "Ir" (iBit),
                           "m" (*(volatile long *)pvBitmap)
                         : "memory");
# else
    __asm
    {
#  ifdef RT_ARCH_AMD64
        mov     rax, [pvBitmap]
        mov     edx, [iBit]
        lock btr [rax], edx
#  else
        mov     eax, [pvBitmap]
        mov     edx, [iBit]
        lock btr [eax], edx
#  endif
    }
# endif
}
#endif


/**
 * Toggles a bit in a bitmap.
 *
 * @param   pvBitmap    Pointer to the bitmap.
 * @param   iBit        The bit to toggle.
 *
 * @remarks The 32-bit aligning of pvBitmap is not a strict requirement.
 *          However, doing so will yield better performance as well as avoiding
 *          traps accessing the last bits in the bitmap.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(void) ASMBitToggle(volatile void *pvBitmap, int32_t iBit);
#else
DECLINLINE(void) ASMBitToggle(volatile void *pvBitmap, int32_t iBit)
{
# if RT_INLINE_ASM_USES_INTRIN
    _bittestandcomplement((long *)pvBitmap, iBit);
# elif RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__("btcl %1, %0"
                         : "=m" (*(volatile long *)pvBitmap)
                         : "Ir" (iBit),
                           "m" (*(volatile long *)pvBitmap)
                         : "memory");
# else
    __asm
    {
#  ifdef RT_ARCH_AMD64
        mov     rax, [pvBitmap]
        mov     edx, [iBit]
        btc     [rax], edx
#  else
        mov     eax, [pvBitmap]
        mov     edx, [iBit]
        btc     [eax], edx
#  endif
    }
# endif
}
#endif


/**
 * Atomically toggles a bit in a bitmap, ordered.
 *
 * @param   pvBitmap    Pointer to the bitmap. Must be 32-bit aligned, otherwise
 *                      the memory access isn't atomic!
 * @param   iBit        The bit to test and set.
 */
#if RT_INLINE_ASM_EXTERNAL
DECLASM(void) ASMAtomicBitToggle(volatile void *pvBitmap, int32_t iBit);
#else
DECLINLINE(void) ASMAtomicBitToggle(volatile void *pvBitmap, int32_t iBit)
{
    AssertMsg(!((uintptr_t)pvBitmap & 3), ("address %p not 32-bit aligned", pvBitmap));
# if RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__("lock; btcl %1, %0"
                         : "=m" (*(volatile long *)pvBitmap)
                         : "Ir" (iBit),
                           "m" (*(volatile long *)pvBitmap)
                         : "memory");
# else
    __asm
    {
#  ifdef RT_ARCH_AMD64
        mov     rax, [pvBitmap]
        mov     edx, [iBit]
        lock btc [rax], edx
#  else
        mov     eax, [pvBitmap]
        mov     edx, [iBit]
        lock btc [eax], edx
#  endif
    }
# endif
}
#endif


/**
 * Tests and sets a bit in a bitmap.
 *
 * @returns true if the bit was set.
 * @returns false if the bit was clear.
 *
 * @param   pvBitmap    Pointer to the bitmap.
 * @param   iBit        The bit to test and set.
 *
 * @remarks The 32-bit aligning of pvBitmap is not a strict requirement.
 *          However, doing so will yield better performance as well as avoiding
 *          traps accessing the last bits in the bitmap.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(bool) ASMBitTestAndSet(volatile void *pvBitmap, int32_t iBit);
#else
DECLINLINE(bool) ASMBitTestAndSet(volatile void *pvBitmap, int32_t iBit)
{
    union { bool f; uint32_t u32; uint8_t u8; } rc;
# if RT_INLINE_ASM_USES_INTRIN
    rc.u8 = _bittestandset((long *)pvBitmap, iBit);

# elif RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__("btsl %2, %1\n\t"
                         "setc %b0\n\t"
                         "andl $1, %0\n\t"
                         : "=q" (rc.u32),
                           "=m" (*(volatile long *)pvBitmap)
                         : "Ir" (iBit),
                           "m" (*(volatile long *)pvBitmap)
                         : "memory");
# else
    __asm
    {
        mov     edx, [iBit]
#  ifdef RT_ARCH_AMD64
        mov     rax, [pvBitmap]
        bts     [rax], edx
#  else
        mov     eax, [pvBitmap]
        bts     [eax], edx
#  endif
        setc    al
        and     eax, 1
        mov     [rc.u32], eax
    }
# endif
    return rc.f;
}
#endif


/**
 * Atomically tests and sets a bit in a bitmap, ordered.
 *
 * @returns true if the bit was set.
 * @returns false if the bit was clear.
 *
 * @param   pvBitmap    Pointer to the bitmap. Must be 32-bit aligned, otherwise
 *                      the memory access isn't atomic!
 * @param   iBit        The bit to set.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(bool) ASMAtomicBitTestAndSet(volatile void *pvBitmap, int32_t iBit);
#else
DECLINLINE(bool) ASMAtomicBitTestAndSet(volatile void *pvBitmap, int32_t iBit)
{
    union { bool f; uint32_t u32; uint8_t u8; } rc;
    AssertMsg(!((uintptr_t)pvBitmap & 3), ("address %p not 32-bit aligned", pvBitmap));
# if RT_INLINE_ASM_USES_INTRIN
    rc.u8 = _interlockedbittestandset((long *)pvBitmap, iBit);
# elif RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__("lock; btsl %2, %1\n\t"
                         "setc %b0\n\t"
                         "andl $1, %0\n\t"
                         : "=q" (rc.u32),
                           "=m" (*(volatile long *)pvBitmap)
                         : "Ir" (iBit),
                           "m" (*(volatile long *)pvBitmap)
                         : "memory");
# else
    __asm
    {
        mov     edx, [iBit]
#  ifdef RT_ARCH_AMD64
        mov     rax, [pvBitmap]
        lock bts [rax], edx
#  else
        mov     eax, [pvBitmap]
        lock bts [eax], edx
#  endif
        setc    al
        and     eax, 1
        mov     [rc.u32], eax
    }
# endif
    return rc.f;
}
#endif


/**
 * Tests and clears a bit in a bitmap.
 *
 * @returns true if the bit was set.
 * @returns false if the bit was clear.
 *
 * @param   pvBitmap    Pointer to the bitmap.
 * @param   iBit        The bit to test and clear.
 *
 * @remarks The 32-bit aligning of pvBitmap is not a strict requirement.
 *          However, doing so will yield better performance as well as avoiding
 *          traps accessing the last bits in the bitmap.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(bool) ASMBitTestAndClear(volatile void *pvBitmap, int32_t iBit);
#else
DECLINLINE(bool) ASMBitTestAndClear(volatile void *pvBitmap, int32_t iBit)
{
    union { bool f; uint32_t u32; uint8_t u8; } rc;
# if RT_INLINE_ASM_USES_INTRIN
    rc.u8 = _bittestandreset((long *)pvBitmap, iBit);

# elif RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__("btrl %2, %1\n\t"
                         "setc %b0\n\t"
                         "andl $1, %0\n\t"
                         : "=q" (rc.u32),
                           "=m" (*(volatile long *)pvBitmap)
                         : "Ir" (iBit),
                           "m" (*(volatile long *)pvBitmap)
                         : "memory");
# else
    __asm
    {
        mov     edx, [iBit]
#  ifdef RT_ARCH_AMD64
        mov     rax, [pvBitmap]
        btr     [rax], edx
#  else
        mov     eax, [pvBitmap]
        btr     [eax], edx
#  endif
        setc    al
        and     eax, 1
        mov     [rc.u32], eax
    }
# endif
    return rc.f;
}
#endif


/**
 * Atomically tests and clears a bit in a bitmap, ordered.
 *
 * @returns true if the bit was set.
 * @returns false if the bit was clear.
 *
 * @param   pvBitmap    Pointer to the bitmap. Must be 32-bit aligned, otherwise
 *                      the memory access isn't atomic!
 * @param   iBit        The bit to test and clear.
 *
 * @remarks No memory barrier, take care on smp.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(bool) ASMAtomicBitTestAndClear(volatile void *pvBitmap, int32_t iBit);
#else
DECLINLINE(bool) ASMAtomicBitTestAndClear(volatile void *pvBitmap, int32_t iBit)
{
    union { bool f; uint32_t u32; uint8_t u8; } rc;
    AssertMsg(!((uintptr_t)pvBitmap & 3), ("address %p not 32-bit aligned", pvBitmap));
# if RT_INLINE_ASM_USES_INTRIN
    rc.u8 = _interlockedbittestandreset((long *)pvBitmap, iBit);

# elif RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__("lock; btrl %2, %1\n\t"
                         "setc %b0\n\t"
                         "andl $1, %0\n\t"
                         : "=q" (rc.u32),
                           "=m" (*(volatile long *)pvBitmap)
                         : "Ir" (iBit),
                           "m" (*(volatile long *)pvBitmap)
                         : "memory");
# else
    __asm
    {
        mov     edx, [iBit]
#  ifdef RT_ARCH_AMD64
        mov     rax, [pvBitmap]
        lock btr [rax], edx
#  else
        mov     eax, [pvBitmap]
        lock btr [eax], edx
#  endif
        setc    al
        and     eax, 1
        mov     [rc.u32], eax
    }
# endif
    return rc.f;
}
#endif


/**
 * Tests and toggles a bit in a bitmap.
 *
 * @returns true if the bit was set.
 * @returns false if the bit was clear.
 *
 * @param   pvBitmap    Pointer to the bitmap.
 * @param   iBit        The bit to test and toggle.
 *
 * @remarks The 32-bit aligning of pvBitmap is not a strict requirement.
 *          However, doing so will yield better performance as well as avoiding
 *          traps accessing the last bits in the bitmap.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(bool) ASMBitTestAndToggle(volatile void *pvBitmap, int32_t iBit);
#else
DECLINLINE(bool) ASMBitTestAndToggle(volatile void *pvBitmap, int32_t iBit)
{
    union { bool f; uint32_t u32; uint8_t u8; } rc;
# if RT_INLINE_ASM_USES_INTRIN
    rc.u8 = _bittestandcomplement((long *)pvBitmap, iBit);

# elif RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__("btcl %2, %1\n\t"
                         "setc %b0\n\t"
                         "andl $1, %0\n\t"
                         : "=q" (rc.u32),
                           "=m" (*(volatile long *)pvBitmap)
                         : "Ir" (iBit),
                           "m" (*(volatile long *)pvBitmap)
                         : "memory");
# else
    __asm
    {
        mov   edx, [iBit]
#  ifdef RT_ARCH_AMD64
        mov   rax, [pvBitmap]
        btc   [rax], edx
#  else
        mov   eax, [pvBitmap]
        btc   [eax], edx
#  endif
        setc  al
        and   eax, 1
        mov   [rc.u32], eax
    }
# endif
    return rc.f;
}
#endif


/**
 * Atomically tests and toggles a bit in a bitmap, ordered.
 *
 * @returns true if the bit was set.
 * @returns false if the bit was clear.
 *
 * @param   pvBitmap    Pointer to the bitmap. Must be 32-bit aligned, otherwise
 *                      the memory access isn't atomic!
 * @param   iBit        The bit to test and toggle.
 */
#if RT_INLINE_ASM_EXTERNAL
DECLASM(bool) ASMAtomicBitTestAndToggle(volatile void *pvBitmap, int32_t iBit);
#else
DECLINLINE(bool) ASMAtomicBitTestAndToggle(volatile void *pvBitmap, int32_t iBit)
{
    union { bool f; uint32_t u32; uint8_t u8; } rc;
    AssertMsg(!((uintptr_t)pvBitmap & 3), ("address %p not 32-bit aligned", pvBitmap));
# if RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__("lock; btcl %2, %1\n\t"
                         "setc %b0\n\t"
                         "andl $1, %0\n\t"
                         : "=q" (rc.u32),
                           "=m" (*(volatile long *)pvBitmap)
                         : "Ir" (iBit),
                           "m" (*(volatile long *)pvBitmap)
                         : "memory");
# else
    __asm
    {
        mov     edx, [iBit]
#  ifdef RT_ARCH_AMD64
        mov     rax, [pvBitmap]
        lock btc [rax], edx
#  else
        mov     eax, [pvBitmap]
        lock btc [eax], edx
#  endif
        setc    al
        and     eax, 1
        mov     [rc.u32], eax
    }
# endif
    return rc.f;
}
#endif


/**
 * Tests if a bit in a bitmap is set.
 *
 * @returns true if the bit is set.
 * @returns false if the bit is clear.
 *
 * @param   pvBitmap    Pointer to the bitmap.
 * @param   iBit        The bit to test.
 *
 * @remarks The 32-bit aligning of pvBitmap is not a strict requirement.
 *          However, doing so will yield better performance as well as avoiding
 *          traps accessing the last bits in the bitmap.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(bool) ASMBitTest(const volatile void *pvBitmap, int32_t iBit);
#else
DECLINLINE(bool) ASMBitTest(const volatile void *pvBitmap, int32_t iBit)
{
    union { bool f; uint32_t u32; uint8_t u8; } rc;
# if RT_INLINE_ASM_USES_INTRIN
    rc.u32 = _bittest((long *)pvBitmap, iBit);
# elif RT_INLINE_ASM_GNU_STYLE

    __asm__ __volatile__("btl %2, %1\n\t"
                         "setc %b0\n\t"
                         "andl $1, %0\n\t"
                         : "=q" (rc.u32)
                         : "m" (*(const volatile long *)pvBitmap),
                           "Ir" (iBit)
                         : "memory");
# else
    __asm
    {
        mov   edx, [iBit]
#  ifdef RT_ARCH_AMD64
        mov   rax, [pvBitmap]
        bt    [rax], edx
#  else
        mov   eax, [pvBitmap]
        bt    [eax], edx
#  endif
        setc  al
        and   eax, 1
        mov   [rc.u32], eax
    }
# endif
    return rc.f;
}
#endif


/**
 * Clears a bit range within a bitmap.
 *
 * @param   pvBitmap    Pointer to the bitmap.
 * @param   iBitStart   The First bit to clear.
 * @param   iBitEnd     The first bit not to clear.
 */
DECLINLINE(void) ASMBitClearRange(volatile void *pvBitmap, int32_t iBitStart, int32_t iBitEnd)
{
    if (iBitStart < iBitEnd)
    {
        volatile uint32_t *pu32 = (volatile uint32_t *)pvBitmap + (iBitStart >> 5);
        int iStart = iBitStart & ~31;
        int iEnd   = iBitEnd & ~31;
        if (iStart == iEnd)
            *pu32 &= ((1 << (iBitStart & 31)) - 1) | ~((1 << (iBitEnd & 31)) - 1);
        else
        {
            /* bits in first dword. */
            if (iBitStart & 31)
            {
                *pu32 &= (1 << (iBitStart & 31)) - 1;
                pu32++;
                iBitStart = iStart + 32;
            }

            /* whole dword. */
            if (iBitStart != iEnd)
                ASMMemZero32(pu32, (iEnd - iBitStart) >> 3);

            /* bits in last dword. */
            if (iBitEnd & 31)
            {
                pu32 = (volatile uint32_t *)pvBitmap + (iBitEnd >> 5);
                *pu32 &= ~((1 << (iBitEnd & 31)) - 1);
            }
        }
    }
}


/**
 * Sets a bit range within a bitmap.
 *
 * @param   pvBitmap    Pointer to the bitmap.
 * @param   iBitStart   The First bit to set.
 * @param   iBitEnd     The first bit not to set.
 */
DECLINLINE(void) ASMBitSetRange(volatile void *pvBitmap, int32_t iBitStart, int32_t iBitEnd)
{
    if (iBitStart < iBitEnd)
    {
        volatile uint32_t *pu32 = (volatile uint32_t *)pvBitmap + (iBitStart >> 5);
        int iStart = iBitStart & ~31;
        int iEnd   = iBitEnd & ~31;
        if (iStart == iEnd)
            *pu32 |= ((1 << (iBitEnd - iBitStart)) - 1) << iBitStart;
        else
        {
            /* bits in first dword. */
            if (iBitStart & 31)
            {
                *pu32 |= ~((1 << (iBitStart & 31)) - 1);
                pu32++;
                iBitStart = iStart + 32;
            }

            /* whole dword. */
            if (iBitStart != iEnd)
                ASMMemFill32(pu32, (iEnd - iBitStart) >> 3, ~0);

            /* bits in last dword. */
            if (iBitEnd & 31)
            {
                pu32 = (volatile uint32_t *)pvBitmap + (iBitEnd >> 5);
                *pu32 |= (1 << (iBitEnd & 31)) - 1;
            }
        }
    }
}


/**
 * Finds the first clear bit in a bitmap.
 *
 * @returns Index of the first zero bit.
 * @returns -1 if no clear bit was found.
 * @param   pvBitmap    Pointer to the bitmap.
 * @param   cBits       The number of bits in the bitmap. Multiple of 32.
 */
#if RT_INLINE_ASM_EXTERNAL
DECLASM(int) ASMBitFirstClear(const volatile void *pvBitmap, uint32_t cBits);
#else
DECLINLINE(int) ASMBitFirstClear(const volatile void *pvBitmap, uint32_t cBits)
{
    if (cBits)
    {
        int32_t iBit;
# if RT_INLINE_ASM_GNU_STYLE
        RTCCUINTREG uEAX, uECX, uEDI;
        cBits = RT_ALIGN_32(cBits, 32);
        __asm__ __volatile__("repe; scasl\n\t"
                             "je    1f\n\t"
#  ifdef RT_ARCH_AMD64
                             "lea   -4(%%rdi), %%rdi\n\t"
                             "xorl  (%%rdi), %%eax\n\t"
                             "subq  %5, %%rdi\n\t"
#  else
                             "lea   -4(%%edi), %%edi\n\t"
                             "xorl  (%%edi), %%eax\n\t"
                             "subl  %5, %%edi\n\t"
#  endif
                             "shll  $3, %%edi\n\t"
                             "bsfl  %%eax, %%edx\n\t"
                             "addl  %%edi, %%edx\n\t"
                             "1:\t\n"
                             : "=d" (iBit),
                               "=&c" (uECX),
                               "=&D" (uEDI),
                               "=&a" (uEAX)
                             : "0" (0xffffffff),
                               "mr" (pvBitmap),
                               "1" (cBits >> 5),
                               "2" (pvBitmap),
                               "3" (0xffffffff));
# else
        cBits = RT_ALIGN_32(cBits, 32);
        __asm
        {
#  ifdef RT_ARCH_AMD64
            mov     rdi, [pvBitmap]
            mov     rbx, rdi
#  else
            mov     edi, [pvBitmap]
            mov     ebx, edi
#  endif
            mov     edx, 0ffffffffh
            mov     eax, edx
            mov     ecx, [cBits]
            shr     ecx, 5
            repe    scasd
            je      done

#  ifdef RT_ARCH_AMD64
            lea     rdi, [rdi - 4]
            xor     eax, [rdi]
            sub     rdi, rbx
#  else
            lea     edi, [edi - 4]
            xor     eax, [edi]
            sub     edi, ebx
#  endif
            shl     edi, 3
            bsf     edx, eax
            add     edx, edi
        done:
            mov     [iBit], edx
        }
# endif
        return iBit;
    }
    return -1;
}
#endif


/**
 * Finds the next clear bit in a bitmap.
 *
 * @returns Index of the first zero bit.
 * @returns -1 if no clear bit was found.
 * @param   pvBitmap    Pointer to the bitmap.
 * @param   cBits       The number of bits in the bitmap. Multiple of 32.
 * @param   iBitPrev    The bit returned from the last search.
 *                      The search will start at iBitPrev + 1.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(int) ASMBitNextClear(const volatile void *pvBitmap, uint32_t cBits, uint32_t iBitPrev);
#else
DECLINLINE(int) ASMBitNextClear(const volatile void *pvBitmap, uint32_t cBits, uint32_t iBitPrev)
{
    const volatile uint32_t *pau32Bitmap = (const volatile uint32_t *)pvBitmap;
    int                      iBit = ++iBitPrev & 31;
    if (iBit)
    {
        /*
         * Inspect the 32-bit word containing the unaligned bit.
         */
        uint32_t  u32 = ~pau32Bitmap[iBitPrev / 32] >> iBit;

# if RT_INLINE_ASM_USES_INTRIN
        unsigned long ulBit = 0;
        if (_BitScanForward(&ulBit, u32))
            return ulBit + iBitPrev;
# else
#  if RT_INLINE_ASM_GNU_STYLE
        __asm__ __volatile__("bsf %1, %0\n\t"
                             "jnz 1f\n\t"
                             "movl $-1, %0\n\t"
                             "1:\n\t"
                             : "=r" (iBit)
                             : "r" (u32));
#  else
        __asm
        {
            mov     edx, [u32]
            bsf     eax, edx
            jnz     done
            mov     eax, 0ffffffffh
        done:
            mov     [iBit], eax
        }
#  endif
        if (iBit >= 0)
            return iBit + iBitPrev;
# endif

        /*
         * Skip ahead and see if there is anything left to search.
         */
        iBitPrev |= 31;
        iBitPrev++;
        if (cBits <= (uint32_t)iBitPrev)
            return -1;
    }

    /*
     * 32-bit aligned search, let ASMBitFirstClear do the dirty work.
     */
    iBit = ASMBitFirstClear(&pau32Bitmap[iBitPrev / 32], cBits - iBitPrev);
    if (iBit >= 0)
        iBit += iBitPrev;
    return iBit;
}
#endif


/**
 * Finds the first set bit in a bitmap.
 *
 * @returns Index of the first set bit.
 * @returns -1 if no clear bit was found.
 * @param   pvBitmap    Pointer to the bitmap.
 * @param   cBits       The number of bits in the bitmap. Multiple of 32.
 */
#if RT_INLINE_ASM_EXTERNAL
DECLASM(int) ASMBitFirstSet(const volatile void *pvBitmap, uint32_t cBits);
#else
DECLINLINE(int) ASMBitFirstSet(const volatile void *pvBitmap, uint32_t cBits)
{
    if (cBits)
    {
        int32_t iBit;
# if RT_INLINE_ASM_GNU_STYLE
        RTCCUINTREG uEAX, uECX, uEDI;
        cBits = RT_ALIGN_32(cBits, 32);
        __asm__ __volatile__("repe; scasl\n\t"
                             "je    1f\n\t"
#  ifdef RT_ARCH_AMD64
                             "lea   -4(%%rdi), %%rdi\n\t"
                             "movl  (%%rdi), %%eax\n\t"
                             "subq  %5, %%rdi\n\t"
#  else
                             "lea   -4(%%edi), %%edi\n\t"
                             "movl  (%%edi), %%eax\n\t"
                             "subl  %5, %%edi\n\t"
#  endif
                             "shll  $3, %%edi\n\t"
                             "bsfl  %%eax, %%edx\n\t"
                             "addl  %%edi, %%edx\n\t"
                             "1:\t\n"
                             : "=d" (iBit),
                               "=&c" (uECX),
                               "=&D" (uEDI),
                               "=&a" (uEAX)
                             : "0" (0xffffffff),
                               "mr" (pvBitmap),
                               "1" (cBits >> 5),
                               "2" (pvBitmap),
                               "3" (0));
# else
        cBits = RT_ALIGN_32(cBits, 32);
        __asm
        {
#  ifdef RT_ARCH_AMD64
            mov     rdi, [pvBitmap]
            mov     rbx, rdi
#  else
            mov     edi, [pvBitmap]
            mov     ebx, edi
#  endif
            mov     edx, 0ffffffffh
            xor     eax, eax
            mov     ecx, [cBits]
            shr     ecx, 5
            repe    scasd
            je      done
#  ifdef RT_ARCH_AMD64
            lea     rdi, [rdi - 4]
            mov     eax, [rdi]
            sub     rdi, rbx
#  else
            lea     edi, [edi - 4]
            mov     eax, [edi]
            sub     edi, ebx
#  endif
            shl     edi, 3
            bsf     edx, eax
            add     edx, edi
        done:
            mov   [iBit], edx
        }
# endif
        return iBit;
    }
    return -1;
}
#endif


/**
 * Finds the next set bit in a bitmap.
 *
 * @returns Index of the next set bit.
 * @returns -1 if no set bit was found.
 * @param   pvBitmap    Pointer to the bitmap.
 * @param   cBits       The number of bits in the bitmap. Multiple of 32.
 * @param   iBitPrev    The bit returned from the last search.
 *                      The search will start at iBitPrev + 1.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(int) ASMBitNextSet(const volatile void *pvBitmap, uint32_t cBits, uint32_t iBitPrev);
#else
DECLINLINE(int) ASMBitNextSet(const volatile void *pvBitmap, uint32_t cBits, uint32_t iBitPrev)
{
    const volatile uint32_t *pau32Bitmap = (const volatile uint32_t *)pvBitmap;
    int                      iBit = ++iBitPrev & 31;
    if (iBit)
    {
        /*
         * Inspect the 32-bit word containing the unaligned bit.
         */
        uint32_t  u32 = pau32Bitmap[iBitPrev / 32] >> iBit;

# if RT_INLINE_ASM_USES_INTRIN
        unsigned long ulBit = 0;
        if (_BitScanForward(&ulBit, u32))
            return ulBit + iBitPrev;
# else
#  if RT_INLINE_ASM_GNU_STYLE
        __asm__ __volatile__("bsf %1, %0\n\t"
                             "jnz 1f\n\t"
                             "movl $-1, %0\n\t"
                             "1:\n\t"
                             : "=r" (iBit)
                             : "r" (u32));
#  else
        __asm
        {
            mov     edx, [u32]
            bsf     eax, edx
            jnz     done
            mov     eax, 0ffffffffh
        done:
            mov     [iBit], eax
        }
#  endif
        if (iBit >= 0)
            return iBit + iBitPrev;
# endif

        /*
         * Skip ahead and see if there is anything left to search.
         */
        iBitPrev |= 31;
        iBitPrev++;
        if (cBits <= (uint32_t)iBitPrev)
            return -1;
    }

    /*
     * 32-bit aligned search, let ASMBitFirstClear do the dirty work.
     */
    iBit = ASMBitFirstSet(&pau32Bitmap[iBitPrev / 32], cBits - iBitPrev);
    if (iBit >= 0)
        iBit += iBitPrev;
    return iBit;
}
#endif


/**
 * Finds the first bit which is set in the given 32-bit integer.
 * Bits are numbered from 1 (least significant) to 32.
 *
 * @returns index [1..32] of the first set bit.
 * @returns 0 if all bits are cleared.
 * @param   u32     Integer to search for set bits.
 * @remark  Similar to ffs() in BSD.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(unsigned) ASMBitFirstSetU32(uint32_t u32);
#else
DECLINLINE(unsigned) ASMBitFirstSetU32(uint32_t u32)
{
# if RT_INLINE_ASM_USES_INTRIN
    unsigned long iBit;
    if (_BitScanForward(&iBit, u32))
        iBit++;
    else
        iBit = 0;
# elif RT_INLINE_ASM_GNU_STYLE
    uint32_t iBit;
    __asm__ __volatile__("bsf  %1, %0\n\t"
                         "jnz  1f\n\t"
                         "xorl %0, %0\n\t"
                         "jmp  2f\n"
                         "1:\n\t"
                         "incl %0\n"
                         "2:\n\t"
                         : "=r" (iBit)
                         : "rm" (u32));
# else
    uint32_t iBit;
    _asm
    {
        bsf     eax, [u32]
        jnz     found
        xor     eax, eax
        jmp     done
    found:
        inc     eax
    done:
        mov     [iBit], eax
    }
# endif
    return iBit;
}
#endif


/**
 * Finds the first bit which is set in the given 32-bit integer.
 * Bits are numbered from 1 (least significant) to 32.
 *
 * @returns index [1..32] of the first set bit.
 * @returns 0 if all bits are cleared.
 * @param   i32     Integer to search for set bits.
 * @remark  Similar to ffs() in BSD.
 */
DECLINLINE(unsigned) ASMBitFirstSetS32(int32_t i32)
{
    return ASMBitFirstSetU32((uint32_t)i32);
}


/**
 * Finds the last bit which is set in the given 32-bit integer.
 * Bits are numbered from 1 (least significant) to 32.
 *
 * @returns index [1..32] of the last set bit.
 * @returns 0 if all bits are cleared.
 * @param   u32     Integer to search for set bits.
 * @remark  Similar to fls() in BSD.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(unsigned) ASMBitLastSetU32(uint32_t u32);
#else
DECLINLINE(unsigned) ASMBitLastSetU32(uint32_t u32)
{
# if RT_INLINE_ASM_USES_INTRIN
    unsigned long iBit;
    if (_BitScanReverse(&iBit, u32))
        iBit++;
    else
        iBit = 0;
# elif RT_INLINE_ASM_GNU_STYLE
    uint32_t iBit;
    __asm__ __volatile__("bsrl %1, %0\n\t"
                         "jnz   1f\n\t"
                         "xorl %0, %0\n\t"
                         "jmp  2f\n"
                         "1:\n\t"
                         "incl %0\n"
                         "2:\n\t"
                         : "=r" (iBit)
                         : "rm" (u32));
# else
    uint32_t iBit;
    _asm
    {
        bsr     eax, [u32]
        jnz     found
        xor     eax, eax
        jmp     done
    found:
        inc     eax
    done:
        mov     [iBit], eax
    }
# endif
    return iBit;
}
#endif


/**
 * Finds the last bit which is set in the given 32-bit integer.
 * Bits are numbered from 1 (least significant) to 32.
 *
 * @returns index [1..32] of the last set bit.
 * @returns 0 if all bits are cleared.
 * @param   i32     Integer to search for set bits.
 * @remark  Similar to fls() in BSD.
 */
DECLINLINE(unsigned) ASMBitLastSetS32(int32_t i32)
{
    return ASMBitLastSetU32((uint32_t)i32);
}

/**
 * Reverse the byte order of the given 16-bit integer.
 *
 * @returns Revert
 * @param   u16     16-bit integer value.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(uint16_t) ASMByteSwapU16(uint16_t u16);
#else
DECLINLINE(uint16_t) ASMByteSwapU16(uint16_t u16)
{
# if RT_INLINE_ASM_USES_INTRIN
    u16 = _byteswap_ushort(u16);
# elif RT_INLINE_ASM_GNU_STYLE
    __asm__ ("rorw $8, %0" : "=r" (u16) : "0" (u16));
# else
    _asm
    {
        mov     ax, [u16]
        ror     ax, 8
        mov     [u16], ax
    }
# endif
    return u16;
}
#endif


/**
 * Reverse the byte order of the given 32-bit integer.
 *
 * @returns Revert
 * @param   u32     32-bit integer value.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(uint32_t) ASMByteSwapU32(uint32_t u32);
#else
DECLINLINE(uint32_t) ASMByteSwapU32(uint32_t u32)
{
# if RT_INLINE_ASM_USES_INTRIN
    u32 = _byteswap_ulong(u32);
# elif RT_INLINE_ASM_GNU_STYLE
    __asm__ ("bswapl %0" : "=r" (u32) : "0" (u32));
# else
    _asm
    {
        mov     eax, [u32]
        bswap   eax
        mov     [u32], eax
    }
# endif
    return u32;
}
#endif


/**
 * Reverse the byte order of the given 64-bit integer.
 *
 * @returns Revert
 * @param   u64     64-bit integer value.
 */
DECLINLINE(uint64_t) ASMByteSwapU64(uint64_t u64)
{
#if defined(RT_ARCH_AMD64) && RT_INLINE_ASM_USES_INTRIN
    u64 = _byteswap_uint64(u64);
#else
    u64 = (uint64_t)ASMByteSwapU32((uint32_t)u64) << 32
        | (uint64_t)ASMByteSwapU32((uint32_t)(u64 >> 32));
#endif
    return u64;
}


/** @} */


/** @} */

/* KLUDGE: Play safe for now as I cannot test all solaris and linux usages. */
#if !defined(__cplusplus) && !defined(DEBUG)
# if defined(RT_ARCH_AMD64) || defined(RT_ARCH_X86)
#  include <iprt/asm-amd64-x86.h>
# endif
# include <iprt/asm-math.h>
#endif

#endif

