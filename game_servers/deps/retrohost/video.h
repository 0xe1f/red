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

#ifndef __VIDEO_H__
#define __VIDEO_H__

typedef enum {
    ROTATE_NONE   = 0,
    ROTATE_CCW90  = 1,
    ROTATE_CCW180 = 2,
    ROTATE_CCW270 = 3
} Rotation;

#define IS_LANDSCAPE(rot) (((rot)&1) == 0)
#define IS_PORTRAIT(rot)  (((rot)&1) != 0)

typedef enum {
    SCALE_MODE_NONE            = 0,
    SCALE_MODE_SHORTESTXASPECT = 1,
    SCALE_MODE_FIT             = 2,
    SCALE_MODE_HALF            = 3
} ScaleMode;

typedef struct {
    void *data;
    unsigned short width;
    unsigned short height;
    unsigned short bpp;
    unsigned short pitch;
    unsigned int size;
} VideoBuffer;

void blit(
    ScaleMode scale_mode,
    const void *data, unsigned width, unsigned height, size_t pitch,
    const unsigned char **out, size_t *out_size
);

#endif // __VIDEO_H__
