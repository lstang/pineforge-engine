#pragma once

#if defined(_WIN32)
#include <time.h>
#include <stdlib.h>
#include <string.h>

static inline time_t portable_timegm(const struct tm* t) {
    if (!t) return 0;
    int y = t->tm_year + 1900;
    int m = t->tm_mon + 1;
    int d = t->tm_mday;
    y -= (m <= 2);
    long long era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (153u * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    long long days = era * 146097LL + (long long)doe - 719468LL;
    return (time_t)(days * 86400LL + t->tm_hour * 3600LL + t->tm_min * 60LL + t->tm_sec);
}

static inline struct tm* portable_gmtime_r(const time_t* timer, struct tm* result) {
    if (!timer || !result) return NULL;
    long long t = *timer;
    long long days = (t >= 0 ? t : t - 86399) / 86400;
    long long rem_sec = t - days * 86400;
    if (rem_sec < 0) rem_sec += 86400;

    result->tm_sec = (int)(rem_sec % 60);
    result->tm_min = (int)((rem_sec / 60) % 60);
    result->tm_hour = (int)(rem_sec / 3600);

    long long wday = (days + 4) % 7;
    if (wday < 0) wday += 7;
    result->tm_wday = (int)wday;

    days += 719468;
    long long era = (days >= 0 ? days : days - 146096) / 146097;
    unsigned doe = (unsigned)(days - era * 146097);
    unsigned yoe = (doe - doe/1460 + doe/36524 - doe/146096) / 365;
    long long y = (long long)(yoe) + era * 400;
    unsigned doy = doe - (365*yoe + yoe/4 - yoe/100);
    unsigned mp = (5*doy + 2)/153;
    unsigned d = doy - (153*mp+2)/5 + 1;
    unsigned m = mp + (mp < 10 ? 3 : -9);
    y += (m <= 2);

    result->tm_year = (int)(y - 1900);
    result->tm_mon = (int)(m - 1);
    result->tm_mday = (int)d;
    result->tm_yday = (int)doy;
    result->tm_isdst = 0;
    return result;
}

static inline struct tm* portable_localtime_r(const time_t* timer, struct tm* buf) {
    if (!timer || !buf) return NULL;
    time_t t = *timer;
    int shift_cycles = 0;
    if (t < 0) {
        shift_cycles = (int)((-t + 146097LL * 86400LL - 1) / (146097LL * 86400LL));
        t += shift_cycles * 146097LL * 86400LL;
    }
    if (localtime_s(buf, &t) != 0) {
        return NULL;
    }
    buf->tm_year -= shift_cycles * 400;
    return buf;
}

static inline time_t portable_mktime(struct tm* t) {
    if (!t) return (time_t)(-1);
    int orig_year = t->tm_year;
    int shift_cycles = 0;
    if (t->tm_year <= 75) {
        shift_cycles = (75 - t->tm_year + 399) / 400;
        t->tm_year += shift_cycles * 400;
    } else if (t->tm_year > 1000) {
        shift_cycles = -((t->tm_year - 1000 + 399) / 400);
        t->tm_year += shift_cycles * 400;
    }
    time_t res = mktime(t);
    t->tm_year = orig_year;
    if (res == (time_t)(-1)) {
        return (time_t)(-1);
    }
    return (time_t)(res - (long long)shift_cycles * 146097LL * 86400LL);
}

#ifndef gmtime_r
#define gmtime_r portable_gmtime_r
#endif

#ifndef localtime_r
#define localtime_r portable_localtime_r
#endif

#ifndef mktime
#define mktime portable_mktime
#endif

#ifndef timegm
#define timegm portable_timegm
#endif

#ifndef tzset
#define tzset _tzset
#endif

static inline const char* portable_win32_normalize_tz(const char* tz) {
    if (!tz || !*tz || strcmp(tz, "UTC") == 0 || strcmp(tz, "Etc/UTC") == 0 || strcmp(tz, "GMT") == 0 || strcmp(tz, "Etc/GMT") == 0) {
        return "UTC";
    }
    if (strcmp(tz, "America/New_York") == 0 || strcmp(tz, "US/Eastern") == 0 || strcmp(tz, "America/Toronto") == 0) return "EST5EDT,M3.2.0,M11.1.0";
    if (strcmp(tz, "America/Chicago") == 0 || strcmp(tz, "US/Central") == 0) return "CST6CDT,M3.2.0,M11.1.0";
    if (strcmp(tz, "America/Denver") == 0 || strcmp(tz, "US/Mountain") == 0) return "MST7MDT,M3.2.0,M11.1.0";
    if (strcmp(tz, "America/Phoenix") == 0) return "MST7";
    if (strcmp(tz, "America/Los_Angeles") == 0 || strcmp(tz, "US/Pacific") == 0 || strcmp(tz, "America/Vancouver") == 0) return "PST8PDT,M3.2.0,M11.1.0";
    if (strcmp(tz, "America/Anchorage") == 0 || strcmp(tz, "US/Alaska") == 0) return "AKS9AKD,M3.2.0,M11.1.0";
    if (strcmp(tz, "Pacific/Honolulu") == 0 || strcmp(tz, "US/Hawaii") == 0) return "HST10";
    if (strcmp(tz, "America/Mexico_City") == 0) return "CST6";
    if (strcmp(tz, "America/Sao_Paulo") == 0) return "BRT3";
    if (strcmp(tz, "America/Argentina/Buenos_Aires") == 0 || strcmp(tz, "America/Buenos_Aires") == 0) return "ART3";
    if (strcmp(tz, "America/Santiago") == 0) return "CLT4";
    if (strcmp(tz, "America/Bogota") == 0 || strcmp(tz, "America/Lima") == 0) return "COT5";
    if (strcmp(tz, "America/Havana") == 0) return "CST5CDT,M3.2.0/0,M11.1.0/1";
    if (strcmp(tz, "Europe/London") == 0) return "GMT0BST,M3.5.0/1,M10.5.0/2";
    if (strcmp(tz, "Europe/Dublin") == 0) return "GMT0IST,M3.5.0/1,M10.5.0/2";
    if (strcmp(tz, "Europe/Madrid") == 0 || strcmp(tz, "Europe/Paris") == 0 || strcmp(tz, "Europe/Berlin") == 0 ||
        strcmp(tz, "Europe/Rome") == 0 || strcmp(tz, "Europe/Amsterdam") == 0 || strcmp(tz, "Europe/Brussels") == 0 ||
        strcmp(tz, "Europe/Vienna") == 0 || strcmp(tz, "Europe/Warsaw") == 0 || strcmp(tz, "Europe/Prague") == 0 ||
        strcmp(tz, "Europe/Zurich") == 0 || strcmp(tz, "Europe/Stockholm") == 0 || strcmp(tz, "Europe/Oslo") == 0 ||
        strcmp(tz, "Europe/Copenhagen") == 0) return "CET-1CES,M3.5.0,M10.5.0/3";
    if (strcmp(tz, "Europe/Athens") == 0 || strcmp(tz, "Europe/Helsinki") == 0 || strcmp(tz, "Europe/Bucharest") == 0 ||
        strcmp(tz, "Europe/Kyiv") == 0 || strcmp(tz, "Europe/Kiev") == 0) return "EET-2EES,M3.5.0/3,M10.5.0/4";
    if (strcmp(tz, "Europe/Istanbul") == 0) return "TRT-3";
    if (strcmp(tz, "Europe/Moscow") == 0) return "MSK-3";
    if (strcmp(tz, "Asia/Dubai") == 0) return "GST-4";
    if (strcmp(tz, "Asia/Tehran") == 0) return "UTC-3:30";
    if (strcmp(tz, "Asia/Kolkata") == 0 || strcmp(tz, "Asia/Calcutta") == 0 || strcmp(tz, "Asia/Colombo") == 0) return "IST-5:30";
    if (strcmp(tz, "Asia/Kathmandu") == 0) return "UTC-5:45";
    if (strcmp(tz, "Asia/Dhaka") == 0) return "BST-6";
    if (strcmp(tz, "Asia/Bangkok") == 0 || strcmp(tz, "Asia/Jakarta") == 0 || strcmp(tz, "Asia/Ho_Chi_Minh") == 0) return "ICT-7";
    if (strcmp(tz, "Asia/Shanghai") == 0 || strcmp(tz, "Asia/Hong_Kong") == 0 || strcmp(tz, "Asia/Taipei") == 0 ||
        strcmp(tz, "Asia/Singapore") == 0 || strcmp(tz, "Asia/Kuala_Lumpur") == 0 || strcmp(tz, "Asia/Manila") == 0) return "CST-8";
    if (strcmp(tz, "Asia/Tokyo") == 0 || strcmp(tz, "Asia/Seoul") == 0) return "JST-9";
    if (strcmp(tz, "Asia/Almaty") == 0) return "ALM-5";
    if (strcmp(tz, "Asia/Riyadh") == 0) return "AST-3";
    if (strcmp(tz, "Asia/Jerusalem") == 0 || strcmp(tz, "Asia/Tel_Aviv") == 0) return "IST-2IDT,M3.4.4/26,M10.5.0/2";
    if (strcmp(tz, "Australia/Perth") == 0) return "UTC-8";
    if (strcmp(tz, "Australia/Adelaide") == 0) return "UTC-9:30";
    if (strcmp(tz, "Australia/Darwin") == 0) return "UTC-9:30";
    if (strcmp(tz, "Australia/Brisbane") == 0) return "UTC-10";
    if (strcmp(tz, "Australia/Sydney") == 0 || strcmp(tz, "Australia/Melbourne") == 0) return "UTC-10";
    if (strcmp(tz, "Australia/Lord_Howe") == 0) return "UTC-10:30";
    if (strcmp(tz, "Pacific/Auckland") == 0) return "NZS-12";
    if (strcmp(tz, "Pacific/Chatham") == 0) return "UTC-12:45";
    if (strcmp(tz, "Africa/Cairo") == 0) return "EES-3";
    if (strcmp(tz, "Africa/Johannesburg") == 0) return "SAS-2";
    if (strcmp(tz, "Africa/Lagos") == 0) return "WAT-1";
    return tz;
}

static inline int setenv(const char* name, const char* value, int overwrite) {
    if (!name) return -1;
    if (!overwrite) {
        size_t envsize = 0;
        getenv_s(&envsize, NULL, 0, name);
        if (envsize > 0) return 0;
    }
    if (strcmp(name, "TZ") == 0 && value) {
        value = portable_win32_normalize_tz(value);
    }
    return _putenv_s(name, value ? value : "");
}
#endif
