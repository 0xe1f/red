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

#ifndef __RETRO_SYM_H__
#define __RETRO_SYM_H__

#include <stdbool.h>
#include <stddef.h>
#include "libretro.h"

#define RETRO_SYMBOLS(X) \
    X(retro_api_version, unsigned, void) \
    X(retro_get_system_info, void, struct retro_system_info*) \
    X(retro_load_game, bool, const struct retro_game_info*) \
    X(retro_set_video_refresh, void, retro_video_refresh_t) \
    X(retro_set_audio_sample, void, retro_audio_sample_t) \
    X(retro_set_audio_sample_batch, void, retro_audio_sample_batch_t) \
    X(retro_set_input_poll, void, retro_input_poll_t) \
    X(retro_set_input_state, void, retro_input_state_t) \
    X(retro_set_environment, void, retro_environment_t) \
    X(retro_init, void, void) \
    X(retro_run, void, void) \
    X(retro_unload_game, void, void) \
    X(retro_deinit, void, void) \
    X(retro_get_system_av_info, void, struct retro_system_av_info*) \
    /* X(retro_reset, void, void) */ \
    X(retro_serialize_size, size_t, void) \
    X(retro_serialize, bool, void*, size_t) \
    X(retro_unserialize, bool, const void*, size_t) \
    /* X(retro_set_controller_port_device, void, unsigned port, unsigned device) */ \
    /* X(retro_load_game_special, bool, unsigned, const struct retro_game_info *, size_t) */ \
    /* X(retro_cheat_reset, void, void) */ \
    /* X(retro_cheat_set, void, unsigned index, bool enabled, const char *code) */ \
    X(retro_get_memory_data, void*, unsigned) \
    X(retro_get_memory_size, size_t, unsigned) \
    X(retro_get_region, unsigned, void)

#endif // __RETRO_SYM_H__
