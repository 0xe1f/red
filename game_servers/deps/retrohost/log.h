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

#ifndef __LOG_H__
#define __LOG_H__

enum LogLevel {
    LOG_FATAL   = 0,
    LOG_ERROR   = 1,
    LOG_WARN    = 2,
    LOG_INFO    = 3,
    LOG_DEBUG   = 4,
    LOG_VERBOSE = 5
};

void log_set_fd(FILE *fd);
void log_set_color(bool enable);
void log_set_level(LogLevel level);
void vlog_d(const char *tag, const char *fmt, va_list args);
void vlog_i(const char *tag, const char *fmt, va_list args);
void vlog_w(const char *tag, const char *fmt, va_list args);
void vlog_e(const char *tag, const char *fmt, va_list args);
void log_v(const char *tag, const char *fmt, ...);
void log_d(const char *tag, const char *fmt, ...);
void log_i(const char *tag, const char *fmt, ...);
void log_w(const char *tag, const char *fmt, ...);
void log_e(const char *tag, const char *fmt, ...);
void log_f(const char *tag, const char *fmt, ...);

#endif // __LOG_H__
