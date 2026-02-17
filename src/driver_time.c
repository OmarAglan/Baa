/**
 * @file driver_time.c
 * @brief أدوات وقت بسيطة للـ Driver.
 * @version 0.3.2.9.4
 */

#include "driver_time.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <stddef.h>
#include <sys/time.h>
#endif

double driver_time_seconds(void)
{
#ifdef _WIN32
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / (double)freq.QuadPart;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
#endif
}
