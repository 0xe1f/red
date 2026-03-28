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

#ifndef __INPUT_H__
#define __INPUT_H__

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    unsigned long delay_ms;
    unsigned long duration_ms;
    unsigned int keycode;
} DeferredKeypress;

void input_init();
void input_poll();
void input_clean_up();
int16_t callback_input_state(unsigned port, unsigned device, unsigned index, unsigned id);
bool input_parse_deferred_keypress(const char *spec, DeferredKeypress *out);
void input_schedule_keypress(const DeferredKeypress *keypress);

#endif // __INPUT_H__
