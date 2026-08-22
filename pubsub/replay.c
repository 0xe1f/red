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

#define _GNU_SOURCE
#define _FILE_OFFSET_BITS 64

#include "replay.h"

#include <fcntl.h>
#include <stdio.h>
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

struct __attribute__((__packed__)) RecordingHeader {
    char magic[4];
    uint32_t version;
    uint64_t start_state_uncompressed_size;
};

struct __attribute__((__packed__)) RecordingFooter {
    uint64_t input_frame_offset;
    uint64_t input_frame_count;
    uint64_t trailing_state_offset;
};

static bool write_footer(Replay *replay);
static void cleanup(Replay *replay);
static bool verify_header(int fd, struct RecordingHeader *header);
static bool verify_footer(int fd, struct RecordingFooter *footer);
static bool copy_file(int src, int dest, uint64_t length);
static bool restore_state(int fd, size_t size);
static bool write_state(int fd, size_t size);
static bool read_all(int fd, void *buf, size_t size);
static bool write_all(int fd, const void *buf, size_t size);
static gzFile attach_gz(int fd, const char *mode);
static bool open_inputs(Replay *replay, const char *mode);
static char *sibling_temp_path(const char *path);

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

    if ((replay->file_fd = open(replay->file_path, O_RDWR | O_CREAT | O_TRUNC, 0666)) < 0) {
        log_e(LOG_TAG, "Failed to open recording at '%s'\n", replay->file_path);
        free((void *)replay->file_path);
        replay->file_path = NULL;
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
        .start_state_uncompressed_size = size,
    };
    if (!write_all(replay->file_fd, &header, sizeof(header))) {
        log_e(LOG_TAG, "Failed to write header\n");
        cleanup(replay);
        return false;
    }

    if (!write_state(replay->file_fd, size)) {
        log_e(LOG_TAG, "Failed to serialize or write state\n");
        cleanup(replay);
        return false;
    }

    off_t offset = lseek(replay->file_fd, 0, SEEK_CUR);
    if (offset < 0 || !open_inputs(replay, "wb")) {
        log_e(LOG_TAG, "Failed to begin input stream\n");
        cleanup(replay);
        return false;
    }

    replay->input_frame_offset = (uint64_t)offset;
    replay->input_frame_count = 0;
    replay->mode = MODE_RECORD;
    return true;
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
    if (!(replay->tmp_path = sibling_temp_path(replay->file_path))) {
        log_e(LOG_TAG, "Failed to allocate memory for temporary file path\n");
        cleanup(replay);
        return false;
    }
    if ((replay->file_fd = mkstemp((char *)replay->tmp_path)) < 0) {
        log_e(LOG_TAG, "Failed to create temporary file\n");
        cleanup(replay);
        return false;
    }

    // Copy the existing file for reading
    int src = open(replay->file_path, O_RDONLY);
    if (src < 0) {
        log_e(LOG_TAG, "Failed to open existing recording at '%s'\n",
            replay->file_path);
        cleanup(replay);
        return false;
    }

    // Read and verify the header and footer of the existing recording
    struct RecordingHeader header;
    if (!verify_header(src, &header)) {
        log_e(LOG_TAG, "Existing recording header is invalid\n");
        close(src);
        cleanup(replay);
        return false;
    }
    struct RecordingFooter footer;
    if (!verify_footer(src, &footer)) {
        log_e(LOG_TAG, "Existing recording footer is invalid\n");
        close(src);
        cleanup(replay);
        return false;
    }

    // Read the trailing state
    if (lseek(src, (off_t)footer.trailing_state_offset, SEEK_SET) < 0
        || !restore_state(src, core.retro_serialize_size())) {
        close(src);
        cleanup(replay);
        return false;
    }

    // Copy header, start state, and inputs (omit trailing state and footer)
    if (!copy_file(src, replay->file_fd, footer.trailing_state_offset)) {
        close(src);
        cleanup(replay);
        return false;
    }
    close(src);

    if (lseek(replay->file_fd, (off_t)footer.trailing_state_offset, SEEK_SET) < 0
        || !open_inputs(replay, "wb")) {
        log_e(LOG_TAG, "Failed to resume input stream\n");
        cleanup(replay);
        return false;
    }

    log_d(LOG_TAG, "Resuming recording at '%s'\n", replay->tmp_path);
    replay->input_frame_offset = footer.input_frame_offset;
    replay->input_frame_count = footer.input_frame_count;
    replay->mode = MODE_RECORD;
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

    if ((replay->file_fd = open(path, O_RDONLY)) < 0) {
        log_e(LOG_TAG, "Failed to open recording at '%s'\n", path);
        return false;
    }

    log_d(LOG_TAG, "Starting replay from '%s'\n", path);

    // Validate header
    struct RecordingHeader header;
    if (!verify_header(replay->file_fd, &header)) {
        cleanup(replay);
        return false;
    }

    // Validate footer
    struct RecordingFooter footer;
    if (!verify_footer(replay->file_fd, &footer)) {
        cleanup(replay);
        return false;
    }

    size_t size = core.retro_serialize_size();
    if (size == 0 || size != header.start_state_uncompressed_size) {
        log_e(LOG_TAG, "Serialization size mismatch\n");
        cleanup(replay);
        return false;
    }

    replay->input_frame_count = footer.input_frame_count;

    // Read the state, then seek to inputs (gzread read-ahead invalidates offset)
    if (restore_state(replay->file_fd, size)
        && lseek(replay->file_fd, (off_t)footer.input_frame_offset, SEEK_SET) >= 0
        && open_inputs(replay, "rb")) {
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
        if (replay->input_frame_count == 0) {
            log_i(LOG_TAG, "Reached stop offset in replay file\n");
            replay_end(replay);
            return false;
        }
        if (gzread(replay->gz, input_state, size) != (int)size) {
            log_w(LOG_TAG, "Replay file ended abruptly\n");
            replay_abort(replay);
            return false;
        }
        replay->input_frame_count--;
    } else {
        log_e(LOG_TAG, "Replay is not in playback mode\n");
        return false;
    }

    return true;
}

bool replay_write_input(Replay *replay, const void *input_state, size_t size)
{
    if (replay->mode == MODE_RECORD) {
        if (gzwrite(replay->gz, input_state, size) != (int)size) {
            log_e(LOG_TAG, "Failed to write input state to replay file\n");
            replay_abort(replay);
            return false;
        }
        replay->input_frame_count++;
    } else {
        log_e(LOG_TAG, "Replay is not in record mode\n");
        return false;
    }

    return true;
}

static bool write_footer(Replay *replay)
{
    if (replay->gz) {
        gzclose(replay->gz);
        replay->gz = NULL;
    }

    off_t offset = lseek(replay->file_fd, 0, SEEK_CUR);
    if (offset < 0) {
        log_e(LOG_TAG, "Failed to locate trailing state offset\n");
        return false;
    }

    // Get the size of the serialized state
    size_t size = core.retro_serialize_size();
    if (size == 0) {
        log_e(LOG_TAG, "Serialization size is zero\n");
        return false;
    }

    if (!write_state(replay->file_fd, size)) {
        log_e(LOG_TAG, "Failed to serialize or write trailing state\n");
        return false;
    }

    // Write the footer
    struct RecordingFooter footer = {
        .input_frame_offset = replay->input_frame_offset,
        .input_frame_count = replay->input_frame_count,
        .trailing_state_offset = (uint64_t)offset,
    };
    if (!write_all(replay->file_fd, &footer, sizeof(footer))) {
        log_e(LOG_TAG, "Failed to write footer\n");
        return false;
    }

    return true;
}

static void cleanup(Replay *replay)
{
    if (replay->gz) {
        gzclose(replay->gz);
        replay->gz = NULL;
    }

    if (replay->file_fd >= 0) {
        close(replay->file_fd);
        replay->file_fd = -1;
    }

    if (replay->tmp_path) {
        unlink(replay->tmp_path);
    }

    free((void *)replay->file_path);
    free((void *)replay->tmp_path);
    replay->file_path = NULL;
    replay->tmp_path = NULL;

    replay->input_frame_offset = 0;
    replay->input_frame_count = 0;
    replay->mode = MODE_NONE;
}

static bool verify_header(int fd, struct RecordingHeader *header)
{
    // Read the recording header from the file
    if (!read_all(fd, header, sizeof(*header))) {
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

static bool verify_footer(int fd, struct RecordingFooter *footer)
{
    // Unlike verify_header, on success, this function resets the read position
    // to the location it was when the function was called.

    // Record offset post-header
    off_t eoh = lseek(fd, 0, SEEK_CUR);
    if (eoh < 0) {
        log_e(LOG_TAG, "Failed to record header offset\n");
        return false;
    }

    // Read the footer
    if (lseek(fd, -(off_t)sizeof(struct RecordingFooter), SEEK_END) < 0) {
        log_e(LOG_TAG, "Failed to seek to footer\n");
        return false;
    }
    if (!read_all(fd, footer, sizeof(*footer))) {
        log_e(LOG_TAG, "Failed to read footer\n");
        return false;
    }

    // Reposition to end of header
    if (lseek(fd, eoh, SEEK_SET) < 0) {
        log_e(LOG_TAG, "Failed to seek to end of header\n");
        return false;
    }

    return true;
}

static bool copy_file(int src, int dest, uint64_t length)
{
    log_d(LOG_TAG, "Copying %llu bytes of input\n",
        (unsigned long long)length);

    off_t off_in = 0;
    off_t off_out = 0;
    uint64_t remaining = length;
    while (remaining > 0) {
        size_t chunk = remaining > SIZE_MAX ? SIZE_MAX : (size_t)remaining;
        ssize_t copied = copy_file_range(src, &off_in, dest, &off_out, chunk, 0);
        if (copied <= 0) {
            log_e(LOG_TAG, "Failed to copy recording\n");
            return false;
        }
        remaining -= (uint64_t)copied;
    }

    return true;
}

static bool restore_state(int fd, size_t size)
{
    if (size == 0) {
        log_e(LOG_TAG, "Serialization size is zero\n");
        return false;
    }

    void *buffer = malloc(size);
    if (!buffer) {
        log_e(LOG_TAG, "Failed to allocate buffer for serialization\n");
        return false;
    }

    gzFile gz = attach_gz(fd, "rb");
    bool ok = gz && gzread(gz, buffer, size) == (int)size
        && core.retro_unserialize(buffer, size);
    if (gz) {
        gzclose(gz);
    }
    free(buffer);

    return ok;
}

static bool write_state(int fd, size_t size)
{
    void *buffer = malloc(size);
    if (!buffer) {
        log_e(LOG_TAG, "Failed to allocate buffer for serialization\n");
        return false;
    }

    gzFile gz = attach_gz(fd, "wb");
    bool written = gz && core.retro_serialize(buffer, size)
        && gzwrite(gz, buffer, size) == (int)size;
    if (gz) {
        written = (gzclose(gz) == Z_OK) && written;
    }
    free(buffer);

    return written;
}

static bool read_all(int fd, void *buf, size_t size)
{
    uint8_t *p = buf;
    while (size > 0) {
        ssize_t n = read(fd, p, size);
        if (n <= 0) {
            return false;
        }
        p += n;
        size -= (size_t)n;
    }
    return true;
}

static bool write_all(int fd, const void *buf, size_t size)
{
    const uint8_t *p = buf;
    while (size > 0) {
        ssize_t n = write(fd, p, size);
        if (n <= 0) {
            return false;
        }
        p += n;
        size -= (size_t)n;
    }
    return true;
}

static gzFile attach_gz(int fd, const char *mode)
{
    int dupfd = dup(fd);
    if (dupfd < 0) {
        return NULL;
    }
    gzFile gz = gzdopen(dupfd, mode);
    if (!gz) {
        close(dupfd);
    }
    return gz;
}

static bool open_inputs(Replay *replay, const char *mode)
{
    replay->gz = attach_gz(replay->file_fd, mode);
    return replay->gz != NULL;
}

static char *sibling_temp_path(const char *path)
{
    size_t len = strlen(path) + sizeof(".XXXXXX");
    char *tmp = malloc(len);
    if (!tmp) {
        return NULL;
    }
    snprintf(tmp, len, "%s.XXXXXX", path);
    return tmp;
}
