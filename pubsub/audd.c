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

#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <SDL.h>

#include "audd_args.h"
#include "audio.h"
#include "audd_xm.h"
#include "log.h"
#include "timing.h"

#define LOG_TAG "audd"

#define FRAME_TIMEOUT_US 1000000.0 // 1s

static ArgsOptions args = {0};
static bool exit_main_loop = false;
static Audio audio = {0};
static SDL_mutex *audio_lock = NULL;
static FILE *log_file = NULL;
static double last_frame_time = 0;
static uint16_t opened_rate = 0;
static uint8_t opened_channels = 0;

static void render(const AudFrame *frame);
static void xm_callback(const AudFrame *frame);
static void sigint_callback(int s);
static void silence_audio(void);
static void clean_up(void);

static void render(const AudFrame *frame)
{
    const AudFrameHeader *hdr = &frame->header;
    if (hdr->sample_rate == 0 || hdr->channel_count == 0) {
        return;
    }

    SDL_LockMutex(audio_lock);
    if (!audio.sound_open ||
        hdr->sample_rate != opened_rate ||
        hdr->channel_count != opened_channels) {
        log_d(LOG_TAG, "Audio format: %u Hz, %u ch\n",
            hdr->sample_rate, hdr->channel_count);
        audio_stop(&audio);
        if (!audio_start(&audio, hdr->sample_rate, hdr->channel_count)) {
            opened_rate = 0;
            opened_channels = 0;
            SDL_UnlockMutex(audio_lock);
            return;
        }
        opened_rate = hdr->sample_rate;
        opened_channels = hdr->channel_count;
    }

    if (frame->content_size >= sizeof(int16_t)) {
        audio_write(&audio, (const short *)frame->content,
            (int)(frame->content_size / sizeof(int16_t)), true);
    }
    SDL_UnlockMutex(audio_lock);
}

static void xm_callback(const AudFrame *frame)
{
    last_frame_time = micros();
    render(frame);
}

static void sigint_callback(int s)
{
    log_i(LOG_TAG, "Caught SIGINT, exiting...\n");
    exit_main_loop = true;
}

static void silence_audio(void)
{
    if (!audio_lock) {
        return;
    }
    SDL_LockMutex(audio_lock);
    if (audio.sound_open) {
        audio_stop(&audio);
        opened_rate = 0;
        opened_channels = 0;
    }
    SDL_UnlockMutex(audio_lock);
}

static void clean_up(void)
{
    xm_cleanup();
    silence_audio();
    audio_cleanup(&audio);
    if (audio_lock) {
        SDL_DestroyMutex(audio_lock);
        audio_lock = NULL;
    }
    args_free(&args);
    if (log_file != NULL) {
        fclose(log_file);
        log_file = NULL;
    }
}

int main(int argc, char **argv)
{
    if (!args_parse(argc, (const char **)argv, &args)) {
        return 1;
    }

    log_set_level(args.log_level);

    if (args.log_path) {
        FILE *f = fopen(args.log_path, args.log_overwrite ? "w" : "a");
        if (!f) {
            log_f(LOG_TAG, "Failed to open log file '%s'\n", args.log_path);
            clean_up();
            return 1;
        }
        log_file = f;
        log_set_fd(f);

        log_i(LOG_TAG, "%s output to '%s'\n",
            args.log_overwrite ? "Writing" : "Appending", args.log_path);
    }

    audio_init(&audio);
    audio_lock = SDL_CreateMutex();
    if (!audio_lock) {
        log_f(LOG_TAG, "Couldn't create audio mutex\n");
        clean_up();
        return 1;
    }

    signal(SIGINT, sigint_callback);

    xm_init(args.server_url);
    xm_set_callback(xm_callback);
    while (!exit_main_loop) {
        // Virtually all work is done via callbacks, and SIGINT
        // will cause sleep to be interrupted anyway
        sleep(1);
        if (last_frame_time > 0 && micros() - last_frame_time > FRAME_TIMEOUT_US) {
            log_d(LOG_TAG, "No frames received for %.02f seconds, stopping audio...\n",
                (micros() - last_frame_time) / 1000000.0);
            silence_audio();
            last_frame_time = 0.0;
        }
    }

    log_i(LOG_TAG, "done\n");
    clean_up();

    return 0;
}
