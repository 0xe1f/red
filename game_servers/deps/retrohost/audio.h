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

// Code is based on the audio engine of GearGrafx
// Geargrafx - PC Engine / TurboGrafx Emulator
// Copyright (C) 2024  Ignacio Sanchez
// Licensed under GPL

#ifndef __AUDIO_H__
#define __AUDIO_H__

typedef struct {
    unsigned short* volatile buffers;
    void* volatile free_sem;
    unsigned short* volatile currently_playing;
    int volatile read_buffer;
    int write_buffer;
    int write_position;
    bool sound_open;
    bool sync_output;
    int buffer_size;
    int buffer_count;
} Audio;

void audio_init(Audio *audio);
void audio_cleanup(Audio *audio);
bool audio_start(Audio *audio, int sample_rate, int channel_count);
void audio_stop(Audio *audio);
void audio_write(Audio *audio, const short *samples, int count, bool sync);

#endif // __AUDIO_H__
