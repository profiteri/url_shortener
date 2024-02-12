#pragma once

#include <string.h>
#include <stdarg.h>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <stdexcept>

namespace Log {

    static FILE* log_file;
    static std::mutex log_mutex;

    inline void timeToBuffer(char* buffer, size_t len) {

    // Get current time
    time_t rawtime;
    struct tm* timeinfo;
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    rawtime = ts.tv_sec;
    timeinfo = localtime(&rawtime);

    // Format the timestamp
    snprintf(buffer, len, "%02d-%02d-%04d %02d:%02d:%02d:%04ld",
             timeinfo->tm_mday, timeinfo->tm_mon + 1, timeinfo->tm_year + 1900,
             timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec,
             ts.tv_nsec / 100000); // 'ff' part as nanoseconds converted to hundredths of a second

    }

    inline void stat(const char* format, ...) {

        const std::lock_guard lock(log_mutex);

        va_list args;
        char buffer[1000];
        va_start(args, format);
        timeToBuffer(buffer, sizeof(buffer));

        fprintf(log_file, "%s : ", buffer);
        vfprintf(log_file, format, args);
        fprintf(log_file, "\n");
        fflush(log_file);

        va_end(args);

    }

    inline void init() {

        log_file = fopen("/cbdp/data/stat.txt", "a+");
        if (log_file == NULL) throw std::runtime_error("Creation of stat file failed");
        stat("$$$$$$$$$$$$$$$$$$$$$");
        stat("Stat file initialized");
        stat("$$$$$$$$$$$$$$$$$$$$$");

    }
/*
    inline void info(const char* format, ...) {

        va_list args;
        char buffer[1000];
        va_start(args, format);
        timeToBuffer(buffer, sizeof(buffer));
        fprintf(stdout, "%s : ", buffer);
        vfprintf(stdout, format, args);
        fprintf(stdout, "\n");
        va_end(args);

    }

    inline void error(const char* format, ...) {

        va_list args;
        char buffer[1000];
        va_start(args, format);
        timeToBuffer(buffer, sizeof(buffer));
        fprintf(stderr, "%s : ", buffer);
        vfprintf(stderr, format, args);
        fprintf(stderr, "\n");
        va_end(args);

    }
*/

}