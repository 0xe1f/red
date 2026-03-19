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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <SDL.h>
#include "libretro.h"
#include "audio.h"

#define BUFFER_SIZE  2048
#define BUFFER_COUNT 3

static void callback_fill_buffer(void* user_data, unsigned char* buffer, int count);
static unsigned short* buffer_ptr(const Audio *audio, int index);

void audio_init(Audio* audio)
{
    audio->buffers = NULL;
    audio->free_sem = NULL;
    audio->sound_open = false;
    audio->sync_output = true;

    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        fprintf(stderr, "audio: init() failed\n");
    }
}

void audio_cleanup(Audio *audio)
{
    audio_stop(audio);
}

bool audio_start(Audio *audio, int sample_rate, int channel_count)
{
    audio->write_buffer = 0;
    audio->write_position = 0;
    audio->read_buffer = 0;
    audio->buffer_size = BUFFER_SIZE;
    audio->buffer_count = BUFFER_COUNT;

    audio->buffers = (unsigned short *) malloc(audio->buffer_size * audio->buffer_count * sizeof(unsigned short));
    audio->currently_playing = audio->buffers;

    for (int i = 0; i < (audio->buffer_size * audio->buffer_count); i++) {
        audio->buffers[i] = 0;
    }

    audio->free_sem = SDL_CreateSemaphore(audio->buffer_count - 1);
    if (!audio->free_sem) {
        fprintf(stderr, "audio: Couldn't create semaphore\n");
        return false;
    }

    SDL_AudioSpec spec;
    spec.freq = sample_rate;
    spec.format = AUDIO_S16SYS;
    spec.channels = channel_count;
    spec.silence = 0;
    spec.samples = audio->buffer_size / channel_count;
    spec.size = 0;
    spec.callback = callback_fill_buffer;
    spec.userdata = audio;

    if (SDL_OpenAudio(&spec, NULL) < 0) {
        fprintf(stderr, "audio: Couldn't open SDL audio\n");
        return false;
    }

    fprintf(stderr, "audio: frequency: %dHz; channels: %d; samples: %d\n",
        spec.freq, spec.channels, spec.samples);

    SDL_PauseAudio(false);
    audio->sound_open = true;

    return true;
}

void audio_stop(Audio *audio)
{
    if (audio->sound_open) {
        audio->sound_open = false;
        SDL_PauseAudio(true);
        SDL_CloseAudio();
    }

    if (audio->free_sem) {
        SDL_DestroySemaphore((SDL_sem *)audio->free_sem);
        audio->free_sem = NULL;
    }

    free(audio->buffers);
    audio->buffers = NULL;
}

void audio_write(Audio *audio, const short *samples, int count, bool sync)
{
    if (!audio->sound_open) {
        return;
    }

    audio->sync_output = sync;

    while (count) {
        int n = audio->buffer_size - audio->write_position;
        if (n > count) {
            n = count;
        }

        memcpy(
            buffer_ptr(audio, audio->write_buffer) + audio->write_position,
            samples,
            n * sizeof(unsigned short));

        samples += n;
        audio->write_position += n;
        count -= n;

        if (audio->write_position >= audio->buffer_size) {
            audio->write_position = 0;
            audio->write_buffer = (audio->write_buffer + 1) % audio->buffer_count;

            if (audio->sync_output) {
                SDL_SemWait((SDL_sem *) audio->free_sem);
            }
        }
    }
}

static void callback_fill_buffer(void* user_data, unsigned char* buffer, int count)
{
    Audio* audio = (Audio*) user_data;
    SDL_sem *sem = (SDL_sem *) audio->free_sem;
    if ((SDL_SemValue(sem) < (unsigned int)audio->buffer_count - 1) || !audio->sync_output) {
        unsigned short *buf_ptr = buffer_ptr(audio, audio->read_buffer);
        audio->currently_playing = buf_ptr;
        memcpy(buffer, buf_ptr, count);
        audio->read_buffer = (audio->read_buffer + 1) % audio->buffer_count;

        if (audio->sync_output) {
            SDL_SemPost(sem);
        }
    } else {
        memset(buffer, 0, count);
    }
}

static unsigned short* buffer_ptr(const Audio *audio, int index)
{
    return audio->buffers + (index * audio->buffer_size);
}
