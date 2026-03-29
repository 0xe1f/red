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

#include <stdlib.h>
#include <string.h>
#include "matrix.h"

// RGB565
#define RED_RGB565(x) (((x>>11)&0x1f)*255/31)
#define GREEN_RGB565(x) (((x>>5)&0x3f)*255/63)
#define BLUE_RGB565(x) ((x&0x1f)*255/31)
// RGBA8888
#define RED_RGBA8888(x) ((x>>24)&0xff)
#define GREEN_RGBA8888(x) ((x>>16)&0xff)
#define BLUE_RGBA8888(x) ((x>>8)&0xff)
// ARGB8888
#define RED_ARGB8888(x) ((x>>16)&0xff)
#define GREEN_ARGB8888(x) ((x>>8)&0xff)
#define BLUE_ARGB8888(x) ((x)&0xff)
// RGBA5551
#define RED_RGBA5551(x) (((x>>11)&0x1f)*255/31)
#define GREEN_RGBA5551(x) (((x>>6)&0x1f)*255/31)
#define BLUE_RGBA5551(x) (((x>>1)&0x1f)*255/31)

bool viewrect_validate(const ViewRect *rect)
{
    if (rect->sx >= rect->dx) {
        return false;
    } else if (rect->sy >= rect->dy) {
        return false;
    }

    return true;
}

bool viewrect_parse(const char *arg, ViewRect *rect)
{
    char temp[128];
    strncpy(temp, arg, 127);
    char *delim = strchr(temp, '-');
    if (delim == NULL) {
        return false;
    }
    *delim = '\0';

    char *sdelim = strchr(temp, ',');
    if (sdelim == NULL) {
        return false;
    }
    *sdelim = '\0';

    char *ddelim = strchr(delim + 1, ',');
    if (ddelim == NULL) {
        return false;
    }
    *ddelim = '\0';

    rect->sx = strtol(temp, NULL, 10);
    rect->sy = strtol(sdelim + 1, NULL, 10);
    rect->dx = strtol(delim + 1, NULL, 10);
    rect->dy = strtol(ddelim + 1, NULL, 10);

    return true;
}

bool viewrect_is_zero(const ViewRect *rect)
{
    return rect->sx == 0 && rect->sy == 0 && rect->dx == 0 && rect->dy == 0;
}

void pixel_unpack(Pixel *dest, const Red__Geometry__PixelFormat pixel_format, const unsigned char *src, int offset)
{
    if (pixel_format == RED__GEOMETRY__PIXEL_FORMAT__PF_RGB565) {
        uint16_t rcol = *((uint16_t *)src + offset);
        dest->r = RED_RGB565(rcol);
        dest->g = GREEN_RGB565(rcol);
        dest->b = BLUE_RGB565(rcol);
    } else if (pixel_format == RED__GEOMETRY__PIXEL_FORMAT__PF_RGBA8888) {
        uint32_t rcol = *((uint32_t *)src + offset);
        dest->r = RED_RGBA8888(rcol);
        dest->g = GREEN_RGBA8888(rcol);
        dest->b = BLUE_RGBA8888(rcol);
    } else if (pixel_format == RED__GEOMETRY__PIXEL_FORMAT__PF_ARGB8888) {
        uint32_t rcol = *((uint32_t *)src + offset);
        dest->r = RED_ARGB8888(rcol);
        dest->g = GREEN_ARGB8888(rcol);
        dest->b = BLUE_ARGB8888(rcol);
    } else if (pixel_format == RED__GEOMETRY__PIXEL_FORMAT__PF_RGBA5551) {
        uint16_t rcol = *((uint16_t *)src + offset);
        dest->r = RED_RGBA5551(rcol);
        dest->g = GREEN_RGBA5551(rcol);
        dest->b = BLUE_RGBA5551(rcol);
    }
}

const char* pixel_format_str(Red__Geometry__PixelFormat pixel_format)
{
    switch(pixel_format) {
        case RED__GEOMETRY__PIXEL_FORMAT__PF_RGB565:
            return "RGB565";
        case RED__GEOMETRY__PIXEL_FORMAT__PF_RGBA8888:
            return "RGBA8888";
        case RED__GEOMETRY__PIXEL_FORMAT__PF_ARGB8888:
            return "ARGB8888";
        case RED__GEOMETRY__PIXEL_FORMAT__PF_RGBA5551:
            return "RGBA5551";
        default:
            return "UNKNOWN";
    }
}
