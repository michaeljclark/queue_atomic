//
//  rdtsc.h
//

#ifndef rdtsc_h
#define rdtsc_h

#ifdef _MSC_VER

#ifdef _M_IX86

inline uint64_t rdtsc()
{
    uint64_t c;
    __asm {
        cpuid
        rdtsc
        mov dword ptr [c + 0], eax
        mov dword ptr [c + 4], edx
    }
    return c;
}

#elif defined(_M_X64)

extern "C" unsigned __int64 __rdtsc();
#pragma intrinsic(__rdtsc)
inline uint64_t rdtsc()
{
    return __rdtsc();
}

#endif

#elif defined (__GNUC__)

#if defined(__i386__)

static __inline__ unsigned long long rdtsc(void)
{
    unsigned long long int x;
    __asm__ volatile (".byte 0x0f, 0x31" : "=A" (x));
    return x;
}

#elif defined(__x86_64__)

static __inline__ unsigned long long rdtsc(void)
{
    unsigned hi, lo;
    __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
    return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
}

#endif

#endif

#endif /* rdtsc_h */
