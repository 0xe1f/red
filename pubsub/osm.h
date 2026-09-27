// Copyright (c) 2026 Akop Karapetyan
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

#ifndef __OSM_H__
#define __OSM_H__

#include <stdint.h>

#define OSM_POSITION_BOTTOM_LEFT  0
#define OSM_POSITION_BOTTOM_RIGHT 1

#define OSM_DURATION_LONG_MS  3000
#define OSM_DURATION_SHORT_MS 1000

#define OSM_COLOR_DEBUG  0xff018fc7
#define OSM_COLOR_WARN   0xfff69337
#define OSM_COLOR_ERROR  0xffcc0000
#define OSM_COLOR_INFO   0xffcccccc
#define OSM_COLOR_RECORD 0xffcc0000
#define OSM_COLOR_PLAY   0xff448d44
#define OSM_COLOR_STOP   0xff018fc7

struct OnScreenMessage {
    uint8_t position;
    unsigned int duration_ms;
    uint32_t color_argb;
    char text[256];
};

#endif // __OSM_H__
