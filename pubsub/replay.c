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

#include "replay.h"

#include <stdlib.h>
#include <string.h>
#include "log.h"
#include "libretro.h"
#include "core.h"

#define LOG_TAG "replay"

extern CoreFn core;

struct RecordingHeader {
    char magic[4];
    uint32_t version;
};

bool replay_start_recording(Replay *replay, const char *path)
{
    if (replay->mode == MODE_RECORD) {
        log_w(LOG_TAG, "Already recording\n");
        return false;
    } else if (replay->mode == MODE_PLAYBACK) {
        log_w(LOG_TAG, "Can't record while playing back\n");
        return false;
    }

    if (!(replay->file = fopen(path, "w"))) {
        log_e(LOG_TAG, "Failed to open recording at '%s'\n", path);
        return false;
    }

    log_d(LOG_TAG, "Starting recording at '%s'\n", path);

    // Write the recording header to the file
    struct RecordingHeader header = { .magic = "REC", .version = 1 };
    if (fwrite(&header, sizeof(header), 1, replay->file) != 1) {
        log_e(LOG_TAG, "Failed to write header\n");
        replay_stop(replay);
        return false;
    }

    // Write the state
    size_t size = core.retro_serialize_size();
    if (size == 0) {
        log_e(LOG_TAG, "Serialization size is zero\n");
        replay_stop(replay);
        return false;
    }

    void *buffer = malloc(size);
    if (!buffer) {
        log_e(LOG_TAG, "Failed to allocate buffer for serialization\n");
        replay_stop(replay);
        return false;
    }

    bool written = core.retro_serialize(buffer, size) 
        && fwrite(buffer, size, 1, replay->file) == 1;

    free(buffer);
    if (written) {
        replay->mode = MODE_RECORD;
        return true;
    }

    // If we reached here, either serialization or writing failed
    log_e(LOG_TAG, "Failed to serialize or write state\n");
    replay_stop(replay);

    return false;
}

bool replay_start_playback(Replay *replay, const char *path)
{
    if (replay->mode == MODE_RECORD) {
        log_i(LOG_TAG, "Can't start playback while recording\n");
        return false;
    } else if (replay->mode == MODE_PLAYBACK) {
        log_w(LOG_TAG, "Already in playback mode\n");
        return false;
    }

    if (!(replay->file = fopen(path, "r"))) {
        log_e(LOG_TAG, "Failed to open recording at '%s'\n", path);
        return false;
    }

    log_d(LOG_TAG, "Starting replay from '%s'\n", path);

    // Read the recording header from the file
    struct RecordingHeader header;
    if (fread(&header, sizeof(header), 1, replay->file) != 1) {
        log_e(LOG_TAG, "Failed to read header\n");
        replay_stop(replay);
        return false;
    }
    if (strncmp(header.magic, "REC", 3) != 0 || header.version != 1) {
        log_e(LOG_TAG, "Invalid recording header\n");
        replay_stop(replay);
        return false;
    }

    // Read the state
    size_t size = core.retro_serialize_size();
    if (size == 0) {
        log_e(LOG_TAG, "Serialization size is zero\n");
        replay_stop(replay);
        return false;
    }

    void *buffer = malloc(size);
    if (!buffer) {
        log_e(LOG_TAG, "Failed to allocate buffer for serialization\n");
        replay_stop(replay);
        return false;
    }

    bool read = fread(buffer, size, 1, replay->file) == 1
        && core.retro_unserialize(buffer, size);

    free(buffer);
    if (read) {
        replay->mode = MODE_PLAYBACK;
        return true;
    }

    // If we reached here, either serialization or writing failed
    log_e(LOG_TAG, "Failed to unserialize or read state\n");
    replay_stop(replay);

    return false;
}

void replay_stop(Replay *replay)
{
    ReplayMode mode = replay->mode;
    if (replay->file) {
        fclose(replay->file);
        replay->file = NULL;
    }
    replay->mode = MODE_NONE;
    log_d(LOG_TAG, "Stopped %s\n",
        mode == MODE_RECORD ? "recording" : "playback");
}

bool replay_read_input(Replay *replay, void *input_state, size_t size)
{
    if (replay->mode == MODE_PLAYBACK) {
        if (fread(input_state, size, 1, replay->file) == 0) {
            log_i(LOG_TAG, "Reached end of replay file\n");
            replay_stop(replay);
            return false;
        }
    } else {
        log_e(LOG_TAG, "Replay is not in playback mode\n");
        return false;
    }

    return true;
}

bool replay_write_input(Replay *replay, const void *input_state, size_t size)
{
    if (replay->mode == MODE_RECORD) {
        if (fwrite(input_state, size, 1, replay->file) != 1) {
            log_e(LOG_TAG, "Failed to write input state to replay file\n");
            return false;
        }
    } else {
        log_e(LOG_TAG, "Replay is not in record mode\n");
        return false;
    }

    return true;
}
