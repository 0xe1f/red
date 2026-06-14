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

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "filter.h"

static uint8_t *phosphor_buf      = NULL;
static size_t   phosphor_buf_size = 0;

void filter_free(void)
{
    free(phosphor_buf);
    phosphor_buf = NULL;
    phosphor_buf_size = 0;
}

void filter_apply(const FilterOptions *opts, VideoBuffer *buf)
{
    if (!buf || !buf->data || buf->bpp == 0) {
        return;
    }

    bool is_argb = (buf->bpp == 4);
    unsigned w = buf->width;
    unsigned h = buf->height;

    // --- Brightness ---
    if (opts->brightness >= 0.0f && opts->brightness < 1.0f) {
        float f = opts->brightness;
        if (is_argb) {
            uint32_t *px = (uint32_t *) buf->data;
            for (unsigned i = 0; i < w * h; i++) {
                uint8_t r = (uint8_t)(((px[i] >> 16) & 0xFF) * f);
                uint8_t g = (uint8_t)(((px[i] >>  8) & 0xFF) * f);
                uint8_t b = (uint8_t)(((px[i]      ) & 0xFF) * f);
                px[i] = (px[i] & 0xFF000000u) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
            }
        } else {
            uint16_t *px = (uint16_t *) buf->data;
            for (unsigned i = 0; i < w * h; i++) {
                uint8_t r = (uint8_t)(((px[i] >> 11) & 0x1F) * f);
                uint8_t g = (uint8_t)(((px[i] >>  5) & 0x3F) * f);
                uint8_t b = (uint8_t)(((px[i]      ) & 0x1F) * f);
                px[i] = (uint16_t)(((r & 0x1F) << 11) | ((g & 0x3F) << 5) | (b & 0x1F));
            }
        }
    }

    // --- Phosphor / afterglow ---
    if (opts->phosphor > 0.0f && opts->phosphor < 1.0f) {
        if (phosphor_buf_size != buf->size) {
            free(phosphor_buf);
            phosphor_buf = calloc(buf->size, 1);
            phosphor_buf_size = phosphor_buf ? buf->size : 0;
        }
        if (phosphor_buf) {
            float decay = opts->phosphor;
            float fresh = 1.0f - decay;
            if (is_argb) {
                uint32_t *cur  = (uint32_t *) buf->data;
                uint32_t *hist = (uint32_t *) phosphor_buf;
                for (unsigned i = 0; i < w * h; i++) {
                    uint8_t r = (uint8_t)(((cur[i]  >> 16) & 0xFF) * fresh + ((hist[i] >> 16) & 0xFF) * decay);
                    uint8_t g = (uint8_t)(((cur[i]  >>  8) & 0xFF) * fresh + ((hist[i] >>  8) & 0xFF) * decay);
                    uint8_t b = (uint8_t)(((cur[i]       ) & 0xFF) * fresh + ((hist[i]       ) & 0xFF) * decay);
                    cur[i]  = (cur[i] & 0xFF000000u) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
                    hist[i] = cur[i];
                }
            } else {
                uint16_t *cur  = (uint16_t *) buf->data;
                uint16_t *hist = (uint16_t *) phosphor_buf;
                for (unsigned i = 0; i < w * h; i++) {
                    uint8_t r = (uint8_t)(((cur[i]  >> 11) & 0x1F) * fresh + ((hist[i] >> 11) & 0x1F) * decay);
                    uint8_t g = (uint8_t)(((cur[i]  >>  5) & 0x3F) * fresh + ((hist[i] >>  5) & 0x3F) * decay);
                    uint8_t b = (uint8_t)(((cur[i]       ) & 0x1F) * fresh + ((hist[i]       ) & 0x1F) * decay);
                    cur[i]  = (uint16_t)((r << 11) | (g << 5) | b);
                    hist[i] = cur[i];
                }
            }
        }
    }
}
