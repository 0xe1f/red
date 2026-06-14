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

#ifndef __FILTER_H__
#define __FILTER_H__

#include "video.h"

// Post-blit filter options. Applied in order: brightness → phosphor.
// All filters are disabled by default.
typedef struct {
    float brightness;  // channel scale: 1.0 = no change, 0.0 = black
    float phosphor;    // afterglow decay: 0.0 = disabled, approaching 1.0 = more trail
} FilterOptions;

// Apply enabled filters to buf in-place. Output dimensions are unchanged.
// Silently does nothing if buf->data is NULL.
void filter_apply(const FilterOptions *opts, VideoBuffer *buf);

// Free all internal state.
void filter_free(void);

#endif // __FILTER_H__
