/* Win32 services used by both OS2HOST32 execution backends. */

/* GetTickCount64 is part of the Vista-and-later API surface.  Declare the
 * minimum explicitly so a MinGW build does not depend on its header default. */
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stddef.h>

#include "os2_win32_services.h"

os2_api_ret_t os2_win32_query_local_datetime(
    void *opaque,
    struct Os2LocalDateTime *value)
{
    SYSTEMTIME system_time;
    TIME_ZONE_INFORMATION timezone;
    DWORD timezone_id;
    LONG bias;

    (void)opaque;
    if (value == NULL)
        return OS2_PERSONALITY_ERROR_INVALID_PARAMETER;

    GetLocalTime(&system_time);
    timezone_id = GetTimeZoneInformation(&timezone);
    if (timezone_id == TIME_ZONE_ID_INVALID) {
        bias = -1;
    } else {
        bias = timezone.Bias;
        if (timezone_id == TIME_ZONE_ID_STANDARD)
            bias += timezone.StandardBias;
        else if (timezone_id == TIME_ZONE_ID_DAYLIGHT)
            bias += timezone.DaylightBias;
    }

    value->hours = (uint8_t)system_time.wHour;
    value->minutes = (uint8_t)system_time.wMinute;
    value->seconds = (uint8_t)system_time.wSecond;
    value->hundredths = (uint8_t)(system_time.wMilliseconds / 10u);
    value->day = (uint8_t)system_time.wDay;
    value->month = (uint8_t)system_time.wMonth;
    value->year = (uint16_t)system_time.wYear;
    value->timezone_minutes_west = (int16_t)bias;
    value->weekday = (uint8_t)system_time.wDayOfWeek;
    return OS2_PERSONALITY_NO_ERROR;
}

uint64_t os2_win32_monotonic_milliseconds(void *opaque)
{
    (void)opaque;
    return (uint64_t)GetTickCount64();
}
