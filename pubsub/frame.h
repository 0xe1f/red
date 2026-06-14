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

#ifndef __FRAME_H__
#define __FRAME_H__

#include <stddef.h>
#include <stdint.h>

typedef enum {
    PF_UNKNOWN  = 0,
    PF_RGB565   = 1,
    PF_RGBA8888 = 2,
    PF_RGBA5551 = 3,
    PF_ARGB8888 = 4,
} PixelFormat;

#define ATTR_NONE   0x00
#define ATTR_ROT180 0x01

// Wire header: 8 bytes, little-endian, immediately followed by LZ4-compressed
// pixel data. Decompressed size = pitch * height.
typedef struct __attribute__((packed)) {
    uint16_t pitch;
    uint16_t width;
    uint16_t height;
    uint8_t  pixel_format; // PixelFormat
    uint8_t  attrs;
} FrameHeader;

typedef struct {
    FrameHeader  header;
    uint8_t     *content;
    size_t       content_size;
} Frame;

typedef void (*xm_callback_t)(const Frame *);

#endif // __FRAME_H__
