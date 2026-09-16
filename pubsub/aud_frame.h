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

#ifndef __AUD_FRAME_H__
#define __AUD_FRAME_H__

#include <stddef.h>
#include <stdint.h>

typedef struct __attribute__((packed)) {
    uint16_t sample_rate;
    uint8_t channel_count;
    uint16_t buffer_size;
} AudFrameHeader;

typedef struct {
    AudFrameHeader  header;
    uint8_t     *content;
    size_t       content_size;
} AudFrame;

#endif // __AUD_FRAME_H__
