#include "timer.h"

#ifdef _WIN32
static struct timeval tic_ts;
#else
static struct timespec tic_ts;
#endif

#ifdef _WIN32
static LARGE_INTEGER getFILETIMEoffset()
{
    SYSTEMTIME s;
    FILETIME f;
    LARGE_INTEGER t;

    s.wYear = 1970;
    s.wMonth = 1;
    s.wDay = 1;
    s.wHour = 0;
    s.wMinute = 0;
    s.wSecond = 0;
    s.wMilliseconds = 0;
    SystemTimeToFileTime(&s, &f);
    t.QuadPart = f.dwHighDateTime;
    t.QuadPart <<= 32;
    t.QuadPart |= f.dwLowDateTime;
    return (t);
}

static int clock_gettime(int X, struct timeval *tv)
{
    LARGE_INTEGER           t;
    FILETIME            f;
    double                  microseconds;
    static LARGE_INTEGER    offset;
    static double           frequencyToMicroseconds;
    static int              initialized = 0;
    static BOOL             usePerformanceCounter = 0;

    if (!initialized) {
        LARGE_INTEGER performanceFrequency;
        initialized = 1;
        usePerformanceCounter = QueryPerformanceFrequency(&performanceFrequency);
        if (usePerformanceCounter) {
            QueryPerformanceCounter(&offset);
            frequencyToMicroseconds = (double)performanceFrequency.QuadPart / 1000000.;
        } else {
            offset = getFILETIMEoffset();
            frequencyToMicroseconds = 10.;
        }
    }
    if (usePerformanceCounter) QueryPerformanceCounter(&t);
    else {
        GetSystemTimeAsFileTime(&f);
        t.QuadPart = f.dwHighDateTime;
        t.QuadPart <<= 32;
        t.QuadPart |= f.dwLowDateTime;
    }

    t.QuadPart -= offset.QuadPart;
    microseconds = (double)t.QuadPart / frequencyToMicroseconds;
    t.QuadPart = (LONGLONG)microseconds;
    tv->tv_sec = (long)(t.QuadPart / 1000000);
    tv->tv_usec = t.QuadPart % 1000000;
    return (0);
}
#endif

void tic (void)
{
    #ifdef HAVE_MACH_ABSOLUTE_TIME
    tic_ts = mach_absolute_time();
    #else
    clock_gettime (0, &tic_ts);
    #endif
}

double toc (void)
{
    double out;
    #ifdef HAVE_MACH_ABSOLUTE_TIME
    uint64_t ts, nanosec;
    static mach_timebase_info_data_t info = {0,0};
    if (info.denom == 0) {
        mach_timebase_info(&info);
    }
    ts = mach_absolute_time();
    nanosec = (ts - tic_ts) * (info.numer / info.denom);
    out = 1.0e-9 * nanosec;
    #else
        #ifdef _WIN32
        struct timeval ts;
        clock_gettime (0, &ts);
        out = (ts.tv_usec - (double)tic_ts.tv_usec) * 1.0e-9;
        out += (ts.tv_sec - (double)tic_ts.tv_sec);
        #else
        struct timespec ts;
        clock_gettime (0, &ts);
        out = (ts.tv_nsec - (double)tic_ts.tv_nsec) * 1.0e-9;
        out += (ts.tv_sec - (double)tic_ts.tv_sec);
        #endif
    #endif
    return out;
}
