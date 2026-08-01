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
#include "frame.h"
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
    ((pf) == PF_RGBA8888 || (pf) == PF_ARGB8888 ? 4 : \
     (pf) == PF_RGB565   || (pf) == PF_RGBA5551  ? 2 : 0)

extern struct retro_system_av_info av_info;
extern Rotation rotation;
extern PixelFormat pixel_format;

static const VideoFont font_8x8 = {
#include "video_font_8x8.inc"
};

const VideoFont *Font8x8 = &font_8x8;

static void blit_plain(VideoBuffer *buffer,
    const void *data, unsigned width, unsigned height, size_t pitch);
static void blit_half(VideoBuffer *buffer,
    const void *data, unsigned width, unsigned height, size_t pitch);
static void blit_scale(VideoBuffer *buffer,
    const void *data, unsigned width, unsigned height, size_t pitch,
    unsigned int dest_width, unsigned int dest_height);

void buffer_realloc_if_needed(VideoBuffer *buffer, int width, int height)
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

void buffer_blit(
    VideoBuffer *buffer,
    ScaleMode scale_mode,
    const void *data, unsigned width, unsigned height, size_t pitch,
    const unsigned char **out, size_t *out_size
)
{
    if (!buffer->data) {
        // No interim buffer, use source directly
        *out = (const unsigned char *) data;
        *out_size = (int) pitch * height;
        return;
    }

    switch (scale_mode) {
        case SCALE_MODE_HALF:
            blit_half(buffer, data, width, height, pitch);
            break;
        case SCALE_MODE_SHORTESTXASPECT: {
            unsigned int dest_width = width;
            unsigned int dest_height = height;
            bool is_portrait = IS_PORTRAIT(rotation);

            if ((width > buffer->width && !is_portrait)
                || (height > buffer->height && is_portrait)
            ) {
                dest_width = (int)(height * av_info.geometry.aspect_ratio);
            } else if ((height > buffer->height && !is_portrait)
                || (width > buffer->width && is_portrait)
            ) {
                dest_height = (int)(width * av_info.geometry.aspect_ratio);
            }
            if (dest_width != width || dest_height != height) {
                blit_scale(buffer, data, width, height, pitch, dest_width, dest_height);
            } else {
                blit_plain(buffer, data, width, height, pitch);
            }
            break;
        }
        case SCALE_MODE_FIT: {
            unsigned int dest_width = width;
            unsigned int dest_height = height;
            bool is_portrait = IS_PORTRAIT(rotation);

            if ((width > buffer->width && !is_portrait)
                || (height > buffer->height && is_portrait)
            ) {
                dest_width = buffer->width;
                dest_height = (int)(dest_width / av_info.geometry.aspect_ratio);
            } else if ((height > buffer->height && !is_portrait)
                || (width > buffer->width && is_portrait)
            ) {
                dest_height = buffer->height;
                dest_width = (int)(dest_height * av_info.geometry.aspect_ratio);
            }
            if (dest_width != width || dest_height != height) {
                blit_scale(buffer, data, width, height, pitch, dest_width, dest_height);
            } else {
                blit_plain(buffer, data, width, height, pitch);
            }
            break;
        }
        case SCALE_MODE_UPSCALE: {
            unsigned int dest_width = width;
            unsigned int dest_height = height;
            bool is_portrait = IS_PORTRAIT(rotation);

            if ((width < buffer->width && !is_portrait)
                || (height < buffer->height && is_portrait)
            ) {
                dest_width = buffer->width;
                dest_height = (int)(dest_width / av_info.geometry.aspect_ratio);
            } else if ((height < buffer->height && !is_portrait)
                || (width < buffer->width && is_portrait)
            ) {
                dest_height = buffer->height;
                dest_width = (int)(dest_height * av_info.geometry.aspect_ratio);
            }
            if (dest_width != width || dest_height != height) {
                blit_scale(buffer, data, width, height, pitch, dest_width, dest_height);
            } else {
                blit_plain(buffer, data, width, height, pitch);
            }
            break;
        }
        case SCALE_MODE_NONE:
        default:
            blit_plain(buffer, data, width, height, pitch);
            break;
    }

    *out = (const unsigned char *) buffer->data;
    *out_size = buffer->size;
}

static void blit_plain(VideoBuffer *buffer,
    const void *data, unsigned width, unsigned height, size_t pitch)
{
    unsigned char *dst = (unsigned char *) buffer->data;
    const unsigned char *src = (const unsigned char *) data;

    // Calculate source dimensions to blit
    unsigned int src_width = width;
    unsigned int src_height = height;
    unsigned int dst_width = buffer->width;
    unsigned int dst_height = buffer->height;

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
    size_t bpp = buffer->bpp;
    for (unsigned int y = 0; y < blit_height; y++) {
        memcpy(
            dst + (offset_y + y) * buffer->pitch + offset_x * bpp,
            src + (src_offset_y + y) * pitch + src_offset_x * bpp,
            blit_width * bpp
        );
    }
}

static void blit_half(VideoBuffer *buffer,
    const void *data, unsigned width, unsigned height, size_t pitch)
{
    unsigned char *dst = (unsigned char *) buffer->data;
    const unsigned char *src = (const unsigned char *) data;

    unsigned int src_width = width;
    bool halve_width = src_width > buffer->width;
    unsigned int src_px_factor = halve_width ? 2 : 1;
    if (halve_width) {
        src_width /= 2;
    }
    unsigned int src_height = height;
    bool halve_height = src_height > buffer->height;
    if (halve_height) {
        src_height /= 2;
    }
    unsigned int dst_width = buffer->width;
    unsigned int dst_height = buffer->height;

    int offset_x = (dst_width > src_width) ? (int)(dst_width - src_width) / 2 : 0;
    int offset_y = (dst_height > src_height) ? (int)(dst_height - src_height) / 2 : 0;

    unsigned int blit_width = (src_width > dst_width) ? dst_width : src_width;
    unsigned int blit_height = (src_height > dst_height) ? dst_height : src_height;

    unsigned int src_offset_x = (src_width > dst_width) ? (src_width - dst_width) / 2 : 0;
    unsigned int src_offset_y = (src_height > dst_height) ? (src_height - dst_height) / 2 : 0;

    size_t bpp = buffer->bpp;
    size_t src_row_step = halve_height ? pitch * 2 : pitch;

    const unsigned char *sr_row = src + src_offset_y * pitch + src_offset_x * bpp * src_px_factor;
    unsigned char *dr_row = dst + offset_y * buffer->pitch + offset_x * bpp;

    if (halve_width) {
        size_t src_px_step = bpp * 2;
        if (bpp == 2) {
            for (unsigned int y = 0; y < blit_height; y++) {
                const unsigned char *sr = sr_row;
                unsigned char *dr = dr_row;
                for (unsigned int x = 0; x < blit_width; x++) {
                    *(unsigned short *) dr = *(const unsigned short *) sr;
                    sr += src_px_step;
                    dr += 2;
                }
                sr_row += src_row_step;
                dr_row += buffer->pitch;
            }
        } else if (bpp == 4) {
            for (unsigned int y = 0; y < blit_height; y++) {
                const unsigned char *sr = sr_row;
                unsigned char *dr = dr_row;
                for (unsigned int x = 0; x < blit_width; x++) {
                    *(unsigned int *) dr = *(const unsigned int *) sr;
                    sr += src_px_step;
                    dr += 4;
                }
                sr_row += src_row_step;
                dr_row += buffer->pitch;
            }
        }
    } else {
        for (unsigned int y = 0; y < blit_height; y++) {
            memcpy(dr_row, sr_row, blit_width * bpp);
            sr_row += src_row_step;
            dr_row += buffer->pitch;
        }
    }
}

static void blit_scale(VideoBuffer *buffer,
    const void *data, unsigned width, unsigned height, size_t pitch,
    unsigned int dest_width, unsigned int dest_height
)
{
    unsigned char *dst = (unsigned char *) buffer->data;
    const unsigned char *src = (const unsigned char *) data;

    dest_width = MIN(dest_width, buffer->width);
    dest_height = MIN(dest_height, buffer->height);

    int offset_y = MAX((buffer->height - (int) dest_height) / 2, 0);
    int offset_x = MAX((buffer->width - (int) dest_width) / 2, 0);

    int bpp = buffer->bpp;
    bool is_argb = (pixel_format == PF_ARGB8888);

    // Fixed-point 16.16 step sizes mapping dest [0, dest-1] onto src [0, src-1]
    uint32_t step_x = (width  > 1) ? ((width  - 1) << 16) / (dest_width  > 1 ? dest_width  - 1 : 1) : 0;
    uint32_t step_y = (height > 1) ? ((height - 1) << 16) / (dest_height > 1 ? dest_height - 1 : 1) : 0;

    dst += offset_y * buffer->pitch;

    uint32_t fy = 0;
    for (int y = 0; y < (int) dest_height; y++, fy += step_y) {
        int y0 = fy >> 16;
        int y1 = MIN(y0 + 1, (int) height - 1);
        uint32_t wy1 = fy & 0xffff;
        uint32_t wy0 = 0x10000 - wy1;

        const unsigned char *row0 = src + y0 * pitch;
        const unsigned char *row1 = src + y1 * pitch;
        unsigned char *dr = dst + offset_x * bpp;

        uint32_t fx = 0;
        for (int x = 0; x < (int) dest_width; x++, fx += step_x) {
            int x0 = fx >> 16;
            int x1 = MIN(x0 + 1, (int) width - 1);
            uint32_t wx1 = fx & 0xffff;
            uint32_t wx0 = 0x10000 - wx1;

            // Bilinear weights (each in 0..65536, sum = 65536)
            uint32_t w00 = (wx0 * wy0) >> 16;
            uint32_t w10 = (wx1 * wy0) >> 16;
            uint32_t w01 = (wx0 * wy1) >> 16;
            uint32_t w11 = (wx1 * wy1) >> 16;

            uint32_t r, g, b;
            if (is_argb) {
                uint32_t p00 = ((const uint32_t *) row0)[x0];
                uint32_t p10 = ((const uint32_t *) row0)[x1];
                uint32_t p01 = ((const uint32_t *) row1)[x0];
                uint32_t p11 = ((const uint32_t *) row1)[x1];
                r = (RED_ARGB8888(p00)*w00 + RED_ARGB8888(p10)*w10
                   + RED_ARGB8888(p01)*w01 + RED_ARGB8888(p11)*w11) >> 16;
                g = (GREEN_ARGB8888(p00)*w00 + GREEN_ARGB8888(p10)*w10
                   + GREEN_ARGB8888(p01)*w01 + GREEN_ARGB8888(p11)*w11) >> 16;
                b = (BLUE_ARGB8888(p00)*w00 + BLUE_ARGB8888(p10)*w10
                   + BLUE_ARGB8888(p01)*w01 + BLUE_ARGB8888(p11)*w11) >> 16;
                *(uint32_t *) dr = RGB_ARGB8888(r, g, b);
            } else {
                uint16_t p00 = ((const uint16_t *) row0)[x0];
                uint16_t p10 = ((const uint16_t *) row0)[x1];
                uint16_t p01 = ((const uint16_t *) row1)[x0];
                uint16_t p11 = ((const uint16_t *) row1)[x1];
                r = (RED_RGB565(p00)*w00 + RED_RGB565(p10)*w10
                   + RED_RGB565(p01)*w01 + RED_RGB565(p11)*w11) >> 16;
                g = (GREEN_RGB565(p00)*w00 + GREEN_RGB565(p10)*w10
                   + GREEN_RGB565(p01)*w01 + GREEN_RGB565(p11)*w11) >> 16;
                b = (BLUE_RGB565(p00)*w00 + BLUE_RGB565(p10)*w10
                   + BLUE_RGB565(p01)*w01 + BLUE_RGB565(p11)*w11) >> 16;
                *(uint16_t *) dr = RGB_RGB565(r, g, b);
            }

            dr += bpp;
        }
        dst += buffer->pitch;
    }
}

void buffer_print(VideoBuffer *buffer,
    const VideoFont *font,
    unsigned short x, unsigned short y, const char *text,
    unsigned char color_r, unsigned char color_g, unsigned char color_b
)
{
    if (!text || !buffer->data || !font || !font->data
        || !buffer->width || !buffer->height || !buffer->bpp
    ) {
        return;
    }

    int gw = font->glyph_width;
    int gh = font->glyph_height;
    int bytes_per_row = (gw + 7) / 8;
    int bytes_per_glyph = bytes_per_row * gh;
    size_t bpp = buffer->bpp;
    int buf_w = buffer->width;
    int buf_h = buffer->height;
    int origin_x = x;
    int cx = x;
    int cy = y;

    if (cy >= buf_h) {
        return;
    }

    for (const char *c = text; *c; c++) {
        if (*c == '\n') {
            cy += gh;
            cx = origin_x;
            if (cy >= buf_h) {
                return;
            }
            continue;
        }

        unsigned char ch = (unsigned char) *c;
        if (ch < font->first_code || ch > font->last_code) {
            continue;
        }

        if (cx >= buf_w) {
            continue;
        }
        if (cx + gw <= 0 || cy + gh <= 0) {
            cx += gw;
            continue;
        }

        const unsigned char *glyph =
            (const unsigned char *) font->data
            + (ch - font->first_code) * bytes_per_glyph;

        int j0 = cx < 0 ? -cx : 0;
        int i0 = cy < 0 ? -cy : 0;
        int j1 = cx + gw > buf_w ? buf_w - cx : gw;
        int i1 = cy + gh > buf_h ? buf_h - cy : gh;

        unsigned char *row =
            (unsigned char *) buffer->data
            + (cy + i0) * buffer->pitch
            + (cx + j0) * bpp;

        for (int i = i0; i < i1; i++) {
            unsigned char *px = row;
            for (int j = j0; j < j1; j++) {
                unsigned char bits = glyph[i * bytes_per_row + (j / 8)];
                if (bits & (1 << (7 - (j % 8)))) {
                    if (bpp == 2) {
                        *(unsigned short *) px = RGB_RGB565(color_r, color_g, color_b);
                    } else if (bpp == 4) {
                        *(unsigned int *) px = RGB_ARGB8888(color_r, color_g, color_b);
                    }
                }
                px += bpp;
            }
            row += buffer->pitch;
        }
        cx += gw;
    }
}

void buffer_apply_overlay(VideoBuffer *buffer, VideoBuffer *overlay)
{
    if (!buffer->data || !overlay->data || buffer->bpp != overlay->bpp) {
        return;
    }

    size_t bpp = buffer->bpp;
    unsigned char *dst = buffer->data;
    const unsigned char *src = overlay->data;
    for (int y = 0, h = MIN(buffer->height, overlay->height); y < h; y++) {
        unsigned char *dst_row = dst;
        const unsigned char *src_row = src;
        for (int x = 0, w = MIN(buffer->width, overlay->width); x < w; x++) {
            if (bpp == 2) {
                uint16_t p = *(const uint16_t *) src_row;
                if (p) {
                    *(uint16_t *) dst_row = p;
                }
            } else if (bpp == 4) {
                uint32_t p = *(const uint32_t *) src_row;
                if (p) {
                    *(uint32_t *) dst_row = p;
                }
            }
            dst_row += bpp;
            src_row += bpp;
        }
        dst += buffer->pitch;
        src += overlay->pitch;
    }
}

void buffer_measure_text(const VideoFont *font,
    const char *text,
    unsigned short *out_width, unsigned short *out_height)
{
    if (!font || !text) {
        return;
    }

    unsigned short gw = font->glyph_width;
    unsigned short gh = font->glyph_height;
    unsigned int line_width = 0;
    unsigned int max_width = 0;
    unsigned int lines = 1;

    for (const char *c = text; *c; c++) {
        if (*c == '\n') {
            if (line_width > max_width) {
                max_width = line_width;
            }
            line_width = 0;
            lines++;
            continue;
        }

        unsigned char ch = (unsigned char) *c;
        if (ch < font->first_code || ch > font->last_code) {
            continue;
        }
        line_width += gw;
    }
    if (line_width > max_width) {
        max_width = line_width;
    }

    *out_width = (unsigned short) max_width;
    *out_height = (unsigned short) (lines * gh);
}

void buffer_clear(VideoBuffer *buffer)
{
    if (buffer->data) {
        memset(buffer->data, 0, buffer->size);
    }
}

void buffer_free(VideoBuffer *buffer)
{
    if (buffer->data) {
        free(buffer->data);
        buffer->data = NULL;
        buffer->width = 0;
        buffer->height = 0;
        buffer->bpp = 0;
        buffer->pitch = 0;
        buffer->size = 0;
    }
}
