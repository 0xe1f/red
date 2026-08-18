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
#include <unistd.h>
#include "log.h"
#include "libretro.h"
#include "core.h"

#define LOG_TAG "replay"

extern CoreFn core;

#define MAGIC   "REC"
#define VERSION 1
#define CHUNK_SIZE (1024 * 1024) // 1 MB

static const char *temp_template = "/tmp/red.recording.XXXXXX";

struct RecordingHeader {
    char magic[4];
    uint32_t version;
    uint64_t leading_state_size;
};

struct RecordingFooter {
    uint64_t trailing_state_offset;
    uint64_t file_size;
};

static bool write_footer(Replay *replay);
static void cleanup(Replay *replay);
static bool verify_header(FILE *file, struct RecordingHeader *header);
static bool verify_footer(FILE *file, struct RecordingFooter *footer);
static bool copy(FILE *src, FILE *dest, size_t max_to_copy);
static bool restore_state(FILE *file);

bool replay_start_recording(Replay *replay, const char *path)
{
    if (!path) {
        log_e(LOG_TAG, "No path specified\n");
        return false;
    }

    if (replay->mode == MODE_RECORD) {
        log_e(LOG_TAG, "Already recording\n");
        return false;
    } else if (replay->mode == MODE_PLAYBACK) {
        log_e(LOG_TAG, "Can't record while playing back\n");
        return false;
    }

    if (!(replay->file_path = strdup(path))) {
        log_e(LOG_TAG, "Failed to allocate memory for file path\n");
        return false;
    }

    if (!(replay->file = fopen(replay->file_path, "w"))) {
        log_e(LOG_TAG, "Failed to open recording at '%s'\n", replay->file_path);
        free((void *)replay->file_path);
        return false;
    }

    log_d(LOG_TAG, "Starting recording at '%s'\n", replay->file_path);

    // Get the size of the serialized state
    size_t size = core.retro_serialize_size();
    if (size == 0) {
        log_e(LOG_TAG, "Serialization size is zero\n");
        cleanup(replay);
        return false;
    }

    // Write the recording header to the file
    struct RecordingHeader header = {
        .magic = MAGIC,
        .version = VERSION,
        .leading_state_size = size,
    };
    if (fwrite(&header, sizeof(header), 1, replay->file) != 1) {
        log_e(LOG_TAG, "Failed to write header\n");
        cleanup(replay);
        return false;
    }

    // Serialize the state and write it to the file
    void *buffer = malloc(size);
    if (!buffer) {
        log_e(LOG_TAG, "Failed to allocate buffer for serialization\n");
        cleanup(replay);
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
    cleanup(replay);

    return false;
}

bool replay_continue_recording(Replay *replay, const char *path)
{
    if (!path) {
        log_e(LOG_TAG, "No path specified\n");
        return false;
    }

    if (replay->mode == MODE_RECORD) {
        log_e(LOG_TAG, "Already recording\n");
        return false;
    } else if (replay->mode == MODE_PLAYBACK) {
        log_e(LOG_TAG, "Can't record while playing back\n");
        return false;
    }

    // Create paths
    if (!(replay->file_path = strdup(path))) {
        log_e(LOG_TAG, "Failed to allocate memory for file path\n");
        return false;
    }
    if (!(replay->tmp_path = strdup(temp_template))) {
        log_e(LOG_TAG, "Failed to allocate memory for temporary file path\n");
        cleanup(replay);
        return false;
    }
    int fd = mkstemp((char *) replay->tmp_path);
    if (fd == -1) {
        log_e(LOG_TAG, "Failed to create temporary file\n");
        cleanup(replay);
        return false;
    }

    if (!(replay->file = fdopen(fd, "w"))) {
        log_e(LOG_TAG, "Failed to open temporary file\n");
        close(fd);
        cleanup(replay);
        return false;
    }

    // Copy the existing file for reading
    FILE *src = fopen(replay->file_path, "r");
    if (!src) {
        log_e(LOG_TAG, "Failed to open existing recording at '%s'\n",
            replay->file_path);
        cleanup(replay);
        return false;
    }

    // Read and verify the header and footer of the existing recording
    struct RecordingHeader header;
    if (!verify_header(src, &header)) {
        log_e(LOG_TAG, "Existing recording header is invalid\n");
        fclose(src);
        cleanup(replay);
        return false;
    }
    struct RecordingFooter footer;
    if (!verify_footer(src, &footer)) {
        log_e(LOG_TAG, "Existing recording footer is invalid\n");
        fclose(src);
        cleanup(replay);
        return false;
    }

    // Read the trailing state
    fseek(src, footer.trailing_state_offset, SEEK_SET);
    if (!restore_state(src)) {
        fclose(src);
        cleanup(replay);
        return false;
    }

    // Copy the existing recording to the temporary file
    fseek(src, 0, SEEK_SET);
    if (!copy(src, replay->file, footer.trailing_state_offset)) {
        fclose(src);
        cleanup(replay);
        return false;
    }

    log_d(LOG_TAG, "Resuming recording at '%s'\n", replay->tmp_path);
    replay->mode = MODE_RECORD;
    fclose(src);

    return true;
}

bool replay_start_playback(Replay *replay, const char *path)
{
    if (!path) {
        log_e(LOG_TAG, "No path specified\n");
        return false;
    }

    if (replay->mode == MODE_RECORD) {
        log_e(LOG_TAG, "Can't start playback while recording\n");
        return false;
    } else if (replay->mode == MODE_PLAYBACK) {
        log_e(LOG_TAG, "Already in playback mode\n");
        return false;
    }

    if (!(replay->file = fopen(path, "r"))) {
        log_e(LOG_TAG, "Failed to open recording at '%s'\n", path);
        return false;
    }

    log_d(LOG_TAG, "Starting replay from '%s'\n", path);

    // Validate header
    struct RecordingHeader header;
    if (!verify_header(replay->file, &header)) {
        cleanup(replay);
        return false;
    }

    // Validate footer
    struct RecordingFooter footer;
    if (!verify_footer(replay->file, &footer)) {
        cleanup(replay);
        return false;
    }

    replay->stop_offset = footer.trailing_state_offset;

    // Read the state
    if (restore_state(replay->file)) {
        replay->mode = MODE_PLAYBACK;
        return true;
    }

    // If we reached here, either serialization or writing failed
    log_e(LOG_TAG, "Failed to unserialize or read state\n");
    cleanup(replay);

    return false;
}

void replay_abort(Replay *replay)
{
    ReplayMode mode = replay->mode;
    if (replay->tmp_path) {
        unlink(replay->tmp_path);
    }
    cleanup(replay);
    if (mode == MODE_RECORD) {
        log_d(LOG_TAG, "Aborted recording\n");
    } else {
        log_d(LOG_TAG, "Aborted playback\n");
    }
}

void replay_end(Replay *replay)
{
    ReplayMode mode = replay->mode;
    if (mode == MODE_RECORD) {
        write_footer(replay);
        fclose(replay->file);
        replay->file = NULL;
        if (replay->tmp_path && rename(replay->tmp_path, replay->file_path) != 0) {
            log_e(LOG_TAG, "Failed to replace '%s' with '%s'\n",
                replay->file_path, replay->tmp_path);
        }
    }
    cleanup(replay);
    if (mode == MODE_RECORD) {
        log_d(LOG_TAG, "Stopped recording\n");
    } else {
        log_d(LOG_TAG, "Stopped playback\n");
    }
}

bool replay_read_input(Replay *replay, void *input_state, size_t size)
{
    if (replay->mode == MODE_PLAYBACK) {
        if (ftell(replay->file) >= replay->stop_offset) {
            log_i(LOG_TAG, "Reached stop offset in replay file\n");
            replay_end(replay);
            return false;
        }
        if (fread(input_state, size, 1, replay->file) == 0) {
            log_w(LOG_TAG, "Replay file ended abruptly\n");
            replay_abort(replay);
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
            replay_abort(replay);
            return false;
        }
    } else {
        log_e(LOG_TAG, "Replay is not in record mode\n");
        return false;
    }

    return true;
}

static bool write_footer(Replay *replay)
{
    size_t offset = ftell(replay->file);

    // Get the size of the serialized state
    size_t size = core.retro_serialize_size();
    if (size == 0) {
        log_e(LOG_TAG, "Serialization size is zero\n");
        return false;
    }

    // Serialize the state and write it to the file
    void *buffer = malloc(size);
    if (!buffer) {
        log_e(LOG_TAG, "Failed to allocate buffer for serialization\n");
        return false;
    }

    bool written = core.retro_serialize(buffer, size) 
        && fwrite(buffer, size, 1, replay->file) == 1;
    free(buffer);
    if (!written) {
        log_e(LOG_TAG, "Failed to serialize or write trailing state\n");
        return false;
    }

    // Write the footer
    size_t footer_size = sizeof(struct RecordingFooter);
    struct RecordingFooter footer = {
        .trailing_state_offset = offset,
        .file_size = ftell(replay->file) + footer_size,
    };
    if (fwrite(&footer, footer_size, 1, replay->file) != 1) {
        log_e(LOG_TAG, "Failed to write footer\n");
        return false;
    }

    return true;
}

static void cleanup(Replay *replay)
{
    if (replay->file) {
        fclose(replay->file);
        replay->file = NULL;
    }

    free((void *)replay->file_path);
    free((void *)replay->tmp_path);
    replay->file_path = NULL;
    replay->tmp_path = NULL;

    replay->mode = MODE_NONE;
}

static bool verify_header(FILE *file, struct RecordingHeader *header)
{
    // Read the recording header from the file
    if (fread(header, sizeof(*header), 1, file) != 1) {
        log_e(LOG_TAG, "Failed to read header\n");
        return false;
    }

    // Verify basics
    if (strncmp(header->magic, MAGIC, 3) != 0 || header->version != VERSION) {
        log_e(LOG_TAG, "Invalid recording header\n");
        return false;
    }

    return true;
}

static bool verify_footer(FILE *file, struct RecordingFooter *footer)
{
    // Unlike verify_header, on success, this function resets the read position
    // to the location it was when the function was called.

    // Record offset post-header
    size_t eoh = ftell(file);

    // Read the footer
    if (fseek(file, -sizeof(struct RecordingFooter), SEEK_END) != 0) {
        log_e(LOG_TAG, "Failed to seek to footer\n");
        return false;
    }
    if (fread(footer, sizeof(*footer), 1, file) != 1) {
        log_e(LOG_TAG, "Failed to read footer\n");
        return false;
    }

    // Basic sanity check
    if (ftell(file) != footer->file_size) {
        log_e(LOG_TAG, "Footer file size does not match actual file size\n");
        return false;
    }

    // Reposition to end of header
    fseek(file, eoh, SEEK_SET);

    return true;
}

static bool copy(FILE *src, FILE *dest, size_t max_to_copy)
{
    static char buffer[CHUNK_SIZE];
    log_d(LOG_TAG, "Copying %zu bytes of input\n", max_to_copy);

    size_t bytes_remaining = max_to_copy;
    while (bytes_remaining > 0) {
        size_t to_read = (bytes_remaining < CHUNK_SIZE)
            ? bytes_remaining : CHUNK_SIZE;

        // Read data into the buffer
        size_t read = fread(buffer, 1, to_read, src);
        if (read > 0) {
            if (fwrite(buffer, read, 1, dest) != 1) {
                log_e(LOG_TAG, "Failed to write input data to destination file\n");
                return false;
            }
        } else if (read == 0) {
            break;
        }

        // Decrement the counter
        bytes_remaining -= read;
    }

    return bytes_remaining == 0;
}

static bool restore_state(FILE *file)
{
    // Read the state
    size_t size = core.retro_serialize_size();
    if (size == 0) {
        log_e(LOG_TAG, "Serialization size is zero\n");
        return false;
    }

    void *buffer = malloc(size);
    if (!buffer) {
        log_e(LOG_TAG, "Failed to allocate buffer for serialization\n");
        return false;
    }

    bool read = fread(buffer, size, 1, file) == 1
        && core.retro_unserialize(buffer, size);

    free(buffer);

    return read;
}
