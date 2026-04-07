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
#include <stdlib.h>
#include <string.h>
#include "geometry.pb-c.h"
#include "libretro.h"
#include "log.h"
#include "video.h"

#define LOG_TAG  "video"

#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))

#define RED_RGB565(c) (int)((((c)>>11)&0x1f)*(255.0f/31))
#define GREEN_RGB565(c) (int)((((c)>>5)&0x3f)*(255.0f/63))
#define BLUE_RGB565(c) (int)(((c)&0x1f)*(255.0f/31))
#define RGB_RGB565(r,g,b) ((((r)&0xff)>>3)<<11)|((((g)&0xff)>>2)<<5)|(((b)&0xff)>>3)

#define RED_ARGB8888(x) ((x>>16)&0xff)
#define GREEN_ARGB8888(x) ((x>>8)&0xff)
#define BLUE_ARGB8888(x) ((x)&0xff)
#define RGB_ARGB8888(r,g,b) (0xff<<24)|(((r)&0xff)<<16)|(((g)&0xff)<<8)|(((b)&0xff))

#define PIXEL_FORMAT_BPP(pf) \
    ((pf) == RED__GEOMETRY__PIXEL_FORMAT__PF_RGBA8888 || (pf) == RED__GEOMETRY__PIXEL_FORMAT__PF_ARGB8888 ? 4 : \
     (pf) == RED__GEOMETRY__PIXEL_FORMAT__PF_RGB565 || (pf) == RED__GEOMETRY__PIXEL_FORMAT__PF_RGBA5551 ? 2 : 0)

extern VideoBuffer video_buffer;
extern struct retro_system_av_info av_info;
extern Rotation rotation;
extern Red__Geometry__PixelFormat pixel_format;

static void blit_plain(const void *data, unsigned width, unsigned height, size_t pitch);
static void blit_half(const void *data, unsigned width, unsigned height, size_t pitch);
static void blit_scale(
    const void *data, unsigned width, unsigned height, size_t pitch,
    unsigned int dest_width, unsigned int dest_height
);

void realloc_buffer_if_needed(VideoBuffer *buffer, int width, int height)
{
    if (buffer->data) {
        free(buffer->data);
        buffer->data = NULL;
    }

    if (width > 0 && height > 0) {
        buffer->width = width;
        buffer->height = height;
        buffer->bpp = PIXEL_FORMAT_BPP(pixel_format);
        buffer->pitch = buffer->width * buffer->bpp;
        buffer->size = buffer->pitch * buffer->height;
        buffer->data = calloc(buffer->size, 1);

        log_d(LOG_TAG, "Creating interim buffer of size %ux%u (bpp=%d)\n",
            buffer->width, buffer->height, buffer->bpp);
    }
}

void blit(
    ScaleMode scale_mode,
    const void *data, unsigned width, unsigned height, size_t pitch,
    const unsigned char **out, size_t *out_size
)
{
    if (!video_buffer.data) {
        // No interim buffer, use source directly
        *out = (const unsigned char *) data;
        *out_size = (int) pitch * height;
        return;
    }

    switch (scale_mode) {
        case SCALE_MODE_HALF:
            blit_half(data, width, height, pitch);
            break;
        case SCALE_MODE_SHORTESTXASPECT: {
            unsigned int dest_width = width;
            unsigned int dest_height = height;
            bool is_portrait = IS_PORTRAIT(rotation);

            if ((width > video_buffer.width && !is_portrait)
                || (height > video_buffer.height && is_portrait)
            ) {
                dest_width = (int)(height * av_info.geometry.aspect_ratio);
            } else if ((height > video_buffer.height && !is_portrait)
                || (width > video_buffer.width && is_portrait)
            ) {
                dest_height = (int)(width * av_info.geometry.aspect_ratio);
            }
            if (dest_width != width || dest_height != height) {
                blit_scale(data, width, height, pitch, dest_width, dest_height);
            } else {
                blit_plain(data, width, height, pitch);
            }
            break;
        }
        case SCALE_MODE_FIT: {
            unsigned int dest_width = width;
            unsigned int dest_height = height;
            bool is_portrait = IS_PORTRAIT(rotation);

            if ((width > video_buffer.width && !is_portrait)
                || (height > video_buffer.height && is_portrait)
            ) {
                dest_width = video_buffer.width;
                dest_height = (int)(dest_width / av_info.geometry.aspect_ratio);
            } else if ((height > video_buffer.height && !is_portrait)
                || (width > video_buffer.width && is_portrait)
            ) {
                dest_height = video_buffer.height;
                dest_width = (int)(dest_height * av_info.geometry.aspect_ratio);
            }
            if (dest_width != width || dest_height != height) {
                blit_scale(data, width, height, pitch, dest_width, dest_height);
            } else {
                blit_plain(data, width, height, pitch);
            }
            break;
        }
        case SCALE_MODE_NONE:
        default:
            blit_plain(data, width, height, pitch);
            break;
    }

    *out = (const unsigned char *) video_buffer.data;
    *out_size = video_buffer.size;
}

static void blit_plain(const void *data, unsigned width, unsigned height, size_t pitch)
{
    unsigned char *dst = (unsigned char *) video_buffer.data;
    const unsigned char *src = (const unsigned char *) data;

    // Calculate source dimensions to blit
    unsigned int src_width = width;
    unsigned int src_height = height;
    unsigned int dst_width = video_buffer.width;
    unsigned int dst_height = video_buffer.height;

    // If source is larger, center it in destination
    int offset_x = (dst_width > src_width) ? (dst_width - src_width) / 2 : 0;
    int offset_y = (dst_height > src_height) ? (dst_height - src_height) / 2 : 0;

    // Clamp source dimensions to destination if larger
    unsigned int blit_width = (src_width > dst_width) ? dst_width : src_width;
    unsigned int blit_height = (src_height > dst_height) ? dst_height : src_height;

    // If source is larger, start from center
    unsigned int src_offset_x = (src_width > dst_width) ? (src_width - dst_width) / 2 : 0;
    unsigned int src_offset_y = (src_height > dst_height) ? (src_height - dst_height) / 2 : 0;

    // Blit source to destination
    size_t bpp = video_buffer.bpp;
    for (unsigned int y = 0; y < blit_height; y++) {
        memcpy(
            dst + (offset_y + y) * video_buffer.pitch + offset_x * bpp,
            src + (src_offset_y + y) * pitch + src_offset_x * bpp,
            blit_width * bpp
        );
    }
}

static void blit_half(const void *data, unsigned width, unsigned height, size_t pitch)
{
    unsigned char *dst = (unsigned char *) video_buffer.data;
    const unsigned char *src = (const unsigned char *) data;

    // Calculate source dimensions to blit
    unsigned int src_width = width;
    bool halve_width = src_width > video_buffer.width;
    unsigned int src_px_factor = halve_width ? 2 : 1;
    if (halve_width) {
        src_width /= 2;
    }
    unsigned int src_height = height;
    bool halve_height = src_height > video_buffer.height;
    if (halve_height) {
        src_height /= 2;
    }
    unsigned int dst_width = video_buffer.width;
    unsigned int dst_height = video_buffer.height;

    // If source is larger, center it in destination
    int offset_x = (dst_width > src_width) ? (dst_width - src_width) / 2 : 0;
    int offset_y = (dst_height > src_height) ? (dst_height - src_height) / 2 : 0;

    // Clamp source dimensions to destination if larger
    unsigned int blit_width = (src_width > dst_width) ? dst_width : src_width;
    unsigned int blit_height = (src_height > dst_height) ? dst_height : src_height;

    // If source is larger, start from center
    unsigned int src_offset_x = (src_width > dst_width) ? (src_width - dst_width) / 2 : 0;
    unsigned int src_offset_y = (src_height > dst_height) ? (src_height - dst_height) / 2 : 0;

    // Blit source to destination
    size_t bpp = video_buffer.bpp;
    for (unsigned int y = 0; y < blit_height; y++) {
        unsigned int sy = y;
        if (halve_height) {
            sy <<= 1;
        }
        unsigned char *dr = dst + (offset_y + y) * video_buffer.pitch + offset_x * bpp;
        const unsigned char *sr = src + (src_offset_y + sy) * pitch + src_offset_x * bpp * src_px_factor;
        if (halve_width) {
            for (unsigned int x = 0; x < blit_width; x++) {
                if (bpp == 2) {
                    *(unsigned short *)dr = *(const unsigned short *)sr;
                } else if (bpp == 4) {
                    *(unsigned int *)dr = *(const unsigned int *)sr;
                }
                sr += bpp << 1;
                dr += bpp;
            }
        } else {
            memcpy(dr, sr, blit_width * bpp);
        }
    }
}

static void blit_scale(
    const void *data, unsigned width, unsigned height, size_t pitch,
    unsigned int dest_width, unsigned int dest_height
)
{
    // Based on https://www.reddit.com/r/C_Programming/comments/16j7k4d/optimizing_image_downsampling_for_speed/
    unsigned char *dst = (unsigned char *) video_buffer.data;
    const unsigned char *src = (const unsigned char *) data;

    // clamp dimensions
    dest_width = MIN(dest_width, video_buffer.width);
    dest_height = MIN(dest_height, video_buffer.height);

    // clamp offsets
    int offset_y = MAX((video_buffer.height - (int) dest_height) / 2, 0);
    int offset_x = MAX((video_buffer.width - (int) dest_width) / 2, 0);

    int src_width = width;
    int src_height = height;
    int bpp = video_buffer.bpp;

    float rx = src_width / (float) (dest_width + 1);
    float ry = src_height / (float) (dest_height + 1);

    dst += offset_y * video_buffer.pitch;
    for (int y = 0; y < (int) dest_height; ++y) {
        int y1 = y * ry;
        int y2 = (y + 1) * ry;
        if (y1 == y2) {
            y2++; // ensure at least one row is sampled
        }

        const unsigned char *sr = src + y1 * src_width * bpp;
        unsigned char *dr = dst + offset_x * bpp;

        for (int x = 0; x < (int) dest_width; ++x) {
            int x1 = x * rx;
            int x2 = (x + 1) * rx;

            unsigned int r = 0;
            unsigned int g = 0;
            unsigned int b = 0;
            const unsigned char *pixels = sr;

            for (int i = y1; i < y2; ++i) {
                for (int j = x1; j < x2; ++j) {
                    if (pixel_format == RED__GEOMETRY__PIXEL_FORMAT__PF_ARGB8888) {
                        unsigned int c = ((unsigned int *)pixels)[j];
                        r += RED_ARGB8888(c);
                        g += GREEN_ARGB8888(c);
                        b += BLUE_ARGB8888(c);
                    } else if (pixel_format == RED__GEOMETRY__PIXEL_FORMAT__PF_RGB565) {
                        unsigned short c = ((unsigned short *)pixels)[j];
                        r += RED_RGB565(c);
                        g += GREEN_RGB565(c);
                        b += BLUE_RGB565(c);
                    }
                }
                pixels += src_width * bpp;
            }

            int pixel_count = (x2 - x1) * (y2 - y1);
            r /= pixel_count;
            g /= pixel_count;
            b /= pixel_count;

            if (pixel_format == RED__GEOMETRY__PIXEL_FORMAT__PF_ARGB8888) {
                (*(unsigned int *)dr) = RGB_ARGB8888(r,g,b);
            } else if (pixel_format == RED__GEOMETRY__PIXEL_FORMAT__PF_RGB565) {
                (*(unsigned short *)dr) = RGB_RGB565(r,g,b);
            }

            dr += bpp;
        }
        dst += video_buffer.pitch;
    }
}
