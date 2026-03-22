// Copyright (c) 2024 Akop Karapetyan
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdio.h>
#include <time.h>
#include <stdarg.h>
#include <unistd.h>
#include "log.h"

#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define KNRM  "\x1B[0m"

static FILE *log_file = NULL;

static LogLevel current_level = LOG_INFO;
static bool enable_color = false;
static int pid = 0;

static void vlog(const char *tag, LogLevel level, const char *fmt, va_list args);

void log_set_fd(FILE *fd)
{
    log_file = fd;
}

void log_set_color(bool enable)
{
    enable_color = enable;
}

void log_set_level(LogLevel level)
{
    current_level = level;
}

void vlog_d(const char *tag, const char *fmt, va_list args)
{
    if (current_level >= LOG_DEBUG) {
        vlog(tag, LOG_DEBUG, fmt, args);
    }
}

void vlog_i(const char *tag, const char *fmt, va_list args)
{
    if (current_level >= LOG_INFO) {
        vlog(tag, LOG_INFO, fmt, args);
    }
}

void vlog_w(const char *tag, const char *fmt, va_list args)
{
    if (current_level >= LOG_WARN) {
        vlog(tag, LOG_WARN, fmt, args);
    }
}

void vlog_e(const char *tag, const char *fmt, va_list args)
{
    if (current_level >= LOG_ERROR) {
        vlog(tag, LOG_ERROR, fmt, args);
    }
}

void log_v(const char *tag, const char *fmt, ...)
{
    if (current_level >= LOG_VERBOSE) {
        va_list va;
        va_start(va, fmt);
        vlog(tag, LOG_VERBOSE, fmt, va);
        va_end(va);
    }
}

void log_d(const char *tag, const char *fmt, ...)
{
    if (current_level >= LOG_DEBUG) {
        va_list va;
        va_start(va, fmt);
        vlog(tag, LOG_DEBUG, fmt, va);
        va_end(va);
    }
}

void log_i(const char *tag, const char *fmt, ...)
{
    if (current_level >= LOG_INFO) {
        va_list va;
        va_start(va, fmt);
        vlog(tag, LOG_INFO, fmt, va);
        va_end(va);
    }
}

void log_w(const char *tag, const char *fmt, ...)
{
    if (current_level >= LOG_WARN) {
        va_list va;
        va_start(va, fmt);
        vlog(tag, LOG_WARN, fmt, va);
        va_end(va);
    }
}

void log_e(const char *tag, const char *fmt, ...)
{
    if (current_level >= LOG_ERROR) {
        va_list va;
        va_start(va, fmt);
        vlog(tag, LOG_ERROR, fmt, va);
        va_end(va);
    }
}

void log_f(const char *tag, const char *fmt, ...)
{
    if (current_level >= LOG_FATAL) {
        va_list va;
        va_start(va, fmt);
        vlog(tag, LOG_FATAL, fmt, va);
        va_end(va);
    }
}

static void vlog(const char *tag, LogLevel level, const char *fmt, va_list args)
{
    time_t now = time(NULL);
    struct tm tm = *localtime(&now);

    if (pid == 0) {
        pid = getpid();
    }

    const char *lname;
    switch (level) {
        case LOG_VERBOSE: lname = "V"; break;
        case LOG_DEBUG:   lname = "D"; break;
        case LOG_INFO:    lname = "I"; break;
        case LOG_WARN:    lname = "W"; break;
        case LOG_ERROR:   lname = "E"; break;
        case LOG_FATAL:   lname = "F"; break;
        default:          lname = "?"; break;
    }

    FILE *fd = log_file ? log_file : stderr;
    if (enable_color) {
        switch(level) {
            case LOG_WARN: fprintf(fd, KYEL); break;
            case LOG_ERROR: fprintf(fd, KRED); break;
            default: break;
        }
    }

    fprintf(fd,
        "%d-%02d-%02d %02d:%02d:%02d %8d %s %-8s ",
        tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
        tm.tm_hour, tm.tm_min, tm.tm_sec,
        pid,
        lname,
        tag);
    vfprintf(fd, fmt, args);

    if (enable_color) {
        fprintf(fd, KNRM);
    }
    fflush(fd);
}
