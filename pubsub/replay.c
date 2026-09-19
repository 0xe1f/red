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
#include <zlib.h>
#include "log.h"
#include "libretro.h"
#include "core.h"

#define LOG_TAG "replay"

extern CoreFn core;
extern struct retro_system_info system_info;
extern struct retro_system_av_info av_info;

#define MAGIC   "REC"
#define VERSION 2

struct Replay {
    ReplayMode mode;
    const char *file_path;
    const char *tmp_path;
    uint64_t input_frame_offset;
    uint64_t input_frame_count;
    double fps;
    int file_fd;
    gzFile gz;
    uint8_t file_frame_type_count;
    uint8_t *file_frame_type_sizes;
    uint8_t frame_type_count;
    void **frame_content;
    bool *frame_primed;
    uint64_t *type_frames;
    bool peek_valid;
    uint8_t peek_type;
    uint32_t peek_frame_index;
    uint8_t *peek_payload;
};

static uint8_t frame_type_count = 0;
static uint8_t *frame_type_sizes = NULL;
static void **frame_type_defaults = NULL;

struct __attribute__((__packed__)) RecordingHeader {
    char magic[4];
    uint32_t version;
    uint64_t start_state_uncompressed_size;
    char core_name[40];
    char core_version[40];
    uint8_t frame_type_count;
};
// RecordingHeader followed by frame_type_count bytes:
// payload size of each type.

struct __attribute__((__packed__)) RecordingFooter {
    uint64_t input_frame_offset;
    uint64_t input_frame_count;
    uint64_t trailing_state_offset;
    uint32_t duration_ms;
};

struct __attribute__((__packed__)) FrameHeader {
    uint8_t frame_type;
    uint32_t frame_index;
};
// FrameHeader followed by the payload for frame_type
// (length from the header size table).

static Replay *replay_create();
static bool write_footer(Replay *replay);
static void cleanup(Replay *replay);
static void cleanup_and_free(Replay *replay);
static bool read_header(int fd, struct RecordingHeader *header, uint8_t **sizes_out);
static bool verify_footer(int fd, struct RecordingFooter *footer);
static bool copy_file(int src, int dest, off_t src_off, off_t dest_off, uint64_t length);
static bool restore_state(int fd, size_t size);
static bool write_state(int fd, size_t size);
static bool read_all(int fd, void *buf, size_t size);
static bool write_all(int fd, const void *buf, size_t size);
static gzFile attach_gz(int fd, const char *mode);
static bool open_inputs(Replay *replay, const char *mode);
static char *sibling_temp_path(const char *path);
static void copy_core_field(char *dst, size_t dst_len, const char *src);
static uint32_t duration_ms_from(uint64_t frames, double fps);
static bool header_matches_runtime(const struct RecordingHeader *header,
    const uint8_t *file_sizes, bool for_resume);
static bool frame_types_ready();
static void free_frame_type_tables();
static bool write_recording_header(int fd, uint64_t start_state_size);
static bool alloc_session_frames(Replay *replay);
static bool emit_event(Replay *replay, uint8_t frame_ix, uint32_t frame_index,
    const void *content, size_t size);
static bool gz_skip(gzFile gz, size_t size);
static bool refill_peek(Replay *replay);
static bool apply_events_through(Replay *replay, uint64_t frame_index);

Replay* replay_start_recording(const char *path)
{
    if (!path) {
        log_e(LOG_TAG, "No path specified\n");
        return NULL;
    }
    if (!frame_types_ready()) {
        log_e(LOG_TAG, "Frame types not set\n");
        return NULL;
    }

    Replay *replay = replay_create();
    if (!replay) {
        return NULL;
    }

    if (!(replay->file_path = strdup(path))) {
        log_e(LOG_TAG, "Failed to allocate memory for file path\n");
        cleanup_and_free(replay);
        return NULL;
    }

    if ((replay->file_fd = open(replay->file_path, O_RDWR | O_CREAT | O_TRUNC, 0666)) < 0) {
        log_e(LOG_TAG, "Failed to open recording at '%s'\n", replay->file_path);
        cleanup_and_free(replay);
        return NULL;
    }

    log_d(LOG_TAG, "Starting recording at '%s'\n", replay->file_path);

    size_t size = core.retro_serialize_size();
    if (size == 0) {
        log_e(LOG_TAG, "Serialization size is zero\n");
        cleanup_and_free(replay);
        return NULL;
    }

    if (!alloc_session_frames(replay)) {
        log_e(LOG_TAG, "Failed to allocate frame buffers\n");
        cleanup_and_free(replay);
        return NULL;
    }

    if (!write_recording_header(replay->file_fd, size)) {
        log_e(LOG_TAG, "Failed to write header\n");
        cleanup_and_free(replay);
        return NULL;
    }

    if (!write_state(replay->file_fd, size)) {
        log_e(LOG_TAG, "Failed to serialize or write state\n");
        cleanup_and_free(replay);
        return NULL;
    }

    off_t offset = lseek(replay->file_fd, 0, SEEK_CUR);
    if (offset < 0 || !open_inputs(replay, "wb")) {
        log_e(LOG_TAG, "Failed to begin input stream\n");
        cleanup_and_free(replay);
        return NULL;
    }

    replay->file_frame_type_count = frame_type_count;
    replay->input_frame_offset = (uint64_t)offset;
    replay->input_frame_count = 0;
    replay->fps = av_info.timing.fps;
    for (int i = 0; i < replay->frame_type_count; i++) {
        replay->frame_primed[i] = true;
    }
    replay->mode = MODE_RECORD;
    return replay;
}

Replay* replay_continue_recording(const char *path)
{
    if (!path) {
        log_e(LOG_TAG, "No path specified\n");
        return NULL;
    }
    if (!frame_types_ready()) {
        log_e(LOG_TAG, "Frame types not set\n");
        return NULL;
    }

    Replay *replay = replay_create();
    if (!replay) {
        return NULL;
    }

    if (!(replay->file_path = strdup(path))) {
        log_e(LOG_TAG, "Failed to allocate memory for file path\n");
        cleanup_and_free(replay);
        return NULL;
    }
    if (!(replay->tmp_path = sibling_temp_path(replay->file_path))) {
        log_e(LOG_TAG, "Failed to allocate memory for temporary file path\n");
        cleanup_and_free(replay);
        return NULL;
    }
    if ((replay->file_fd = mkstemp((char *)replay->tmp_path)) < 0) {
        log_e(LOG_TAG, "Failed to create temporary file\n");
        cleanup_and_free(replay);
        return NULL;
    }

    int src = open(replay->file_path, O_RDONLY);
    if (src < 0) {
        log_e(LOG_TAG, "Failed to open existing recording at '%s'\n",
            replay->file_path);
        cleanup_and_free(replay);
        return NULL;
    }

    struct RecordingHeader header;
    uint8_t *file_sizes = NULL;
    if (!read_header(src, &header, &file_sizes)
        || !header_matches_runtime(&header, file_sizes, true)) {
        log_e(LOG_TAG, "Existing recording header is invalid\n");
        free(file_sizes);
        close(src);
        cleanup_and_free(replay);
        return NULL;
    }

    struct RecordingFooter footer;
    if (!verify_footer(src, &footer)) {
        log_e(LOG_TAG, "Existing recording footer is invalid\n");
        free(file_sizes);
        close(src);
        cleanup_and_free(replay);
        return NULL;
    }

    if (lseek(src, (off_t)footer.trailing_state_offset, SEEK_SET) < 0
        || !restore_state(src, core.retro_serialize_size())) {
        free(file_sizes);
        close(src);
        cleanup_and_free(replay);
        return NULL;
    }

    uint64_t old_header_len = sizeof(header) + header.frame_type_count;
    uint64_t new_header_len = sizeof(header) + frame_type_count;
    if (footer.trailing_state_offset < old_header_len
        || footer.input_frame_offset < old_header_len) {
        log_e(LOG_TAG, "Existing recording offsets are invalid\n");
        free(file_sizes);
        close(src);
        cleanup_and_free(replay);
        return NULL;
    }

    if (!write_recording_header(replay->file_fd, header.start_state_uncompressed_size)) {
        log_e(LOG_TAG, "Failed to write header\n");
        free(file_sizes);
        close(src);
        cleanup_and_free(replay);
        return NULL;
    }

    uint64_t copy_len = footer.trailing_state_offset - old_header_len;
    if (!copy_file(src, replay->file_fd, (off_t)old_header_len,
            (off_t)new_header_len, copy_len)) {
        free(file_sizes);
        close(src);
        cleanup_and_free(replay);
        return NULL;
    }
    close(src);
    free(file_sizes);
    file_sizes = NULL;

    uint64_t delta = new_header_len - old_header_len;
    if (lseek(replay->file_fd, (off_t)(footer.trailing_state_offset + delta), SEEK_SET) < 0
        || !open_inputs(replay, "wb")) {
        log_e(LOG_TAG, "Failed to resume input stream\n");
        cleanup_and_free(replay);
        return NULL;
    }

    if (!alloc_session_frames(replay)) {
        log_e(LOG_TAG, "Failed to allocate frame buffers\n");
        cleanup_and_free(replay);
        return NULL;
    }

    replay->file_frame_type_count = frame_type_count;
    replay->input_frame_offset = footer.input_frame_offset + delta;
    replay->input_frame_count = footer.input_frame_count;
    replay->fps = av_info.timing.fps;

    for (int i = 0; i < replay->frame_type_count; i++) {
        replay->type_frames[i] = footer.input_frame_count;
        replay->frame_primed[i] = false;
    }

    log_d(LOG_TAG, "Resuming recording at '%s'\n", replay->tmp_path);
    replay->mode = MODE_RECORD;
    return replay;
}

Replay* replay_start_playback(const char *path)
{
    if (!path) {
        log_e(LOG_TAG, "No path specified\n");
        return NULL;
    }
    if (!frame_types_ready()) {
        log_e(LOG_TAG, "Frame types not set\n");
        return NULL;
    }

    Replay *replay = replay_create();
    if (!replay) {
        return NULL;
    }

    if ((replay->file_fd = open(path, O_RDONLY)) < 0) {
        log_e(LOG_TAG, "Failed to open recording at '%s'\n", path);
        cleanup_and_free(replay);
        return NULL;
    }

    log_d(LOG_TAG, "Starting replay from '%s'\n", path);

    struct RecordingHeader header;
    if (!read_header(replay->file_fd, &header, &replay->file_frame_type_sizes)
        || !header_matches_runtime(&header, replay->file_frame_type_sizes, false)) {
        cleanup_and_free(replay);
        return NULL;
    }
    replay->file_frame_type_count = header.frame_type_count;

    struct RecordingFooter footer;
    if (!verify_footer(replay->file_fd, &footer)) {
        cleanup_and_free(replay);
        return NULL;
    }

    size_t size = core.retro_serialize_size();
    if (size == 0 || size != header.start_state_uncompressed_size) {
        log_e(LOG_TAG, "Serialization size mismatch\n");
        cleanup_and_free(replay);
        return NULL;
    }

    replay->input_frame_count = footer.input_frame_count;
    log_v(LOG_TAG, "Replay core '%s' '%s', %u ms\n",
        header.core_name, header.core_version, footer.duration_ms);

    if (!alloc_session_frames(replay)) {
        log_e(LOG_TAG, "Failed to allocate frame buffers\n");
        cleanup_and_free(replay);
        return NULL;
    }

    if (restore_state(replay->file_fd, size)
        && lseek(replay->file_fd, (off_t)footer.input_frame_offset, SEEK_SET) >= 0
        && open_inputs(replay, "rb")) {
        replay->mode = MODE_PLAYBACK;
        return replay;
    }

    log_e(LOG_TAG, "Failed to unserialize or read state\n");
    cleanup_and_free(replay);
    return NULL;
}

void replay_stop(Replay *replay)
{
    if (!replay) {
        return;
    }

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
    } else if (mode == MODE_PLAYBACK) {
        log_d(LOG_TAG, "Stopped playback\n");
    }
}

void replay_destroy(Replay *replay)
{
    cleanup_and_free(replay);
}

ReplayMode replay_mode(const Replay *replay)
{
    return replay ? replay->mode : MODE_NONE;
}

bool replay_set_frame_type_count(uint8_t count)
{
    if (count == frame_type_count) {
        return true;
    }

    free_frame_type_tables();
    if (count == 0) {
        return true;
    }

    frame_type_sizes = calloc(count, sizeof(uint8_t));
    frame_type_defaults = calloc(count, sizeof(void *));
    if (!frame_type_sizes || !frame_type_defaults) {
        log_e(LOG_TAG, "Failed to allocate memory for frame types\n");
        free_frame_type_tables();
        return false;
    }

    frame_type_count = count;
    return true;
}

bool replay_set_frame_type_shape(uint8_t frame_ix, const void *default_state, uint8_t size)
{
    if (frame_ix >= frame_type_count) {
        log_e(LOG_TAG, "Frame index too large: %d >= %d\n",
            frame_ix, frame_type_count);
        return false;
    }

    free(frame_type_defaults[frame_ix]);
    frame_type_defaults[frame_ix] = NULL;
    frame_type_sizes[frame_ix] = 0;

    if (size == 0) {
        return true;
    }
    if (!default_state) {
        log_e(LOG_TAG, "Default state required for frame type %d\n", frame_ix);
        return false;
    }

    if (!(frame_type_defaults[frame_ix] = malloc(size))) {
        log_e(LOG_TAG, "Failed to allocate default state for frame type %d\n", frame_ix);
        return false;
    }
    memcpy(frame_type_defaults[frame_ix], default_state, size);
    frame_type_sizes[frame_ix] = size;
    return true;
}

bool replay_read_frame(Replay *replay, uint8_t frame_ix, void *content, size_t size)
{
    if (!replay || replay->mode != MODE_PLAYBACK) {
        log_e(LOG_TAG, "Replay is not in playback mode\n");
        return false;
    }
    if (frame_ix >= replay->frame_type_count || size != frame_type_sizes[frame_ix]
        || !content || !replay->frame_content[frame_ix]) {
        log_e(LOG_TAG, "Invalid frame type %u or size %zu\n", frame_ix, size);
        return false;
    }

    uint64_t n = replay->type_frames[frame_ix];
    if (n >= replay->input_frame_count) {
        log_i(LOG_TAG, "Reached stop offset in replay file\n");
        replay_stop(replay);
        return false;
    }

    if (!apply_events_through(replay, n)) {
        log_w(LOG_TAG, "Replay file ended abruptly\n");
        cleanup(replay);
        return false;
    }

    memcpy(content, replay->frame_content[frame_ix], size);
    replay->type_frames[frame_ix]++;
    return true;
}

bool replay_write_frame(Replay *replay, uint8_t frame_ix, const void *content, size_t size)
{
    if (!replay || replay->mode != MODE_RECORD) {
        log_e(LOG_TAG, "Replay is not in record mode\n");
        return false;
    }
    if (frame_ix >= replay->frame_type_count || size != frame_type_sizes[frame_ix]
        || !content || !replay->frame_content[frame_ix]) {
        log_e(LOG_TAG, "Invalid frame type %u or size %zu\n", frame_ix, size);
        return false;
    }

    uint64_t n = replay->type_frames[frame_ix];
    if (n > UINT32_MAX) {
        log_e(LOG_TAG, "Frame index exceeds recording limit\n");
        cleanup(replay);
        return false;
    }

    if (!replay->frame_primed[frame_ix]
        || memcmp(replay->frame_content[frame_ix], content, size) != 0) {
        if (!emit_event(replay, frame_ix, (uint32_t)n, content, size)) {
            log_e(LOG_TAG, "Failed to write input state to replay file\n");
            cleanup(replay);
            return false;
        }
        memcpy(replay->frame_content[frame_ix], content, size);
        replay->frame_primed[frame_ix] = true;
    }

    replay->type_frames[frame_ix]++;
    return true;
}

void replay_clean_up()
{
    free_frame_type_tables();
}

static Replay *replay_create()
{
    Replay *replay = calloc(1, sizeof(Replay));
    if (!replay) {
        return NULL;
    }
    replay->file_fd = -1;
    return replay;
}

static bool write_footer(Replay *replay)
{
    if (replay->frame_type_count > 0) {
        uint64_t frames = replay->type_frames[0];
        for (int i = 1; i < replay->frame_type_count; i++) {
            if (replay->type_frames[i] != frames) {
                log_w(LOG_TAG, "Frame type %d count (%llu) != type 0 (%llu)\n",
                    i, (unsigned long long)replay->type_frames[i],
                    (unsigned long long)frames);
                if (replay->type_frames[i] > frames) {
                    frames = replay->type_frames[i];
                }
            }
        }
        replay->input_frame_count = frames;
    }

    if (replay->gz) {
        gzclose(replay->gz);
        replay->gz = NULL;
    }

    off_t offset = lseek(replay->file_fd, 0, SEEK_CUR);
    if (offset < 0) {
        log_e(LOG_TAG, "Failed to locate trailing state offset\n");
        return false;
    }

    size_t size = core.retro_serialize_size();
    if (size == 0) {
        log_e(LOG_TAG, "Serialization size is zero\n");
        return false;
    }

    if (!write_state(replay->file_fd, size)) {
        log_e(LOG_TAG, "Failed to serialize or write trailing state\n");
        return false;
    }

    struct RecordingFooter footer = {
        .input_frame_offset = replay->input_frame_offset,
        .input_frame_count = replay->input_frame_count,
        .trailing_state_offset = (uint64_t)offset,
        .duration_ms = duration_ms_from(replay->input_frame_count, replay->fps),
    };
    if (!write_all(replay->file_fd, &footer, sizeof(footer))) {
        log_e(LOG_TAG, "Failed to write footer\n");
        return false;
    }

    return true;
}

static void cleanup(Replay *replay)
{
    if (!replay) {
        return;
    }

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

    if (replay->frame_content) {
        for (int i = 0; i < replay->frame_type_count; i++) {
            free(replay->frame_content[i]);
        }
        free(replay->frame_content);
        replay->frame_content = NULL;
    }
    free(replay->frame_primed);
    replay->frame_primed = NULL;
    free(replay->type_frames);
    replay->type_frames = NULL;
    free(replay->peek_payload);
    replay->peek_payload = NULL;
    replay->peek_valid = false;
    free(replay->file_frame_type_sizes);
    replay->file_frame_type_sizes = NULL;
    replay->file_frame_type_count = 0;
    replay->frame_type_count = 0;

    free((void *)replay->file_path);
    free((void *)replay->tmp_path);
    replay->file_path = NULL;
    replay->tmp_path = NULL;

    replay->input_frame_offset = 0;
    replay->input_frame_count = 0;
    replay->fps = 0.0;
    replay->mode = MODE_NONE;
}

static void cleanup_and_free(Replay *replay)
{
    cleanup(replay);
    free(replay);
}

static bool read_header(int fd, struct RecordingHeader *header, uint8_t **sizes_out)
{
    if (sizes_out) {
        *sizes_out = NULL;
    }
    if (!read_all(fd, header, sizeof(*header))) {
        log_e(LOG_TAG, "Failed to read header\n");
        return false;
    }

    if (strncmp(header->magic, MAGIC, 3) != 0 || header->version != VERSION) {
        log_e(LOG_TAG, "Invalid recording header\n");
        return false;
    }
    if (header->frame_type_count == 0) {
        log_e(LOG_TAG, "Recording has no frame types\n");
        return false;
    }

    header->core_name[sizeof(header->core_name) - 1] = '\0';
    header->core_version[sizeof(header->core_version) - 1] = '\0';

    uint8_t *sizes = malloc(header->frame_type_count);
    if (!sizes) {
        log_e(LOG_TAG, "Failed to allocate frame type sizes\n");
        return false;
    }
    if (!read_all(fd, sizes, header->frame_type_count)) {
        log_e(LOG_TAG, "Failed to read frame type sizes\n");
        free(sizes);
        return false;
    }
    if (sizes_out) {
        *sizes_out = sizes;
    } else {
        free(sizes);
    }
    return true;
}

static bool verify_footer(int fd, struct RecordingFooter *footer)
{
    off_t eoh = lseek(fd, 0, SEEK_CUR);
    if (eoh < 0) {
        log_e(LOG_TAG, "Failed to record header offset\n");
        return false;
    }

    if (lseek(fd, -(off_t)sizeof(struct RecordingFooter), SEEK_END) < 0) {
        log_e(LOG_TAG, "Failed to seek to footer\n");
        return false;
    }
    if (!read_all(fd, footer, sizeof(*footer))) {
        log_e(LOG_TAG, "Failed to read footer\n");
        return false;
    }

    if (lseek(fd, eoh, SEEK_SET) < 0) {
        log_e(LOG_TAG, "Failed to seek to end of header\n");
        return false;
    }

    return true;
}

static bool copy_file(int src, int dest, off_t src_off, off_t dest_off, uint64_t length)
{
    log_d(LOG_TAG, "Copying %llu bytes of recording\n",
        (unsigned long long)length);

    off_t off_in = src_off;
    off_t off_out = dest_off;
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

static void copy_core_field(char *dst, size_t dst_len, const char *src)
{
    memset(dst, 0, dst_len);
    if (src) {
        strncpy(dst, src, dst_len - 1);
    }
}

static uint32_t duration_ms_from(uint64_t frames, double fps)
{
    if (fps <= 0.0) {
        return 0;
    }
    double ms = (double)frames * 1000.0 / fps;
    if (ms >= (double)UINT32_MAX) {
        return UINT32_MAX;
    }
    return (uint32_t)(ms + 0.5);
}

static bool header_matches_runtime(const struct RecordingHeader *header,
    const uint8_t *file_sizes, bool for_resume)
{
    if (!file_sizes || frame_type_count == 0) {
        log_e(LOG_TAG, "Frame types not set\n");
        return false;
    }
    if (for_resume && header->frame_type_count > frame_type_count) {
        log_e(LOG_TAG, "Recording has more frame types (%u) than runtime (%u)\n",
            header->frame_type_count, frame_type_count);
        return false;
    }

    uint8_t n = header->frame_type_count < frame_type_count
        ? header->frame_type_count : frame_type_count;
    for (uint8_t i = 0; i < n; i++) {
        if (file_sizes[i] != frame_type_sizes[i]) {
            log_e(LOG_TAG, "Frame type %u size mismatch (%u != %u)\n",
                i, file_sizes[i], frame_type_sizes[i]);
            return false;
        }
    }

    if (system_info.library_name
        && strncmp(header->core_name, system_info.library_name, sizeof(header->core_name)) != 0) {
        log_e(LOG_TAG, "Core name mismatch (file '%s', runtime '%s')\n",
            header->core_name, system_info.library_name);
        return false;
    }
    if (system_info.library_version
        && strncmp(header->core_version, system_info.library_version, sizeof(header->core_version)) != 0) {
        log_w(LOG_TAG, "Core version mismatch (file '%s', runtime '%s')\n",
            header->core_version, system_info.library_version);
    }

    return true;
}

static bool frame_types_ready()
{
    if (frame_type_count == 0 || !frame_type_sizes || !frame_type_defaults) {
        return false;
    }
    for (int i = 0; i < frame_type_count; i++) {
        if (frame_type_sizes[i] == 0 || !frame_type_defaults[i]) {
            return false;
        }
    }
    return true;
}

static void free_frame_type_tables()
{
    if (frame_type_defaults) {
        for (int i = 0; i < frame_type_count; i++) {
            free(frame_type_defaults[i]);
        }
        free(frame_type_defaults);
        frame_type_defaults = NULL;
    }
    free(frame_type_sizes);
    frame_type_sizes = NULL;
    frame_type_count = 0;
}

static bool write_recording_header(int fd, uint64_t start_state_size)
{
    struct RecordingHeader header = {
        .magic = MAGIC,
        .version = VERSION,
        .start_state_uncompressed_size = start_state_size,
        .frame_type_count = frame_type_count,
    };
    copy_core_field(header.core_name, sizeof(header.core_name), system_info.library_name);
    copy_core_field(header.core_version, sizeof(header.core_version), system_info.library_version);
    return write_all(fd, &header, sizeof(header))
        && write_all(fd, frame_type_sizes, frame_type_count);
}

static bool alloc_session_frames(Replay *replay)
{
    replay->frame_type_count = frame_type_count;
    replay->frame_content = calloc(replay->frame_type_count, sizeof(void *));
    replay->frame_primed = calloc(replay->frame_type_count, sizeof(bool));
    replay->type_frames = calloc(replay->frame_type_count, sizeof(uint64_t));
    if (!replay->frame_content || !replay->frame_primed || !replay->type_frames) {
        return false;
    }

    uint8_t peek_size = 0;
    for (int i = 0; i < replay->frame_type_count; i++) {
        uint8_t size = frame_type_sizes[i];
        if (size > peek_size) {
            peek_size = size;
        }
        if (!size) {
            continue;
        }
        if (!(replay->frame_content[i] = malloc(size))) {
            return false;
        }
        memcpy(replay->frame_content[i], frame_type_defaults[i], size);
    }
    if (peek_size && !(replay->peek_payload = malloc(peek_size))) {
        return false;
    }
    return true;
}

static bool emit_event(Replay *replay, uint8_t frame_ix, uint32_t frame_index,
    const void *content, size_t size)
{
    struct FrameHeader header = {
        .frame_type = frame_ix,
        .frame_index = frame_index,
    };
    return gzwrite(replay->gz, &header, sizeof(header)) == (int)sizeof(header)
        && gzwrite(replay->gz, content, size) == (int)size;
}

static bool gz_skip(gzFile gz, size_t size)
{
    uint8_t buf[256];
    while (size > 0) {
        size_t n = size < sizeof(buf) ? size : sizeof(buf);
        if (gzread(gz, buf, n) != (int)n) {
            return false;
        }
        size -= n;
    }
    return true;
}

static bool refill_peek(Replay *replay)
{
    replay->peek_valid = false;
    while (replay->gz) {
        struct FrameHeader header;
        int n = gzread(replay->gz, &header, sizeof(header));
        if (n == 0) {
            return true;
        }
        if (n != (int)sizeof(header)) {
            return false;
        }
        if (header.frame_type >= replay->file_frame_type_count) {
            return false;
        }

        uint8_t payload_size = replay->file_frame_type_sizes[header.frame_type];
        if (header.frame_type >= replay->frame_type_count) {
            if (!gz_skip(replay->gz, payload_size)) {
                return false;
            }
            continue;
        }
        if (payload_size != frame_type_sizes[header.frame_type] || !replay->peek_payload) {
            return false;
        }
        if (gzread(replay->gz, replay->peek_payload, payload_size) != (int)payload_size) {
            return false;
        }
        replay->peek_type = header.frame_type;
        replay->peek_frame_index = header.frame_index;
        replay->peek_valid = true;
        return true;
    }
    return true;
}

static bool apply_events_through(Replay *replay, uint64_t frame_index)
{
    while (true) {
        if (!replay->peek_valid && !refill_peek(replay)) {
            return false;
        }
        if (!replay->peek_valid || (uint64_t)replay->peek_frame_index > frame_index) {
            return true;
        }
        uint8_t type = replay->peek_type;
        memcpy(replay->frame_content[type], replay->peek_payload, frame_type_sizes[type]);
        replay->peek_valid = false;
    }
}
