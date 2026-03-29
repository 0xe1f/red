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

#ifndef __MATRIX_H__
#define __MATRIX_H__

#include <stdbool.h>
#include "geometry.pb-c.h"

typedef struct {
    short sx;
    short sy;
    short dx;
    short dy;
} ViewRect;

typedef struct {
    unsigned char r;
    unsigned char g;
    unsigned char b;
} Pixel;

bool viewrect_validate(const ViewRect *rect);
bool viewrect_parse(const char *arg, ViewRect *rect);
bool viewrect_is_zero(const ViewRect *rect);

void pixel_unpack(Pixel *dest, const Red__Geometry__PixelFormat pixel_format, const unsigned char *src, int offset);
const char* pixel_format_str(Red__Geometry__PixelFormat pixel_format);

#endif // __MATRIX_H__
