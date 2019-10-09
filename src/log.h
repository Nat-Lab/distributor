#ifndef DIST_LOG_H
#define DIST_LOG_H
#include <stdio.h>
#include <time.h>

#define log_debug(fmt, ...) log("DEBUG", fmt, __VA_ARGS__)
#define log_info(fmt, ...) log("INFO ", fmt, __VA_ARGS__)
#define log_warn(fmt, ...) log("WARN ", fmt, __VA_ARGS__)
#define log_error(fmt, ...) log("ERROR", fmt, __VA_ARGS__)
#define log_fatal(fmt, ...) log("FATAL", fmt, __VA_ARGS__)

#ifndef DIST_SILENCE
#define log(log_level, fmt, ...) fprintf(stderr, "[%ld " log_level "] %s: " fmt, (long) time(NULL), __FUNCTION__, __VA_ARGS__)
#else
#define log(...)
#endif // DIST_SILENCE

#endif // DIST_LOG_H